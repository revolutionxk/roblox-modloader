using System.Text.Json;
using System.Text.Json.Nodes;
using RML.Core.Api;
using Roblox;

namespace ScriptEditorWebview.Engine;

/// <summary>
///     The .NET side of the engine agent: it loads <c>luau/agent.luau</c> into the Edit data model's
///     Luau VM once and then talks to the table that script returns.
///     Everything this mod does to a script document goes through here, which keeps the mod standing
///     on the documented <c>ScriptEditorService</c> API instead of on the engine's memory layout. The
///     three calls it makes never yield on the engine side, so none of them can stall Studio: writes
///     are queued and applied by a coroutine the engine itself resumes.
/// </summary>
internal sealed class EngineAgent : IDisposable
{
    /// <summary>How long Dispose gives stop, and then the calls already inside the engine.</summary>
    private const int SettleTimeoutMs = 2000;

    private readonly string _agentPath;

    private int _inFlight;
    private volatile bool _stopping;

    private LuauRef? _poll;
    private LuauRef? _push;
    private LuauRef? _resync;
    private LuauRef? _text;
    private LuauRef? _check;
    private LuauRef? _openScratch;
    private LuauRef? _stop;
    private LuauRef? _table;

    public EngineAgent(string agentPath)
    {
        _agentPath = agentPath;
    }

    public bool IsRunning => _table is not null;

    public void Dispose()
    {
        _stopping = true;

        var stop = _stop;
        _stop = null;

        if (stop is not null)
            try
            {
                // Awaited, not fired and forgotten: the refs below are about to be released, and a
                // release that lands while the engine is inside one of these calls frees a handle
                // the VM still holds.
                if (!stop.InvokeAsync().Wait(SettleTimeoutMs))
                    ScriptEditorWebviewMod.Logger.Error(
                        $"the engine agent did not answer stop within {SettleTimeoutMs} ms");
            }
            catch (Exception ex)
            {
                ScriptEditorWebviewMod.Logger.Debug($"stopping the engine agent failed: {ex.Message}");
            }

        WaitForCallsToSettle();

        _push?.Dispose();
        _poll?.Dispose();
        _check?.Dispose();
        _text?.Dispose();
        _openScratch?.Dispose();
        _resync?.Dispose();
        stop?.Dispose();
        _table?.Dispose();

        _push = null;
        _poll = null;
        _check = null;
        _text = null;
        _openScratch = null;
        _resync = null;
        _table = null;
    }

    /// <summary>
    ///     Every call into the agent is counted here, because Dispose has to know what the engine
    ///     is still holding before it releases the refs those calls were made through.
    /// </summary>
    private Task<LuauValue> InvokeTracked(LuauRef target, params object?[] args)
    {
        Interlocked.Increment(ref _inFlight);

        try
        {
            return target.InvokeAsync(args).ContinueWith(task =>
            {
                Interlocked.Decrement(ref _inFlight);
                return task.GetAwaiter().GetResult();
            }, TaskContinuationOptions.ExecuteSynchronously);
        }
        catch
        {
            Interlocked.Decrement(ref _inFlight);
            throw;
        }
    }

    private void WaitForCallsToSettle()
    {
        var deadline = Environment.TickCount64 + SettleTimeoutMs;
        var spin = new SpinWait();

        while (Volatile.Read(ref _inFlight) > 0)
        {
            if (Environment.TickCount64 >= deadline)
            {
                ScriptEditorWebviewMod.Logger.Error(
                    $"{Volatile.Read(ref _inFlight)} engine call(s) never came back; releasing the agent refs anyway");
                return;
            }

            spin.SpinOnce();
        }
    }

    /// <summary>
    ///     Evaluates the agent script and binds its entry points. Returns the reason it could not
    ///     start, or null on success — the caller turns that into one actionable log line.
    /// </summary>
    public async Task<string?> StartAsync()
    {
        if (_table is not null) return null;

        if (!System.IO.File.Exists(_agentPath)) return $"the agent script is missing at '{_agentPath}'";

        if (!LuauScriptManager.IsReady(DataModelType.Edit)) return "the Luau host is not ready yet";

        string source;
        try
        {
            source = await System.IO.File.ReadAllTextAsync(_agentPath);
        }
        catch (Exception ex)
        {
            return $"reading '{_agentPath}' failed: {ex.Message}";
        }

        LuauRef? table;
        try
        {
            var result = await LuauScriptManager.EvaluateAsync(DataModelType.Edit, source, "@rml/monaco/agent");
            table = result.AsRef();
        }
        catch (Exception ex)
        {
            return $"evaluating the agent script failed: {ex.Message}";
        }

        if (table is null) return "the agent script did not return its table";

        var push = (await table.IndexAsync("push")).AsRef();
        var poll = (await table.IndexAsync("poll")).AsRef();
        var check = (await table.IndexAsync("check")).AsRef();
        var text = (await table.IndexAsync("text")).AsRef();
        var openScratch = (await table.IndexAsync("openScratch")).AsRef();
        var resync = (await table.IndexAsync("resync")).AsRef();
        var stop = (await table.IndexAsync("stop")).AsRef();

        if (push is null || poll is null || check is null || text is null || openScratch is null ||
            resync is null || stop is null)
        {
            table.Dispose();
            return "the agent table is missing one of push/poll/check/text/openScratch/resync/stop";
        }

        _table = table;
        _push = push;
        _poll = poll;
        _check = check;
        _text = text;
        _openScratch = openScratch;
        _resync = resync;
        _stop = stop;

        return null;
    }

