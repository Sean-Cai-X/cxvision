#pragma once

#include "TorchRuntimeTypes.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

class TorchRuntimeBridge
{
public:
    bool Load(const std::string& dll_path);
    void Unload();
    const std::string& LastErrorMessage() const;

    bool Create(const TorchRuntimeGuiConfig& config);
    TorchRuntimeGuiResult RunTask(const TorchRuntimeGuiRequest& request);
    void Destroy();

    bool IsLoaded() const;

private:
#ifdef _WIN32
    HMODULE dll_ = nullptr;
#else
    void* dll_ = nullptr;
#endif
    void* handle_ = nullptr;

    using CreateFn = int (*)(const void*, void**);
    using DestroyFn = int (*)(void*);
    using RunTaskFn = int (*)(void*, const void*, void*);
    using FreeResultFn = void (*)(void*);
    using VersionFn = const char* (*)();

    CreateFn create_ = nullptr;
    DestroyFn destroy_ = nullptr;
    RunTaskFn run_task_ = nullptr;
    FreeResultFn free_result_ = nullptr;
    VersionFn version_ = nullptr;
    std::string last_error_message_;
};
