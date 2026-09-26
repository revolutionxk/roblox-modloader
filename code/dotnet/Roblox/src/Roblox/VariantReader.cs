using System.Runtime.InteropServices;

using RML.Interop;

namespace Roblox;

internal static unsafe class VariantReader
{
    public static object? Read(InteropVariant variant, Type targetType, bool freeNativeResources)
    {
        if (freeNativeResources && variant.Tag == InteropVariant.Tags.Instance && variant.AsPointer != 0)
        {
            try
            {
                return Read(variant, targetType, false);
            }
            finally
            {
                Interop.Reflection.InstanceRelease(variant.AsPointer);
            }
        }

        if (targetType == typeof(object[]) || targetType == typeof(object?[]))
        {
            return ReadTupleArray(variant, freeNativeResources);
        }

        if (variant.Tag == InteropVariant.Tags.Null)
        {
            return null;
        }

        Type? nullableUnderlying = Nullable.GetUnderlyingType(targetType);
        if (nullableUnderlying != null)
        {
            targetType = nullableUnderlying;
        }

        if (variant.Tag == InteropVariant.Tags.Blittable)
        {
            return ReadBlittable(variant, targetType, freeNativeResources);
        }

        if (targetType == typeof(string))
        {
            return ReadString(variant, freeNativeResources);
        }

        if (variant.Tag == InteropVariant.Tags.InstanceArray)
        {
            return ReadInstanceArray(variant, targetType, freeNativeResources);
        }

        if (variant.Tag == InteropVariant.Tags.Tuple)
        {
            return ReadTupleAsSingleOrArray(variant, targetType, freeNativeResources);
        }

        if (typeof(Object).IsAssignableFrom(targetType))
        {
            return ReadInstance(variant, targetType);
        }

        if (targetType == typeof(bool))
        {
            return ReadBool(variant, targetType);
        }

        if (targetType == typeof(double))
        {
            return ReadDouble(variant, targetType);
        }

        if (targetType == typeof(float))
        {
            return ReadFloat(variant, targetType);
        }

        if (targetType.IsEnum)
        {
            return ReadEnum(variant, targetType);
        }

        if (targetType == typeof(object))
        {
            return ReadUntyped(variant, freeNativeResources);
        }

        if (IsIntegral(targetType))
        {
            return ReadIntegral(variant, targetType);
        }

        throw new InvalidCastException(
            $"Cannot convert native variant tagged '{TagName(variant.Tag)}' to target type '{targetType}'.");
    }

    private static object?[] ReadTupleArray(InteropVariant variant, bool freeNativeResources)
    {
        if (variant.Tag != InteropVariant.Tags.Tuple)
        {
            return variant.Tag == InteropVariant.Tags.Null ? [] : [ReadUntyped(variant, freeNativeResources)];
        }

        if (variant.AsPointer == 0)
        {
            return [];
        }

        var buf = (byte*)variant.AsPointer;
        var count = *(ulong*)buf;
        var elements = (InteropVariant*)(buf + sizeof(ulong));

        var values = new object?[count];
        for (var i = 0ul; i < count; i++)
        {
            values[i] = ReadUntyped(elements[i], freeNativeResources);
        }

        if (freeNativeResources)
        {
            Interop.FreeNativeArray((nint)variant.AsPointer);
        }

        return values;
    }

    private static object? ReadTupleAsSingleOrArray(InteropVariant variant, Type targetType, bool freeNativeResources)
    {
        if (variant.AsPointer == 0)
        {
            return null;
        }

        var buf = (byte*)variant.AsPointer;
        var count = *(ulong*)buf;
        var elements = (InteropVariant*)(buf + sizeof(ulong));

        object? result = count == 1
            ? Read(elements[0], targetType, freeNativeResources)
            : ReadUntypedTupleElements(elements, count, freeNativeResources);

        if (freeNativeResources)
        {
            Interop.FreeNativeArray((nint)variant.AsPointer);
        }

        return result;
    }

    private static object?[] ReadUntypedTupleElements(InteropVariant* elements, ulong count, bool freeNativeResources)
    {
        var values = new object?[count];
        for (var i = 0ul; i < count; i++)
        {
            values[i] = ReadUntyped(elements[i], freeNativeResources);
        }

        return values;
    }

