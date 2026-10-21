#pragma once

#include <memory>

class LuaManager
{
public:
    static LuaManager& Instance();

    void Initialize();
    void Update();
    void Reload();
    void Shutdown();

    bool IsInitialized() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    LuaManager();
    ~LuaManager();
    LuaManager(const LuaManager&) = delete;
    LuaManager& operator=(const LuaManager&) = delete;
};