    /// <summary>Asks the agent to announce every open document again, for the Mods menu reattach.</summary>
    public void Resync()
    {
        var resync = _resync;
        if (resync is null || _stopping) return;

        try
        {
            InvokeTracked(resync).ContinueWith(
                task => ScriptEditorWebviewMod.Logger.Error(
                    $"resyncing the open documents failed: {task.Exception?.GetBaseException().Message}"),
                TaskContinuationOptions.OnlyOnFaulted);
        }
        catch (Exception ex)
        {
            ScriptEditorWebviewMod.Logger.Error($"resyncing the open documents failed: {ex.Message}");
        }
    }

    /// <summary>
    ///     Queues one batch of editor edits and answers whether the engine took it. A batch the
    ///     agent refused was never queued, so the caller has to tell the editor: it counts the
    ///     edits it is owed, and an edit nobody will ever acknowledge stops its sync for good.
    /// </summary>
    public async Task<bool> PushEditsAsync(int documentId, JsonArray edits)
    {
        var push = _push;
        if (push is null || _stopping || edits.Count == 0) return false;

        var payload = new JsonObject
        {
            ["id"] = documentId,
            ["edits"] = edits.DeepClone()
        }.ToJsonString();

        try
        {
            return (await InvokeTracked(push, payload)).AsBoolean();
        }
        catch (Exception ex)
        {
            ScriptEditorWebviewMod.Logger.Error($"queueing an editor edit failed: {ex.Message}");
            return false;
        }
    }

    /// <summary>Drains everything that happened in the engine since the previous call.</summary>
    public async Task<AgentPoll?> PollAsync()
    {
        var poll = _poll;
        if (poll is null || _stopping) return null;

        var json = (await InvokeTracked(poll)).AsString();
        if (string.IsNullOrEmpty(json)) return null;

        try
        {
            return JsonSerializer.Deserialize<AgentPoll>(json);
        }
        catch (Exception ex)
        {
            ScriptEditorWebviewMod.Logger.Error($"the agent returned a payload we cannot read: {ex.Message}");
            return null;
        }
    }

    /// <summary>Text the engine holds for one document, for the sync check the editor can request.</summary>
    public async Task<string?> TextAsync(int documentId)
    {
        var text = _text;
        if (text is null || _stopping) return null;

        try
        {
            return (await InvokeTracked(text, documentId)).AsString();
        }
        catch (Exception ex)
        {
            ScriptEditorWebviewMod.Logger.Debug($"reading the document text failed: {ex.Message}");
            return null;
        }
    }

    /// <summary>Opens a scratch script document, so an automated check has something to type into.</summary>
    public void OpenScratchDocument()
    {
        var openScratch = _openScratch;
        if (openScratch is null || _stopping) return;

        try
        {
            InvokeTracked(openScratch).ContinueWith(
                task => ScriptEditorWebviewMod.Logger.Error(
                    $"opening the scratch document failed: {task.Exception?.GetBaseException().Message}"),
                TaskContinuationOptions.OnlyOnFaulted);
        }
        catch (Exception ex)
        {
            ScriptEditorWebviewMod.Logger.Error($"opening the scratch document failed: {ex.Message}");
        }
    }

    /// <summary>
    ///     Asks the engine to prove that a write reaches this document on this Studio build. The
    ///     answer arrives as a "checked" event on a later poll: the write yields, and a call into
    ///     Luau returns as soon as the thread parks, so a yielding result cannot come back inline.
    /// </summary>
    public void RequestCheck(int documentId)
    {
        var check = _check;
        if (check is null || _stopping) return;

        try
        {
            InvokeTracked(check, documentId).ContinueWith(
                task => ScriptEditorWebviewMod.Logger.Error(
                    $"asking the engine agent for a self-check failed: {task.Exception?.GetBaseException().Message}"),
                TaskContinuationOptions.OnlyOnFaulted);
        }
        catch (Exception ex)
        {
            ScriptEditorWebviewMod.Logger.Error($"asking the engine agent for a self-check failed: {ex.Message}");
        }
    }
}
