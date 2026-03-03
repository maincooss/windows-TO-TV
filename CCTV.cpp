#define UNICODE
#define _UNICODE
#include <windows.h>
#include <tlhelp32.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <algorithm>

struct Config {
    std::wstring chromePath;
    std::wstring userDataDir;
    std::vector<std::wstring> channels;
    UINT shutdownVk = VK_NUMPAD9;
    std::wstring shutdownKeyText = L"NUMPAD9";
    std::wstring lastpage;
};

static Config g_cfg;
static int g_currentIndex = 0;
static std::wstring trim(const std::wstring& s) {
    const wchar_t* ws = L" \t\r\n";
    const size_t b = s.find_first_not_of(ws);
    if (b == std::wstring::npos) return L"";
    const size_t e = s.find_last_not_of(ws);
    return s.substr(b, e - b + 1);
}

static std::wstring utf8ToWide(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), w.data(), n);
    return w;
}

static std::string wideToUtf8(const std::wstring& w) {
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), s.data(), n, nullptr, nullptr);
    return s;
}

static std::wstring upper(const std::wstring& in) {
    std::wstring out = in;
    std::transform(out.begin(), out.end(), out.begin(), ::towupper);
    return out;
}

static UINT parseVk(const std::wstring& key) {
    std::wstring k = upper(trim(key));
    if (k.empty()) return VK_NUMPAD9;

    if (k.size() == 1) {
        if (k[0] >= L'A' && k[0] <= L'Z') return (UINT)k[0];
        if (k[0] >= L'0' && k[0] <= L'9') return (UINT)k[0];
    }

    if (k.rfind(L"NUMPAD", 0) == 0 && k.size() == 7 && k[6] >= L'0' && k[6] <= L'9') {
        return VK_NUMPAD0 + (k[6] - L'0');
    }
    if (k.size() >= 2 && k[0] == L'F') {
        int f = _wtoi(k.substr(1).c_str());
        if (f >= 1 && f <= 12) return VK_F1 + (f - 1);
    }

    static const std::unordered_map<std::wstring, UINT> table = {
        {L"ESC", VK_ESCAPE}, {L"ENTER", VK_RETURN}, {L"SPACE", VK_SPACE},
        {L"LEFT", VK_LEFT}, {L"RIGHT", VK_RIGHT}, {L"UP", VK_UP}, {L"DOWN", VK_DOWN},
        {L"TAB", VK_TAB}, {L"BACKSPACE", VK_BACK}, {L"DELETE", VK_DELETE},
        {L"HOME", VK_HOME}, {L"END", VK_END}, {L"PGUP", VK_PRIOR}, {L"PGDN", VK_NEXT},
        {L"INS", VK_INSERT}
    };
    auto it = table.find(k);
    if (it != table.end()) return it->second;

    if (std::all_of(k.begin(), k.end(), iswdigit)) {
        int v = _wtoi(k.c_str());
        if (v >= 0 && v <= 255) return (UINT)v;
    }

    return VK_NUMPAD9;
}

static bool loadConfig(Config& cfg) {
    std::ifstream in("config.txt", std::ios::binary);
    if (!in) return false;

    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (content.size() >= 3 && (unsigned char)content[0] == 0xEF && (unsigned char)content[1] == 0xBB && (unsigned char)content[2] == 0xBF) {
        content.erase(0, 3);
    }

    std::istringstream ss(content);
    std::string line;
    while (std::getline(ss, line)) {
        std::wstring wline = trim(utf8ToWide(line));
        if (wline.empty() || wline[0] == L'#') continue;
        auto pos = wline.find(L'=');
        if (pos == std::wstring::npos) continue;
        std::wstring key = trim(wline.substr(0, pos));
        std::wstring val = trim(wline.substr(pos + 1));

        if (key == L"chromePath") cfg.chromePath = val;
        else if (key == L"userDataDir") cfg.userDataDir = val;
        else if (key == L"channels") {
            cfg.channels.clear();
            size_t start = 0;
            while (start <= val.size()) {
                size_t p = val.find(L'|', start);
                std::wstring u = trim(val.substr(start, p == std::wstring::npos ? std::wstring::npos : p - start));
                if (!u.empty()) cfg.channels.push_back(u);
                if (p == std::wstring::npos) break;
                start = p + 1;
            }
        } else if (key == L"shutdownKey") {
            cfg.shutdownKeyText = val;
            cfg.shutdownVk = parseVk(val);
        } else if (key == L"lastpage") cfg.lastpage = val;
    }

    if (cfg.lastpage.empty() && !cfg.channels.empty()) cfg.lastpage = cfg.channels[0];
    return !cfg.chromePath.empty() && !cfg.userDataDir.empty() && !cfg.channels.empty();
}

