using System.Collections.Concurrent;
using RML.Core.Api;
using RML.Core.Modding;
using RML.Logging;
using Roblox;
using ScriptEditorWebview.Editor;
using ScriptEditorWebview.Engine;
using ScriptEditorWebview.Native;
using ScriptEditorWebview.Qt;
using ScriptEditorWebview.Threading;

namespace ScriptEditorWebview;

/// <summary>
///     Puts a Monaco editor over Studio's native script editor.
///     The engine half lives in <c>luau/agent.luau</c> and is driven from <see cref="EngineAgent" />;
///     this class owns only what Luau cannot reach: the Qt window to cover, the WebView on top of it,
///     and the language server beside it.
/// </summary>
[Mod("script-editor-webview", "0.1.0", Author = "Revolution", Description = "Monaco Editor on Roblox")]
public sealed class ScriptEditorWebviewMod : ModBase, IDataModelAware
{
    private const int ReconcileIntervalMs = 200;

    /// <summary>Ticks a document waits for a freshly created editor window before we guess one.</summary>
    private const int WidgetGraceTicks = 3;

    /// <summary>Ticks we give the engine to answer a self-check before treating it as broken.</summary>
    private const int CheckTimeoutTicks = 25;

    public new static readonly ILogger Logger = Log.CreateLogger("ScriptEditorWebview");

    private static readonly string[] ScriptEditorClassNames =
    [
        "StudioScriptEditor",
        "RBX::ScriptEditor::ScriptEditor",
        "ScriptTextEditorWidget"
    ];

    private readonly Dictionary<IntPtr, bool> _knownWidgets = [];
    private readonly Queue<IntPtr> _newWidgets = new();
    private readonly List<PendingDocument> _pending = [];

    // Mutated on the Qt GUI thread by the reconcile loop, and torn down on whichever thread unloads
    // the mod; those two never meet, so the map has to hold them apart itself.
    private readonly ConcurrentDictionary<int, ScriptEditorSession> _sessions = new();

    private EngineAgent? _agent;
    private string? _agentFailure;
    private bool _agentStarting;
    private bool _editDataModelLive;

    private GuiDispatcher? _gui;
    private IModsMenuAction? _modsAction;
    private bool _pollInFlight;
    private Timer? _reconcileTimer;
    private string? _sourcemap;

    public void OnDataModelLoaded(DataModel game, DataModelType dataModelType)
    {
        if (dataModelType != DataModelType.Edit) return;

        _editDataModelLive = true;
        Logger.Info("edit data model is live; starting the engine agent");
    }

    public void OnDataModelUnloaded(DataModel game, DataModelType dataModelType)
    {
        if (dataModelType != DataModelType.Edit) return;

        _editDataModelLive = false;

        _gui?.Post(() =>
        {
            DisposeAllSessions();
            _pending.Clear();
            _knownWidgets.Clear();
            _newWidgets.Clear();

            _agent?.Dispose();
            _agent = null;
            _agentFailure = null;
        });
    }

    public override int OnLoad()
    {
        try
        {
            _modsAction = ModsMenu.AddAction("Script Editor Webview: Reattach", RequestReattach);
        }
        catch (Exception ex)
        {
            Logger.Error($"failed to register the Mods menu action: {ex.Message}");
        }

        _gui = new GuiDispatcher();
        _reconcileTimer = new Timer(_ => OnReconcileTick(), null, TimeSpan.FromMilliseconds(250),
            TimeSpan.FromMilliseconds(ReconcileIntervalMs));

        Logger.Info($"loaded from '{Context.Directory}'");
        return 0;
    }

    public override void OnUnload()
    {
        _reconcileTimer?.Dispose();
        _reconcileTimer = null;

        _modsAction?.Dispose();
        _modsAction = null;

        DisposeAllSessions();

        _agent?.Dispose();
        _agent = null;

        _gui?.Dispose();
        _gui = null;

        Logger.Info("unloaded");
    }

    private void OnReconcileTick()
    {
        var gui = _gui;
        if (gui is null) return;

        if (!gui.IsReady)
        {
            gui.EnsureStarted(() => Logger.Info("GUI dispatcher online"));
            return;
        }

        gui.Post(Reconcile);
    }

    private void Reconcile()
    {
        if (_gui is null) return;

        if (!EnsureAgent()) return;

        TrackEditorWidgets();
        BeginPoll();
        AttachPendingDocuments();

        foreach (var session in _sessions.Values) session.SyncBounds();
    }

