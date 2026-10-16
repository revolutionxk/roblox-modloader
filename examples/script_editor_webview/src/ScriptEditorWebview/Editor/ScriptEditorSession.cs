using System.Text.Json.Nodes;
using RML.Core.Api;
using ScriptEditorWebview.Engine;
using ScriptEditorWebview.Lsp;
using ScriptEditorWebview.Native;
using ScriptEditorWebview.Threading;
using ScriptEditorWebview.WebView;

namespace ScriptEditorWebview.Editor;

/// <summary>
///     One Monaco instance over one native script editor. The session never touches the engine: it
///     hands the editor's edits to <see cref="EngineAgent" /> and receives the document's text back
///     from the poll loop, so the only Roblox contract it depends on lives in agent.luau.
/// </summary>
internal sealed class ScriptEditorSession : IDisposable
{
    private readonly EngineAgent _agent;
    private readonly GuiDispatcher _gui;
    private readonly LuauLspBridge _lsp;
    private readonly string _uri;
    private readonly string _webRoot;

    private readonly WebViewHost _webView;
    private bool _disposed;
    private bool _editorReady;

    private string _lastSyncedText = string.Empty;
    private volatile bool _lspReady;
    private string? _pendingText;
    private string? _sourcemap;

    public ScriptEditorSession(GuiDispatcher gui, EngineAgent agent, int documentId, string name, string uri,
        string text, IntPtr editorHwnd, ModContext context)
    {
        _gui = gui;
        _agent = agent;
        DocumentId = documentId;
        Name = name;
        _uri = string.IsNullOrEmpty(uri) ? "file:///rml/main.luau" : uri;
        _pendingText = text;
        EditorHwnd = editorHwnd;
        // The assembly lives in '<mod>/dotnet'; the payload beside it lives at the mod root, which
        // is what GetPath resolves against.
        _webRoot = context.GetPath("web");

        _webView = new WebViewHost(gui);
        _lsp = new LuauLspBridge(
            context.GetPath("tools", "bin", "luau-lsp.exe"),
            context.GetPath("tools", "cache", "globalTypes.PluginSecurity.d.luau"));
    }

    public int DocumentId { get; }

    /// <summary>Full name of the script this session mirrors, for logging.</summary>
    public string Name { get; }

    public IntPtr EditorHwnd { get; }

    public void Dispose()
    {
        if (_disposed) return;

        _disposed = true;

        _webView.MessageReceived -= OnWebMessage;
        _lsp.ServerMessage -= OnLspServerMessage;
        _lsp.Initialized -= OnLspInitialized;

        _lsp.Dispose();
        _webView.Dispose();
    }

    public void Start()
    {
        _webView.MessageReceived += OnWebMessage;

        _lsp.ServerMessage += OnLspServerMessage;
        _lsp.Initialized += OnLspInitialized;
        _lsp.Start();

        Win32.EnsureClipChildren(EditorHwnd);

        _webView.Initialize(EditorHwnd, _webRoot);
    }

    public void SyncBounds()
    {
        _webView.SyncBounds();
    }

    /// <summary>Text the engine reports for this document, on its way to Monaco.</summary>
    public void OnEngineText(string text)
    {
        if (_disposed) return;

        if (!_editorReady)
        {
            _pendingText = text;
            return;
        }

        PushTextToEditor(text);
    }

    /// <summary>
    ///     Tells the editor how many of its edits the engine committed. Monaco counts the ones it is
    ///     still owed and ignores a full-text push while any is outstanding, which is what keeps a
    ///     report that crossed a keystroke from rolling the buffer back.
    /// </summary>
    public void OnEditsApplied(int count)
    {
        if (_disposed || count <= 0) return;

        for (var i = 0; i < count; i++) _webView.PostMessage("{\"type\":\"editor.applied\"}");
    }

