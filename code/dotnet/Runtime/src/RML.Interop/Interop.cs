using System.Buffers;
using System.Collections.Concurrent;
using System.Runtime.InteropServices;
using System.Text;

using RML.Logging;

namespace RML.Interop;

public static unsafe class Interop
{
    public static bool IsInitialized => Table != null;

    internal static NativeInterop.InteropTable* Table { get; private set; }

    public static void Log(LogLevel level, string message)
    {
        if (message is null)
        {
            return;
        }

        var table = Table;
        try
        {
            if (table != null && table->Log != null)
            {
                var bytes = Encoding.UTF8.GetBytes(message);
                fixed (byte* p = bytes)
                {
                    table->Log((int)level, (sbyte*)p, bytes.Length);
                }

                return;
            }
        }
        catch (NullReferenceException ex)
        {
            Console.Error.WriteLine($"[RML/Error] Interop.Log native call failed: {ex.Message}");
        }

        var writer = level >= LogLevel.Warn ? Console.Error : Console.Out;
        writer.WriteLine($"[RML/{level}] {message}");
    }

    public static void Initialize(nint tablePtr)
    {
        if (tablePtr == nint.Zero)
        {
            throw new ArgumentException("Invalid interop table pointer or size.");
        }

        if (sizeof(InteropVariant) != 16)
        {
            throw new InvalidOperationException(
                $"InteropVariant ABI mismatch: managed size is {sizeof(InteropVariant)}, expected 16.");
        }

        var table = (NativeInterop.InteropTable*)tablePtr;
        if (table->Version != NativeInterop.InteropTableVersion)
        {
            throw new InvalidOperationException(
                $"Unsupported interop table version: {table->Version} (expected {NativeInterop.InteropTableVersion}).");
        }

        if (table->Size != 0 && table->Size != (uint)sizeof(NativeInterop.InteropTable))
        {
            throw new InvalidOperationException(
                $"Interop table size mismatch: native {table->Size} != managed {sizeof(NativeInterop.InteropTable)}.");
        }

        Table = table;
    }

    public static void Uninitialize()
    {
        Reflection.ClearCaches();
        Table = null;
    }

    public static void FreeNativeString(nint ptr)
    {
        if (ptr == nint.Zero || !IsInitialized || Table == null || Table->FreeString == null)
        {
            return;
        }

        Table->FreeString((sbyte*)ptr);
    }

    public static void FreeNativeArray(nint ptr)
    {
        if (ptr == nint.Zero || !IsInitialized || Table == null || Table->FreeNativePtr == null)
        {
            return;
        }

        Table->FreeNativePtr((void*)ptr);
    }

    public static nuint ModsMenuAddAction(nuint parent, string text, nint callback, nint state)
    {
        if (!IsInitialized || Table == null || Table->ModsMenuAddAction == null || callback == nint.Zero)
        {
            return 0;
        }

        ArgumentNullException.ThrowIfNull(text);

        var byteCount = Encoding.UTF8.GetByteCount(text);
        var buffer = stackalloc byte[byteCount + 1];
        Encoding.UTF8.GetBytes(text, new Span<byte>(buffer, byteCount));
        buffer[byteCount] = 0;

        var cb = (delegate* unmanaged[Cdecl]<void*, InteropVariant*, uint, void>)callback;
        return Table->ModsMenuAddAction(parent, (sbyte*)buffer, cb, (void*)state);
    }

    public static nuint ModsMenuAddSubmenu(nuint parent, string text)
    {
        if (!IsInitialized || Table == null || Table->ModsMenuAddSubmenu == null)
        {
            return 0;
        }

        ArgumentNullException.ThrowIfNull(text);

        var byteCount = Encoding.UTF8.GetByteCount(text);
        var buffer = stackalloc byte[byteCount + 1];
        Encoding.UTF8.GetBytes(text, new Span<byte>(buffer, byteCount));
        buffer[byteCount] = 0;

        return Table->ModsMenuAddSubmenu(parent, (sbyte*)buffer);
    }