    private static object? ReadUntyped(InteropVariant variant, bool freeNativeResources)
    {
        switch (variant.Tag)
        {
            case InteropVariant.Tags.Null:
                return null;
            case InteropVariant.Tags.Bool:
                return variant.AsBool;
            case InteropVariant.Tags.Int64:
                return variant.AsInt64;
            case InteropVariant.Tags.Double:
                return variant.AsDouble;
            case InteropVariant.Tags.Float:
                return variant.AsFloat;
            case InteropVariant.Tags.String:
                return ReadString(variant, freeNativeResources);
            case InteropVariant.Tags.Instance:
                if (variant.AsPointer == 0)
                {
                    return null;
                }

                try
                {
                    return RobloxTypeRegistry.Create(variant.AsPointer);
                }
                finally
                {
                    if (freeNativeResources)
                    {
                        Interop.Reflection.InstanceRelease(variant.AsPointer);
                    }
                }
            case InteropVariant.Tags.InstanceArray:
                return ReadInstanceArray(variant, typeof(List<Instance>), freeNativeResources);
            case InteropVariant.Tags.Tuple:
                return ReadTupleArray(variant, freeNativeResources);
            case InteropVariant.Tags.Blittable:
                if (freeNativeResources && variant.AsPointer != 0)
                {
                    Interop.FreeNativeArray((nint)variant.AsPointer);
                }

                throw new InvalidCastException(
                    "Tuple element carries a Blittable payload with no target type to decode it against; " +
                    "the native marshaller does not put raw blittable values inside a tuple without a known type.");
            default:
                throw new InvalidCastException($"Unrecognized native variant tag '{variant.Tag}' in tuple element.");
        }
    }

    private static object? ReadString(InteropVariant variant, bool freeNativeResources)
    {
        if (variant.Tag != InteropVariant.Tags.String)
        {
            throw Mismatch(variant, typeof(string), InteropVariant.Tags.String);
        }

        var ptr = (nint)variant.AsPointer;
        if (ptr == 0)
        {
            return null;
        }

        var s = Marshal.PtrToStringUTF8(ptr);
        if (freeNativeResources)
        {
            Interop.FreeNativeString(ptr);
        }

        return s;
    }

    private static object ReadInstanceArray(InteropVariant variant, Type targetType, bool freeNativeResources)
    {
        var list = new List<Instance>();

        if (variant.AsPointer != 0)
        {
            var buf = (byte*)variant.AsPointer;
            var count = *(uint*)buf;
            var handles = (nuint*)(buf + sizeof(ulong));

            list.Capacity = (int)count;
            try
            {
                for (var i = 0u; i < count; i++)
                {
                    var handle = handles[i];
                    if (handle != 0)
                    {
                        list.Add((Instance)RobloxTypeRegistry.Create(handle));
                    }
                }
            }
            finally
            {
                if (freeNativeResources)
                {
                    for (var i = 0u; i < count; i++)
                    {
                        Interop.Reflection.InstanceRelease(handles[i]);
                    }

                    Interop.FreeNativeArray((nint)variant.AsPointer);
                }
            }
        }

        if (targetType == typeof(Instance[]) || (targetType.IsArray && targetType.GetElementType() == typeof(Instance)))
        {
            return list.ToArray();
        }

        return list;
    }

    private static object? ReadInstance(InteropVariant variant, Type targetType)
    {
        if (variant.Tag != InteropVariant.Tags.Instance)
        {
            throw Mismatch(variant, targetType, InteropVariant.Tags.Instance);
        }

        var handle = variant.AsPointer;
        if (handle == 0)
        {
            return null;
        }

        Object instance = RobloxTypeRegistry.Create(handle);
        return targetType.IsInstanceOfType(instance) ? instance : RobloxTypeRegistry.CreateAs(targetType, handle);
    }

    private static bool ReadBool(InteropVariant variant, Type targetType)
    {
        if (variant.Tag != InteropVariant.Tags.Bool)
        {
            throw Mismatch(variant, targetType, InteropVariant.Tags.Bool);
        }

        return variant.AsBool;
    }

    private static double ReadDouble(InteropVariant variant, Type targetType) => variant.Tag switch
    {
        InteropVariant.Tags.Double => variant.AsDouble,
        InteropVariant.Tags.Float => variant.AsFloat,
        _ => throw Mismatch(variant, targetType, InteropVariant.Tags.Double)
    };

    private static float ReadFloat(InteropVariant variant, Type targetType)
    {
        if (variant.Tag != InteropVariant.Tags.Float)
        {
            throw Mismatch(variant, targetType, InteropVariant.Tags.Float);
        }

        return variant.AsFloat;
    }

    private static object ReadEnum(InteropVariant variant, Type targetType)
    {
        if (variant.Tag != InteropVariant.Tags.Int64)
        {
            throw Mismatch(variant, targetType, InteropVariant.Tags.Int64);
        }

        var underlyingType = System.Enum.GetUnderlyingType(targetType);
        var raw = IsUnsigned(underlyingType) ? (object)variant.AsUInt64 : variant.AsInt64;
        return System.Enum.ToObject(targetType, Convert.ChangeType(raw, underlyingType));
    }

