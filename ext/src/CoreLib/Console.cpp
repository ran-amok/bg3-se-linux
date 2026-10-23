// Console.cpp — PORT (linux): the base console writes lines to the extender's log file
// and, when output is enabled, to stderr with ANSI colours. The Win32 console
// (AllocConsole, code pages, fonts) has no counterpart; the interactive side is the
// socket console in Extender/Shared/Console.cpp.
#include "stdafx.h"
#include <CoreLib/Console.h>
#include <iostream>
#include <mutex>

BEGIN_SE()

static std::mutex gConsoleMutex;

Console::~Console()
{
    Destroy();
}

void Console::Create()
{
    created_ = true;
    enabled_ = true;
}

void Console::Destroy()
{
    CloseLogFile();
    created_ = false;
}

void Console::OpenLogFile(std::wstring const& path)
{
    if (logToFile_) {
        CloseLogFile();
    }

    logFile_.rdbuf()->pubsetbuf(0, 0);
    logFile_.open(ToStdUTF8(path).c_str(), std::ios::app | std::ios::out);
    logToFile_ = logFile_.is_open();
}

void Console::CloseLogFile()
{
    if (logToFile_) {
        logFile_.close();
        logToFile_ = false;
    }
}

void Console::SetColor(DebugMessageType type)
{
    char const* seq;
    switch (type) {
    case DebugMessageType::Error: seq = "\x1b[1;31m"; break;
    case DebugMessageType::Warning: seq = "\x1b[33m"; break;
    case DebugMessageType::Osiris: seq = "\x1b[1;36m"; break;
    case DebugMessageType::Info: seq = "\x1b[1;37m"; break;
    case DebugMessageType::Debug:
    default: seq = "\x1b[0m"; break;
    }
    std::cerr << seq;
}

void Console::LocalPrint(DebugMessageType type, char const* msg)
{
    std::lock_guard<std::mutex> _(gConsoleMutex);
    if (enabled_ && (!inputEnabled_ || !silence_)) {
        SetColor(type);
        std::cerr << msg << "\x1b[0m" << std::endl;
    }

    if (logToFile_) {
        logFile_.write(msg, strlen(msg));
        logFile_.write("\n", 1);
        logFile_.flush();
    }
}

void Console::Print(DebugMessageType type, char const* msg)
{
    LocalPrint(type, msg);

    if (logCallback_) {
        logCallback_(type, msg);
    }
}

void Console::Clear()
{
    if (enabled_) std::cerr << "\x1b[2J" "\x1b[H" "\x1b[3J";
}

void Console::EnableOutput(bool enabled)
{
    enabled_ = enabled;
}

void Console::SetLogCallback(LogCallbackProc* callback)
{
    logCallback_ = callback;
}

END_SE()
