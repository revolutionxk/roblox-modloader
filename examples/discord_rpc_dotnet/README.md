# discord_rpc_dotnet

Managed Discord Rich Presence for Roblox Studio. It shows the place being edited or play tested,
with the session elapsed time and the place thumbnail.

## Transports

The mod publishes the same activity over two transports and always prefers the first one:

| Transport | Target | Notes |
|---|---|---|
| Discord IPC | Discord desktop app | Handled by the `DiscordRichPresence` package, as before |
| arRPC bridge | Discord Web | WebSocket server on `127.0.0.1:1337` and `[::1]:1337`, served by `WebPresenceBridge` |

While the desktop app is connected the bridge stays silent and clears whatever it had published, so
the activity is never shown twice. When the desktop app is closed, the current activity is pushed to
the web clients immediately, and vice versa when it comes back.

At startup the bridge holds the first web publish for a few seconds: the desktop app announces that
it connected but never announces that it is absent, so only a short wait can tell the two apart. A
desktop app that is still starting therefore claims the presence before the web ever shows it.

## Using the Discord Web fallback

Discord Web cannot host the local RPC server the desktop app provides, so the page needs a client
that listens for the activity. Any [arRPC](https://github.com/OpenAsar/arrpc) bridge client works;
the mod replaces the arRPC server itself, which does not need to run.

1. Install a Discord client mod in the browser, for example
   [Vencord](https://vencord.dev/download) (extension or userscript).
2. Enable the `WebRichPresence (arRPC)` plugin.
3. Start Studio. The plugin connects when it starts, so enable it (or hit `Retry` in its notice)
   after Studio is running.

If the port is already taken — a real arRPC server, or a second Studio — the bridge logs a warning
and stays disabled; the desktop transport is unaffected.

## Bridge configuration

`RML_DISCORD_WEB_BRIDGE_PORT` overrides the listening port; `0` disables the bridge entirely and an
unparseable value falls back to `1337` with a warning.

The bridge accepts at most 4 clients, only over loopback, and only from `discord.com`,
`ptb.discord.com` and `canary.discord.com` origins, so another page you visit cannot read which
place you have open. Anything a client sends is discarded.
