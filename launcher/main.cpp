#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <winhttp.h>
#include <bcrypt.h>
#include <shellapi.h>
#include <shlobj.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "gdi32.lib")

namespace
{
    constexpr wchar_t kWindowClass[] = L"MirickLauncherWindow";
    constexpr wchar_t kProductName[] = L"mirick launcher";
    constexpr wchar_t kApiHost[] = L"api.github.com";
    constexpr wchar_t kReleasePath[] = L"/repos/grom666-sss/mirick/releases/latest";
    constexpr char kProgramAsset[] = "mirick-client.exe";
    constexpr char kChecksumAsset[] = "mirick-client.exe.sha256";
    constexpr UINT WM_APP_RESULT = WM_APP + 1;
    constexpr UINT WM_APP_PROGRESS = WM_APP + 2;

    enum control_id
    {
        IdInstallPath = 100,
        IdBrowse,
        IdCheck,
        IdInstall,
        IdLaunch,
        IdClose,
        IdMinimize
    };

    enum class job_kind { none, check, install };

    struct release_info
    {
        std::string version;
        std::wstring program_url;
        std::wstring checksum_url;
    };

    struct app_state
    {
        HWND window{};
        HWND path_edit{};
        HWND browse_button{};
        HWND check_button{};
        HWND install_button{};
        HWND launch_button{};
        HWND close_button{};
        HWND minimize_button{};
        HFONT font{};
        HFONT title_font{};
        HBRUSH edit_brush{};
        std::atomic_bool busy{ false };
        std::atomic_int progress{ 0 };
        release_info latest{};
        std::wstring status = L"Готов к проверке обновлений";
        std::wstring installed_version = L"не установлено";
        bool update_available = false;
    } g_app;

