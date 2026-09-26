using System.Runtime.InteropServices;

using RML.Interop;

namespace Roblox;

internal sealed class InstanceReference : SafeHandle
{
    private InstanceReference() : base(0, true)
    {
    }

    public override bool IsInvalid => handle == 0;

    public static InstanceReference Retain(nuint instance)
    {
        var reference = new InstanceReference();
        if (Interop.Reflection.InstanceRetain(instance))
        {
            reference.SetHandle((nint)instance);
        }

        return reference;
    }

    public static InstanceReference Adopt(nuint instance)
    {
        var reference = new InstanceReference();
        reference.SetHandle((nint)instance);
        return reference;
    }

    protected override bool ReleaseHandle()
    {
        Interop.Reflection.InstanceRelease((nuint)handle);
        return true;
    }
}
