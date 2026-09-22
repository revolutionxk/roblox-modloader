using System.Net;
using System.Net.Sockets;
using System.Net.WebSockets;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json.Nodes;

namespace DiscordRpc;

/// <summary>
/// Publishes presence over the arRPC bridge protocol on loopback, so Discord Web clients
/// (for example Vencord's "WebRichPresence (arRPC)" plugin) can show the activity while the
/// Discord desktop app is not running. The desktop IPC transport is independent of this one.
/// </summary>
internal sealed class WebPresenceBridge : IDisposable
{
    private const int DefaultPort = 1337;
    private const string PortVariable = "RML_DISCORD_WEB_BRIDGE_PORT";
    private const string SocketId = "roblox-studio";
    private const string HandshakeGuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    private const int MaxHandshakeBytes = 8 * 1024;
    private const int MaxClients = 4;
    private const int MaxAcceptFailures = 8;

    /// <summary>A terminator can straddle two reads, so a scan carries the previous three bytes.</summary>
    private const int TerminatorOverlap = 3;

    private static readonly TimeSpan HandshakeTimeout = TimeSpan.FromSeconds(5);
    private static readonly TimeSpan CloseTimeout = TimeSpan.FromMilliseconds(500);
    private static readonly TimeSpan KeepAliveInterval = TimeSpan.FromSeconds(30);
    private static readonly TimeSpan AcceptRetryDelay = TimeSpan.FromMilliseconds(500);

    /// <summary>Browser origins allowed to read the activity; anything else is a local page snooping.</summary>
    private static readonly string[] AllowedOrigins =
    [
        "https://discord.com",
        "https://ptb.discord.com",
        "https://canary.discord.com"
    ];

    private readonly CancellationTokenSource _shutdown = new();
    private readonly List<BridgeClient> _clients = [];
    private readonly List<Task> _acceptTasks = [];
    private readonly List<Task> _serveTasks = [];
    private readonly TcpListener? _listener;
    private readonly TcpListener? _listenerV6;
    private JsonObject? _lastActivity;
    private int _disposed;

    public WebPresenceBridge()
    {
        var port = ResolvePort();
        if (port == 0)
        {
            DiscordRpc.Logger.Info($"Web presence bridge disabled by {PortVariable}.");
            return;
        }

        var listener = new TcpListener(IPAddress.Loopback, port);

        try
        {
            listener.Start();
        }
        catch (SocketException ex)
        {
            DiscordRpc.Logger.Warn($"Web presence bridge disabled, 127.0.0.1:{port} is taken ({ex.SocketErrorCode}).");
            return;
        }

        _listener = listener;

        // A client dialling ws://localhost can resolve to ::1, which the IPv4 socket never answers.
        // Both binds stay on loopback; neither ever accepts a routable address.
        var listenerV6 = new TcpListener(IPAddress.IPv6Loopback, port);

        try
        {
            listenerV6.Start();
            _listenerV6 = listenerV6;
        }
        catch (SocketException ex)
        {
            DiscordRpc.Logger.Info(
                $"Web presence bridge could not bind [::1]:{port} ({ex.SocketErrorCode}); only 127.0.0.1 will answer.");
        }

        DiscordRpc.Logger.Info(_listenerV6 is null
            ? $"Web presence bridge listening on 127.0.0.1:{port}."
            : $"Web presence bridge listening on 127.0.0.1:{port} and [::1]:{port}.");

        _acceptTasks.Add(Task.Run(() => AcceptLoopAsync(listener)));

        if (_listenerV6 is not null)
        {
            _acceptTasks.Add(Task.Run(() => AcceptLoopAsync(listenerV6)));
        }
    }

    /// <summary>Broadcasts an activity to every connected Discord Web client; null clears it.</summary>
    public void Publish(JsonObject? activity)
    {
        // The bridge is disabled, so no client can ever connect and nothing is worth remembering.
        if (_listener is null) return;

        BridgeClient[] targets;

        lock (_clients)
        {
            _lastActivity = activity;
            targets = _clients.ToArray();
        }

        // Nobody is connected: the next client replays the activity remembered above, so skip
        // serializing it on the engine thread here.
        if (targets.Length == 0) return;

        var message = BuildMessage(activity);

        foreach (var client in targets)
        {
            _ = SendAsync(client, () => message);
        }
    }

