using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text;

using RML.Interop;

namespace Roblox;

public static unsafe class Reflection
{
    public static Task<T?> InvokeAsync<T>(Object @object, string methodName, params object?[] args)
    {
        ArgumentNullException.ThrowIfNull(@object);
        var task = InvokeAsync<T>(@object.Handle, methodName, args);
        GC.KeepAlive(@object);
        return task;
    }

    public static Task<T?> InvokeAsync<T>(nuint handle, string methodName, params object?[] args)
    {
        ArgumentNullException.ThrowIfNull(methodName);

        if (handle == 0)
        {
            throw new ArgumentException("Instance handle is null", nameof(handle));
        }

        var tcs = new TaskCompletionSource<T?>(TaskCreationOptions.RunContinuationsAsynchronously);

        var call = new AsyncCall
        {
            Complete = (variant, error) =>
            {
                if (error is not null)
                {
                    tcs.TrySetException(new InvalidOperationException(error));
                    return;
                }

                try
                {
                    tcs.TrySetResult((T?)ConvertResult(variant, typeof(T), true));
                }
                catch (Exception ex)
                {
                    tcs.TrySetException(ex);
                }
            }
        };

        var gcHandle = GCHandle.Alloc(call);
        try
        {
            var callback = (delegate* unmanaged[Cdecl]<void*, InteropVariant*, sbyte*, void>)&AsyncComplete;
            Interop.Reflection.InvokeAsync((void*)handle, methodName, callback, (void*)GCHandle.ToIntPtr(gcHandle),
                ToNativeArguments(args));
            GC.KeepAlive(args);
        }
        catch
        {
            gcHandle.Free();
            throw;
        }

        return tcs.Task;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static void AsyncComplete(void* state, InteropVariant* result, sbyte* error)
    {
        var handle = GCHandle.FromIntPtr((nint)state);
        try
        {
            var call = (AsyncCall)handle.Target!;
            var err = error == null ? null : Marshal.PtrToStringUTF8((nint)error);
            InteropVariant variant = result == null ? InteropVariant.Null : *result;
            call.Complete(variant, err);
        }
        catch
        {
        }
        finally
        {
            handle.Free();
        }
    }

    public static T? Invoke<T>(Object @object, string methodName, params object?[] args)
    {
        ArgumentNullException.ThrowIfNull(@object);

        var result = Invoke<T>(@object.Handle, methodName, args);
        GC.KeepAlive(@object);
        return result;
    }

    public static T? Invoke<T>(nuint handle, string methodName, params object?[] args)
    {
        ArgumentNullException.ThrowIfNull(methodName);

        if (handle == 0)
        {
            throw new ArgumentException("Instance handle is null", nameof(handle));
        }

        InteropVariant variant = Interop.Reflection.Invoke((void*)handle, methodName, ToNativeArguments(args));
        GC.KeepAlive(args);
        return ConvertResult<T>(variant);
    }

    public static T? GetProperty<T>(Object @object, string propertyName)
    {
        ArgumentNullException.ThrowIfNull(@object);

        var value = GetProperty<T>(@object.Handle, propertyName);
        GC.KeepAlive(@object);
        return value;
    }

    public static T? GetProperty<T>(nuint handle, string propertyName)
    {
        ArgumentNullException.ThrowIfNull(propertyName);
        if (handle == 0)
        {
            throw new ArgumentException("Instance handle is null", nameof(handle));
        }

        InteropVariant variant = Interop.Reflection.GetProperty((void*)handle, propertyName);
        return ConvertResult<T>(variant);
    }

    public static void SetProperty<T>(Object @object, string propertyName, T value)
    {
        ArgumentNullException.ThrowIfNull(@object);

        SetProperty(@object.Handle, propertyName, value);
        GC.KeepAlive(@object);
        GC.KeepAlive(value);
    }

    public static void SetProperty<T>(nuint handle, string propertyName, T value)
    {
        ArgumentNullException.ThrowIfNull(propertyName);

        if (handle == 0)
        {
            throw new ArgumentException("Instance handle is null", nameof(handle));
        }

        if (value is null)
        {
            Interop.Reflection.SetProperty((void*)handle, propertyName, default);
            return;
        }

        if (value is string s)
        {
            var b = Encoding.UTF8.GetBytes(s);
            var strPtr = Marshal.AllocHGlobal(b.Length + 1);
            try
            {
                Marshal.Copy(b, 0, strPtr, b.Length);
                Marshal.WriteByte(strPtr + b.Length, 0);
                Interop.Reflection.SetProperty((void*)handle, propertyName,
                    InteropVariant.FromString((nuint)(ulong)strPtr));
            }
            finally
            {
                Marshal.FreeHGlobal(strPtr);
            }

            return;
        }

        if (value is NumberSequence ns)
        {
            WriteSequence(handle, propertyName, ns.Keypoints);
            return;
        }

        if (value is ColorSequence cs)
        {
            WriteSequence(handle, propertyName, cs.Keypoints);
            return;
        }

        if (value is IRobloxDataType)
        {
            var size = Marshal.SizeOf((object)value);
            var buffer = Marshal.AllocHGlobal(size);
            try
            {
                Marshal.StructureToPtr((object)value, buffer, false);
                Interop.Reflection.SetProperty((void*)handle, propertyName,
                    InteropVariant.FromBlittable((nuint)buffer));
            }
            finally
            {
                Marshal.FreeHGlobal(buffer);
            }

            return;
        }

        InteropVariant variantValue = value switch
        {
            Object inst => InteropVariant.FromPointer(inst.Handle),
            bool bb => InteropVariant.FromBool(bb),
            double dd => InteropVariant.FromDouble(dd),
            float ff => InteropVariant.FromFloat(ff),
            System.Enum => InteropVariant.FromInt64(Convert.ToInt64(value)),
            nuint nu => InteropVariant.FromPointer(nu),
            nint ni => InteropVariant.FromPointer((nuint)ni),
            _ => InteropVariant.FromInt64(ToInt64OrThrow(value, propertyName))
        };

        Interop.Reflection.SetProperty((void*)handle, propertyName, variantValue);
    }

    public static T CreateInstance<T>(CreatorRole role) where T : Instance
    {
        var handle = CreateOwnedHandle(RobloxTypeRegistry.ClassNameOf<T>(), role);
        try
        {
            return RobloxTypeRegistry.CreateAs<T>(handle);
        }
        finally
        {
            Interop.Reflection.InstanceRelease(handle);
        }
    }

    public static T CreateInstance<T>() where T : Instance => CreateInstance<T>(CreatorRole.Engine);

    public static Instance CreateInstance(string className, CreatorRole role)
    {
        var handle = CreateOwnedHandle(className, role);
        try
        {
            return (Instance)RobloxTypeRegistry.Create(handle);
        }
        finally
        {
            Interop.Reflection.InstanceRelease(handle);
        }
    }

    public static Instance CreateInstance(string className) => CreateInstance(className, CreatorRole.Engine);

    internal static nuint CreateOwnedHandle(string className, CreatorRole role)
    {
        var handle = Interop.Reflection.CreateInstanceByName(className, (int)role);
        return handle == 0
            ? throw new InvalidOperationException($"Failed to create instance of type '{className}'")
            : handle;
    }

    private static object?[] ToNativeArguments(object?[] args)
    {
        object?[]? converted = null;
        for (var i = 0; i < args.Length; i++)
        {
            if (args[i] is Object instance)
            {
                converted ??= (object?[])args.Clone();
                converted[i] = instance.Handle;
            }
        }

        return converted ?? args;
    }

    private static long ToInt64OrThrow(object value, string propertyName)
    {
        try
        {
            return Convert.ToInt64(value);
        }
        catch (Exception ex)
        {
            throw new InvalidCastException(
                $"Cannot marshal value of type '{value.GetType()}' for property '{propertyName}'.", ex);
        }
    }

    private static T? ConvertResult<T>(InteropVariant variant)
    {
        var converted = ConvertResult(variant, typeof(T), true);
        return converted is null ? default : (T)converted;
    }

    private static void WriteSequence<T>(nuint handle, string propertyName, IReadOnlyList<T> keypoints)
        where T : struct
    {
        var stride = Marshal.SizeOf<T>();
        var size = sizeof(int) + keypoints.Count * stride;
        var buffer = Marshal.AllocHGlobal(size);
        try
        {
            Marshal.WriteInt32(buffer, keypoints.Count);
            var elems = buffer + sizeof(int);
            for (var i = 0; i < keypoints.Count; i++)
            {
                Marshal.StructureToPtr(keypoints[i], elems + i * stride, false);
            }

            Interop.Reflection.SetProperty((void*)handle, propertyName,
                InteropVariant.FromBlittable((nuint)buffer));
        }
        finally
        {
            Marshal.FreeHGlobal(buffer);
        }
    }

    internal static object? ConvertVariant(InteropVariant variant, Type targetType)
        => ConvertResult(variant, targetType, false);

    internal static object? ConvertResult(InteropVariant variant, Type t, bool freeNativeResources)
        => VariantReader.Read(variant, t, freeNativeResources);

    private sealed class AsyncCall
    {
        public required Action<InteropVariant, string?> Complete;
    }
}