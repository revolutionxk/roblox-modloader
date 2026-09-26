namespace Roblox;

/// <summary>
///     Root base class for all Roblox engine objects.
///     Wraps a native (C++) pointer and exposes it via the reflection system.
/// </summary>
/// <remarks>
///     API members declared in the Roblox API dump are generated into
///     <c>Generated/Classes/Object.Members.cs</c>.  Do not edit that file manually.
/// </remarks>
public partial class Object : IDisposable
{
    internal readonly nuint Handle;
    private readonly InstanceReference _reference;

    internal Object(nuint handle)
    {
        if (handle == 0)
        {
            throw new ArgumentException("Cannot wrap a null native handle", nameof(handle));
        }

        Handle = handle;
        _reference = InstanceReference.Retain(handle);
    }

    protected Object(string className)
    {
        Handle = Reflection.CreateOwnedHandle(className, CreatorRole.Engine);
        _reference = InstanceReference.Adopt(Handle);
    }

    public void Dispose() => _reference.Dispose();

    public override bool Equals(object? obj)
        => obj is Object other && other.Handle == Handle;

    public override int GetHashCode() => Handle.GetHashCode();

    protected internal void AddEventHandler(string eventName, Delegate handler)
        => EventManager.Add(Handle, eventName, handler);

    protected internal void RemoveEventHandler(string eventName, Delegate handler)
        => EventManager.Remove(Handle, eventName, handler);

    public IReadOnlyList<EventConnection> GetConnections(string eventName)
    {
        var handles = EngineEvents.Slots(Handle, eventName);
        var connections = new EventConnection[handles.Length];
        for (var i = 0; i < handles.Length; i++)
        {
            connections[i] = new EventConnection(Handle, eventName, handles[i]);
        }

        return connections;
    }

    public void FireEvent(string eventName, params object?[] args)
        => EngineEvents.Fire(Handle, eventName, args);

    public void DisconnectAll(string eventName)
        => EngineEvents.DisconnectAll(Handle, eventName);

    public bool IsA<T>() where T : Object
        => this is T || (RobloxTypeRegistry.ActualType(Handle) is { } actual && typeof(T).IsAssignableFrom(actual));

    public T? As<T>() where T : Object
        => this is T cast
            ? cast
            : RobloxTypeRegistry.ActualType(Handle) is { } actual && typeof(T).IsAssignableFrom(actual)
                ? RobloxTypeRegistry.CreateAs<T>(Handle)
                : null;

    public T Cast<T>() where T : Object
        => As<T>() ?? throw new InvalidCastException(
            $"Instance of type '{GetType().Name}' is not a '{typeof(T).Name}'.");
}