    public void Dispose()
    {
        if (Interlocked.Exchange(ref _disposed, 1) != 0) return;

        _listener?.Stop();
        _listenerV6?.Stop();

        // Cancel before anything that can throw: a faulting close must never strand a client, an
        // accept loop, or a serve task with a live token.
        _shutdown.Cancel();

        // Close politely anyway: a queued clear may still be on the wire.
        var clients = Snapshot();

        try
        {
            Task.WhenAll(clients.Select(client => client.CloseAsync())).Wait(CloseTimeout);
        }
        catch (Exception ex)
        {
            DiscordRpc.Logger.Error($"Web presence bridge failed to close a client cleanly: {ex.Message}");
        }

        foreach (var client in clients)
        {
            Drop(client);
        }

        // Accept loops first: once they are done no further serve task can be spawned, so the
        // wait below covers every one of them and nothing outlives this dispose.
        AwaitBounded(_acceptTasks);
        AwaitBounded(_serveTasks);

        _shutdown.Dispose();
    }

    private static int ResolvePort()
    {
        var configured = Environment.GetEnvironmentVariable(PortVariable);
        if (string.IsNullOrWhiteSpace(configured)) return DefaultPort;

        if (int.TryParse(configured, out var port) && port is >= 0 and <= ushort.MaxValue) return port;

        DiscordRpc.Logger.Warn($"Ignoring {PortVariable}='{configured}', expected a port between 0 and 65535.");

        return DefaultPort;
    }

    /// <summary>
    /// Wraps an activity in the arRPC envelope. The activity is cloned because a JsonNode accepts
    /// exactly one parent, and the same instance is published again every time a client connects
    /// or the desktop app hands the presence back.
    /// </summary>
    private static string BuildMessage(JsonObject? activity)
    {
        return new JsonObject
        {
            ["activity"] = activity?.DeepClone(),
            ["pid"] = Environment.ProcessId,
            ["socketId"] = SocketId
        }.ToJsonString();
    }

    /// <summary>Waits for background tasks without letting a stuck one hold the mod's unload.</summary>
    private static void AwaitBounded(List<Task> tasks)
    {
        lock (tasks)
        {
            try
            {
                Task.WhenAll(tasks).Wait(CloseTimeout);
            }
            catch (Exception)
            {
                // The tasks are cancelled and report their own failures; shutdown continues.
            }
        }
    }

    private BridgeClient[] Snapshot()
    {
        lock (_clients)
        {
            return _clients.ToArray();
        }
    }

    private void Drop(BridgeClient client)
    {
        lock (_clients)
        {
            _clients.Remove(client);
        }

        client.Dispose();
    }

    private async Task AcceptLoopAsync(TcpListener listener)
    {
        var failures = 0;

        while (!_shutdown.IsCancellationRequested)
        {
            try
            {
                var connection = await listener.AcceptTcpClientAsync(_shutdown.Token).ConfigureAwait(false);

                lock (_serveTasks)
                {
                    // Dispose has already drained the list; a task added now would never be waited
                    // on, so the connection is closed instead.
                    if (Volatile.Read(ref _disposed) != 0)
                    {
                        connection.Dispose();
                        return;
                    }

                    // Completed probes and past clients are no longer worth waiting for at dispose.
                    _serveTasks.RemoveAll(task => task.IsCompleted);
                    _serveTasks.Add(Task.Run(() => ServeAsync(connection)));
                }

                failures = 0;
            }
            catch (Exception ex) when (ex is OperationCanceledException or ObjectDisposedException ||
                                       Volatile.Read(ref _disposed) != 0)
            {
                return;
            }
            catch (Exception ex)
            {
                // A transient accept error (a peer resetting mid-handshake, for one) must not kill
                // the bridge for the rest of the session.
                if (++failures >= MaxAcceptFailures)
                {
                    DiscordRpc.Logger.Error(
                        $"Web presence bridge stopped accepting clients after {failures} consecutive failures: {ex.Message}");

                    return;
                }

                DiscordRpc.Logger.Warn(
                    $"Web presence bridge failed to accept a client ({failures}/{MaxAcceptFailures}): {ex.Message}");

                await Task.Delay(AcceptRetryDelay, _shutdown.Token).ConfigureAwait(false);
            }
        }
    }

