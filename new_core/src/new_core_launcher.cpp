#include <windows.h>
#include <commdlg.h>
#include <commctrl.h>
#include <shlobj.h>

#include <array>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

#include "new_core_launcher_core.h"

namespace
{

constexpr wchar_t WindowClassName[] = L"Hoi3NewCoreLauncherWindow";
constexpr UINT LaunchCompletedMessage = WM_APP + 1;

enum ControlId
{
    InjectMode = 1001,
    OriginalMode,
    GamePath,
    BrowseGame,
    OriginalLauncherPath,
    BrowseOriginalLauncher,
    CoreLibraryPath,
    BrowseCoreLibrary,
    ProjectRootPath,
    BrowseProjectRoot,
    ModDescriptorPath,
    BrowseModDescriptor,
    ExtraArguments,
    PreventDuplicate,
    StartButton,
    StatusText
};

struct LauncherWindowState
{
    std::filesystem::path configPath;
    HFONT font = nullptr;
    bool busy = false;
};

struct LaunchTask
{
    HWND window = nullptr;
    new_core::LauncherConfig config;
};

std::filesystem::path ExecutableDirectory()
{
    std::array<wchar_t, 32768> path{};
    const DWORD length = GetModuleFileNameW(
        nullptr,
        path.data(),
        static_cast<DWORD>(path.size())
    );
    return length > 0 && length < path.size()
        ? std::filesystem::path(path.data()).parent_path()
        : std::filesystem::current_path();
}

std::wstring GetText(HWND window, int controlId)
{
    HWND control = GetDlgItem(window, controlId);
    const int length = GetWindowTextLengthW(control);
    std::wstring value(static_cast<std::size_t>(length + 1), L'\0');
    if (length > 0)
    {
        GetWindowTextW(control, value.data(), length + 1);
    }
    value.resize(static_cast<std::size_t>(length));
    return value;
}

void SetText(HWND window, int controlId, std::wstring_view value)
{
    SetWindowTextW(
        GetDlgItem(window, controlId),
        std::wstring(value).c_str()
    );
}

std::wstring Utf8ToWide(std::string_view value)
{
    if (value.empty())
    {
        return {};
    }
    const int length = MultiByteToWideChar(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0
    );
    if (length <= 0)
    {
        return L"?";
    }
    std::wstring output(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        output.data(),
        length
    );
    return output;
}

void ApplyFont(HWND window, HFONT font)
{
    EnumChildWindows(
        window,
        [](HWND child, LPARAM value) -> BOOL
        {
            SendMessageW(child, WM_SETFONT, value, TRUE);
            return TRUE;
        },
        reinterpret_cast<LPARAM>(font)
    );
}

HWND AddControl(
    HWND parent,
    const wchar_t* className,
    const wchar_t* text,
    DWORD style,
    int x,
    int y,
    int width,
    int height,
    int id
)
{
    return CreateWindowExW(
        _wcsicmp(className, WC_EDITW) == 0 ? WS_EX_CLIENTEDGE : 0,
        className,
        text,
        WS_CHILD | WS_VISIBLE | style,
        x,
        y,
        width,
        height,
        parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        GetModuleHandleW(nullptr),
        nullptr
    );
}

void AddPathRow(
    HWND window,
    int y,
    const wchar_t* label,
    int editId,
    int buttonId
)
{
    AddControl(
        window,
        WC_STATICW,
        label,
        SS_LEFT,
        24,
        y + 4,
        125,
        22,
        0
    );
    AddControl(
        window,
        WC_EDITW,
        L"",
        ES_AUTOHSCROLL,
        150,
        y,
        510,
        26,
        editId
    );
    AddControl(
        window,
        WC_BUTTONW,
        L"浏览…",
        BS_PUSHBUTTON,
        670,
        y,
        72,
        26,
        buttonId
    );
}

void SetInjectionControlsEnabled(HWND window, bool enabled)
{
    const int controls[] = {
        GamePath,
        BrowseGame,
        CoreLibraryPath,
        BrowseCoreLibrary,
        ProjectRootPath,
        BrowseProjectRoot,
        ModDescriptorPath,
        BrowseModDescriptor,
        ExtraArguments,
        PreventDuplicate
    };
    for (int control : controls)
    {
        EnableWindow(GetDlgItem(window, control), enabled ? TRUE : FALSE);
    }
    EnableWindow(
        GetDlgItem(window, OriginalLauncherPath),
        enabled ? FALSE : TRUE
    );
    EnableWindow(
        GetDlgItem(window, BrowseOriginalLauncher),
        enabled ? FALSE : TRUE
    );
}

void WriteConfigToControls(
    HWND window,
    const new_core::LauncherConfig& config
)
{
    CheckRadioButton(
        window,
        InjectMode,
        OriginalMode,
        config.mode == new_core::LauncherMode::InjectedGame
            ? InjectMode : OriginalMode
    );
    SetText(window, GamePath, config.gameExecutable.wstring());
    SetText(
        window,
        OriginalLauncherPath,
        config.originalLauncher.wstring()
    );
    SetText(window, CoreLibraryPath, config.coreLibrary.wstring());
    SetText(window, ProjectRootPath, config.projectRoot.wstring());
    SetText(window, ModDescriptorPath, config.modDescriptor.wstring());
    SetText(window, ExtraArguments, config.extraArguments);
    SendMessageW(
        GetDlgItem(window, PreventDuplicate),
        BM_SETCHECK,
        config.preventDuplicateGame ? BST_CHECKED : BST_UNCHECKED,
        0
    );
    SetInjectionControlsEnabled(
        window,
        config.mode == new_core::LauncherMode::InjectedGame
    );
}

new_core::LauncherConfig ReadConfigFromControls(HWND window)
{
    new_core::LauncherConfig config;
    config.mode = SendMessageW(
            GetDlgItem(window, OriginalMode),
            BM_GETCHECK,
            0,
            0
        ) == BST_CHECKED
        ? new_core::LauncherMode::OriginalLauncher
        : new_core::LauncherMode::InjectedGame;
    config.gameExecutable = GetText(window, GamePath);
    config.originalLauncher = GetText(window, OriginalLauncherPath);
    config.coreLibrary = GetText(window, CoreLibraryPath);
    config.projectRoot = GetText(window, ProjectRootPath);
    config.modDescriptor = GetText(window, ModDescriptorPath);
    config.extraArguments = GetText(window, ExtraArguments);
    config.preventDuplicateGame = SendMessageW(
            GetDlgItem(window, PreventDuplicate),
            BM_GETCHECK,
            0,
            0
        ) == BST_CHECKED;
    return config;
}

bool BrowseFile(
    HWND window,
    int editId,
    const wchar_t* filter
)
{
    std::array<wchar_t, 32768> path{};
    const std::wstring current = GetText(window, editId);
    wcsncpy_s(path.data(), path.size(), current.c_str(), _TRUNCATE);
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = window;
    dialog.lpstrFilter = filter;
    dialog.lpstrFile = path.data();
    dialog.nMaxFile = static_cast<DWORD>(path.size());
    dialog.Flags = OFN_FILEMUSTEXIST
        | OFN_PATHMUSTEXIST
        | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&dialog))
    {
        return false;
    }
    SetText(window, editId, path.data());
    return true;
}

