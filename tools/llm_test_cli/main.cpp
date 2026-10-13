#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <winhttp.h>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <chrono>
#include <iomanip>

#pragma comment(lib, "winhttp.lib")

// Console color helpers
namespace ConsoleColor {
    void Set(WORD color) {
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        SetConsoleTextAttribute(hConsole, color);
    }
    void Reset() {
        Set(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    }
    void Green(const std::string& text) {
        Set(FOREGROUND_GREEN | FOREGROUND_INTENSITY);
        std::cout << text;
        Reset();
    }
    void Red(const std::string& text) {
        Set(FOREGROUND_RED | FOREGROUND_INTENSITY);
        std::cout << text;
        Reset();
    }
    void Yellow(const std::string& text) {
        Set(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
        std::cout << text;
        Reset();
    }
    void Cyan(const std::string& text) {
        Set(FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
        std::cout << text;
        Reset();
    }
}

// Convert UTF-8 std::string to std::wstring
std::wstring Utf8ToWide(const std::string& str) {
    if (str.empty()) return L"";
    int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
    std::wstring wstr(sizeNeeded, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wstr[0], sizeNeeded);
    return wstr;
}

// Convert std::wstring to UTF-8 std::string
std::string WideToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string str(sizeNeeded, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &str[0], sizeNeeded, NULL, NULL);
    return str;
}

// Simple JSON string escape preserving UTF-8 multibyte characters
std::string EscapeJsonString(const std::string& input) {
    std::ostringstream ss;
    for (size_t i = 0; i < input.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(input[i]);
        switch (c) {
            case '"':  ss << "\\\""; break;
            case '\\': ss << "\\\\"; break;
            case '\b': ss << "\\b";  break;
            case '\f': ss << "\\f";  break;
            case '\n': ss << "\\n";  break;
            case '\r': ss << "\\r";  break;
            case '\t': ss << "\\t";  break;
            default:
                if (c < 0x20) {
                    ss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)c;
                } else {
                    ss << input[i];
                }
        }
    }
    return ss.str();
}

// Lightweight HTTP Client using Windows WinHTTP API
struct HttpResponse {
    int statusCode;
    std::string body;
    std::string error;
    double elapsedMs;
};

HttpResponse SendHttpRequest(
    const std::string& host,
    int port,
    const std::string& path,
    const std::string& method,
    const std::string& body = "",
    const std::string& token = ""
) {
    HttpResponse result = {0, "", "", 0.0};
    auto startTime = std::chrono::high_resolution_clock::now();

    HINTERNET hSession = WinHttpOpen(
        L"VTA-LLM-TestCLI/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );

    if (!hSession) {
        result.error = "WinHttpOpen に失敗しました (Error code: " + std::to_string(GetLastError()) + ")";
        return result;
    }

    std::wstring wHost = Utf8ToWide(host);
    HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(), (INTERNET_PORT)port, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        result.error = "WinHttpConnect に失敗しました。Gatewayが起動しているか確認してください (Error code: " + std::to_string(GetLastError()) + ")";
        return result;
    }

    std::wstring wPath = Utf8ToWide(path);
    std::wstring wMethod = Utf8ToWide(method);

    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect,
        wMethod.c_str(),
        wPath.c_str(),
        NULL,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        0 // HTTP
    );

    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        result.error = "WinHttpOpenRequest に失敗しました (Error code: " + std::to_string(GetLastError()) + ")";
        return result;
    }

    // Set Timeouts (connect: 5s, send: 30s, receive: 60s)
    WinHttpSetTimeouts(hRequest, 5000, 5000, 30000, 60000);

    // Build Headers
    std::wstring headers = L"Content-Type: application/json; charset=utf-8\r\n";
    if (!token.empty()) {
        headers += L"Authorization: Bearer " + Utf8ToWide(token) + L"\r\n";
    }

    BOOL sendSuccess = FALSE;
    if (method == "POST" && !body.empty()) {
        sendSuccess = WinHttpSendRequest(
            hRequest,
            headers.c_str(),
            (DWORD)headers.length(),
            (LPVOID)body.c_str(),
            (DWORD)body.length(),
            (DWORD)body.length(),
            0
        );
    } else {
        sendSuccess = WinHttpSendRequest(
            hRequest,
            headers.c_str(),
            (DWORD)headers.length(),
            NULL,
            0,
            0,
            0
        );
    }

    if (!sendSuccess) {
        DWORD err = GetLastError();
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        result.error = "リクエストの送信に失敗しました (WinHttpSendRequest Error: " + std::to_string(err) + ")";
        return result;
    }

    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        DWORD err = GetLastError();
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        result.error = "レスポンスの受信に失敗しました (WinHttpReceiveResponse Error: " + std::to_string(err) + ")";
        return result;
    }

    // Get HTTP Status Code
    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    WinHttpQueryHeaders(
        hRequest,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &statusCode,
        &statusCodeSize,
        WINHTTP_NO_HEADER_INDEX
    );
    result.statusCode = (int)statusCode;

    // Read Response Body
    std::string responseBody;
    DWORD bytesAvailable = 0;
    while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
        std::vector<char> buffer(bytesAvailable + 1, 0);
        DWORD bytesRead = 0;
        if (WinHttpReadData(hRequest, buffer.data(), bytesAvailable, &bytesRead) && bytesRead > 0) {
            responseBody.append(buffer.data(), bytesRead);
        }
    }

    result.body = responseBody;

    auto endTime = std::chrono::high_resolution_clock::now();
    result.elapsedMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return result;
}