    public static nuint ModsMenuAddSeparator(nuint parent)
    {
        if (!IsInitialized || Table == null || Table->ModsMenuAddSeparator == null)
        {
            return 0;
        }

        return Table->ModsMenuAddSeparator(parent);
    }

    public static nuint ModsMenuAddCheckable(nuint parent, string text, bool initial, nint callback, nint state)
    {
        if (!IsInitialized || Table == null || Table->ModsMenuAddCheckable == null || callback == nint.Zero)
        {
            return 0;
        }

        ArgumentNullException.ThrowIfNull(text);

        var byteCount = Encoding.UTF8.GetByteCount(text);
        var buffer = stackalloc byte[byteCount + 1];
        Encoding.UTF8.GetBytes(text, new Span<byte>(buffer, byteCount));
        buffer[byteCount] = 0;

        var cb = (delegate* unmanaged[Cdecl]<void*, InteropVariant*, uint, void>)callback;
        return Table->ModsMenuAddCheckable(parent, (sbyte*)buffer, initial ? 1 : 0, cb, (void*)state);
    }

    public static void ModsMenuSetItemIcon(nuint id, string path)
    {
        if (id == 0 || !IsInitialized || Table == null || Table->ModsMenuSetItemIcon == null)
        {
            return;
        }

        ArgumentNullException.ThrowIfNull(path);

        var byteCount = Encoding.UTF8.GetByteCount(path);
        var buffer = stackalloc byte[byteCount + 1];
        Encoding.UTF8.GetBytes(path, new Span<byte>(buffer, byteCount));
        buffer[byteCount] = 0;

        Table->ModsMenuSetItemIcon(id, (sbyte*)buffer);
    }

    public static void ModsMenuRemove(nuint id)
    {
        if (id == 0 || !IsInitialized || Table == null || Table->ModsMenuRemove == null)
        {
            return;
        }

        Table->ModsMenuRemove(id);
    }

    public static bool LuauHostReady(int dataModelType)
    {
        if (!IsInitialized || Table == null || Table->LuauHostReady == null)
        {
            return false;
        }

        return Table->LuauHostReady(dataModelType) != 0;
    }

    public static bool LuauSchedule(int dataModelType, string source, string? chunkName, nint callback, nint state)
    {
        if (!IsInitialized || Table == null || Table->LuauSchedule == null || callback == nint.Zero)
        {
            return false;
        }

        ArgumentNullException.ThrowIfNull(source);
        return RunLuauChunk(Table->LuauSchedule, dataModelType, source, chunkName, callback, state);
    }

    public static bool LuauEvaluate(int dataModelType, string source, string? chunkName, nint callback, nint state)
    {
        if (!IsInitialized || Table == null || Table->LuauEvaluate == null || callback == nint.Zero)
        {
            return false;
        }

        ArgumentNullException.ThrowIfNull(source);
        return RunLuauChunk(Table->LuauEvaluate, dataModelType, source, chunkName, callback, state);
    }

    private static bool RunLuauChunk(
        delegate* unmanaged[Cdecl]<int, sbyte*, sbyte*, delegate* unmanaged[Cdecl]<void*, InteropVariant*, sbyte*, void>, void*, void> slot,
        int dataModelType,
        string source,
        string? chunkName,
        nint callback,
        nint state)
    {
        var name = string.IsNullOrEmpty(chunkName) ? "@managed" : chunkName;

        var nameCount = Encoding.UTF8.GetByteCount(name);
        var nameBuffer = stackalloc byte[nameCount + 1];
        Encoding.UTF8.GetBytes(name, new Span<byte>(nameBuffer, nameCount));
        nameBuffer[nameCount] = 0;

        var sourceBytes = new byte[Encoding.UTF8.GetByteCount(source) + 1];
        Encoding.UTF8.GetBytes(source, sourceBytes);

        var cb = (delegate* unmanaged[Cdecl]<void*, InteropVariant*, sbyte*, void>)callback;

        fixed (byte* sourceBuffer = sourceBytes)
        {
            slot(dataModelType, (sbyte*)nameBuffer, (sbyte*)sourceBuffer, cb, (void*)state);
        }

        return true;
    }