    private static object ReadIntegral(InteropVariant variant, Type targetType)
    {
        if (variant.Tag != InteropVariant.Tags.Int64)
        {
            throw Mismatch(variant, targetType, InteropVariant.Tags.Int64);
        }

        var raw = IsUnsigned(targetType) ? (object)variant.AsUInt64 : variant.AsInt64;
        return Convert.ChangeType(raw, targetType);
    }

    private static object? ReadBlittable(InteropVariant variant, Type targetType, bool freeNativeResources)
    {
        if (variant.AsPointer == 0)
        {
            return null;
        }

        var ptr = (nint)variant.AsPointer;

        object? value;
        if (targetType == typeof(NumberSequence))
        {
            value = ReadNumberSequence(ptr);
        }
        else if (targetType == typeof(ColorSequence))
        {
            value = ReadColorSequence(ptr);
        }
        else if (targetType == typeof(byte[]))
        {
            value = ReadByteArray(ptr);
        }
        else if (IsScalarOrReferenceTargetType(targetType))
        {
            throw new InvalidCastException(
                $"Native variant is tagged Blittable but target type '{targetType}' expects a different wire representation.");
        }
        else
        {
            value = Marshal.PtrToStructure(ptr, targetType);
        }

        if (freeNativeResources)
        {
            Interop.FreeNativeArray(ptr);
        }

        return value;
    }

    private static bool IsScalarOrReferenceTargetType(Type t)
        => t == typeof(string) || t == typeof(bool) || t == typeof(double) || t == typeof(float) ||
           t == typeof(object) || t.IsEnum || IsIntegral(t) || typeof(Object).IsAssignableFrom(t);

    private static bool IsIntegral(Type t)
        => t == typeof(byte) || t == typeof(sbyte) || t == typeof(short) || t == typeof(ushort) ||
           t == typeof(int) || t == typeof(uint) || t == typeof(long) || t == typeof(ulong) ||
           t == typeof(nint) || t == typeof(nuint);

    private static bool IsUnsigned(Type t)
        => t == typeof(byte) || t == typeof(ushort) || t == typeof(uint) || t == typeof(ulong) || t == typeof(nuint);

    private static NumberSequence ReadNumberSequence(nint ptr)
    {
        NumberSequenceKeypoint[] keys = ReadKeypoints<NumberSequenceKeypoint>(ptr);
        return NumberSequence.FromEngine(keys);
    }

    private static ColorSequence ReadColorSequence(nint ptr)
    {
        ColorSequenceKeypoint[] keys = ReadKeypoints<ColorSequenceKeypoint>(ptr);
        return ColorSequence.FromEngine(keys);
    }

    private static T[] ReadKeypoints<T>(nint ptr) where T : struct
    {
        var count = Marshal.ReadInt32(ptr);
        if (count <= 0)
        {
            return [];
        }

        var stride = Marshal.SizeOf<T>();
        var elems = ptr + sizeof(int);
        var keys = new T[count];
        for (var i = 0; i < count; i++)
        {
            keys[i] = Marshal.PtrToStructure<T>(elems + i * stride);
        }

        return keys;
    }

    private static byte[] ReadByteArray(nint ptr)
    {
        var count = Marshal.ReadInt32(ptr);
        if (count <= 0)
        {
            return [];
        }

        var bytes = new byte[count];
        Marshal.Copy(ptr + sizeof(int), bytes, 0, count);
        return bytes;
    }

    private static InvalidCastException Mismatch(InteropVariant variant, Type targetType, byte expectedTag)
        => new(
            $"Cannot convert native variant tagged '{TagName(variant.Tag)}' to '{targetType}'; expected tag '{TagName(expectedTag)}'.");

    private static string TagName(byte tag) => tag switch
    {
        InteropVariant.Tags.Null => nameof(InteropVariant.Tags.Null),
        InteropVariant.Tags.Bool => nameof(InteropVariant.Tags.Bool),
        InteropVariant.Tags.Int64 => nameof(InteropVariant.Tags.Int64),
        InteropVariant.Tags.Double => nameof(InteropVariant.Tags.Double),
        InteropVariant.Tags.Float => nameof(InteropVariant.Tags.Float),
        InteropVariant.Tags.String => nameof(InteropVariant.Tags.String),
        InteropVariant.Tags.Instance => nameof(InteropVariant.Tags.Instance),
        InteropVariant.Tags.InstanceArray => nameof(InteropVariant.Tags.InstanceArray),
        InteropVariant.Tags.Blittable => nameof(InteropVariant.Tags.Blittable),
        InteropVariant.Tags.Tuple => nameof(InteropVariant.Tags.Tuple),
        _ => $"Unknown({tag})"
    };
}
