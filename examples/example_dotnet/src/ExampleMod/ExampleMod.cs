using System.Runtime.InteropServices;
using RML.Core.Api;
using RML.Core.Modding;
using RML.Logging;
using Roblox;

namespace ExampleMod;

[Mod(
    "dotnet-example",
    "1.0.0",
    Author = "Revolution",
    Description = "Example managed mod for RobloxModLoader"
)]
public sealed class ExampleMod : ModBase, IDataModelAware
{
    public new static ILogger Logger { get; } = Log.CreateLogger("ExampleMod");

    public void OnDataModelLoaded(DataModel dataModel, DataModelType dataModelType)
    {
        Logger.Info($"[DOTNET]: DataModel loaded: {dataModelType}");
        var runService = dataModel.RunService;

        Logger.Info($"[DOTNET]: RunService name: {runService.Name}");
        Logger.Info($"[DOTNET]: RunService instance: {runService.FindFirstChild("Teste", true)}");

        var dmChildren = dataModel.GetChildren();
        Logger.Info($"[DOTNET]: DataModel name: {dataModel.Name}");
        Logger.Info($"[DOTNET]: DataModel.GetChildren count: {dmChildren.Count}");
        foreach (var service in dmChildren) Logger.Info($"[DOTNET]: Service: {service.Name}");

        Task.Run(() =>
        {
            Thread.Sleep(5000);
            var gameId = dataModel.GameId;
            var placeId = dataModel.PlaceId;

            Logger.Info($"[DOTNET]: GameId: {gameId}");
            Logger.Info($"[DOTNET]: PlaceId: {placeId}");
        });

        var workspace = dataModel.GetService<Workspace>().Cast<Workspace>();
        Logger.Info($"[DOTNET]: Workspace name: {workspace.Name}");

        var part = new Part
        {
            Parent = workspace,
            Name = "Example Part",
            Position = new Vector3(10, 10, 0),
            Size = new Vector3(10, 10, 10)
        };

        var part2 = Instance.New<Part>(p =>
        {
            p.Parent = workspace;
            p.Name = "Example Part2";
            p.Position = new Vector3(0, 10, 0);
            p.Size = new Vector3(10, 10, 10);
        });

        runService.PreRender += deltaTime =>
        {
            //part.CFrame *= CFrame.Angles((float)deltaTime * 5, (float)deltaTime * 5, (float)deltaTime * 5);
            //part2.CFrame *= CFrame.Angles((float)deltaTime * 5, (float)deltaTime * 5, (float)deltaTime * 5);

            part.Color = Color3.FromHSV((float)(DateTime.Now.TimeOfDay.TotalSeconds % 1), 1, 1);
            part2.Color = Color3.FromHSV((float)((DateTime.Now.TimeOfDay.TotalSeconds + 0.5) % 1), 1, 1);

            part2.Position = new Vector3((float)Math.Sin(DateTime.Now.TimeOfDay.TotalSeconds) * 15, 10, 0);
            part.Position = new Vector3((float)Math.Cos(DateTime.Now.TimeOfDay.TotalSeconds) * 15, (float)(Math.Cos(
                DateTime.Now.TimeOfDay.TotalSeconds) * 15) + 10, 0);
        };

        if (dataModelType == DataModelType.Edit)
        {
            workspace.DescendantAdded += instance =>
            {
                Logger.Info(
                    $"[DOTNET]: Instance added: {instance.Name} {instance.Parent?.Name} ({instance.ClassName})");
            };

            workspace.DescendantRemoving += instance =>
            {
                Logger.Info(
                    $"[DOTNET]: Instance removed: {instance.Name} {instance.Parent?.Name} ({instance.ClassName})");
            };

            RunEventDiagnostics(workspace);
            RunLifetimeDiagnostics(workspace, part);

            _ = Task.Run(async () =>
            {
                try
                {
                    await LuauScriptManager.ScheduleAsync(
                        DataModelType.Edit,
                        "print('Hello from ExampleMod!')",
                        "ExampleMod"
                    );

                    var valueFromLua = await LuauScriptManager.EvaluateAsync(
                        DataModelType.Edit,
                        "return 10",
                        "ExampleMod"
                    );

                    Logger.Info($"[DOTNET]: Luau returned {valueFromLua.AsNumber()}");
                }
                catch (Exception ex)
                {
                    Logger.Error($"[DOTNET]: Luau call failed: {ex.GetBaseException().Message}");
                }
            });
        }
    }

