using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

using Xunit;

namespace RML.Interop.Tests;

public unsafe class LuauInteropTests : IDisposable
{
    private readonly NativeInterop.InteropTable* _table;

    private static readonly List<(int Context, string ChunkName, string Source)> s_scheduleCalls = [];
    private static readonly List<(int Context, string ChunkName, string Source)> s_evaluateCalls = [];
    private static readonly List<(nuint Handle, uint ArgCount)> s_refCalls = [];
    private static readonly List<(nuint Handle, string Key)> s_refIndexCalls = [];
    private static readonly List<nuint> s_refReleases = [];
    private static int s_hostReadyResult;

    public LuauInteropTests()
    {
        s_scheduleCalls.Clear();
        s_evaluateCalls.Clear();
        s_refCalls.Clear();
        s_refIndexCalls.Clear();
        s_refReleases.Clear();
        s_hostReadyResult = 1;

        _table = (NativeInterop.InteropTable*)NativeMemory.AllocZeroed((nuint)sizeof(NativeInterop.InteropTable));
        _table->Version = NativeInterop.InteropTableVersion;
        _table->Size = (uint)sizeof(NativeInterop.InteropTable);
        _table->LuauHostReady = &LuauHostReadyStub;
        _table->LuauSchedule = &LuauScheduleStub;
        _table->LuauEvaluate = &LuauEvaluateStub;
        _table->LuauRefCall = &LuauRefCallStub;
        _table->LuauRefIndex = &LuauRefIndexStub;
        _table->LuauRefRelease = &LuauRefReleaseStub;

        Interop.Initialize((nint)_table);
    }

    public void Dispose()
    {
        Interop.Uninitialize();
        NativeMemory.Free(_table);
    }