bool BrowseFolder(HWND window, int editId)
{
    BROWSEINFOW browse{};
    browse.hwndOwner = window;
    browse.lpszTitle = L"选择 Mod/New Core 根目录";
    browse.ulFlags = BIF_RETURNONLYFSDIRS
        | BIF_NEWDIALOGSTYLE;
    PIDLIST_ABSOLUTE item = SHBrowseForFolderW(&browse);
    if (!item)
    {
        return false;
    }
    std::array<wchar_t, MAX_PATH> path{};
    const bool success = SHGetPathFromIDListW(
        item,
        path.data()
    ) != FALSE;
    CoTaskMemFree(item);
    if (success)
    {
        SetText(window, editId, path.data());
    }
    return success;
}

DWORD WINAPI LaunchWorker(LPVOID parameter)
{
    std::unique_ptr<LaunchTask> task(
        static_cast<LaunchTask*>(parameter)
    );
    auto result = std::make_unique<new_core::LaunchResult>(
        new_core::LaunchFromConfig(task->config)
    );
    if (!IsWindow(task->window)
        || !PostMessageW(
            task->window,
            LaunchCompletedMessage,
            0,
            reinterpret_cast<LPARAM>(result.get())
        ))
    {
        return 0;
    }
    result.release();
    return 0;
}