    public void OnDataModelUnloaded(DataModel dataModel, DataModelType dataModelType)
    {
    }

    private static void RunLifetimeDiagnostics(Workspace workspace, Part part)
    {
        try
        {
            for (var i = 0; i < 2000; i++)
            {
                var folder = Instance.New<Folder>(f => f.Name = $"RML_Lifetime_{i}");
                if (i % 2 == 0)
                {
                    folder.Dispose();
                }
            }

            GC.Collect();
            GC.WaitForPendingFinalizers();
            Logger.Info("[LIFETIME]: dropped 2000 unparented folders (1000 disposed, 1000 left to the finalizer)");

            var clone = part.Clone<Part>();
            GC.Collect();
            GC.WaitForPendingFinalizers();
            Logger.Info($"[LIFETIME]: clone outlives the call that made it: {clone?.Name} ({clone?.ClassName})");

            Logger.Info($"[LIFETIME]: workspace.IsAncestorOf(part) = {workspace.IsAncestorOf(part)}");
            Logger.Info($"[LIFETIME]: workspace children wrapped: {workspace.GetChildren().Count}");
        }
        catch (Exception ex)
        {
            Logger.Error($"[LIFETIME]: {ex.GetBaseException().Message}");
        }
    }

    private static void RunEventDiagnostics(Workspace workspace)
    {
        try
        {
            var bindable = Instance.New<BindableEvent>(b =>
            {
                b.Parent = workspace;
                b.Name = "RML_EventTest";
            });

            var fired = new List<string>();
            Action<object> handlerA = arg =>
            {
                fired.Add("A");
                Step($"  handler A got: {arg}");
            };
            Action<object> handlerB = arg =>
            {
                fired.Add("B");
                Step($"  handler B got: {arg}");
            };

            void FireAll(string tag)
            {
                fired.Clear();
                bindable.Fire(tag);
                Step($"Fire('{tag}') control -> [{string.Join(",", fired)}]");

                fired.Clear();
                bindable.FireEvent("Event", tag);
                Step($"FireEvent('{tag}') raw -> [{string.Join(",", fired)}]");
            }

            bindable.Event += handlerA;
            bindable.Event += handlerB;
            Step($"connect A + B -> {SlotCount(bindable)} slot(s)");
            FireAll("all");

            bindable.Event -= handlerA;
            bindable.Event -= handlerB;
            Step($"after -= A,B -> {SlotCount(bindable)} slot(s)");
            FireAll("after-disconnect");

            bindable.Event += handlerA;
            var connections = bindable.GetConnections("Event");
            Step($"GetConnections -> {connections.Count} slot(s)");
            fired.Clear();
            if (connections.Count > 0) connections[0].Fire("slot-fire");

            Step($"slot[0].Fire -> [{string.Join(",", fired)}]");
            foreach (var connection in connections) connection.Dispose();

            bindable.Event -= handlerA;

            bindable.Event += handlerA;
            var toDrop = bindable.GetConnections("Event");
            if (toDrop.Count > 0) toDrop[0].Disconnect();

            foreach (var connection in toDrop) connection.Dispose();

            Step($"after slot[0].Disconnect -> {SlotCount(bindable)} slot(s)");
            bindable.Event -= handlerA;

            bindable.Event += handlerA;
            bindable.DisconnectAll("Event");
            Step($"after DisconnectAll -> {SlotCount(bindable)} slot(s)");
            bindable.Event -= handlerA;

            bindable.Destroy();
            Step("done");
        }
        catch (Exception ex)
        {
            Logger.Error($"[EVT-TEST] failed: {ex}");
        }

        return;

        void Step(string message)
        {
            Logger.Info($"[EVT-TEST] {message}");
        }
    }

    private static int SlotCount(Instance instance)
    {
        var connections = instance.GetConnections("Event");
        foreach (var connection in connections) connection.Dispose();

        return connections.Count;
    }

    public override int OnLoad()
    {
        Logger.Info("Hello from ExampleMod!");
        return 0;
    }

    public override void OnUnload()
    {
        Logger.Info("Goodbye from ExampleMod!");
    }

    [DllImport("user32.dll", SetLastError = true)]
    private static extern int MessageBoxA(IntPtr hWnd, string lpText, string lpCaption, uint uType);
}