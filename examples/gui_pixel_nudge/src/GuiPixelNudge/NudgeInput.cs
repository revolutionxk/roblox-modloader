using Roblox;

using KeyCode = Roblox.Enum.KeyCode;
using UserInputType = Roblox.Enum.UserInputType;

namespace GuiPixelNudge;

internal sealed class NudgeInput : IDisposable
{
    public Func<int, int, bool, bool>? Handler { get; set; }

    private UserInputService? _input;

    public bool IsAttached => _input is not null;

    public void Attach(UserInputService input)
    {
        Detach();
        _input = input;
        _input.InputBegan += OnInputBegan;
    }

    public void Detach()
    {
        if (_input is null)
            return;

        _input.InputBegan -= OnInputBegan;
        _input = null;
    }

    private void OnInputBegan(InputObject input, bool processed)
    {
        var handler = Handler;
        var service = _input;
        if (handler is null || service is null || input.UserInputType != UserInputType.Keyboard)
            return;

        var step = IsDown(service, KeyCode.LeftShift, KeyCode.RightShift) ? 10 : 1;

        var (dx, dy) = input.KeyCode switch
        {
            KeyCode.Left => (-step, 0),
            KeyCode.Right => (step, 0),
            KeyCode.Up => (0, -step),
            KeyCode.Down => (0, step),
            _ => (0, 0)
        };

        if (dx == 0 && dy == 0)
            return;

        handler(dx, dy, IsDown(service, KeyCode.LeftAlt, KeyCode.RightAlt));
    }

    private static bool IsDown(UserInputService service, KeyCode left, KeyCode right)
        => service.IsKeyDown(left) || service.IsKeyDown(right);

    public void Dispose()
    {
        Handler = null;
        Detach();
    }
}