static void saveLastPage(const std::wstring& lastPage) {
    std::ifstream in("config.txt", std::ios::binary);
    if (!in) return;
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (content.size() >= 3 && (unsigned char)content[0] == 0xEF && (unsigned char)content[1] == 0xBB && (unsigned char)content[2] == 0xBF) {
        content.erase(0, 3);
    }

    std::istringstream ss(content);
    std::string line;
    std::vector<std::wstring> lines;
    bool replaced = false;
    while (std::getline(ss, line)) {
        std::wstring wline = utf8ToWide(line);
        std::wstring t = trim(wline);
        if (t.rfind(L"lastpage=", 0) == 0) {
            lines.push_back(L"lastpage=" + lastPage);
            replaced = true;
        } else {
            lines.push_back(wline);
        }
    }
    if (!replaced) lines.push_back(L"lastpage=" + lastPage);

    std::ofstream out("config.txt", std::ios::binary | std::ios::trunc);
    out.write("\xEF\xBB\xBF", 3);
    for (size_t i = 0; i < lines.size(); ++i) {
        std::string u8 = wideToUtf8(lines[i]);
        out.write(u8.c_str(), (std::streamsize)u8.size());
        out.write("\n", 1);
    }
}

static bool isChromeName(const std::wstring& exe) {
    std::wstring n = upper(exe);
    return n == L"CHROME.EXE";
}

static std::vector<DWORD> listChromePids() {
    std::vector<DWORD> pids;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return pids;
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap, &pe)) {
        do {
            if (isChromeName(pe.szExeFile)) pids.push_back(pe.th32ProcessID);
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return pids;
}

static void killAllChrome() {
    auto pids = listChromePids();
    for (DWORD pid : pids) {
        HANDLE hp = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pid);
        if (hp) {
            TerminateProcess(hp, 0);
            CloseHandle(hp);
        }
    }
}

static bool waitChromeGone(DWORD timeoutMs) {
    DWORD start = GetTickCount();
    while (GetTickCount() - start < timeoutMs) {
        if (listChromePids().empty()) return true;
        Sleep(120);
    }
    return listChromePids().empty();
}

struct WinFindCtx { DWORD pid; HWND hwnd; };

static BOOL CALLBACK enumWinForPid(HWND hwnd, LPARAM lParam) {
    auto* ctx = (WinFindCtx*)lParam;
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == ctx->pid && IsWindowVisible(hwnd) && GetWindow(hwnd, GW_OWNER) == nullptr) {
        ctx->hwnd = hwnd;
        return FALSE;
    }
    return TRUE;
}

static HWND findMainWindowForPid(DWORD pid) {
    WinFindCtx ctx{ pid, nullptr };
    EnumWindows(enumWinForPid, (LPARAM)&ctx);
    return ctx.hwnd;
}

static bool bringToFront(HWND hwnd) {
    if (!hwnd) return false;
    ShowWindow(hwnd, SW_RESTORE);
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    SetForegroundWindow(hwnd);
    return true;
}

static bool launchChrome(const std::wstring& url, DWORD& outPid, HWND& outHwnd) {
    std::wstring cmd = L"\"" + g_cfg.chromePath + L"\" --kiosk --user-data-dir=\"" + g_cfg.userDataDir + L"\" " + url;
    std::vector<wchar_t> buf(cmd.begin(), cmd.end());
    buf.push_back(L'\0');

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    BOOL ok = CreateProcessW(nullptr, buf.data(), nullptr, nullptr, FALSE, CREATE_UNICODE_ENVIRONMENT, nullptr, nullptr, &si, &pi);
    if (!ok) return false;

    outPid = pi.dwProcessId;
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    DWORD start = GetTickCount();
    outHwnd = nullptr;
    while (GetTickCount() - start < 10000) {
        outHwnd = findMainWindowForPid(outPid);
        if (outHwnd) return true;
        Sleep(150);
    }
    return false;
}