    /// <summary>
    ///     Tells the editor that edits it is counting never reached the document. Without this the
    ///     editor waits for an acknowledgement nobody will send and stops taking the engine's text
    ///     for the rest of the session.
    /// </summary>
    public void OnEditsRejected(int count, string? reason)
    {
        if (_disposed || count <= 0) return;

        ScriptEditorWebviewMod.Logger.Error(
            $"the engine did not take {count} edit(s) for '{Name}': {reason ?? "no reason given"}");

        var envelope = new JsonObject
        {
            ["type"] = "editor.rejected",
            ["count"] = count
        };
        _webView.PostMessage(envelope.ToJsonString());
    }

    public void PushSourcemap(string? sourcemap)
    {
        if (sourcemap is { Length: > 0 }) _sourcemap = sourcemap;

        if (_disposed || !_lspReady || _sourcemap is null or { Length: 0 }) return;

        try
        {
            _lsp.SendNotification("$/plugin/full", _sourcemap);
        }
        catch (Exception ex)
        {
            ScriptEditorWebviewMod.Logger.Debug($"pushing sourcemap failed: {ex.Message}");
        }
    }

    private void OnLspInitialized()
    {
        _lspReady = true;
        PushSourcemap(null);
    }

    private void OnWebMessage(string raw)
    {
        JsonObject? message;
        try
        {
            message = JsonNode.Parse(raw) as JsonObject;
        }
        catch
        {
            return;
        }

        var type = message?["type"]?.GetValue<string>();
        switch (type)
        {
            case "editor.ready":
                _gui.Post(() =>
                {
                    _editorReady = true;

                    if (_pendingText is { } text)
                    {
                        _pendingText = null;
                        PushTextToEditor(text);
                    }
                });
                break;

            // The editor asks for the engine's own copy of the document; a regression check compares
            // the two buffers to prove a Studio update did not break the round trip.
            case "editor.verify":
                _agent.TextAsync(DocumentId).ContinueWith(task =>
                {
                    var text = task.IsFaulted ? null : task.Result;
                    var envelope = new JsonObject
                    {
                        ["type"] = "editor.engineText",
                        ["text"] = text ?? string.Empty
                    };
                    _webView.PostMessage(envelope.ToJsonString());
                }, TaskContinuationOptions.ExecuteSynchronously);

                break;

            // The editor missed a text report while it still owed the engine edits; it is asking
            // for the current document now that it owes nothing.
            case "editor.resync":
                _agent.TextAsync(DocumentId).ContinueWith(task =>
                {
                    if (task.IsFaulted || task.Result is not { } current) return;

                    _gui.Post(() => PushTextToEditor(current, true));
                }, TaskContinuationOptions.ExecuteSynchronously);

                break;

            case "editor.edits":
                if (message?["edits"] is JsonArray edits && edits.Count > 0)
                    PushEdits(edits);

                break;

            case "lsp.message":
                var payload = message?["data"];
                if (payload is not null) _lsp.SendToServer(payload.ToJsonString());

                break;
        }
    }

    /// <summary>
    ///     Hands one batch to the engine and answers the editor either way: a batch the agent
    ///     refused was never queued, and the editor is counting every edit it sent.
    /// </summary>
    private void PushEdits(JsonArray edits)
    {
        var count = edits.Count;

        _agent.PushEditsAsync(DocumentId, edits).ContinueWith(task =>
        {
            if (task.IsCompletedSuccessfully && task.Result) return;

            var reason = task.IsFaulted
                ? task.Exception?.GetBaseException().Message
                : "the agent refused the batch";

            _gui.Post(() => OnEditsRejected(count, reason));
        }, TaskContinuationOptions.ExecuteSynchronously);
    }

    private void OnLspServerMessage(string json)
    {
        var envelope = $"{{\"type\":\"lsp.message\",\"data\":{json}}}";
        _webView.PostMessage(envelope);
    }

    private void PushTextToEditor(string text, bool force = false)
    {
        if (!force && text == _lastSyncedText) return;

        _lastSyncedText = text;

        var envelope = new JsonObject
        {
            ["type"] = "editor.setText",
            ["text"] = text,
            ["uri"] = _uri,
            ["remote"] = true
        };
        _webView.PostMessage(envelope.ToJsonString());
    }
}