    public static bool LuauRefCall(nuint refHandle, object?[]? args, nint callback, nint state)
    {
        if (refHandle == 0 || !IsInitialized || Table == null || Table->LuauRefCall == null || callback == nint.Zero)
        {
            return false;
        }

        var cb = (delegate* unmanaged[Cdecl]<void*, InteropVariant*, sbyte*, void>)callback;
        var argCount = args?.Length ?? 0;

        if (argCount == 0)
        {
            Table->LuauRefCall(refHandle, null, 0, cb, (void*)state);
            return true;
        }

        var tempPtrsArr = ArrayPool<nint>.Shared.Rent(argCount);
        var argVariantsArr = ArrayPool<InteropVariant>.Shared.Rent(argCount);

        try
        {
            fixed (nint* tempPtrs = tempPtrsArr)
            fixed (InteropVariant* argVariants = argVariantsArr)
            {
                var tempPtrCount = 0;
                try
                {
                    Reflection.BuildArgVariants(args!, argCount, tempPtrs, ref tempPtrCount, argVariants);
                    Table->LuauRefCall(refHandle, argVariants, (uint)argCount, cb, (void*)state);
                }
                finally
                {
                    Reflection.FreeTempPtrs(tempPtrs, tempPtrCount);
                }
            }
        }
        finally
        {
            ArrayPool<nint>.Shared.Return(tempPtrsArr);
            ArrayPool<InteropVariant>.Shared.Return(argVariantsArr);
        }

        return true;
    }

    public static bool LuauRefIndex(nuint refHandle, string key, nint callback, nint state)
    {
        if (refHandle == 0 || !IsInitialized || Table == null || Table->LuauRefIndex == null || callback == nint.Zero)
        {
            return false;
        }

        ArgumentNullException.ThrowIfNull(key);

        var byteCount = Encoding.UTF8.GetByteCount(key);
        var buffer = stackalloc byte[byteCount + 1];
        Encoding.UTF8.GetBytes(key, new Span<byte>(buffer, byteCount));
        buffer[byteCount] = 0;

        var cb = (delegate* unmanaged[Cdecl]<void*, InteropVariant*, sbyte*, void>)callback;
        Table->LuauRefIndex(refHandle, (sbyte*)buffer, cb, (void*)state);
        return true;
    }

    public static void LuauRefRelease(nuint refHandle)
    {
        if (refHandle == 0 || !IsInitialized || Table == null || Table->LuauRefRelease == null)
        {
            return;
        }

        Table->LuauRefRelease(refHandle);
    }

    public class Reflection
    {
        private const int StackAllocArgThreshold = 64;
        private const int MaxArgCount = 4096;

        private static readonly ConcurrentDictionary<string, nint> CachedMemberNames = new(StringComparer.Ordinal);

        internal static void ClearCaches()
        {
            foreach (var ptr in CachedMemberNames.Values)
            {
                if (ptr != nint.Zero)
                {
                    Marshal.FreeHGlobal(ptr);
                }
            }

            CachedMemberNames.Clear();
        }

        private static sbyte* GetCachedMemberName(string memberName)
        {
            var ptr = CachedMemberNames.GetOrAdd(memberName, static name =>
            {
                var bytes = Encoding.UTF8.GetBytes(name);
                var mem = Marshal.AllocHGlobal(bytes.Length + 1);
                Marshal.Copy(bytes, 0, mem, bytes.Length);
                Marshal.WriteByte(mem + bytes.Length, 0);
                return mem;
            });

            return (sbyte*)ptr;
        }