    /// <summary>
    ///     Brings the Luau agent up once the Edit data model and the Luau host are both there. A
    ///     failure is reported once per reason, so a Studio build that broke the contract leaves one
    ///     actionable line instead of a line every tick.
    /// </summary>
    private bool EnsureAgent()
    {
        if (_agent is { IsRunning: true }) return true;

        if (!_editDataModelLive || _agentStarting) return false;

        _agent ??= new EngineAgent(Context.GetPath("luau", "agent.luau"));
        _agentStarting = true;

        _agent.StartAsync().ContinueWith(task =>
        {
            var failure = task.IsFaulted ? task.Exception?.GetBaseException().Message : task.Result;

            _gui?.Post(() =>
            {
                _agentStarting = false;

                if (failure is null)
                {
                    _agentFailure = null;
                    Logger.Info("engine agent running: the script editor is driven from Luau");

                    // The regression check needs a document to type into and cannot click one open.
                    if (Environment.GetEnvironmentVariable("RML_MONACO_SELFTEST") is { Length: > 0 })
                    {
                        Logger.Info("RML_MONACO_SELFTEST is set: opening a scratch script document");
                        _agent?.OpenScratchDocument();
                    }

                    return;
                }

                if (_agentFailure == failure) return;

                _agentFailure = failure;
                Logger.Error($"the engine agent could not start: {failure}");
            });
        }, TaskContinuationOptions.ExecuteSynchronously);

        return false;
    }

    private void BeginPoll()
    {
        if (_pollInFlight || _agent is null) return;

        _pollInFlight = true;

        _agent.PollAsync().ContinueWith(task =>
        {
            var payload = task.IsFaulted ? null : task.Result;

            if (task.IsFaulted)
                Logger.Error($"polling the engine agent failed: {task.Exception?.GetBaseException().Message}");

            _gui?.Post(() =>
            {
                _pollInFlight = false;
                if (payload is not null) Consume(payload);
            });
        }, TaskContinuationOptions.ExecuteSynchronously);
    }

    private void Consume(AgentPoll payload)
    {
        // Acknowledgements first: a text report in the same payload describes the document after
        // those writes landed, and the editor only trusts a full-text push once it owes nothing.
        foreach (var applied in payload.Applied)
            if (_sessions.TryGetValue(applied.Id, out var acknowledged))
                acknowledged.OnEditsApplied(applied.Count);

        foreach (var report in payload.Events)
            switch (report.Kind)
            {
                case "open":
                    OnDocumentOpened(report);
                    break;

                case "close":
                    OnDocumentClosed(report.Id);
                    break;

                case "text":
                    if (report.Text is { } text && _sessions.TryGetValue(report.Id, out var textTarget))
                        textTarget.OnEngineText(text);

                    break;

                // A write the engine refused: the editor is counting that edit and has to be told,
                // or it waits for an acknowledgement that never comes.
                case "rejected":
                    if (_sessions.TryGetValue(report.Id, out var rejectedTarget))
                        rejectedTarget.OnEditsRejected(report.Count, report.Message);
                    else
                        Logger.Error($"the engine agent reported: {report.Message}");

                    break;

                case "error":
                    Logger.Error($"the engine agent reported: {report.Message}");
                    break;

                case "checked":
                    FinishAttach(report.Id, report.Ok, report.Message, report.Text);
                    break;
            }

        if (payload.Sourcemap is { Length: > 0 } sourcemap)
        {
            _sourcemap = sourcemap;
            Logger.Info($"sourcemap ready: {payload.SourcemapNodes} instances, {sourcemap.Length} bytes");
            foreach (var session in _sessions.Values) session.PushSourcemap(sourcemap);
        }
    }

    private void OnDocumentOpened(AgentEvent report)
    {
        if (_sessions.ContainsKey(report.Id) || _pending.Any(document => document.Id == report.Id)) return;

        _pending.Add(new PendingDocument(report.Id, report.Name ?? "<script>", report.Uri ?? string.Empty,
            report.Text ?? string.Empty));
    }

    private void OnDocumentClosed(int documentId)
    {
        _pending.RemoveAll(document => document.Id == documentId);

        if (_sessions.TryRemove(documentId, out var session))
        {
            _knownWidgets.Remove(session.EditorHwnd);
            session.Dispose();
        }
    }

    /// <summary>
    ///     Keeps the set of editor windows Studio currently has, and queues the ones that appeared
    ///     since the last tick. A document is paired with the window that showed up with it, which
    ///     beats picking the largest one: the largest visible editor is whichever tab you were on.
    /// </summary>
    private void TrackEditorWidgets()
    {
        foreach (var key in _knownWidgets.Keys.ToArray()) _knownWidgets[key] = false;

        foreach (var widget in QApplication.FindWidgets(IsScriptEditorWidget))
        {
            var hwnd = widget.WinId();
            if (hwnd == IntPtr.Zero) continue;

            if (_knownWidgets.ContainsKey(hwnd))
            {
                _knownWidgets[hwnd] = true;
                continue;
            }

            _knownWidgets[hwnd] = true;

            if (IsUsableEditorWindow(hwnd)) _newWidgets.Enqueue(hwnd);
        }

        foreach (var (hwnd, present) in _knownWidgets.ToArray())
            if (!present)
                _knownWidgets.Remove(hwnd);
    }

