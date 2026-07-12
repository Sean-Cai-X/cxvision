#include "pch.h"
#include "CxCrashLogHandler.h"
#include "CxUnifiedLog.h"
#include <csignal>
#include <windows.h>
#include <sstream>
#include <iostream>

static LONG WINAPI CxUnhandledExceptionFilter(EXCEPTION_POINTERS* exceptionInfo)
{
    std::string run_id = CxUnifiedLog::Instance().IsInitialized() 
        ? CxUnifiedLog::Instance().RunId() 
        : "unknown";

    std::ostringstream oss;
    oss << "exception_code=" << std::hex << exceptionInfo->ExceptionRecord->ExceptionCode
        << ", exception_address=" << exceptionInfo->ExceptionRecord->ExceptionAddress;

    WriteCrashLog("unhandled_exception", oss.str(), run_id, GetCurrentThreadId());

    return EXCEPTION_CONTINUE_SEARCH;
}

static void CxTerminateHandler()
{
    std::string run_id = CxUnifiedLog::Instance().IsInitialized() 
        ? CxUnifiedLog::Instance().RunId() 
        : "unknown";

    WriteCrashLog("terminate", "std::terminate called", run_id, GetCurrentThreadId());
}

void InstallCxCrashLogHandlers()
{
    SetUnhandledExceptionFilter(CxUnhandledExceptionFilter);
    std::set_terminate(CxTerminateHandler);
}

void WriteCrashLog(
    const std::string& event,
    const std::string& message,
    const std::string& run_id,
    unsigned long thread_id)
{
    if (CxUnifiedLog::Instance().IsInitialized())
    {
        CxUnifiedLog::Instance().Write(
            CxLogLevel::Fatal,
            "CxCrashLogHandler",
            event.c_str(),
            "failed",
            message,
            __FILE__,
            __LINE__,
            __FUNCTION__);
        
        CxUnifiedLog::Instance().Flush();
    }

    std::cerr << "[CRASH] event=" << event 
              << ", message=" << message 
              << ", run_id=" << run_id 
              << ", tid=" << thread_id << std::endl;
}