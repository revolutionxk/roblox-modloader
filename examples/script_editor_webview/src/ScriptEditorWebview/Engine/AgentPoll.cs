using System.Text.Json.Serialization;

namespace ScriptEditorWebview.Engine;

/// <summary>What the Luau agent reports back on one poll. Mirrors the payload built in agent.luau.</summary>
internal sealed class AgentPoll
{
    [JsonPropertyName("events")] public AgentEvent[] Events { get; init; } = [];

    [JsonPropertyName("applied")] public AgentApplied[] Applied { get; init; } = [];

    [JsonPropertyName("sourcemap")] public string? Sourcemap { get; init; }

    [JsonPropertyName("sourcemapNodes")] public int SourcemapNodes { get; init; }
}

/// <summary>
///     One thing that happened in the engine: a document opened or closed, its text changed, a
///     write failed, or the attach self-check answered.
/// </summary>
internal sealed class AgentEvent
{
    [JsonPropertyName("kind")] public string Kind { get; init; } = string.Empty;

    [JsonPropertyName("id")] public int Id { get; init; }

    [JsonPropertyName("name")] public string? Name { get; init; }

    [JsonPropertyName("uri")] public string? Uri { get; init; }

    [JsonPropertyName("text")] public string? Text { get; init; }

    /// <summary>How many of the editor's edits a "rejected" report covers.</summary>
    [JsonPropertyName("count")] public int Count { get; init; }

    [JsonPropertyName("ok")] public bool Ok { get; init; }

    [JsonPropertyName("message")] public string? Message { get; init; }
}

/// <summary>How many of the editor's own edits the engine committed for one document.</summary>
internal sealed class AgentApplied
{
    [JsonPropertyName("id")] public int Id { get; init; }

    [JsonPropertyName("count")] public int Count { get; init; }
}