void PrintBanner() {
    ConsoleColor::Cyan("================================================================\n");
    ConsoleColor::Cyan("   Lightweight LLM Gateway C++ CLI Connection Tester\n");
    ConsoleColor::Cyan("================================================================\n\n");
}

void PrintUsage(const char* progName) {
    std::cout << "使用法: " << progName << " [オプション]\n\n"
              << "オプション:\n"
              << "  --host <ホスト>      Gatewayのホスト (デフォルト: 127.0.0.1)\n"
              << "  --port <ポート>      Gatewayのポート (デフォルト: 8765)\n"
              << "  --token <トークン>   Gateway Client Token (任意)\n"
              << "  --model <モデルID>   テストするモデルID (例: openrouter/anthropic/claude-sonnet-5, lmstudio/local-model)\n"
              << "  --prompt <プロンプト> テスト用プロンプト (デフォルト: \"こんにちは。自己紹介をしてください。\")\n"
              << "  --models             /v1/models エンドポイントの一覧取得テストを実行\n"
              << "  --help               ヘルプを表示\n\n"
              << "実行例:\n"
              << "  " << progName << " --models\n"
              << "  " << progName << " --model openrouter/anthropic/claude-sonnet-5\n"
              << "  " << progName << " --model lmstudio/local-model --prompt \"VTA分析について教えて\"\n\n";
}

