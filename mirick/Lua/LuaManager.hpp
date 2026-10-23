#pragma once

#include <memory>
#include <string>
#include <string_view>

class LuaManager
{
public:
    static LuaManager& Instance();

    void Initialize();
    void Update();
    void Reload();
    void Shutdown();

    bool Execute(std::string_view code);
    void DrawNotification();
    bool IsInitialized() const;
    const std::string& GetLastMessage() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    LuaManager();
    ~LuaManager();
    LuaManager(const LuaManager&) = delete;
    LuaManager& operator=(const LuaManager&) = delete;
};