        public static InteropVariant Invoke(void* instance, string methodName, params object?[]? args)
        {
            if (!IsInitialized || Table == null || Table->ReflectionInvoke == null)
            {
                throw new InvalidOperationException(
                    "Interop table is not initialized or ReflectionInvoke is unavailable.");
            }

            ArgumentNullException.ThrowIfNull(methodName);

            var nameS = GetCachedMemberName(methodName);
            var argCount = args?.Length ?? 0;
            InteropVariant result = default;

            if (argCount == 0)
            {
                Table->ReflectionInvoke(instance, nameS, null, 0, &result);
                return result;
            }

            ValidateArgCount(argCount);

            if (argCount <= StackAllocArgThreshold)
            {
                var tempPtrs = stackalloc nint[argCount];
                var argVariants = stackalloc InteropVariant[argCount];
                var tempPtrCount = 0;

                try
                {
                    BuildArgVariants(args!, argCount, tempPtrs, ref tempPtrCount, argVariants);
                    Table->ReflectionInvoke(instance, nameS, argVariants, (uint)argCount, &result);
                    return result;
                }
                finally
                {
                    FreeTempPtrs(tempPtrs, tempPtrCount);
                }
            }

            var tempPtrsArr = ArrayPool<nint>.Shared.Rent(argCount);
            var argVariantsArr = ArrayPool<InteropVariant>.Shared.Rent(argCount);

            try
            {
                fixed (nint* tempPtrs = tempPtrsArr)
                fixed (InteropVariant* argVariants = argVariantsArr)
                {
                    var tempPtrCount = 0;
                    try
                    {
                        BuildArgVariants(args!, argCount, tempPtrs, ref tempPtrCount, argVariants);
                        Table->ReflectionInvoke(instance, nameS, argVariants, (uint)argCount, &result);
                        return result;
                    }
                    finally
                    {
                        FreeTempPtrs(tempPtrs, tempPtrCount);
                    }
                }
            }
            finally
            {
                ArrayPool<nint>.Shared.Return(tempPtrsArr);
                ArrayPool<InteropVariant>.Shared.Return(argVariantsArr);
            }
        }

        public static void InvokeAsync(
            void* instance,
            string methodName,
            delegate* unmanaged[Cdecl]<void*, InteropVariant*, sbyte*, void> callback,
            void* state,
            params object?[]? args)
        {
            if (!IsInitialized || Table == null || Table->ReflectionInvokeAsync == null)
            {
                throw new InvalidOperationException(
                    "Interop table is not initialized or ReflectionInvokeAsync is unavailable.");
            }

            ArgumentNullException.ThrowIfNull(methodName);

            var nameS = GetCachedMemberName(methodName);
            var argCount = args?.Length ?? 0;

            if (argCount == 0)
            {
                Table->ReflectionInvokeAsync(instance, nameS, null, 0, callback, state);
                return;
            }

            ValidateArgCount(argCount);

            if (argCount <= StackAllocArgThreshold)
            {
                var tempPtrs = stackalloc nint[argCount];
                var argVariants = stackalloc InteropVariant[argCount];
                var tempPtrCount = 0;

                try
                {
                    BuildArgVariants(args!, argCount, tempPtrs, ref tempPtrCount, argVariants);
                    Table->ReflectionInvokeAsync(instance, nameS, argVariants, (uint)argCount, callback, state);
                }
                finally
                {
                    FreeTempPtrs(tempPtrs, tempPtrCount);
                }

                return;
            }

            var tempPtrsArr = ArrayPool<nint>.Shared.Rent(argCount);
            var argVariantsArr = ArrayPool<InteropVariant>.Shared.Rent(argCount);

            try
            {
                fixed (nint* tempPtrs = tempPtrsArr)
                fixed (InteropVariant* argVariants = argVariantsArr)
                {
                    var tempPtrCount = 0;
                    try
                    {
                        BuildArgVariants(args!, argCount, tempPtrs, ref tempPtrCount, argVariants);
                        Table->ReflectionInvokeAsync(instance, nameS, argVariants, (uint)argCount, callback, state);
                    }
                    finally
                    {
                        FreeTempPtrs(tempPtrs, tempPtrCount);
                    }
                }
            }
            finally
            {
                ArrayPool<nint>.Shared.Return(tempPtrsArr);
                ArrayPool<InteropVariant>.Shared.Return(argVariantsArr);
            }
        }