    private static string ReadUtf8(sbyte* text) => Marshal.PtrToStringUTF8((nint)text) ?? string.Empty;

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static int LuauHostReadyStub(int dataModelType) => s_hostReadyResult;

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static void LuauScheduleStub(
        int dataModelType, sbyte* chunkName, sbyte* source,
        delegate* unmanaged[Cdecl]<void*, InteropVariant*, sbyte*, void> callback, void* state)
    {
        s_scheduleCalls.Add((dataModelType, ReadUtf8(chunkName), ReadUtf8(source)));
        callback(state, null, null);
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static void LuauEvaluateStub(
        int dataModelType, sbyte* chunkName, sbyte* source,
        delegate* unmanaged[Cdecl]<void*, InteropVariant*, sbyte*, void> callback, void* state)
    {
        s_evaluateCalls.Add((dataModelType, ReadUtf8(chunkName), ReadUtf8(source)));

        var result = InteropVariant.FromDouble(42);
        callback(state, &result, null);
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static void LuauRefCallStub(
        nuint refHandle, InteropVariant* args, uint argCount,
        delegate* unmanaged[Cdecl]<void*, InteropVariant*, sbyte*, void> callback, void* state)
    {
        s_refCalls.Add((refHandle, argCount));
        callback(state, null, null);
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static void LuauRefIndexStub(
        nuint refHandle, sbyte* key,
        delegate* unmanaged[Cdecl]<void*, InteropVariant*, sbyte*, void> callback, void* state)
    {
        s_refIndexCalls.Add((refHandle, ReadUtf8(key)));
        callback(state, null, null);
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static void LuauRefReleaseStub(nuint refHandle) => s_refReleases.Add(refHandle);

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static void NoOpCompletion(void* state, InteropVariant* result, sbyte* error)
    {
    }

    private static nint CompletionPointer =>
        (nint)(delegate* unmanaged[Cdecl]<void*, InteropVariant*, sbyte*, void>)&NoOpCompletion;

    [Fact]
    public void Initialize_Accepts_A_Version11_Table()
    {
        Assert.True(Interop.IsInitialized);
        Assert.Equal(11, NativeInterop.InteropTableVersion);
    }

    [Fact]
    public void InteropTable_Layout_Matches_The_Version11_Native_Abi()
    {
        Assert.Equal(248, sizeof(NativeInterop.InteropTable));
        Assert.Equal(16, sizeof(InteropVariant));
    }

    [Fact]
    public void InteropVariant_FromLuauRef_Carries_Tag_Ten_And_The_Handle()
    {
        var variant = InteropVariant.FromLuauRef(1234);

        Assert.Equal(InteropVariant.Tags.LuauRef, variant.Tag);
        Assert.Equal(10, InteropVariant.Tags.LuauRef);
        Assert.Equal(1234ul, variant.AsUInt64);
    }

    [Fact]
    public void LuauHostReady_Returns_The_Native_Result()
    {
        Assert.True(Interop.LuauHostReady(0));

        s_hostReadyResult = 0;
        Assert.False(Interop.LuauHostReady(0));
    }

    [Fact]
    public void LuauHostReady_Returns_False_When_The_Slot_Is_Null()
    {
        _table->LuauHostReady = null;
        Assert.False(Interop.LuauHostReady(0));
    }

    [Fact]
    public void LuauSchedule_Forwards_The_Context_ChunkName_And_Source()
    {
        var dispatched = Interop.LuauSchedule(2, "print('hi')", "chunk", CompletionPointer, 0);

        Assert.True(dispatched);
        Assert.Single(s_scheduleCalls);
        Assert.Equal((2, "chunk", "print('hi')"), s_scheduleCalls[0]);
    }

    [Fact]
    public void LuauSchedule_Substitutes_A_Default_ChunkName_When_None_Is_Given()
    {
        Interop.LuauSchedule(0, "return 1", null, CompletionPointer, 0);

        Assert.Single(s_scheduleCalls);
        Assert.Equal("@managed", s_scheduleCalls[0].ChunkName);
    }

    [Fact]
    public void LuauSchedule_Returns_False_When_The_Slot_Is_Null()
    {
        _table->LuauSchedule = null;

        Assert.False(Interop.LuauSchedule(0, "return 1", null, CompletionPointer, 0));
        Assert.Empty(s_scheduleCalls);
    }

    [Fact]
    public void LuauEvaluate_Forwards_A_Large_Source_Without_Truncation()
    {
        var source = new string('x', 200_000);

        Assert.True(Interop.LuauEvaluate(1, source, "big", CompletionPointer, 0));
        Assert.Single(s_evaluateCalls);
        Assert.Equal(source.Length, s_evaluateCalls[0].Source.Length);
    }

    [Fact]
    public void LuauRefCall_Forwards_The_Handle_And_Argument_Count()
    {
        Assert.True(Interop.LuauRefCall(7, ["a", 1.5, true], CompletionPointer, 0));

        Assert.Single(s_refCalls);
        Assert.Equal(((nuint)7, 3u), s_refCalls[0]);
    }

    [Fact]
    public void LuauRefCall_Accepts_A_Null_Argument_Array()
    {
        Assert.True(Interop.LuauRefCall(7, null, CompletionPointer, 0));

        Assert.Single(s_refCalls);
        Assert.Equal(((nuint)7, 0u), s_refCalls[0]);
    }

    [Fact]
    public void LuauRefCall_Returns_False_For_A_Zero_Handle()
    {
        Assert.False(Interop.LuauRefCall(0, null, CompletionPointer, 0));
        Assert.Empty(s_refCalls);
    }

    [Fact]
    public void LuauRefIndex_Forwards_The_Key()
    {
        Assert.True(Interop.LuauRefIndex(5, "Name", CompletionPointer, 0));

        Assert.Single(s_refIndexCalls);
        Assert.Equal(((nuint)5, "Name"), s_refIndexCalls[0]);
    }

    [Fact]
    public void LuauRefRelease_Forwards_The_Handle_And_Ignores_Zero()
    {
        Interop.LuauRefRelease(11);
        Interop.LuauRefRelease(0);

        Assert.Single(s_refReleases);
        Assert.Equal((nuint)11, s_refReleases[0]);
    }
}
