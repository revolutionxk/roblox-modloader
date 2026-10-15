using RML.Core.Api;
using RML.Core.Modding;
using RML.Logging;

using Roblox;

namespace GuiPixelNudge;

[Mod(
    "gui-pixel-nudge",
    "1.0.0",
    Author = "Revolution",
    Description = "Nudge selected GUI objects pixel-by-pixel with the arrow keys in Studio edit mode")]
public sealed class GuiPixelNudgeMod : ModBase, IDataModelAware
{
    private static readonly ILogger Log = RML.Logging.Log.CreateLogger("GuiPixelNudge");

    private readonly NudgeInput _input = new();
    private NudgeController? _controller;

    public override int OnLoad()
    {
        _controller = new NudgeController(_input, Log);
        return 0;
    }
    
    public void OnDataModelLoaded(DataModel game, DataModelType dataModelType)
    {
        if (dataModelType != DataModelType.Edit)
            return;

        _input.Attach(game.GetService<UserInputService>());
        _controller?.UseDataModel(game);
        Log.Info("GUI pixel nudge armed.");
    }

    public void OnDataModelUnloaded(DataModel game, DataModelType dataModelType)
    {
        if (dataModelType != DataModelType.Edit)
            return;

        _input.Detach();
        _controller?.ClearDataModel();
    }

    public override void OnUnload()
    {
        _input.Dispose();
        _controller?.Dispose();
        _controller = null;
    }
}