    private async Task ServeAsync(TcpClient connection)
    {
        BridgeClient? client = null;
        var accepted = false;

        try
        {
            connection.NoDelay = true;

            var stream = connection.GetStream();
            var (request, oversize) = await ReadHandshakeAsync(stream).ConfigureAwait(false);

            if (request is null)
            {
                // A peer that sent nothing is a port probe, not a client worth a warning; one that
                // flooded the handshake buffer is worth saying out loud.
                if (oversize)
                {
                    DiscordRpc.Logger.Warn(
                        $"Rejected a presence bridge client: handshake exceeded {MaxHandshakeBytes} bytes.");
                }

                await RejectAsync(stream).ConfigureAwait(false);

                return;
            }

            var rejection = Validate(request);

            if (rejection is not null)
            {
                DiscordRpc.Logger.Warn($"Rejected a presence bridge client: {rejection}.");

                await RejectAsync(stream).ConfigureAwait(false);

                return;
            }

            await WriteAsync(stream, BuildHandshakeResponse(request["sec-websocket-key"])).ConfigureAwait(false);

            client = new BridgeClient(connection,
                WebSocket.CreateFromStream(stream, isServer: true, subProtocol: null, KeepAliveInterval));

            // Taking the slot inside the send callback keeps a concurrent Publish queued behind this
            // replay, so a client that connects mid-update never ends up with the older activity.
            await SendAsync(client, () =>
            {
                lock (_clients)
                {
                    if (_clients.Count >= MaxClients) return null;

                    _clients.Add(client);
                    accepted = true;

                    return _lastActivity is null ? null : BuildMessage(_lastActivity);
                }
            }).ConfigureAwait(false);

            if (!accepted)
            {
                DiscordRpc.Logger.Warn("Rejected a presence bridge client: too many connected clients.");
                await client.CloseAsync(WebSocketCloseStatus.PolicyViolation, "too many connected clients")
                    .ConfigureAwait(false);

                return;
            }

            // The replay may have dropped the client already.
            if (client.Socket.State != WebSocketState.Open) return;

            DiscordRpc.Logger.Info("Discord Web client connected to the presence bridge.");

            await DrainAsync(client).ConfigureAwait(false);
        }
        catch (Exception ex) when (ex is OperationCanceledException or ObjectDisposedException)
        {
            // Shutting down.
        }
        catch (WebSocketException)
        {
            // The tab went away without a close frame.
        }
        catch (Exception ex)
        {
            DiscordRpc.Logger.Error($"Web presence bridge client failed: {ex.Message}");
        }
        finally
        {
            if (client is null)
            {
                connection.Dispose();
            }
            else
            {
                Drop(client);

                if (accepted) DiscordRpc.Logger.Info("Discord Web client disconnected from the presence bridge.");
            }
        }
    }

    /// <summary>Turns a peer away; the caller's finally owns the connection either way.</summary>
    private async Task RejectAsync(NetworkStream stream)
    {
        try
        {
            await WriteAsync(stream, "HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n").ConfigureAwait(false);
        }
        catch (IOException)
        {
            // A port probe that hung up before reading the answer; routine, not an error.
        }
    }

    /// <summary>Reads incoming frames until the client closes; the bridge never trusts their content.</summary>
    private async Task DrainAsync(BridgeClient client)
    {
        var buffer = new byte[256];

        while (client.Socket.State == WebSocketState.Open)
        {
            var received = await client.Socket.ReceiveAsync(buffer, _shutdown.Token).ConfigureAwait(false);
            if (received.MessageType == WebSocketMessageType.Close) return;
        }
    }

    private async Task SendAsync(BridgeClient client, Func<string?> resolve)
    {
        try
        {
            await client.SendAsync(resolve, _shutdown.Token).ConfigureAwait(false);
        }
        catch (Exception ex) when (ex is OperationCanceledException or ObjectDisposedException or WebSocketException)
        {
            Drop(client);
        }
        catch (Exception ex)
        {
            DiscordRpc.Logger.Error($"Failed to push presence to a Discord Web client: {ex.Message}");
            Drop(client);
        }
    }

    private string? Validate(Dictionary<string, string>? request)
    {
        if (request is null) return "malformed handshake";
        if (!request.ContainsKey("sec-websocket-key")) return "not a WebSocket handshake";

        if (!request.TryGetValue("upgrade", out var upgrade) ||
            !upgrade.Equals("websocket", StringComparison.OrdinalIgnoreCase))
        {
            return "missing WebSocket upgrade";
        }

        if (!request.TryGetValue("sec-websocket-version", out var version))
        {
            return "missing Sec-WebSocket-Version header";
        }

        if (version.Trim() != "13")
        {
            return $"unsupported WebSocket version '{version}'";
        }

        if (!request.TryGetValue("origin", out var origin) ||
            !AllowedOrigins.Contains(origin, StringComparer.OrdinalIgnoreCase))
        {
            return $"origin '{origin ?? "none"}' is not a Discord client";
        }

        // The client budget is enforced once, when the slot is actually taken in ServeAsync, so a
        // client that loses that race is closed with 1008 instead of a bare HTTP 400.
        return null;
    }

