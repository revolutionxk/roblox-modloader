using System.Text.Json.Nodes;
using DiscordRPC;

namespace DiscordRpc;

internal sealed class DiscordPresenceService : IDisposable
{
    private const string ApplicationId = "1396335710755098757";
    private const string LargeImageText = "Roblox Studio";
    private const string SmallImageText = "RobloxModLoader";

    /// <summary>
    /// How long the web fallback stays muted at startup. The desktop pipe reports success through
    /// OnReady but never reports its absence, so only elapsed time can settle the question.
    /// </summary>
    private static readonly TimeSpan WebFallbackGrace = TimeSpan.FromSeconds(3);

    private readonly DiscordRpcClient _client;
    private readonly WebPresenceBridge _bridge = new();
    private readonly DateTime _sessionStart = DateTime.UtcNow;
    private readonly Timer _webFallbackTimer;
    private readonly object _gate = new();

    private JsonObject? _activity;
    private bool _desktopConnected;
    private bool _webFallbackReady;

    public DiscordPresenceService()
    {
        _client = new DiscordRpcClient(ApplicationId);
        _client.OnReady += OnDesktopReady;
        _client.OnClose += OnDesktopClosed;
        _client.OnError += (_, e) => DiscordRpc.Logger.Error($"Discord error: {e.Message}");
        _client.Initialize();

        // OnLoad publishes an activity immediately; letting it reach the web before OnReady has had
        // its chance would flash the same presence on both transports.
        _webFallbackTimer = new Timer(ReleaseWebFallback, null, WebFallbackGrace, Timeout.InfiniteTimeSpan);
    }

    public void Dispose()
    {
        try
        {
            // Detach first: disposing the client raises OnClose, which would republish the activity.
            _client.OnReady -= OnDesktopReady;
            _client.OnClose -= OnDesktopClosed;

            _webFallbackTimer.Dispose();

            lock (_gate)
            {
                _bridge.Publish(null);
            }

            // Outside the gate: the bridge blocks on its close handshake and on draining its tasks,
            // and a presence update arriving meanwhile must not queue behind that.
            _bridge.Dispose();

            if (_client.IsDisposed) return;

            _client.ClearPresence();
            _client.Dispose();
        }
        catch (Exception ex)
        {
            DiscordRpc.Logger.Error($"Error during shutdown: {ex.Message}");
        }
    }

    public void SetEditing(string placeName, string? thumbnailUrl)
    {
        Set($"Editing {placeName}", PresenceState.Editing, thumbnailUrl);
    }

    public void SetPlayTesting(string placeName, string? thumbnailUrl)
    {
        Set($"Play testing {placeName}", PresenceState.PlayTesting, thumbnailUrl);
    }

    public void SetIdle()
    {
        Set("In Roblox Studio", PresenceState.Idle, null);
    }

    private void Set(string details, PresenceState state, string? thumbnailUrl)
    {
        var largeImage = string.IsNullOrEmpty(thumbnailUrl) ? "studio" : thumbnailUrl;
        var smallImage = state switch
        {
            PresenceState.Idle => "idle",
            PresenceState.Editing => "editing",
            PresenceState.PlayTesting => "play",
            _ => "studio"
        };

        PublishWebActivity(BuildWebActivity(details, state, largeImage, smallImage));

        if (_client.IsDisposed) return;

        _client.SetPresence(new RichPresence
        {
            Details = details,
            State = state.ToString(),
            Timestamps = new Timestamps(_sessionStart),
            Assets = new Assets
            {
                LargeImageKey = largeImage,
                LargeImageText = LargeImageText,
                SmallImageKey = smallImage,
                SmallImageText = SmallImageText
            }
        });
    }

    /// <summary>
    /// Remembers the current activity and pushes it to Discord Web only while the desktop app is
    /// absent, so a running desktop client never shows the same activity twice.
    /// </summary>
    private void PublishWebActivity(JsonObject activity)
    {
        lock (_gate)
        {
            _activity = activity;

            if (_desktopConnected) return;

            // Still inside the startup grace: the activity is remembered above and the timer
            // publishes it once the desktop app has had its chance to claim the presence.
            if (!_webFallbackReady) return;

            _bridge.Publish(activity);
        }
    }

    /// <summary>Opens the web fallback once the desktop pipe has had time to connect.</summary>
    private void ReleaseWebFallback(object? state)
    {
        lock (_gate)
        {
            if (_webFallbackReady) return;

            _webFallbackReady = true;

            if (_desktopConnected || _activity is null) return;

            DiscordRpc.Logger.Info("Discord desktop did not connect; publishing the activity to Discord Web.");
            _bridge.Publish(_activity);
        }
    }

    private void OnDesktopReady(object sender, DiscordRPC.Message.ReadyMessage message)
    {
        DiscordRpc.Logger.Info($"Connected to Discord as {message.User.Username}; the Discord Web fallback is now idle.");
        SetDesktopConnected(true);
    }

    private void OnDesktopClosed(object sender, DiscordRPC.Message.CloseMessage message)
    {
        DiscordRpc.Logger.Info($"Discord desktop dropped the connection ({message.Reason}); falling back to Discord Web.");
        SetDesktopConnected(false);
    }

    private void SetDesktopConnected(bool connected)
    {
        lock (_gate)
        {
            if (_desktopConnected == connected) return;

            _desktopConnected = connected;

            // A transition is the desktop's own verdict, so the startup grace has nothing left to
            // wait for and the grace timer becomes a no-op.
            _webFallbackReady = true;

            _bridge.Publish(connected ? null : _activity);
        }
    }

    private JsonObject BuildWebActivity(string details, PresenceState state, string largeImage, string smallImage)
    {
        return new JsonObject
        {
            ["application_id"] = ApplicationId,
            ["type"] = 0,
            ["flags"] = 0,
            ["details"] = details,
            ["state"] = state.ToString(),
            ["timestamps"] = new JsonObject
            {
                ["start"] = new DateTimeOffset(_sessionStart, TimeSpan.Zero).ToUnixTimeMilliseconds()
            },
            ["assets"] = new JsonObject
            {
                ["large_image"] = largeImage,
                ["large_text"] = LargeImageText,
                ["small_image"] = smallImage,
                ["small_text"] = SmallImageText
            }
        };
    }

    private enum PresenceState
    {
        Idle,
        Editing,
        PlayTesting
    }
}