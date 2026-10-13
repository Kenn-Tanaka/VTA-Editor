#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <shellapi.h>
#include <shlwapi.h>

#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>
#include <atomic>

// Include cpp-httplib (single-header)
#define CPPHTTPLIB_NO_EXCEPTIONS
#include "../llm-gateway/build/_deps/cpp_httplib-src/httplib.h"

namespace fs = std::filesystem;

static PROCESS_INFORMATION g_gatewayProcInfo = {0};
static std::atomic<bool> g_gatewayStartedByUs(false);
static httplib::Server* g_pServer = nullptr;

BOOL WINAPI ConsoleCtrlHandler(DWORD ctrlType) {
    if (g_pServer) {
        g_pServer->stop();
    }
    if (g_gatewayStartedByUs.load() && g_gatewayProcInfo.hProcess) {
        TerminateProcess(g_gatewayProcInfo.hProcess, 0);
        CloseHandle(g_gatewayProcInfo.hProcess);
        CloseHandle(g_gatewayProcInfo.hThread);
        g_gatewayProcInfo.hProcess = NULL;
    }
    return FALSE;
}

bool IsPortOpen(const std::string& host, int port) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return false;

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        WSACleanup();
        return false;
    }

    u_long mode = 1; // non-blocking
    ioctlsocket(sock, FIONBIO, &mode);

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<u_short>(port));
    inet_pton(AF_INET, host.c_str(), &addr.sin_addr);

    connect(sock, (sockaddr*)&addr, sizeof(addr));

    fd_set writeSet;
    FD_ZERO(&writeSet);
    FD_SET(sock, &writeSet);

    timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 300000; // 300ms

    int sel = select(0, NULL, &writeSet, NULL, &tv);
    closesocket(sock);
    WSACleanup();

    return (sel > 0);
}

fs::path GetExeDir() {
    wchar_t buffer[MAX_PATH];
    GetModuleFileNameW(NULL, buffer, MAX_PATH);
    return fs::path(buffer).parent_path();
}

std::string GetMimeType(const std::string& ext) {
    if (ext == ".html" || ext == ".htm") return "text/html; charset=utf-8";
    if (ext == ".js" || ext == ".mjs") return "application/javascript; charset=utf-8";
    if (ext == ".css") return "text/css; charset=utf-8";
    if (ext == ".json") return "application/json; charset=utf-8";
    if (ext == ".svg") return "image/svg+xml";
    if (ext == ".png") return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".gif") return "image/gif";
    if (ext == ".ico") return "image/x-icon";
    if (ext == ".wasm") return "application/wasm";
    if (ext == ".txt") return "text/plain; charset=utf-8";
    return "application/octet-stream";
}

bool IsAllowedOrigin(const std::string& origin) {
    if (origin.empty()) return false;
    if (origin == "http://localhost" || origin.rfind("http://localhost:", 0) == 0) return true;
    if (origin == "http://127.0.0.1" || origin.rfind("http://127.0.0.1:", 0) == 0) return true;
    return false;
}