    private void AttachPendingDocuments()
    {
        if (_pending.Count == 0 || _agent is null) return;

        foreach (var document in _pending.ToArray())
        {
            document.Ticks++;

            if (document.Checking)
            {
                // The engine answers a check on its own thread; if it never does, the build is
                // broken in a way the overlay must not paper over.
                if (document.Ticks - document.CheckedAtTick > CheckTimeoutTicks)
                    Refuse(document, "the engine never answered the self-check");

                continue;
            }

            var hwnd = TakeEditorWindow(document.Ticks);
            if (hwnd == IntPtr.Zero) continue;

            document.Checking = true;
            document.CheckedAtTick = document.Ticks;
            document.Hwnd = hwnd;

            _agent.RequestCheck(document.Id);
        }
    }

    /// <summary>
    ///     Puts the overlay up only after a write has round-tripped through the engine on this build.
    ///     A failed check leaves Studio's own editor in place, which still works.
    ///     Monaco opens on the text the check just read, not on the text the open event carried: the
    ///     document may have been typed into since, and every range the editor then sends is
    ///     expressed against the buffer it was opened with.
    /// </summary>
    private void FinishAttach(int documentId, bool ok, string? message, string? text)
    {
        var document = _pending.FirstOrDefault(candidate => candidate.Id == documentId);
        if (document is null) return;

        document.Checking = false;

        if (!ok)
        {
            Refuse(document, message ?? "the self-check failed for an unknown reason");
            return;
        }

        if (_gui is null || _agent is null || !IsUsableEditorWindow(document.Hwnd))
        {
            document.Hwnd = IntPtr.Zero;
            return;
        }

        var session = new ScriptEditorSession(_gui, _agent, document.Id, document.Name, document.Uri,
            text ?? document.Text, document.Hwnd, Context);

        _pending.Remove(document);
        _sessions[document.Id] = session;

        session.Start();
        session.PushSourcemap(_sourcemap);

        Logger.Info($"attached editor overlay to '{document.Name}' (hwnd 0x{document.Hwnd:X})");
    }

    private void Refuse(PendingDocument document, string reason)
    {
        _pending.Remove(document);
        Logger.Error(
            $"refusing to cover the editor of '{document.Name}': writing to a script document does not work on this Studio build ({reason})");
    }

    /// <summary>
    ///     The window that appeared with this document, or — once a document has waited out the
    ///     grace period without a new window showing up, which is what happens for the documents
    ///     already open when the mod loads — the largest editor window nobody covers yet.
    /// </summary>
    private IntPtr TakeEditorWindow(int ticks)
    {
        while (_newWidgets.Count > 0)
        {
            var candidate = _newWidgets.Dequeue();
            if (IsUsableEditorWindow(candidate) && !IsAttached(candidate)) return candidate;
        }

        if (ticks < WidgetGraceTicks) return IntPtr.Zero;

        var best = IntPtr.Zero;
        long bestArea = -1;

        foreach (var hwnd in _knownWidgets.Keys)
        {
            if (IsAttached(hwnd) || !Win32.GetClientRect(hwnd, out var rect) || !IsUsableEditorWindow(hwnd)) continue;

            var area = (long)rect.Width * rect.Height;
            if (area <= bestArea) continue;

            bestArea = area;
            best = hwnd;
        }

        return best;
    }

    private bool IsAttached(IntPtr hwnd)
    {
        if (_sessions.Values.Any(session => session.EditorHwnd == hwnd)) return true;

        return _pending.Any(document => document.Hwnd == hwnd);
    }

    private static bool IsUsableEditorWindow(IntPtr hwnd)
    {
        if (hwnd == IntPtr.Zero || !Win32.IsWindowVisible(hwnd)) return false;

        if (!Win32.GetClientRect(hwnd, out var rect)) return false;

        return rect.Width is >= 200 and < 32000 && rect.Height is >= 120 and < 32000;
    }

    private static bool IsScriptEditorWidget(QWidget widget)
    {
        var className = widget.ClassName;
        if (string.IsNullOrEmpty(className)) return false;

        return ScriptEditorClassNames.Any(name => className == name || widget.Inherits(name)) ||
               className.Contains("ScriptEditor", StringComparison.Ordinal);
    }

    private void RequestReattach()
    {
        _gui?.Post(() =>
        {
            DisposeAllSessions();
            _pending.Clear();
            _knownWidgets.Clear();
            _newWidgets.Clear();

            _agent?.Resync();

            Logger.Info("reattach requested from the Mods menu");
        });
    }

    private void DisposeAllSessions()
    {
        // Take each one out before disposing it: a session the reconcile loop can still find is a
        // session it can still call into while it is being torn down.
        foreach (var id in _sessions.Keys.ToArray())
            if (_sessions.TryRemove(id, out var session))
                session.Dispose();
    }

    /// <summary>A document the engine announced, waiting for the window it belongs to.</summary>
    private sealed class PendingDocument(int id, string name, string uri, string text)
    {
        public readonly int Id = id;
        public readonly string Name = name;
        public readonly string Text = text;
        public readonly string Uri = uri;

        public bool Checking;
        public int CheckedAtTick;
        public IntPtr Hwnd;
        public int Ticks;
    }
}