void StartLaunch(HWND window, LauncherWindowState& state)
{
    if (state.busy)
    {
        return;
    }
    new_core::LauncherConfig config = ReadConfigFromControls(window);
    std::wstring error;
    if (!new_core::ValidateLauncherConfig(config, error))
    {
        SetText(window, StatusText, error);
        MessageBoxW(window, error.c_str(), L"配置错误", MB_ICONERROR);
        return;
    }
    if (!new_core::SaveLauncherConfig(state.configPath, config, error))
    {
        SetText(window, StatusText, error);
        return;
    }

    auto task = std::make_unique<LaunchTask>();
    task->window = window;
    task->config = std::move(config);
    HANDLE worker = CreateThread(
        nullptr,
        0,
        LaunchWorker,
        task.get(),
        0,
        nullptr
    );
    if (!worker)
    {
        SetText(window, StatusText, L"无法创建启动工作线程。");
        return;
    }
    task.release();
    CloseHandle(worker);
    state.busy = true;
    EnableWindow(GetDlgItem(window, StartButton), FALSE);
    SetText(
        window,
        StatusText,
        L"正在启动并等待 New Core 报告状态……"
    );
}

void FinishLaunch(
    HWND window,
    LauncherWindowState& state,
    std::unique_ptr<new_core::LaunchResult> result
)
{
    state.busy = false;
    EnableWindow(GetDlgItem(window, StartButton), TRUE);
    std::wstring text = result->message;
    if (result->processId != 0)
    {
        text += L"\r\n进程 PID："
            + std::to_wstring(result->processId);
    }
    if (!result->modules.empty())
    {
        text += L"\r\n模块：" + Utf8ToWide(result->modules);
    }
    if (!result->hooks.empty())
    {
        text += L"\r\nHook：" + Utf8ToWide(result->hooks);
    }
    SetText(window, StatusText, text);
    if (!result->success)
    {
        MessageBoxW(
            window,
            result->message.c_str(),
            L"启动未完全成功",
            MB_ICONWARNING
        );
    }
}

