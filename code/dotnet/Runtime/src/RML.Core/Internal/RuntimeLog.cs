using RML.Logging;

namespace RML.Core.Internal;

internal static class RuntimeLog
{
    private static readonly ILogger Logger = Log.CreateLogger("RML");

    public static void Debug(string message) => Logger.Debug(message);
    public static void Info(string message) => Logger.Info(message);
    public static void Warn(string message) => Logger.Warn(message);
    public static void Error(string message) => Logger.Error(message);
}
