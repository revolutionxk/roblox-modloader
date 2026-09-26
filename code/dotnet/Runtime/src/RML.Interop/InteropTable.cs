using System.Runtime.InteropServices;

namespace RML.Interop;

[StructLayout(LayoutKind.Explicit, Size = 16)]
public readonly struct InteropVariant
{
    [FieldOffset(0)] public readonly byte Tag;

    [FieldOffset(8)] public readonly ulong AsUInt64;
    [FieldOffset(8)] public readonly long AsInt64;
    [FieldOffset(8)] public readonly double AsDouble;
    [FieldOffset(8)] public readonly float AsFloat;
    [FieldOffset(8)] public readonly bool AsBool;
    [FieldOffset(8)] public readonly nuint AsPointer;

    private InteropVariant(byte tag, ulong payload) : this()
    {
        Tag = tag;
        AsUInt64 = payload;
    }

    public static class Tags
    {
        public const byte Null = 0;
        public const byte Bool = 1;
        public const byte Int64 = 2;
        public const byte Double = 3;
        public const byte Float = 4;
        public const byte String = 5;
        public const byte Instance = 6;
        public const byte InstanceArray = 7;
        public const byte Blittable = 8;
        public const byte Tuple = 9;
        public const byte LuauRef = 10;
    }

    public static InteropVariant Null => new(Tags.Null, 0);
    public static InteropVariant FromBool(bool v) => new(Tags.Bool, v ? 1ul : 0ul);
    public static InteropVariant FromInt64(long v) => new(Tags.Int64, unchecked((ulong)v));
    public static InteropVariant FromDouble(double v) => new(Tags.Double, BitConverter.DoubleToUInt64Bits(v));
    public static InteropVariant FromFloat(float v) => new(Tags.Float, BitConverter.SingleToUInt32Bits(v));
    public static InteropVariant FromPointer(nuint v) => new(Tags.Instance, v);
    public static InteropVariant FromString(nuint ptr) => new(Tags.String, ptr);
    public static InteropVariant FromBlittable(nuint ptr) => new(Tags.Blittable, ptr);
    public static InteropVariant FromTuple(nuint ptr) => new(Tags.Tuple, ptr);
    public static InteropVariant FromLuauRef(ulong handle) => new(Tags.LuauRef, handle);
}

internal static unsafe class NativeInterop
{
    public const int InteropTableVersion = 11;

    [StructLayout(LayoutKind.Sequential)]
    public struct InteropTable
    {
        public uint Version;
        public uint Size;

        public delegate* unmanaged[Cdecl]<void*, sbyte*, InteropVariant*, uint, InteropVariant*, void> ReflectionInvoke;
        public delegate* unmanaged[Cdecl]<void*, sbyte*, InteropVariant*, void> ReflectionGetProperty;
        public delegate* unmanaged[Cdecl]<void*, sbyte*, InteropVariant*, void> ReflectionSetProperty;

        public delegate* unmanaged[Cdecl]<void*, sbyte*, delegate* unmanaged[Cdecl]<void*, InteropVariant*, uint, void>,
            void*, nuint> ReflectionEventConnect;

        public delegate* unmanaged[Cdecl]<nuint, void> ReflectionEventDisconnect;

        public delegate* unmanaged[Cdecl]<sbyte*, int, nuint> CreateInstanceByName;

        public delegate* unmanaged[Cdecl]<int, sbyte*, int, void> Log;
        public delegate* unmanaged[Cdecl]<sbyte*, void> FreeString;
        public delegate* unmanaged[Cdecl]<void*, void> FreeNativePtr;

        public delegate* unmanaged[Cdecl]<nuint, sbyte*, delegate* unmanaged[Cdecl]<void*, InteropVariant*, uint, void>,
            void*, nuint> ModsMenuAddAction;
        public delegate* unmanaged[Cdecl]<nuint, sbyte*, nuint> ModsMenuAddSubmenu;
        public delegate* unmanaged[Cdecl]<nuint, nuint> ModsMenuAddSeparator;
        public delegate* unmanaged[Cdecl]<nuint, sbyte*, int, delegate* unmanaged[Cdecl]<void*, InteropVariant*, uint, void>,
            void*, nuint> ModsMenuAddCheckable;
        public delegate* unmanaged[Cdecl]<nuint, sbyte*, void> ModsMenuSetItemIcon;
        public delegate* unmanaged[Cdecl]<nuint, void> ModsMenuRemove;

        public delegate* unmanaged[Cdecl]<void*, sbyte*, InteropVariant*, uint,
            delegate* unmanaged[Cdecl]<void*, InteropVariant*, sbyte*, void>, void*, void> ReflectionInvokeAsync;

        public delegate* unmanaged[Cdecl]<void*, sbyte*, InteropVariant*, uint, void> ReflectionEventFire;
        public delegate* unmanaged[Cdecl]<void*, sbyte*, void> ReflectionEventDisconnectAll;

        public delegate* unmanaged[Cdecl]<void*, sbyte*, uint*, nuint*> ReflectionEventSlots;
        public delegate* unmanaged[Cdecl]<void*, sbyte*, nuint, InteropVariant*, uint, void> EventSlotFire;
        public delegate* unmanaged[Cdecl]<nuint, void> EventSlotDisconnect;
        public delegate* unmanaged[Cdecl]<nuint, void> EventSlotRelease;

        public delegate* unmanaged[Cdecl]<int, int> LuauHostReady;

        public delegate* unmanaged[Cdecl]<int, sbyte*, sbyte*,
            delegate* unmanaged[Cdecl]<void*, InteropVariant*, sbyte*, void>, void*, void> LuauSchedule;
        public delegate* unmanaged[Cdecl]<int, sbyte*, sbyte*,
            delegate* unmanaged[Cdecl]<void*, InteropVariant*, sbyte*, void>, void*, void> LuauEvaluate;

        public delegate* unmanaged[Cdecl]<nuint, InteropVariant*, uint,
            delegate* unmanaged[Cdecl]<void*, InteropVariant*, sbyte*, void>, void*, void> LuauRefCall;
        public delegate* unmanaged[Cdecl]<nuint, sbyte*,
            delegate* unmanaged[Cdecl]<void*, InteropVariant*, sbyte*, void>, void*, void> LuauRefIndex;
        public delegate* unmanaged[Cdecl]<nuint, void> LuauRefRelease;

        public delegate* unmanaged[Cdecl]<nuint, byte> InstanceRetain;
        public delegate* unmanaged[Cdecl]<nuint, void> InstanceRelease;
    }
}