        private static void ValidateArgCount(int argCount)
        {
            if (argCount > MaxArgCount)
            {
                throw new ArgumentException(
                    $"Argument count {argCount} exceeds the maximum supported count of {MaxArgCount}.", nameof(argCount));
            }
        }

        internal static void BuildArgVariants(
            object?[] args, int argCount, nint* tempPtrs, ref int tempPtrCount, InteropVariant* argVariants)
        {
            for (var i = 0; i < argCount; i++)
            {
                argVariants[i] = BuildVariant(args[i], tempPtrs, ref tempPtrCount);
            }
        }

        internal static void FreeTempPtrs(nint* tempPtrs, int tempPtrCount)
        {
            for (var i = 0; i < tempPtrCount; i++)
            {
                if (tempPtrs[i] != 0)
                {
                    Marshal.FreeHGlobal(tempPtrs[i]);
                }
            }
        }

        public static InteropVariant GetProperty(void* instance, string propertyName)
        {
            if (!IsInitialized || Table == null || Table->ReflectionGetProperty == null)
            {
                throw new InvalidOperationException(
                    "Interop table is not initialized or ReflectionGetProperty is unavailable.");
            }

            ArgumentNullException.ThrowIfNull(propertyName);

            var nameS = GetCachedMemberName(propertyName);
            InteropVariant result = default;
            Table->ReflectionGetProperty(instance, nameS, &result);
            return result;
        }

        public static void SetProperty(void* instance, string propertyName, InteropVariant value)
        {
            if (!IsInitialized || Table == null || Table->ReflectionSetProperty == null)
            {
                throw new InvalidOperationException(
                    "Interop table is not initialized or ReflectionSetProperty is unavailable.");
            }

            ArgumentNullException.ThrowIfNull(propertyName);

            var nameS = GetCachedMemberName(propertyName);
            Table->ReflectionSetProperty(instance, nameS, &value);
        }

        public static nuint EventConnect(
            void* instance,
            string eventName,
            delegate* unmanaged[Cdecl]<void*, InteropVariant*, uint, void> callback,
            void* state)
        {
            if (!IsInitialized || Table == null || Table->ReflectionEventConnect == null)
            {
                throw new InvalidOperationException(
                    "Interop table is not initialized or ReflectionEventConnect is unavailable.");
            }

            ArgumentNullException.ThrowIfNull(eventName);

            var nameS = GetCachedMemberName(eventName);
            return Table->ReflectionEventConnect(instance, nameS, callback, state);
        }

        public static void EventDisconnect(nuint connectionHandle)
        {
            if (connectionHandle == 0 || !IsInitialized || Table == null || Table->ReflectionEventDisconnect == null)
            {
                return;
            }

            Table->ReflectionEventDisconnect(connectionHandle);
        }