int main(int argc, char* argv[]) {
    // Enable UTF-8 input/output on Windows console
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    std::string host = "127.0.0.1";
    int port = 8765;
    std::string token = "";
    std::string model = "openrouter/anthropic/claude-sonnet-5";
    std::string prompt = "こんにちは。自己紹介をしてください。";
    std::string chatPath = "/v1/chat/completions";
    bool testModelsOnly = false;

    // Use CommandLineToArgvW to get authentic Unicode arguments regardless of console code page
    int argcW = 0;
    LPWSTR* argvW = CommandLineToArgvW(GetCommandLineW(), &argcW);
    std::vector<std::string> args;
    if (argvW) {
        for (int i = 0; i < argcW; ++i) {
            args.push_back(WideToUtf8(argvW[i]));
        }
        LocalFree(argvW);
    } else {
        for (int i = 0; i < argc; ++i) {
            args.push_back(argv[i]);
        }
    }

    for (size_t i = 1; i < args.size(); ++i) {
        std::string arg = args[i];
        if (arg == "--host" && i + 1 < args.size()) {
            host = args[++i];
        } else if (arg == "--port" && i + 1 < args.size()) {
            port = std::stoi(args[++i]);
        } else if (arg == "--token" && i + 1 < args.size()) {
            token = args[++i];
        } else if (arg == "--model" && i + 1 < args.size()) {
            model = args[++i];
        } else if (arg == "--prompt" && i + 1 < args.size()) {
            prompt = args[++i];
        } else if (arg == "--path" && i + 1 < args.size()) {
            chatPath = args[++i];
        } else if (arg == "--endpoint" && i + 1 < args.size()) {
            std::string ep = args[++i];
            // Simple URL parse: http://host:port/path
            if (ep.rfind("http://", 0) == 0) ep = ep.substr(7);
            else if (ep.rfind("https://", 0) == 0) ep = ep.substr(8);
            size_t slashPos = ep.find('/');
            std::string hostPort = (slashPos != std::string::npos) ? ep.substr(0, slashPos) : ep;
            std::string path = (slashPos != std::string::npos) ? ep.substr(slashPos) : "";
            size_t colonPos = hostPort.find(':');
            if (colonPos != std::string::npos) {
                host = hostPort.substr(0, colonPos);
                port = std::stoi(hostPort.substr(colonPos + 1));
            } else {
                host = hostPort;
            }
            if (path == "/v1/models" || path == "/models") {
                testModelsOnly = true;
            }
        } else if (arg == "--models") {
            testModelsOnly = true;
        } else if (arg == "--help" || arg == "-h") {
            PrintBanner();
            PrintUsage(args[0].c_str());
            return 0;
        }
    }

    PrintBanner();

    std::cout << "[設定情報]\n";
    std::cout << " - Gateway エンドポイント: http://" << host << ":" << port << "\n";
    std::cout << " - Client Token: " << (token.empty() ? "(未指定 / client_auth: false)" : "********") << "\n";
    std::cout << " - テストモデル: " << model << "\n\n";

    // Step 1: Test /v1/models (Discovery Test)
    ConsoleColor::Yellow("▶ ステップ 1: GET /v1/models (モデル一覧取得テスト)\n");
    HttpResponse modelsResp = SendHttpRequest(host, port, "/v1/models", "GET", "", token);

    if (!modelsResp.error.empty()) {
        ConsoleColor::Red("✖ 接続失敗: " + modelsResp.error + "\n\n");
        ConsoleColor::Yellow("【診断ヒント】\n");
        std::cout << "  1. Gateway (127.0.0.1:" << port << ") が起動しているか確認してください。\n"
                  << "  2. ポート番号が正しいか確認してください。\n\n";
        return 1;
    }

    std::cout << "  HTTP ステータス: ";
    if (modelsResp.statusCode == 200) {
        ConsoleColor::Green("200 OK");
    } else {
        ConsoleColor::Red(std::to_string(modelsResp.statusCode));
    }
    std::cout << " (応答時間: " << std::fixed << std::setprecision(1) << modelsResp.elapsedMs << " ms)\n";

    // Count models in JSON
    size_t modelCount = 0;
    size_t pos = 0;
    while ((pos = modelsResp.body.find("\"id\":", pos)) != std::string::npos) {
        ++modelCount;
        pos += 5;
    }

    bool modelFound = (modelsResp.body.find(model) != std::string::npos);
    std::cout << "  発見モデル数: " << modelCount << " 件\n";
    if (modelFound) {
        std::cout << "  指定モデル検証: ";
        ConsoleColor::Green("✔ 一致するモデルIDがAllowlistに登録されています (" + model + ")\n\n");
    } else {
        std::cout << "  指定モデル検証: ";
        ConsoleColor::Yellow("⚠ モデル一覧に完全一致が見つかりませんでしたが、globパターン等で実行を試みます\n\n");
    }

    if (testModelsOnly) {
        if (modelsResp.body.length() < 2000) {
            std::cout << "  レスポンス Body:\n" << modelsResp.body << "\n\n";
        } else {
            std::cout << "  (モデル一覧が膨大（" << modelsResp.body.length() << " bytes）のため要約表示しました)\n\n";
        }
        ConsoleColor::Green("✔ /v1/models テスト完了\n");
        return 0;
    }

    // Step 2: Test /v1/chat/completions (Inference Test)
    ConsoleColor::Yellow("▶ ステップ 2: POST /v1/chat/completions (チャット生成テスト)\n");
    std::cout << "  プロンプト: \"" << prompt << "\"\n";

    std::string requestBody = "{\n"
        "  \"model\": \"" + EscapeJsonString(model) + "\",\n"
        "  \"messages\": [\n"
        "    {\"role\": \"user\", \"content\": \"" + EscapeJsonString(prompt) + "\"}\n"
        "  ],\n"
        "  \"temperature\": 0.2\n"
        "}";

    std::cout << "  リクエスト送信中 (" << model << ") -> " << chatPath << "...\n";
    HttpResponse chatResp = SendHttpRequest(host, port, chatPath, "POST", requestBody, token);

    if (!chatResp.error.empty()) {
        ConsoleColor::Red("✖ 生成リクエスト失敗: " + chatResp.error + "\n");
        return 1;
    }

    std::cout << "  HTTP ステータス: ";
    if (chatResp.statusCode == 200) {
        ConsoleColor::Green("200 OK");
    } else {
        ConsoleColor::Red(std::to_string(chatResp.statusCode));
    }
    std::cout << " (応答時間: " << std::fixed << std::setprecision(1) << chatResp.elapsedMs << " ms)\n\n";

    if (chatResp.statusCode == 200) {
        // Try to extract content string for clean reading
        std::string contentTag = "\"content\":";
        size_t contentPos = chatResp.body.find(contentTag);
        
        ConsoleColor::Cyan("━━━━━━━━━━━━━━━━━━━━ [LLMからの回答] ━━━━━━━━━━━━━━━━━━━━\n");
        if (contentPos != std::string::npos) {
            size_t startQuote = chatResp.body.find("\"", contentPos + contentTag.length());
            if (startQuote != std::string::npos) {
                // Parse escaped JSON string
                std::string text;
                for (size_t k = startQuote + 1; k < chatResp.body.length(); ++k) {
                    if (chatResp.body[k] == '\\' && k + 1 < chatResp.body.length()) {
                        char next = chatResp.body[k + 1];
                        if (next == 'n') { text += '\n'; ++k; }
                        else if (next == 't') { text += '\t'; ++k; }
                        else if (next == 'r') { text += '\r'; ++k; }
                        else if (next == '"') { text += '"'; ++k; }
                        else if (next == '\\') { text += '\\'; ++k; }
                        else { text += next; ++k; }
                    } else if (chatResp.body[k] == '"') {
                        break;
                    } else {
                        text += chatResp.body[k];
                    }
                }
                std::cout << text << "\n";
            } else {
                std::cout << chatResp.body << "\n";
            }
        } else {
            std::cout << chatResp.body << "\n";
        }
        ConsoleColor::Cyan("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n");
        ConsoleColor::Green("✔ 全テスト成功! Gatewayとの通信およびLLM推論が正常に完了しました。\n");
    } else {
        ConsoleColor::Red("  エラーレスポンス Body:\n" + chatResp.body + "\n\n");
        ConsoleColor::Yellow("【エラー時の確認事項】\n");
        std::cout << "  - 400/403: Allowlist にモデル (" << model << ") が登録されているか確認してください。\n"
                  << "  - 401: Client Token (client_auth) または Provider API Key (Credential Manager) を確認してください。\n"
                  << "  - 502: 上流のLLMプロバイダ（OpenRouter / LM Studio）との接続を確認してください。\n";
    }

    return 0;
}
