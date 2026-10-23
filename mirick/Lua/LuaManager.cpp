#include "Lua/LuaManager.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

#include "Core/Config.hpp"

namespace
{
    namespace fs = std::filesystem;

    struct Script
    {
        std::string name;
        sol::environment environment;
        sol::protected_function onTick;
        sol::protected_function onUnload;
        bool tickFailed = false;
    };

    fs::path ScriptDirectory()
    {
        return fs::path(config_t::ConfigDir()) / "scripts";
    }

    fs::path LogPath()
    {
        return fs::path(config_t::ConfigDir()) / "lua.log";
    }

    void WriteLog(const std::string& text)
    {
        std::ofstream log(LogPath(), std::ios::app);
        if (log)
        {
            log << text << '\n';
        }
    }

    bool ReportResult(const std::string& scriptName, const sol::protected_function_result& result)
    {
        if (result.valid())
        {
            return true;
        }

        const sol::error error = result;
        WriteLog("[" + scriptName + "] " + error.what());
        return false;
    }
}

struct LuaManager::Impl
{
    sol::state lua;
    std::vector<Script> scripts;
    std::chrono::steady_clock::time_point lastUpdate = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point notificationUntil{};
    std::string lastMessage;
    bool lastExecutionSucceeded = true;
    bool initialized = false;

    void UnloadScripts()
    {
        for (auto& script : scripts)
        {
            if (script.onUnload.valid())
            {
                ReportResult(script.name, script.onUnload());
            }
        }

        scripts.clear();
    }

    void LoadScripts()
    {
        std::error_code error;
        fs::create_directories(ScriptDirectory(), error);
        if (error)
        {
            WriteLog("[mirick] Failed to create scripts directory: " + error.message());
            return;
        }

        std::vector<fs::path> paths;
        for (const auto& entry : fs::directory_iterator(ScriptDirectory(), error))
        {
            if (entry.is_regular_file() && entry.path().extension() == ".lua")
            {
                paths.push_back(entry.path());
            }
        }

        if (error)
        {
            WriteLog("[mirick] Failed to scan scripts directory: " + error.message());
            return;
        }

        std::sort(paths.begin(), paths.end());

        for (const auto& path : paths)
        {
            const std::string name = path.filename().string();
            sol::environment environment(lua, sol::create, lua.globals());
            const sol::protected_function_result result = lua.safe_script_file(path.string(), environment, sol::script_pass_on_error);

            if (!result.valid())
            {
                ReportResult(name, result);
                continue;
            }

            Script script{ name, std::move(environment), {}, {} };
            script.onTick = script.environment["on_tick"];
            script.onUnload = script.environment["on_unload"];

            const sol::protected_function onLoad = script.environment["on_load"];
            if (onLoad.valid())
            {
                ReportResult(name, onLoad());
            }

            scripts.emplace_back(std::move(script));
            WriteLog("[mirick] Loaded " + name);
        }
    }
};

LuaManager& LuaManager::Instance()
{
    static LuaManager instance;
    return instance;
}

LuaManager::LuaManager() : impl_(std::make_unique<Impl>())
{
}

LuaManager::~LuaManager() = default;

void LuaManager::Initialize()
{
    if (impl_->initialized)
    {
        return;
    }

    impl_->lua.open_libraries(
        sol::lib::base,
        sol::lib::coroutine,
        sol::lib::math,
        sol::lib::string,
        sol::lib::table,
        sol::lib::utf8,
        sol::lib::package,
        sol::lib::io,
        sol::lib::os,
        sol::lib::debug);

    sol::table mirick = impl_->lua.create_named_table("mirick");
    mirick["version"] = "1.0";
    mirick.set_function("log", [](const std::string& message)
    {
        WriteLog("[script] " + message);
    });

    impl_->lua.set_function("print", [](sol::variadic_args arguments)
    {
        std::string line = "[script]";
        for (const auto argument : arguments)
        {
            const sol::object value = argument;
            switch (value.get_type())
            {
            case sol::type::string:
                line += " " + value.as<std::string>();
                break;
            case sol::type::number:
                line += " " + std::to_string(value.as<double>());
                break;
            case sol::type::boolean:
                line += value.as<bool>() ? " true" : " false";
                break;
            case sol::type::nil:
                line += " nil";
                break;
            default:
                line += " <value>";
                break;
            }
        }
        WriteLog(line);
    });

    impl_->initialized = true;
    impl_->lastUpdate = std::chrono::steady_clock::now();
    impl_->LoadScripts();
}