    /// <summary>
    /// Reads the HTTP upgrade request. Null headers mean nothing usable arrived, and Oversize
    /// tells a handshake flood apart from a peer that simply hung up.
    /// </summary>
    private async Task<(Dictionary<string, string>? Headers, bool Oversize)> ReadHandshakeAsync(NetworkStream stream)
    {
        using var deadline = CancellationTokenSource.CreateLinkedTokenSource(_shutdown.Token);
        deadline.CancelAfter(HandshakeTimeout);

        var request = new StringBuilder();
        var buffer = new byte[1024];
        var tail = string.Empty;

        while (true)
        {
            var read = await stream.ReadAsync(buffer, deadline.Token).ConfigureAwait(false);
            if (read == 0) return (null, false);

            var chunk = Encoding.ASCII.GetString(buffer, 0, read);
            request.Append(chunk);

            // Scanning the new chunk plus the carried tail keeps the whole read O(n); rebuilding
            // the StringBuilder on every pass would be quadratic.
            var scanned = tail + chunk;
            if (scanned.Contains("\r\n\r\n", StringComparison.Ordinal)) break;

            tail = scanned.Length <= TerminatorOverlap ? scanned : scanned[^TerminatorOverlap..];

            if (request.Length >= MaxHandshakeBytes) return (null, true);
        }

        var headers = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);

        foreach (var line in request.ToString().Split("\r\n"))
        {
            var separator = line.IndexOf(':');
            if (separator < 0) continue;

            headers[line[..separator].Trim()] = line[(separator + 1)..].Trim();
        }

        return (headers, false);
    }

    private Task WriteAsync(NetworkStream stream, string response)
    {
        return stream.WriteAsync(Encoding.ASCII.GetBytes(response), _shutdown.Token).AsTask();
    }

    private static string BuildHandshakeResponse(string key)
    {
        var accept = Convert.ToBase64String(SHA1.HashData(Encoding.ASCII.GetBytes(key + HandshakeGuid)));

        return "HTTP/1.1 101 Switching Protocols\r\n" +
               "Upgrade: websocket\r\n" +
               "Connection: Upgrade\r\n" +
               $"Sec-WebSocket-Accept: {accept}\r\n\r\n";
    }

    private sealed class BridgeClient(TcpClient connection, WebSocket socket) : IDisposable
    {
        private readonly SemaphoreSlim _sendLock = new(1, 1);

        public WebSocket Socket { get; } = socket;

        /// <summary>Sends what <paramref name="resolve"/> returns, evaluated in send order.</summary>
        public async Task SendAsync(Func<string?> resolve, CancellationToken cancellation)
        {
            await _sendLock.WaitAsync(cancellation).ConfigureAwait(false);

            try
            {
                var message = resolve();
                if (message is null || Socket.State != WebSocketState.Open) return;

                await Socket.SendAsync(Encoding.UTF8.GetBytes(message), WebSocketMessageType.Text, true, cancellation)
                    .ConfigureAwait(false);
            }
            finally
            {
                _sendLock.Release();
            }
        }

        public async Task CloseAsync(
            WebSocketCloseStatus status = WebSocketCloseStatus.NormalClosure,
            string reason = "Studio is shutting down")
        {
            await _sendLock.WaitAsync().ConfigureAwait(false);

            try
            {
                if (Socket.State != WebSocketState.Open) return;

                await Socket.CloseAsync(status, reason, CancellationToken.None).ConfigureAwait(false);
            }
            catch (Exception ex) when (ex is WebSocketException or ObjectDisposedException or OperationCanceledException)
            {
                // The client is already gone.
            }
            finally
            {
                _sendLock.Release();
            }
        }

        /// <summary>The send lock is intentionally left undisposed: a send racing shutdown would
        /// otherwise fault on a disposed semaphore instead of seeing a closed socket.</summary>
        public void Dispose()
        {
            Socket.Dispose();
            connection.Dispose();
        }
    }
}