static bool startChannelWithRetry(const std::wstring& url, int retry = 2) {
    for (int i = 0; i <= retry; ++i) {
        killAllChrome();
        waitChromeGone(5000);
        DWORD pid = 0;
        HWND hwnd = nullptr;
        if (launchChrome(url, pid, hwnd)) {
            bringToFront(hwnd);
            return true;
        }
        Sleep(500);
    }
    return false;
}

static void adjustVolume(float delta) {
    IMMDeviceEnumerator* pEnum = nullptr;
    IMMDevice* pDevice = nullptr;
    IAudioEndpointVolume* pVolume = nullptr;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnum))) return;
    if (FAILED(pEnum->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice))) { pEnum->Release(); return; }
    if (FAILED(pDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, (void**)&pVolume))) {
        pDevice->Release(); pEnum->Release(); return;
    }

    float curr = 0.0f;
    if (SUCCEEDED(pVolume->GetMasterVolumeLevelScalar(&curr))) {
        float next = curr + delta;
        if (next < 0.0f) next = 0.0f;
        if (next > 1.0f) next = 1.0f;
        pVolume->SetMasterVolumeLevelScalar(next, nullptr);
    }

    pVolume->Release();
    pDevice->Release();
    pEnum->Release();
}

static void doShutdown() {
    wchar_t cmd[] = L"shutdown /s /t 0 /f";
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (CreateProcessW(nullptr, cmd, nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
}

static bool switchChannel(int dir) {
    if (g_cfg.channels.empty()) return false;
    int target = g_currentIndex + dir;
    if (target < 0) target = (int)g_cfg.channels.size() - 1;
    if (target >= (int)g_cfg.channels.size()) target = 0;

    std::wstring oldUrl = g_cfg.channels[g_currentIndex];
    std::wstring newUrl = g_cfg.channels[target];

    if (startChannelWithRetry(newUrl, 2)) {
        g_currentIndex = target;
        g_cfg.lastpage = newUrl;
        saveLastPage(newUrl);
        return true;
    }
    if (oldUrl != newUrl) {
        startChannelWithRetry(oldUrl, 1);
    }
    return false;
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    if (!loadConfig(g_cfg)) {
        MessageBoxW(nullptr, L"config.txt 配置无效或缺失。", L"TV Kiosk", MB_ICONERROR);
        CoUninitialize();
        return 1;
    }

    for (size_t i = 0; i < g_cfg.channels.size(); ++i) {
        if (g_cfg.channels[i] == g_cfg.lastpage) {
            g_currentIndex = (int)i;
            break;
        }
    }

    if (!startChannelWithRetry(g_cfg.channels[g_currentIndex], 2)) {
        MessageBoxW(nullptr, L"启动 Chrome 失败。", L"TV Kiosk", MB_ICONERROR);
    }

    RegisterHotKey(nullptr, 1, MOD_NOREPEAT, VK_UP);
    RegisterHotKey(nullptr, 2, MOD_NOREPEAT, VK_DOWN);
    RegisterHotKey(nullptr, 3, MOD_NOREPEAT, VK_LEFT);
    RegisterHotKey(nullptr, 4, MOD_NOREPEAT, VK_RIGHT);
    RegisterHotKey(nullptr, 5, MOD_NOREPEAT, g_cfg.shutdownVk);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == WM_HOTKEY) {
            switch (msg.wParam) {
            case 1: adjustVolume(+0.07f); break;
            case 2: adjustVolume(-0.07f); break;
            case 3: switchChannel(-1); break;
            case 4: switchChannel(+1); break;
            case 5: doShutdown(); break;
            default: break;
            }
        }
    }

    UnregisterHotKey(nullptr, 1);
    UnregisterHotKey(nullptr, 2);
    UnregisterHotKey(nullptr, 3);
    UnregisterHotKey(nullptr, 4);
    UnregisterHotKey(nullptr, 5);

    CoUninitialize();
    return 0;
}