    std::wstring Utf8ToWide(const std::string& value)
    {
        if (value.empty()) return {};
        int count = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
        std::wstring result(static_cast<size_t>(count), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), count);
        return result;
    }

    std::string WideToUtf8(const std::wstring& value)
    {
        if (value.empty()) return {};
        int count = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
        std::string result(static_cast<size_t>(count), '\0');
        WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), count, nullptr, nullptr);
        return result;
    }

    std::filesystem::path SettingsDirectory()
    {
        wchar_t path[MAX_PATH]{};
        if (FAILED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, path)))
            return std::filesystem::current_path();
        return std::filesystem::path(path) / L"mirick";
    }

    std::filesystem::path SettingsPath() { return SettingsDirectory() / L"launcher.ini"; }

    std::wstring DefaultInstallPath()
    {
        return (SettingsDirectory() / L"app").wstring();
    }

    std::wstring GetInstallPath()
    {
        int length = GetWindowTextLengthW(g_app.path_edit);
        if (length <= 0) return {};
        std::wstring value(static_cast<size_t>(length + 1), L'\0');
        GetWindowTextW(g_app.path_edit, value.data(), length + 1);
        value.resize(static_cast<size_t>(length));
        return value;
    }

    void LoadSettings()
    {
        auto file = SettingsPath();
        wchar_t path[1024]{};
        wchar_t version[128]{};
        GetPrivateProfileStringW(L"launcher", L"install_path", DefaultInstallPath().c_str(), path, _countof(path), file.c_str());
        GetPrivateProfileStringW(L"launcher", L"installed_version", L"не установлено", version, _countof(version), file.c_str());
        SetWindowTextW(g_app.path_edit, path);
        g_app.installed_version = version;
    }

    void SaveSettings(const std::wstring& installed_version = {})
    {
        std::error_code ec;
        std::filesystem::create_directories(SettingsDirectory(), ec);
        auto file = SettingsPath();
        WritePrivateProfileStringW(L"launcher", L"install_path", GetInstallPath().c_str(), file.c_str());
        if (!installed_version.empty())
        {
            WritePrivateProfileStringW(L"launcher", L"installed_version", installed_version.c_str(), file.c_str());
            g_app.installed_version = installed_version;
        }
    }

    struct internet_handle
    {
        HINTERNET value{};
        internet_handle() = default;
        explicit internet_handle(HINTERNET handle) : value(handle) {}
        ~internet_handle() { if (value) WinHttpCloseHandle(value); }
        internet_handle(const internet_handle&) = delete;
        internet_handle& operator=(const internet_handle&) = delete;
        operator HINTERNET() const { return value; }
    };

    bool HttpGet(const std::wstring& host, const std::wstring& path, std::vector<unsigned char>& output, std::wstring& error, bool report_progress = false)
    {
        internet_handle session(WinHttpOpen(L"mirick-launcher/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
            WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
        if (!session.value) { error = L"Не удалось открыть WinHTTP"; return false; }
        WinHttpSetTimeouts(session, 10000, 10000, 20000, 30000);

        internet_handle connection(WinHttpConnect(session, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0));
        if (!connection.value) { error = L"Не удалось подключиться к GitHub"; return false; }

        internet_handle request(WinHttpOpenRequest(connection, L"GET", path.c_str(), nullptr,
            WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
        if (!request.value) { error = L"Не удалось создать HTTP-запрос"; return false; }

        const wchar_t headers[] = L"Accept: application/vnd.github+json\r\nX-GitHub-Api-Version: 2022-11-28\r\n";
        if (!WinHttpAddRequestHeaders(request, headers, -1L, WINHTTP_ADDREQ_FLAG_ADD) ||
            !WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
            !WinHttpReceiveResponse(request, nullptr))
        {
            error = L"GitHub не ответил на запрос";
            return false;
        }

        DWORD status = 0;
        DWORD status_size = sizeof(status);
        WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size, WINHTTP_NO_HEADER_INDEX);
        if (status < 200 || status >= 300)
        {
            std::wstringstream text;
            text << L"GitHub вернул HTTP " << status;
            error = text.str();
            return false;
        }

        unsigned long long total = 0;
        wchar_t length_text[64]{};
        DWORD length_size = sizeof(length_text);
        if (WinHttpQueryHeaders(request, WINHTTP_QUERY_CONTENT_LENGTH, WINHTTP_HEADER_NAME_BY_INDEX,
            length_text, &length_size, WINHTTP_NO_HEADER_INDEX))
            total = _wcstoui64(length_text, nullptr, 10);

        for (;;)
        {
            DWORD available = 0;
            if (!WinHttpQueryDataAvailable(request, &available)) { error = L"Ошибка чтения ответа"; return false; }
            if (available == 0) break;
            size_t old_size = output.size();
            output.resize(old_size + available);
            DWORD read = 0;
            if (!WinHttpReadData(request, output.data() + old_size, available, &read)) { error = L"Ошибка загрузки файла"; return false; }
            output.resize(old_size + read);
            if (report_progress && total > 0)
            {
                int percent = static_cast<int>((output.size() * 100ULL) / total);
                PostMessageW(g_app.window, WM_APP_PROGRESS, static_cast<WPARAM>(std::min(percent, 100)), 0);
            }
        }
        return true;
    }

    bool SplitHttpsUrl(const std::wstring& url, std::wstring& host, std::wstring& path)
    {
        URL_COMPONENTSW parts{};
        parts.dwStructSize = sizeof(parts);
        parts.dwHostNameLength = static_cast<DWORD>(-1);
        parts.dwUrlPathLength = static_cast<DWORD>(-1);
        parts.dwExtraInfoLength = static_cast<DWORD>(-1);
        if (!WinHttpCrackUrl(url.c_str(), 0, 0, &parts) || parts.nScheme != INTERNET_SCHEME_HTTPS) return false;
        host.assign(parts.lpszHostName, parts.dwHostNameLength);
        path.assign(parts.lpszUrlPath, parts.dwUrlPathLength);
        if (parts.dwExtraInfoLength) path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
        return !host.empty() && !path.empty();
    }

    bool HttpGetUrl(const std::wstring& url, std::vector<unsigned char>& output, std::wstring& error, bool progress = false)
    {
        std::wstring host, path;
        if (!SplitHttpsUrl(url, host, path)) { error = L"Релиз содержит некорректную ссылку"; return false; }
        return HttpGet(host, path, output, error, progress);
    }

    std::string JsonStringAfter(const std::string& json, size_t start, const std::string& key)
    {
        size_t pos = json.find('"' + key + '"', start);
        if (pos == std::string::npos) return {};
        pos = json.find(':', pos);
        if (pos == std::string::npos) return {};
        pos = json.find('"', pos);
        if (pos == std::string::npos) return {};
        std::string value;
        for (++pos; pos < json.size(); ++pos)
        {
            char ch = json[pos];
            if (ch == '"') break;
            if (ch == '\\' && pos + 1 < json.size())
            {
                char escaped = json[++pos];
                if (escaped == 'n') value.push_back('\n');
                else if (escaped == 'r') value.push_back('\r');
                else if (escaped == 't') value.push_back('\t');
                else value.push_back(escaped);
            }
            else value.push_back(ch);
        }
        return value;
    }

    std::string AssetUrl(const std::string& json, const std::string& asset_name)
    {
        size_t pos = 0;
        while ((pos = json.find("\"name\"", pos)) != std::string::npos)
        {
            std::string name = JsonStringAfter(json, pos, "name");
            if (name == asset_name) return JsonStringAfter(json, pos, "browser_download_url");
            pos += 6;
        }
        return {};
    }

    bool FetchLatestRelease(release_info& info, std::wstring& error)
    {
        std::vector<unsigned char> bytes;
        if (!HttpGet(kApiHost, kReleasePath, bytes, error)) return false;
        std::string json(bytes.begin(), bytes.end());
        info.version = JsonStringAfter(json, 0, "tag_name");
        std::string program = AssetUrl(json, kProgramAsset);
        std::string checksum = AssetUrl(json, kChecksumAsset);
        if (info.version.empty()) { error = L"В последнем релизе отсутствует tag_name"; return false; }
        if (program.empty() || checksum.empty())
        {
            error = L"Релиз должен содержать mirick-client.exe и mirick-client.exe.sha256";
            return false;
        }
        info.program_url = Utf8ToWide(program);
        info.checksum_url = Utf8ToWide(checksum);
        return true;
    }

    std::string Sha256(const std::vector<unsigned char>& data)
    {
        BCRYPT_ALG_HANDLE algorithm{};
        BCRYPT_HASH_HANDLE hash{};
        DWORD object_size = 0, result_size = 0;
        std::string result;
        if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) return {};
        if (BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&object_size), sizeof(object_size), &result_size, 0) < 0)
        { BCryptCloseAlgorithmProvider(algorithm, 0); return {}; }
        std::vector<unsigned char> object(object_size);
        std::array<unsigned char, 32> digest{};
        if (BCryptCreateHash(algorithm, &hash, object.data(), object_size, nullptr, 0, 0) >= 0 &&
            BCryptHashData(hash, const_cast<PUCHAR>(data.data()), static_cast<ULONG>(data.size()), 0) >= 0 &&
            BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) >= 0)
        {
            std::ostringstream stream;
            stream << std::hex << std::setfill('0');
            for (unsigned char byte : digest) stream << std::setw(2) << static_cast<int>(byte);
            result = stream.str();
        }
        if (hash) BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return result;
    }

    std::string NormalizeHash(const std::vector<unsigned char>& data)
    {
        std::string text(data.begin(), data.end());
        std::string hash;
        for (char ch : text)
        {
            if (std::isxdigit(static_cast<unsigned char>(ch)) && hash.size() < 64)
                hash.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
            else if (!hash.empty()) break;
        }
        return hash.size() == 64 ? hash : std::string{};
    }

    void FinishJob(bool success, const std::wstring& message)
    {
        auto* payload = new std::pair<bool, std::wstring>(success, message);
        PostMessageW(g_app.window, WM_APP_RESULT, 0, reinterpret_cast<LPARAM>(payload));
    }

    void CheckWorker()
    {
        release_info info;
        std::wstring error;
        if (!FetchLatestRelease(info, error)) { FinishJob(false, error); return; }
        g_app.latest = info;
        std::wstring latest = Utf8ToWide(info.version);
        g_app.update_available = latest != g_app.installed_version;
        FinishJob(true, g_app.update_available ? L"Доступна версия " + latest : L"Установлена актуальная версия " + latest);
    }

    void InstallWorker()
    {
        release_info info = g_app.latest;
        std::wstring error;
        if (info.version.empty() && !FetchLatestRelease(info, error)) { FinishJob(false, error); return; }

        std::vector<unsigned char> program;
        std::vector<unsigned char> checksum;
        if (!HttpGetUrl(info.program_url, program, error, true)) { FinishJob(false, error); return; }
        if (!HttpGetUrl(info.checksum_url, checksum, error)) { FinishJob(false, error); return; }
        std::string expected = NormalizeHash(checksum);
        std::string actual = Sha256(program);
        if (expected.empty() || actual.empty() || expected != actual)
        {
            FinishJob(false, L"SHA-256 не совпадает. Установка отменена");
            return;
        }

        std::filesystem::path directory(GetInstallPath());
        std::error_code ec;
        std::filesystem::create_directories(directory, ec);
        if (ec) { FinishJob(false, L"Не удалось создать папку установки"); return; }
        std::filesystem::path temporary = directory / L"mirick-client.exe.download";
        std::filesystem::path destination = directory / L"mirick-client.exe";
        {
            std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
            if (!file) { FinishJob(false, L"Не удалось открыть временный файл"); return; }
            file.write(reinterpret_cast<const char*>(program.data()), static_cast<std::streamsize>(program.size()));
            if (!file) { FinishJob(false, L"Не удалось записать обновление"); return; }
        }
        if (!MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        {
            DeleteFileW(temporary.c_str());
            FinishJob(false, L"Не удалось заменить приложение. Закройте его и повторите попытку");
            return;
        }
        g_app.latest = info;
        g_app.update_available = false;
        SaveSettings(Utf8ToWide(info.version));
        PostMessageW(g_app.window, WM_APP_PROGRESS, 100, 0);
        FinishJob(true, L"Версия " + Utf8ToWide(info.version) + L" установлена и проверена");
    }

    void StartJob(job_kind kind)
    {
        if (g_app.busy.exchange(true)) return;
        g_app.progress = 0;
        g_app.status = kind == job_kind::check ? L"Проверяем GitHub Releases..." : L"Загружаем и проверяем обновление...";
        EnableWindow(g_app.check_button, FALSE);
        EnableWindow(g_app.install_button, FALSE);
        InvalidateRect(g_app.window, nullptr, FALSE);
        std::thread(kind == job_kind::check ? CheckWorker : InstallWorker).detach();
    }

    void BrowseInstallPath()
    {
        BROWSEINFOW info{};
        info.hwndOwner = g_app.window;
        info.lpszTitle = L"Выберите папку для установки приложения";
        info.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
        PIDLIST_ABSOLUTE id = SHBrowseForFolderW(&info);
        if (!id) return;
        wchar_t path[MAX_PATH]{};
        if (SHGetPathFromIDListW(id, path))
        {
            SetWindowTextW(g_app.path_edit, path);
            SaveSettings();
        }
        CoTaskMemFree(id);
    }

    void LaunchInstalled()
    {
        std::filesystem::path executable = std::filesystem::path(GetInstallPath()) / L"mirick-client.exe";
        if (!std::filesystem::exists(executable))
        {
            g_app.status = L"Приложение ещё не установлено";
            InvalidateRect(g_app.window, nullptr, FALSE);
            return;
        }
        HINSTANCE result = ShellExecuteW(g_app.window, L"open", executable.c_str(), nullptr, executable.parent_path().c_str(), SW_SHOWNORMAL);
        if (reinterpret_cast<INT_PTR>(result) <= 32) g_app.status = L"Windows не смог запустить приложение";
        else g_app.status = L"Приложение запущено";
        InvalidateRect(g_app.window, nullptr, FALSE);
    }

    COLORREF Color(int r, int g, int b) { return RGB(r, g, b); }

    void Fill(HDC dc, const RECT& rect, COLORREF color)
    {
        HBRUSH brush = CreateSolidBrush(color);
        FillRect(dc, &rect, brush);
        DeleteObject(brush);
    }

    void DrawTextAt(HDC dc, const std::wstring& text, RECT rect, COLORREF color, HFONT font, UINT format = DT_LEFT | DT_VCENTER | DT_SINGLELINE)
    {
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, color);
        SelectObject(dc, font);
        DrawTextW(dc, text.c_str(), -1, &rect, format);
    }

    void PaintWindow(HWND window)
    {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(window, &paint);
        RECT client{};
        GetClientRect(window, &client);
        Fill(dc, client, Color(7, 10, 17));
        Fill(dc, RECT{ 0, 0, client.right, 58 }, Color(15, 21, 33));
        Fill(dc, RECT{ 0, 58, 196, client.bottom }, Color(10, 15, 24));
        Fill(dc, RECT{ 196, 58, 198, client.bottom }, Color(38, 47, 65));
        Fill(dc, RECT{ 222, 86, client.right - 26, 181 }, Color(13, 18, 29));
        Fill(dc, RECT{ 222, 206, client.right - 26, 303 }, Color(13, 18, 29));
        Fill(dc, RECT{ 222, 328, client.right - 26, 423 }, Color(13, 18, 29));
        Fill(dc, RECT{ 222, 179, client.right - 26, 181 }, Color(113, 105, 255));

        DrawTextAt(dc, L"M", RECT{ 20, 10, 51, 48 }, Color(168, 168, 255), g_app.title_font, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        DrawTextAt(dc, L"MIRICK LAUNCHER", RECT{ 58, 8, 300, 50 }, Color(242, 245, 255), g_app.title_font);
        DrawTextAt(dc, L"БЕЗОПАСНЫЙ ЗАПУСК", RECT{ 18, 84, 178, 112 }, Color(112, 124, 148), g_app.font);
        DrawTextAt(dc, L"•  ОБНОВЛЕНИЯ", RECT{ 18, 128, 178, 158 }, Color(220, 224, 238), g_app.font);
        DrawTextAt(dc, L"•  УСТАНОВКА", RECT{ 18, 166, 178, 196 }, Color(150, 158, 178), g_app.font);
        DrawTextAt(dc, L"•  ЗАПУСК", RECT{ 18, 204, 178, 234 }, Color(150, 158, 178), g_app.font);
        DrawTextAt(dc, L"m1racl  •  GitHub Releases", RECT{ 18, client.bottom - 48, 190, client.bottom - 18 }, Color(92, 103, 124), g_app.font);

        DrawTextAt(dc, L"Канал обновлений", RECT{ 242, 92, 520, 119 }, Color(126, 138, 162), g_app.font);
        DrawTextAt(dc, L"GitHub Releases / stable", RECT{ 242, 119, 690, 157 }, Color(239, 242, 251), g_app.title_font);
        DrawTextAt(dc, L"Установленная версия: " + g_app.installed_version, RECT{ 242, 151, 690, 174 }, Color(126, 138, 162), g_app.font);
        DrawTextAt(dc, L"Папка установки", RECT{ 242, 211, 520, 238 }, Color(126, 138, 162), g_app.font);
        DrawTextAt(dc, L"Статус", RECT{ 242, 334, 520, 361 }, Color(126, 138, 162), g_app.font);
        DrawTextAt(dc, g_app.status, RECT{ 242, 360, 704, 389 }, Color(229, 233, 245), g_app.font);

        RECT progress_bg{ 242, 397, client.right - 46, 405 };
        Fill(dc, progress_bg, Color(28, 35, 50));
        int progress_width = MulDiv(progress_bg.right - progress_bg.left, g_app.progress.load(), 100);
        RECT progress_fill{ progress_bg.left, progress_bg.top, progress_bg.left + progress_width, progress_bg.bottom };
        Fill(dc, progress_fill, Color(132, 122, 255));
        EndPaint(window, &paint);
    }

    void LayoutControls(HWND window)
    {
        RECT client{};
        GetClientRect(window, &client);
        MoveWindow(g_app.close_button, client.right - 42, 12, 28, 28, TRUE);
        MoveWindow(g_app.minimize_button, client.right - 76, 12, 28, 28, TRUE);
        MoveWindow(g_app.path_edit, 242, 246, client.right - 370, 34, TRUE);
        MoveWindow(g_app.browse_button, client.right - 118, 246, 72, 34, TRUE);
        MoveWindow(g_app.check_button, 242, client.bottom - 44, 140, 32, TRUE);
        MoveWindow(g_app.install_button, 392, client.bottom - 44, 140, 32, TRUE);
        MoveWindow(g_app.launch_button, client.right - 178, client.bottom - 44, 132, 32, TRUE);
    }

    LRESULT DrawButton(const DRAWITEMSTRUCT* item)
    {
        bool disabled = (item->itemState & ODS_DISABLED) != 0;
        bool pressed = (item->itemState & ODS_SELECTED) != 0;
        bool primary = item->CtlID == IdInstall || item->CtlID == IdLaunch;
        COLORREF background = disabled ? Color(26, 31, 43) : pressed ? Color(96, 88, 205) : primary ? Color(112, 103, 235) : Color(24, 31, 46);
        Fill(item->hDC, item->rcItem, background);
        wchar_t text[128]{};
        GetWindowTextW(item->hwndItem, text, _countof(text));
        RECT rect = item->rcItem;
        DrawTextAt(item->hDC, text, rect, disabled ? Color(91, 99, 118) : Color(239, 242, 251), g_app.font, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return TRUE;
    }

    HWND CreateButton(HWND parent, int id, const wchar_t* text)
    {
        HWND button = CreateWindowExW(0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            0, 0, 10, 10, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandleW(nullptr), nullptr);
        SendMessageW(button, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.font), TRUE);
        return button;
    }

    LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
    {
        switch (message)
        {
        case WM_CREATE:
        {
            g_app.window = window;
            g_app.font = CreateFontW(-15, 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            g_app.title_font = CreateFontW(-18, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Semibold");
            g_app.edit_brush = CreateSolidBrush(Color(20, 27, 40));
            g_app.path_edit = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                0, 0, 10, 10, window, reinterpret_cast<HMENU>(IdInstallPath), GetModuleHandleW(nullptr), nullptr);
            SendMessageW(g_app.path_edit, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.font), TRUE);
            g_app.browse_button = CreateButton(window, IdBrowse, L"Обзор");
            g_app.check_button = CreateButton(window, IdCheck, L"Проверить");
            g_app.install_button = CreateButton(window, IdInstall, L"Установить");
            g_app.launch_button = CreateButton(window, IdLaunch, L"Запустить");
            g_app.close_button = CreateButton(window, IdClose, L"×");
            g_app.minimize_button = CreateButton(window, IdMinimize, L"—");
            LoadSettings();
            LayoutControls(window);
            return 0;
        }
        case WM_SIZE: LayoutControls(window); InvalidateRect(window, nullptr, FALSE); return 0;
        case WM_PAINT: PaintWindow(window); return 0;
        case WM_ERASEBKGND: return 1;
        case WM_NCHITTEST:
        {
            LRESULT hit = DefWindowProcW(window, message, wparam, lparam);
            if (hit == HTCLIENT)
            {
                POINT point{ GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
                ScreenToClient(window, &point);
                if (point.y < 58 && point.x < 650) return HTCAPTION;
            }
            return hit;
        }
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLOREDIT:
        {
            HDC dc = reinterpret_cast<HDC>(wparam);
            SetTextColor(dc, Color(232, 236, 247));
            SetBkColor(dc, Color(20, 27, 40));
            return reinterpret_cast<LRESULT>(g_app.edit_brush);
        }
        case WM_DRAWITEM: return DrawButton(reinterpret_cast<DRAWITEMSTRUCT*>(lparam));
        case WM_COMMAND:
            switch (LOWORD(wparam))
            {
            case IdBrowse: BrowseInstallPath(); return 0;
            case IdCheck: StartJob(job_kind::check); return 0;
            case IdInstall: SaveSettings(); StartJob(job_kind::install); return 0;
            case IdLaunch: LaunchInstalled(); return 0;
            case IdClose:
                if (g_app.busy)
                {
                    g_app.status = L"Дождитесь завершения текущей операции";
                    InvalidateRect(window, nullptr, FALSE);
                }
                else DestroyWindow(window);
                return 0;
            case IdMinimize: ShowWindow(window, SW_MINIMIZE); return 0;
            default: break;
            }
            break;
        case WM_APP_PROGRESS:
            g_app.progress = static_cast<int>(wparam);
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        case WM_APP_RESULT:
        {
            auto* result = reinterpret_cast<std::pair<bool, std::wstring>*>(lparam);
            g_app.status = result->second;
            delete result;
            g_app.busy = false;
            EnableWindow(g_app.check_button, TRUE);
            EnableWindow(g_app.install_button, TRUE);
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        case WM_CLOSE:
            if (g_app.busy)
            {
                g_app.status = L"Дождитесь завершения текущей операции";
                InvalidateRect(window, nullptr, FALSE);
                return 0;
            }
            DestroyWindow(window);
            return 0;
        case WM_DESTROY:
            SaveSettings();
            if (g_app.font) DeleteObject(g_app.font);
            if (g_app.title_font) DeleteObject(g_app.title_font);
            if (g_app.edit_brush) DeleteObject(g_app.edit_brush);
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProcW(window, message, wparam, lparam);
    }
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show)
{
    SetProcessDPIAware();
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    WNDCLASSEXW cls{};
    cls.cbSize = sizeof(cls);
    cls.style = CS_HREDRAW | CS_VREDRAW;
    cls.lpfnWndProc = WindowProc;
    cls.hInstance = instance;
    cls.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    cls.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    cls.hbrBackground = nullptr;
    cls.lpszClassName = kWindowClass;
    RegisterClassExW(&cls);

    RECT area{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &area, 0);
    constexpr int width = 780;
    constexpr int height = 510;
    HWND window = CreateWindowExW(WS_EX_APPWINDOW, kWindowClass, kProductName, WS_POPUP | WS_MINIMIZEBOX,
        area.left + (area.right - area.left - width) / 2,
        area.top + (area.bottom - area.top - height) / 2,
        width, height, nullptr, nullptr, instance, nullptr);
    if (!window) return 1;

    ShowWindow(window, show);
    UpdateWindow(window);
    StartJob(job_kind::check);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    CoUninitialize();
    return static_cast<int>(message.wParam);
}