int main(int argc, char* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);

    std::cout << "================================================================" << std::endl;
    std::cout << "   Variation Tree Analysis Editor & LLM Gateway Starter" << std::endl;
    std::cout << "================================================================" << std::endl;
    std::cout << std::endl;

    fs::path baseDir = GetExeDir();

    // Check dist directory
    fs::path distDir = baseDir / "dist";
    if (!fs::exists(distDir)) {
        // Maybe in parent dir
        if (fs::exists(baseDir.parent_path() / "dist")) {
            distDir = baseDir.parent_path() / "dist";
            baseDir = baseDir.parent_path();
        }
    }

    if (!fs::exists(distDir / "index.html")) {
        std::cerr << "❌ [Error] dist/index.html が見つかりません: " << distDir.string() << std::endl;
        std::cerr << "パッケージが正しく展開されているか確認してください。" << std::endl;
        system("pause");
        return 1;
    }

    // Locate llm-gateway executable
    std::vector<fs::path> gatewayCandidates = {
        baseDir / "tools" / "llm-gateway" / "llm-gateway.exe",
        baseDir / "llm-gateway.exe",
        baseDir / "tools" / "llm-gateway" / "build" / "llm-gateway.exe"
    };

    fs::path gatewayExe;
    for (const auto& p : gatewayCandidates) {
        if (fs::exists(p)) {
            gatewayExe = p;
            break;
        }
    }

    // Locate config
    std::vector<fs::path> configCandidates = {
        baseDir / "tools" / "llm-gateway" / "config" / "gateway.json",
        baseDir / "gateway.json",
        baseDir / "tools" / "llm-gateway" / "config" / "gateway.example.json"
    };

    fs::path gatewayConfig;
    for (const auto& p : configCandidates) {
        if (fs::exists(p)) {
            gatewayConfig = p;
            break;
        }
    }

    const int GATEWAY_PORT = 8765;
    const int WEB_PORT = 5173;

    // Check if Gateway is already running
    bool isGatewayRunning = IsPortOpen("127.0.0.1", GATEWAY_PORT);
    if (isGatewayRunning) {
        std::cout << "✔ [LLM Gateway] ポート " << GATEWAY_PORT << " で既に起動中です。" << std::endl;
    } else if (!gatewayExe.empty()) {
        std::cout << "✔ [LLM Gateway] 実行バイナリ: " << gatewayExe.string() << std::endl;
        if (!gatewayConfig.empty()) {
            std::cout << "✔ [LLM Gateway] 設定ファイル: " << gatewayConfig.string() << std::endl;
        }
        std::cout << "▶ [LLM Gateway] ポート " << GATEWAY_PORT << " で Gateway を起動中..." << std::endl;

        std::wstring cmdLine = L"\"" + gatewayExe.wstring() + L"\"";
        if (!gatewayConfig.empty()) {
            cmdLine += L" \"" + gatewayConfig.wstring() + L"\"";
        }

        STARTUPINFOW si = { sizeof(si) };
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE; // Run gateway background
        std::vector<wchar_t> cmdBuf(cmdLine.begin(), cmdLine.end());
        cmdBuf.push_back(0);

        std::wstring workDir = gatewayExe.parent_path().wstring();

        if (CreateProcessW(NULL, cmdBuf.data(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, workDir.c_str(), &si, &g_gatewayProcInfo)) {
            g_gatewayStartedByUs = true;
            // Wait for Gateway to become ready
            for (int i = 0; i < 30; i++) {
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                if (IsPortOpen("127.0.0.1", GATEWAY_PORT)) {
                    std::cout << "✔ [LLM Gateway] 起動完了 (127.0.0.1:" << GATEWAY_PORT << ")" << std::endl;
                    break;
                }
            }
        } else {
            std::cerr << "⚠ [LLM Gateway] 起動に失敗しました (ErrorCode: " << GetLastError() << ")" << std::endl;
        }
    } else {
        std::cout << "ℹ [LLM Gateway] バイナリが見つかりませんでした (スキップ)" << std::endl;
    }

    // Set up HTTP Server
    httplib::Server svr;
    g_pServer = &svr;

    // Read index.html for SPA fallback
    std::string indexHtmlContent;
    {
        std::ifstream f(distDir / "index.html", std::ios::binary);
        if (f.is_open()) {
            std::stringstream ss;
            ss << f.rdbuf();
            indexHtmlContent = ss.str();
        }
    }

    // Helper to set CORS headers for permitted local origins
    auto applyCorsHeaders = [](const httplib::Request& req, httplib::Response& res) {
        std::string origin = req.get_header_value("Origin");
        if (!origin.empty() && IsAllowedOrigin(origin)) {
            res.set_header("Access-Control-Allow-Origin", origin);
            res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, PATCH, OPTIONS");
            res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With");
            res.set_header("Vary", "Origin");
        }
    };

    // Handle /v1/* Reverse Proxy to Gateway
    auto proxyHandler = [&](const httplib::Request& req, httplib::Response& res) {
        std::string origin = req.get_header_value("Origin");
        // Reject external origins trying to access local LLM Gateway
        if (!origin.empty() && !IsAllowedOrigin(origin)) {
            res.status = 403;
            res.set_header("Content-Type", "application/json; charset=utf-8");
            res.body = "{\"error\":{\"message\":\"Forbidden Origin\",\"type\":\"cors_origin_rejected\"}}";
            return;
        }

        httplib::Client cli("127.0.0.1", GATEWAY_PORT);
        cli.set_connection_timeout(10, 0);
        cli.set_read_timeout(600, 0);
        cli.set_write_timeout(600, 0);

        httplib::Headers headers;
        for (const auto& h : req.headers) {
            std::string k = h.first;
            std::string lowerK = k;
            for (auto& c : lowerK) c = (char)tolower(c);
            if (lowerK != "host" && lowerK != "accept-encoding" && lowerK != "content-length") {
                headers.emplace(h.first, h.second);
            }
        }
        headers.emplace("Host", "127.0.0.1:" + std::to_string(GATEWAY_PORT));
        headers.emplace("Accept-Encoding", "identity");

        std::string contentType = req.get_header_value("Content-Type");
        if (contentType.empty()) {
            contentType = "application/json";
        }

        httplib::Result result;
        if (req.method == "GET") {
            result = cli.Get(req.target, headers);
        } else if (req.method == "POST") {
            result = cli.Post(req.target, headers, req.body, contentType);
        } else if (req.method == "DELETE") {
            result = cli.Delete(req.target, headers);
        } else if (req.method == "PUT") {
            result = cli.Put(req.target, headers, req.body, contentType);
        } else if (req.method == "PATCH") {
            result = cli.Patch(req.target, headers, req.body, contentType);
        } else if (req.method == "OPTIONS") {
            res.status = 200;
            applyCorsHeaders(req, res);
            return;
        }

        if (result) {
            res.status = result->status;
            for (const auto& h : result->headers) {
                std::string k = h.first;
                std::string lowerK = k;
                for (auto& c : lowerK) c = (char)tolower(c);
                if (lowerK != "content-length" && 
                    lowerK != "transfer-encoding" && 
                    lowerK != "connection" && 
                    lowerK != "keep-alive" &&
                    lowerK != "access-control-allow-origin" &&
                    lowerK != "access-control-allow-methods" &&
                    lowerK != "access-control-allow-headers" &&
                    lowerK != "vary") {
                    res.set_header(h.first, h.second);
                }
            }
            applyCorsHeaders(req, res);
            std::string ct = result->get_header_value("Content-Type");
            if (ct.empty()) ct = "application/json; charset=utf-8";
            res.set_content(result->body, ct);
        } else {
            res.status = 502;
            res.set_header("Content-Type", "application/json; charset=utf-8");
            applyCorsHeaders(req, res);
            res.body = "{\"error\":{\"message\":\"LLM Gateway (127.0.0.1:" + std::to_string(GATEWAY_PORT) + ") への接続に失敗しました。\",\"type\":\"gateway_connection_error\"}}";
        }
    };

    svr.Get(R"(/v1/.*)", proxyHandler);
    svr.Post(R"(/v1/.*)", proxyHandler);
    svr.Delete(R"(/v1/.*)", proxyHandler);
    svr.Put(R"(/v1/.*)", proxyHandler);
    svr.Patch(R"(/v1/.*)", proxyHandler);
    svr.Options(R"(/v1/.*)", proxyHandler);

    // Add CORS headers to all responses for permitted local origins
    svr.set_post_routing_handler([applyCorsHeaders](const auto& req, auto& res) {
        applyCorsHeaders(req, res);
    });

    // Static file handler & SPA Fallback
    svr.set_mount_point("/", distDir.string());

    svr.set_error_handler([&](const httplib::Request& req, httplib::Response& res) {
        if (res.status == 404 && req.method == "GET" && req.path.rfind("/v1", 0) != 0) {
            // SPA Fallback to index.html
            res.status = 200;
            res.set_content(indexHtmlContent, "text/html; charset=utf-8");
        }
    });

    int selectedPort = 0;
    for (int p = 5173; p <= 5185; p++) {
        if (!IsPortOpen("127.0.0.1", p)) {
            if (svr.bind_to_port("127.0.0.1", p)) {
                selectedPort = p;
                break;
            }
        }
    }

    if (selectedPort == 0) {
        std::cerr << "❌ 利用可能なポート (5173〜5185) の確保に失敗しました。既に他のプロセスで使用されている可能性があります。" << std::endl;
        system("pause");
        return 1;
    }

    std::cout << "✔ [VTA Web Server] サーバー起動完了: http://127.0.0.1:" << selectedPort << std::endl;
    std::cout << "▶ [ブラウザ] 既定のWebブラウザでエディタを開きます..." << std::endl;
    std::cout << std::endl;
    std::cout << "================================================================" << std::endl;
    std::cout << "  Variation Tree Analysis Editor が正常に起動しました！" << std::endl;
    std::cout << "  URL: http://127.0.0.1:" << selectedPort << std::endl;
    std::cout << "  (終了するにはこのウィンドウを閉じるか Ctrl+C を押してください)" << std::endl;
    std::cout << "================================================================" << std::endl;

    // Launch browser in a separate thread
    std::thread([selectedPort]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        ShellExecuteA(NULL, "open", ("http://127.0.0.1:" + std::to_string(selectedPort)).c_str(), NULL, NULL, SW_SHOWNORMAL);
    }).detach();

    // Start listening on the bound port
    svr.listen_after_bind();

    return 0;
}