        public static void EventFire(void* instance, string eventName, params object?[]? args)
        {
            if (!IsInitialized || Table == null || Table->ReflectionEventFire == null)
            {
                throw new InvalidOperationException(
                    "Interop table is not initialized or ReflectionEventFire is unavailable.");
            }

            ArgumentNullException.ThrowIfNull(eventName);

            var nameS = GetCachedMemberName(eventName);
            var argCount = args?.Length ?? 0;

            if (argCount == 0)
            {
                Table->ReflectionEventFire(instance, nameS, null, 0);
                return;
            }

            ValidateArgCount(argCount);

            if (argCount <= StackAllocArgThreshold)
            {
                var tempPtrs = stackalloc nint[argCount];
                var argVariants = stackalloc InteropVariant[argCount];
                var tempPtrCount = 0;

                try
                {
                    BuildArgVariants(args!, argCount, tempPtrs, ref tempPtrCount, argVariants);
                    Table->ReflectionEventFire(instance, nameS, argVariants, (uint)argCount);
                }
                finally
                {
                    FreeTempPtrs(tempPtrs, tempPtrCount);
                }

                return;
            }

            var tempPtrsArr = ArrayPool<nint>.Shared.Rent(argCount);
            var argVariantsArr = ArrayPool<InteropVariant>.Shared.Rent(argCount);

            try
            {
                fixed (nint* tempPtrs = tempPtrsArr)
                fixed (InteropVariant* argVariants = argVariantsArr)
                {
                    var tempPtrCount = 0;
                    try
                    {
                        BuildArgVariants(args!, argCount, tempPtrs, ref tempPtrCount, argVariants);
                        Table->ReflectionEventFire(instance, nameS, argVariants, (uint)argCount);
                    }
                    finally
                    {
                        FreeTempPtrs(tempPtrs, tempPtrCount);
                    }
                }
            }
            finally
            {
                ArrayPool<nint>.Shared.Return(tempPtrsArr);
                ArrayPool<InteropVariant>.Shared.Return(argVariantsArr);
            }
        }

        public static void EventDisconnectAll(void* instance, string eventName)
        {
            if (!IsInitialized || Table == null || Table->ReflectionEventDisconnectAll == null)
            {
                return;
            }

            ArgumentNullException.ThrowIfNull(eventName);
            Table->ReflectionEventDisconnectAll(instance, GetCachedMemberName(eventName));
        }

        public static nuint[] EventSlots(void* instance, string eventName)
        {
            if (!IsInitialized || Table == null || Table->ReflectionEventSlots == null)
            {
                return [];
            }

            ArgumentNullException.ThrowIfNull(eventName);

            var nameS = GetCachedMemberName(eventName);
            uint count = 0;
            var ptr = Table->ReflectionEventSlots(instance, nameS, &count);
            if (ptr == null || count == 0)
            {
                return [];
            }

            try
            {
                var handles = new nuint[count];
                for (uint i = 0; i < count; i++)
                {
                    handles[i] = ptr[i];
                }

                return handles;
            }
            finally
            {
                FreeNativeArray((nint)ptr);
            }
        }

        public static void EventSlotFire(void* instance, string eventName, nuint slotHandle, params object?[]? args)
        {
            if (slotHandle == 0 || !IsInitialized || Table == null || Table->EventSlotFire == null)
            {
                return;
            }

            ArgumentNullException.ThrowIfNull(eventName);

            var nameS = GetCachedMemberName(eventName);
            var argCount = args?.Length ?? 0;

            if (argCount == 0)
            {
                Table->EventSlotFire(instance, nameS, slotHandle, null, 0);
                return;
            }

            ValidateArgCount(argCount);

            var tempPtrsArr = ArrayPool<nint>.Shared.Rent(argCount);
            var argVariantsArr = ArrayPool<InteropVariant>.Shared.Rent(argCount);

            try
            {
                fixed (nint* tempPtrs = tempPtrsArr)
                fixed (InteropVariant* argVariants = argVariantsArr)
                {
                    var tempPtrCount = 0;
                    try
                    {
                        BuildArgVariants(args!, argCount, tempPtrs, ref tempPtrCount, argVariants);
                        Table->EventSlotFire(instance, nameS, slotHandle, argVariants, (uint)argCount);
                    }
                    finally
                    {
                        FreeTempPtrs(tempPtrs, tempPtrCount);
                    }
                }
            }
            finally
            {
                ArrayPool<nint>.Shared.Return(tempPtrsArr);
                ArrayPool<InteropVariant>.Shared.Return(argVariantsArr);
            }
        }