void LuaManager::Update()
{
    if (!impl_->initialized)
    {
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    const float deltaTime = std::chrono::duration<float>(now - impl_->lastUpdate).count();
    impl_->lastUpdate = now;

    for (auto& script : impl_->scripts)
    {
        if (script.onTick.valid() && !script.tickFailed)
        {
            // Disable a failing callback until reload instead of writing the same
            // error to disk on every rendered frame.
            script.tickFailed = !ReportResult(script.name, script.onTick(deltaTime));
        }
    }
}

void LuaManager::Reload()
{
    if (!impl_->initialized)
    {
        Initialize();
        return;
    }

    impl_->UnloadScripts();
    impl_->LoadScripts();
}

void LuaManager::Shutdown()
{
    if (!impl_->initialized)
    {
        return;
    }

    impl_->UnloadScripts();
    impl_->initialized = false;
}

bool LuaManager::Execute(std::string_view code)
{
    if (code.empty())
    {
        impl_->lastMessage = "Nothing to execute";
        impl_->lastExecutionSucceeded = false;
        impl_->notificationUntil = std::chrono::steady_clock::now() + std::chrono::seconds(4);
        return false;
    }

    if (!impl_->initialized)
    {
        Initialize();
    }

    std::string source(code);
    // Pasted files often contain an UTF-8 BOM or a Unix shebang. Lua's file
    // loader handles those, while execution from a memory buffer does not.
    if (source.size() >= 3
        && static_cast<unsigned char>(source[0]) == 0xEF
        && static_cast<unsigned char>(source[1]) == 0xBB
        && static_cast<unsigned char>(source[2]) == 0xBF)
    {
        source.erase(0, 3);
    }
    if (source.starts_with("#!"))
    {
        const size_t lineEnd = source.find('\n');
        source.erase(0, lineEnd == std::string::npos ? source.size() : lineEnd + 1);
    }

    const sol::protected_function_result result = impl_->lua.safe_script(
        source, sol::script_pass_on_error);

    if (!result.valid())
    {
        const sol::error error = result;
        impl_->lastMessage = error.what();
        impl_->lastExecutionSucceeded = false;
        impl_->notificationUntil = std::chrono::steady_clock::now() + std::chrono::seconds(4);
        WriteLog("[console] " + impl_->lastMessage);
        return false;
    }

    impl_->lastMessage = "Script executed successfully";
    impl_->lastExecutionSucceeded = true;
    impl_->notificationUntil = std::chrono::steady_clock::now() + std::chrono::seconds(4);
    WriteLog("[console] Script executed successfully");
    return true;
}

void LuaManager::DrawNotification()
{
    const auto now = std::chrono::steady_clock::now();
    if (now >= impl_->notificationUntil)
    {
        return;
    }

    const float remaining = std::chrono::duration<float>(impl_->notificationUntil - now).count();
    const float alpha = std::clamp(remaining * 2.0f, 0.0f, 1.0f);
    const bool success = impl_->lastExecutionSucceeded;
    const char* title = success
        ? (g_cfg.language == 1 ? "Lua script executed" : "Lua-скрипт выполнен")
        : (g_cfg.language == 1 ? "Lua execution error" : "Ошибка выполнения Lua");
    const char* detail = success
        ? (g_cfg.language == 1 ? "Execution completed successfully" : "Код успешно выполнен")
        : (g_cfg.language == 1 ? "Open the Lua module or lua.log for details" : "Подробности в модуле Lua и lua.log");

    ImDrawList* draw = ImGui::GetForegroundDrawList();
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const ImVec2 size(278.0f, 58.0f);
    const ImVec2 max(display.x - 18.0f, 78.0f);
    const ImVec2 min(max.x - size.x, max.y - size.y);
    const ImU32 accent = success
        ? IM_COL32(70, 220, 145, static_cast<int>(255.0f * alpha))
        : IM_COL32(255, 88, 96, static_cast<int>(255.0f * alpha));

    draw->AddRectFilled(ImVec2(min.x + 2.0f, min.y + 3.0f), ImVec2(max.x + 2.0f, max.y + 3.0f), IM_COL32(0, 0, 0, static_cast<int>(90.0f * alpha)), 7.0f);
    draw->AddRectFilled(min, max, IM_COL32(10, 14, 22, static_cast<int>(238.0f * alpha)), 7.0f);
    draw->AddRectFilled(min, ImVec2(min.x + 4.0f, max.y), accent, 7.0f, ImDrawCornerFlags_Left);
    draw->AddCircleFilled(ImVec2(min.x + 20.0f, min.y + 20.0f), 5.0f, accent);
    draw->AddText(ImVec2(min.x + 34.0f, min.y + 11.0f), IM_COL32(245, 247, 252, static_cast<int>(255.0f * alpha)), title);
    draw->AddText(ImVec2(min.x + 14.0f, min.y + 35.0f), IM_COL32(145, 154, 174, static_cast<int>(245.0f * alpha)), detail);
}

bool LuaManager::IsInitialized() const
{
    return impl_->initialized;
}

const std::string& LuaManager::GetLastMessage() const
{
    return impl_->lastMessage;
}