LRESULT CALLBACK WindowProcedure(
    HWND window,
    UINT message,
    WPARAM wParam,
    LPARAM lParam
)
{
    auto* state = reinterpret_cast<LauncherWindowState*>(
        GetWindowLongPtrW(window, GWLP_USERDATA)
    );
    switch (message)
    {
    case WM_CREATE:
    {
        auto created = std::make_unique<LauncherWindowState>();
        created->configPath = ExecutableDirectory()
            / "hoi3_new_core_launcher.ini";
        created->font = CreateFontW(
            -16,
            0,
            0,
            0,
            FW_NORMAL,
            FALSE,
            FALSE,
            FALSE,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE,
            L"Microsoft YaHei UI"
        );
        SetWindowLongPtrW(
            window,
            GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(created.get())
        );

        AddControl(window, WC_STATICW, L"HOI3 New Core 启动器", SS_LEFT,
            24, 18, 400, 30, 0);
        AddControl(window, WC_BUTTONW, L"注入模式",
            BS_AUTORADIOBUTTON | WS_GROUP,
            24, 58, 110, 24, InjectMode);
        AddControl(window, WC_BUTTONW, L"原版启动器模式", BS_AUTORADIOBUTTON,
            145, 58, 150, 24, OriginalMode);

        AddPathRow(window, 96, L"HOI3 程序", GamePath, BrowseGame);
        AddPathRow(window, 132, L"原版启动器", OriginalLauncherPath,
            BrowseOriginalLauncher);
        AddPathRow(window, 168, L"New Core DLL", CoreLibraryPath,
            BrowseCoreLibrary);
        AddPathRow(window, 204, L"项目根目录", ProjectRootPath,
            BrowseProjectRoot);
        AddPathRow(window, 240, L"Mod 描述文件", ModDescriptorPath,
            BrowseModDescriptor);

        AddControl(window, WC_STATICW, L"额外启动参数", SS_LEFT,
            24, 280, 125, 22, 0);
        AddControl(window, WC_EDITW, L"", ES_AUTOHSCROLL,
            150, 276, 592, 26, ExtraArguments);
        AddControl(window, WC_BUTTONW, L"阻止同名游戏进程重复注入",
            BS_AUTOCHECKBOX, 150, 312, 260, 24, PreventDuplicate);
        AddControl(window, WC_BUTTONW, L"启动", BS_DEFPUSHBUTTON,
            610, 310, 132, 34, StartButton);
        AddControl(window, WC_STATICW, L"启动状态", SS_LEFT,
            24, 354, 120, 22, 0);
        AddControl(
            window,
            WC_EDITW,
            L"等待启动。",
            ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL,
            24,
            378,
            718,
            100,
            StatusText
        );

        ApplyFont(window, created->font);
        new_core::LauncherConfig config =
            new_core::MakeDefaultLauncherConfig(ExecutableDirectory());
        std::wstring loadError;
        new_core::LoadLauncherConfig(
            created->configPath,
            config,
            loadError
        );
        WriteConfigToControls(window, config);
        state = created.release();
        return 0;
    }
    case WM_COMMAND:
        if (!state)
        {
            break;
        }
        switch (LOWORD(wParam))
        {
        case InjectMode:
            SetInjectionControlsEnabled(window, true);
            return 0;
        case OriginalMode:
            SetInjectionControlsEnabled(window, false);
            return 0;
        case BrowseGame:
            BrowseFile(window, GamePath,
                L"HOI3 可执行文件\0*.exe\0所有文件\0*.*\0");
            return 0;
        case BrowseOriginalLauncher:
            BrowseFile(window, OriginalLauncherPath,
                L"启动器\0*.exe\0所有文件\0*.*\0");
            return 0;
        case BrowseCoreLibrary:
            BrowseFile(window, CoreLibraryPath,
                L"New Core DLL\0*.dll\0所有文件\0*.*\0");
            return 0;
        case BrowseProjectRoot:
            BrowseFolder(window, ProjectRootPath);
            return 0;
        case BrowseModDescriptor:
            BrowseFile(window, ModDescriptorPath,
                L"HOI3 Mod 描述文件\0*.mod\0所有文件\0*.*\0");
            return 0;
        case StartButton:
            StartLaunch(window, *state);
            return 0;
        default:
            break;
        }
        break;
    case LaunchCompletedMessage:
        if (state)
        {
            FinishLaunch(
                window,
                *state,
                std::unique_ptr<new_core::LaunchResult>(
                    reinterpret_cast<new_core::LaunchResult*>(lParam)
                )
            );
        }
        return 0;
    case WM_DESTROY:
        if (state)
        {
            if (state->font)
            {
                DeleteObject(state->font);
            }
            delete state;
            SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        }
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

}

int WINAPI wWinMain(
    HINSTANCE instance,
    HINSTANCE,
    PWSTR,
    int showCommand
)
{
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    INITCOMMONCONTROLSEX controls{};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&controls);

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = WindowProcedure;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName = WindowClassName;
    if (!RegisterClassExW(&windowClass))
    {
        CoUninitialize();
        return 1;
    }

    HWND window = CreateWindowExW(
        0,
        WindowClassName,
        L"HOI3 New Core Launcher",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        790,
        535,
        nullptr,
        nullptr,
        instance,
        nullptr
    );
    if (!window)
    {
        CoUninitialize();
        return 2;
    }
    ShowWindow(window, showCommand);
    UpdateWindow(window);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    CoUninitialize();
    return static_cast<int>(message.wParam);
}