        public static void EventSlotDisconnect(nuint slotHandle)
        {
            if (slotHandle == 0 || !IsInitialized || Table == null || Table->EventSlotDisconnect == null)
            {
                return;
            }

            Table->EventSlotDisconnect(slotHandle);
        }

        public static void EventSlotRelease(nuint slotHandle)
        {
            if (slotHandle == 0 || !IsInitialized || Table == null || Table->EventSlotRelease == null)
            {
                return;
            }

            Table->EventSlotRelease(slotHandle);
        }

        public static bool InstanceRetain(nuint instance)
        {
            if (instance == 0 || !IsInitialized || Table == null || Table->InstanceRetain == null)
            {
                return false;
            }

            return Table->InstanceRetain(instance) != 0;
        }

        public static void InstanceRelease(nuint instance)
        {
            if (instance == 0 || !IsInitialized || Table == null || Table->InstanceRelease == null)
            {
                return;
            }

            Table->InstanceRelease(instance);
        }

        public static nuint CreateInstanceByName(string className, int creatorRole)
        {
            if (!IsInitialized || Table == null || Table->CreateInstanceByName == null)
            {
                throw new InvalidOperationException(
                    "Interop table is not initialized or CreateInstanceByName is unavailable.");
            }

            ArgumentNullException.ThrowIfNull(className);

            var bytes = Encoding.UTF8.GetBytes(className);
            fixed (byte* p = bytes)
            {
                return Table->CreateInstanceByName((sbyte*)p, creatorRole);
            }
        }

        private static InteropVariant BuildVariant(object? arg, nint* tempPtrs, ref int tempPtrCount)
        {
            if (arg is null)
            {
                return default;
            }

            return arg switch
            {
                string s => BuildStringVariant(s, tempPtrs, ref tempPtrCount),
                bool b => InteropVariant.FromBool(b),
                double d => InteropVariant.FromDouble(d),
                float f => InteropVariant.FromFloat(f),
                Enum e => InteropVariant.FromInt64(Convert.ToInt64(e)),
                nuint nu => InteropVariant.FromPointer(nu),
                nint ni => InteropVariant.FromPointer((nuint)ni),
                _ => BuildFallbackVariant(arg, tempPtrs, ref tempPtrCount)
            };
        }

        private static InteropVariant BuildFallbackVariant(object arg, nint* tempPtrs, ref int tempPtrCount)
        {
            var type = arg.GetType();
            if (type.IsValueType && !type.IsPrimitive && !type.IsEnum)
            {
                try
                {
                    var size = Marshal.SizeOf(arg);
                    var p = Marshal.AllocHGlobal(size);
                    Marshal.StructureToPtr(arg, p, false);
                    tempPtrs[tempPtrCount++] = p;
                    return InteropVariant.FromBlittable((nuint)p);
                }
                catch (ArgumentException)
                {
                }
            }
            else if (arg is IConvertible)
            {
                try
                {
                    return InteropVariant.FromInt64(Convert.ToInt64(arg));
                }
                catch (Exception ex) when (ex is InvalidCastException or FormatException or OverflowException)
                {
                }
            }

            throw new NotSupportedException($"Cannot marshal argument of type {type}");
        }

        private static InteropVariant BuildStringVariant(string s, nint* tempPtrs, ref int tempPtrCount)
        {
            var bytes = Encoding.UTF8.GetBytes(s);
            var p = Marshal.AllocHGlobal(bytes.Length + 1);
            Marshal.Copy(bytes, 0, p, bytes.Length);
            Marshal.WriteByte(p + bytes.Length, 0);
            tempPtrs[tempPtrCount++] = p;
            return InteropVariant.FromString((nuint)(ulong)p);
        }
    }
}