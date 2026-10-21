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
        sol::lib::utf8);

    // Keep scripts deterministic: file and process access are not part of the API.
    impl_->lua["dofile"] = sol::nil;
    impl_->lua["loadfile"] = sol::nil;

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

bool LuaManager::IsInitialized() const
{
    return impl_->initialized;
}
