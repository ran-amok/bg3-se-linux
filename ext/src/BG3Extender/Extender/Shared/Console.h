#pragma once

#include <GameDefinitions/Base/Base.h>
#include <functional>
#include <mutex>
#include <vector>
#include <thread>

BEGIN_SE()

extern char const* BuildDate;

class DebugConsole : public Console
{
public:
    void Create() override;
    void HandleCommand(std::string const& cmd);
    void Print(DebugMessageType type, char const* msg) override;
    void SetSocketPath(std::string const& path);

private:
    bool consoleRunning_{ false };
    bool serverContext_{ true };
    bool multiLineMode_{ false };
    std::thread* consoleThread_{ nullptr };
    std::string multiLineCommand_;
    uint32_t width_{ 0 };
    uint32_t height_{ 0 };
    uint32_t resized_{ false };

    // PORT (linux): a Unix-socket console (Console.cpp)
    std::string socketPath_;
    int listenFd_{ -1 };
    std::mutex clientsMutex_;

    // PORT (linux): Broadcast() runs on whichever game thread printed the line, so a
    // client that stops reading its socket must never hold that thread up. Each client
    // carries the number of lines dropped since its last complete one, whether that
    // last line was torn by a partial send, and whether the episode has been logged.
    struct Client
    {
        int fd;
        uint64_t dropped{ 0 };
        bool torn{ false };
        bool reported{ false };
    };

    std::vector<Client> clients_;

    void ConsoleThread();
    void InputLoop(int fd);
    void Broadcast(char const* msg);
    void SubmitTaskAndWait(bool server, std::function<void()> fun);
    void PrintHelp();
    void ResetLua();
    void ResetLuaClient();
    void ResetLuaServer();
    void ExecLuaCommand(std::string const& cmd);
    void ClearFromReset();
};

END_SE()
