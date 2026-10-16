using System.Drawing;
using Microsoft.Web.WebView2.Core;
using ScriptEditorWebview.Native;
using ScriptEditorWebview.Threading;

namespace ScriptEditorWebview.WebView;

internal sealed class WebViewHost(GuiDispatcher gui) : IDisposable
{
    private const string VirtualHost = "rml.scripteditor";

    private CoreWebView2Controller? _controller;
    private Rectangle _lastBounds;
    private bool _disposed;
    private IntPtr _parentHwnd;
    private string _webRootPath = string.Empty;
    private CoreWebView2? _webView;

    public bool IsReady { get; private set; }

    public void Dispose()
    {
        if (_disposed) return;

        _disposed = true;
        IsReady = false;

        gui.Post(() =>
        {
            if (_webView is not null)
            {
                _webView.WebMessageReceived -= OnWebMessageReceived;
                _webView = null;
            }

            if (_controller is null) return;

            try
            {
                _controller.Close();
            }
            catch
            {
                // ignored
            }

            _controller = null;
        });
    }

    public event Action<string>? MessageReceived;

    public event Action? Ready;

    public void Initialize(IntPtr parentHwnd, string webRootPath)
    {
        _parentHwnd = parentHwnd;
        _webRootPath = webRootPath;
        gui.Post(BeginInitialize);
    }

    private void BeginInitialize()
    {
        if (_disposed || _parentHwnd == IntPtr.Zero) return;

        var userDataFolder = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "RobloxModLoader", "ScriptEditorWebview", "WebView2");
        Directory.CreateDirectory(userDataFolder);

        CoreWebView2Environment.CreateAsync(null, userDataFolder)
            .ContinueWith(task =>
            {
                if (task.IsFaulted || task.Result is null)
                {
                    ScriptEditorWebviewMod.Logger.Error(
                        $"WebView2 environment creation failed (is the WebView2 Runtime installed?): {task.Exception}");
                    return;
                }

                gui.Post(() => OnEnvironmentReady(task.Result));
            });
    }

    private void OnEnvironmentReady(CoreWebView2Environment environment)
    {
        if (_disposed || _parentHwnd == IntPtr.Zero) return;

        environment.CreateCoreWebView2ControllerAsync(_parentHwnd)
            .ContinueWith(task =>
            {
                if (task.IsFaulted || task.Result is null)
                {
                    ScriptEditorWebviewMod.Logger.Error($"WebView2 controller creation failed: {task.Exception}");
                    return;
                }

                gui.Post(() => OnControllerReady(task.Result));
            });
    }

    private void OnControllerReady(CoreWebView2Controller controller)
    {
        if (_disposed)
        {
            controller.Close();
            return;
        }

        _controller = controller;

        if (!Directory.Exists(_webRootPath))
        {
            ScriptEditorWebviewMod.Logger.Error(
                $"web assets missing at '{_webRootPath}' — they must sit at the mod root, beside 'dotnet/'");
            controller.Close();
            _controller = null;
            return;
        }

        _webView = controller.CoreWebView2;

        _webView.WebMessageReceived += OnWebMessageReceived;

        var settings = _webView.Settings;
        settings.AreDevToolsEnabled = true;
        settings.IsStatusBarEnabled = false;
        settings.AreDefaultContextMenusEnabled = false;
        settings.IsZoomControlEnabled = false;

        _webView.SetVirtualHostNameToFolderMapping(
            VirtualHost, _webRootPath, CoreWebView2HostResourceAccessKind.Allow);

        var bounds = ClientRect(_parentHwnd);
        controller.Bounds = bounds;
        controller.IsVisible = true;

        Win32.RaiseChildWindows(_parentHwnd, "Chrome_WidgetWin");

        _webView.Navigate($"https://{VirtualHost}/index.html");

        IsReady = true;
        ScriptEditorWebviewMod.Logger.Info(
            $"WebView2 host ready (parent 0x{_parentHwnd:X}, bounds {bounds.Width}x{bounds.Height})");
        Ready?.Invoke();
    }

    private void OnWebMessageReceived(object? sender, CoreWebView2WebMessageReceivedEventArgs e)
    {
        string payload;
        try
        {
            payload = e.TryGetWebMessageAsString();
        }
        catch
        {
            payload = e.WebMessageAsJson;
        }

        try
        {
            MessageReceived?.Invoke(payload);
        }
        catch (Exception ex)
        {
            ScriptEditorWebviewMod.Logger.Error($"WebView message handler threw: {ex}");
        }
    }

    public void PostMessage(string payload)
    {
        if (_disposed) return;

        gui.Post(() =>
        {
            if (!IsReady || _webView is null) return;

            try
            {
                _webView.PostWebMessageAsString(payload);
            }
            catch (Exception ex)
            {
                ScriptEditorWebviewMod.Logger.Error($"PostWebMessageAsString failed: {ex}");
            }
        });
    }

    public void SyncBounds()
    {
        if (_disposed) return;

        gui.Post(() =>
        {
            if (!IsReady || _controller is null || _parentHwnd == IntPtr.Zero) return;

            // Reassigning Bounds relays out the whole web view; at the reconcile tick rate that is a
            // resize storm, so only a real geometry change is worth one.
            var bounds = ClientRect(_parentHwnd);
            if (bounds == _lastBounds) return;

            _lastBounds = bounds;
            _controller.Bounds = bounds;
            Win32.RaiseChildWindows(_parentHwnd, "Chrome_WidgetWin");
        });
    }

    public void SetVisible(bool visible)
    {
        if (_disposed) return;

        gui.Post(() => { _controller?.IsVisible = visible; });
    }

    private static Rectangle ClientRect(IntPtr hwnd)
    {
        if (hwnd != IntPtr.Zero && Win32.GetClientRect(hwnd, out var rect))
            return new Rectangle(0, 0, Math.Max(1, rect.Width), Math.Max(1, rect.Height));

        return new Rectangle(0, 0, 1, 1);
    }
}