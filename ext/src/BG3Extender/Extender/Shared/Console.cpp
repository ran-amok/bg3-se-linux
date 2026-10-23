#include <stdafx.h>
#include <Extender/Version.h>
#include <Extender/Shared/Console.h>
#include <Extender/ScriptExtender.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>
#include <cerrno>

BEGIN_SE()

char const* BuildDate = __DATE__ " " __TIME__;

void DebugConsole::SubmitTaskAndWait(bool server, std::function<void()> task)
{
    if (server) {
        auto state = GetStaticSymbols().GetServerState();
        if (!state) {
            ERR("Cannot queue server commands when the server state machine is not initialized");
        } else if (*state == esv::GameState::Paused || *state == esv::GameState::Running) {
            gExtender->GetServer().SubmitTaskAndWait(task);
        } else {
            ERR("Cannot queue server commands in game state %s", EnumInfo<esv::GameState>::Find(*state).GetString());
        }
    } else {
        auto state = GetStaticSymbols().GetClientState();
        if (!state) {
            ERR("Cannot queue client commands when the client state machine is not initialized");
        } else if (*state == ecl::GameState::Menu 
            || *state == ecl::GameState::Lobby 
            || *state == ecl::GameState::Paused 
            || *state == ecl::GameState::Running) {
            gExtender->GetClient().SubmitTaskAndWait(task);
        } else {
            ERR("Cannot queue client commands in game state %s", EnumInfo<ecl::GameState>::Find(*state).GetString());
        }
    }
}

void DebugConsole::PrintHelp()
{
    DEBUG("Anything typed in will be executed as Lua code except the following special commands:");
    DEBUG("  server - Switch to server context");
    DEBUG("  client - Switch to client context");
    DEBUG("  reset client - Reset client Lua state");
    DEBUG("  reset server - Reset server Lua state");
    DEBUG("  reset - Reset client and server Lua states");
    DEBUG("  silence <on|off> - Enable/disable silent mode (log output when in input mode)");
    DEBUG("  clear - Clear the console");
    DEBUG("  exit - Leave console mode");
    DEBUG("  !<cmd> <arg1> ... <argN> - Trigger Lua \"ConsoleCommand\" event with arguments cmd, arg1, ..., argN");
}

void DebugConsole::ClearFromReset()
{
    // Clear console if the setting is enabled
    if (gExtender->GetConfig().ClearOnReset)
    {
        gCoreLibPlatformInterface.GlobalConsole->Clear();
    }
}

void DebugConsole::ResetLua()
{
    ClearFromReset();

    DEBUG("Resetting Lua states.");
    SubmitTaskAndWait(true, []() {
        gExtender->GetServer().ResetLuaState();
    });

    SubmitTaskAndWait(false, []() {
        if (!gExtender->GetServer().RequestResetClientLuaState()) {
            gExtender->GetClient().ResetLuaState();
        }
    });
}

void DebugConsole::ResetLuaClient()
{
    ClearFromReset();

    DEBUG("Resetting client Lua state.");
    SubmitTaskAndWait(false, []() {
        if (!gExtender->GetServer().RequestResetClientLuaState()) {
            gExtender->GetClient().ResetLuaState();
        }
    });
}

void DebugConsole::ResetLuaServer()
{
    ClearFromReset();

    DEBUG("Resetting server Lua state.");
    SubmitTaskAndWait(true, []() {
        gExtender->GetServer().ResetLuaState();
    });
}

void DebugConsole::ExecLuaCommand(std::string const& cmd)
{
    auto task = [cmd]() {
        auto state = gExtender->GetCurrentExtensionState();

        if (!state) {
            ERR("Extensions not initialized!");
            return;
        }

        LuaVirtualPin pin(*state);
        if (!pin) {
            ERR("Lua state not initialized!");
            return;
        }

        if (cmd[0] == '!') {
            lua::DoConsoleCommandEvent params;
            params.Command = cmd.substr(1);
            pin->ThrowEvent("DoConsoleCommand", params, false);
        } else {
            auto L = pin->GetState();
            lua::StaticLifetimeStackPin _(L, lua::LifetimeHandle{});
            lua::ProfilerStackGuard _p(&*pin);
            if (luaL_loadstring(L, cmd.c_str()) || lua::CallWithTraceback(L, 0, 0)) { // stack: errmsg
                ERR("%s", lua_tostring(L, -1));
                lua_pop(L, 1);
            }
        }
    };

    SubmitTaskAndWait(serverContext_, task);
}

void DebugConsole::HandleCommand(std::string const& cmd)
{
    if (cmd.empty()) {
    } else if (cmd == "server") {
        DEBUG("Switching to server context.");
        serverContext_ = true;
    } else if (cmd == "client") {
        DEBUG("Switching to client context.");
        serverContext_ = false;
#if defined(_DEBUG)
    } else if (cmd == "debugbreak") {
        gCoreLibPlatformInterface.EnableDebugBreak = !gCoreLibPlatformInterface.EnableDebugBreak;
        if (gCoreLibPlatformInterface.EnableDebugBreak) {
            DEBUG("Debug breaks ON");
        } else {
            DEBUG("Debug breaks OFF");
        }
#endif
    } else if (cmd == "reset") {
        ResetLua();
    } else if (cmd == "reset server") {
        ResetLuaServer();
    } else if (cmd == "reset client") {
        ResetLuaClient();
    } else if (cmd == "silence on") {
        DEBUG("Silent mode ON");
        silence_ = true;
    } else if (cmd == "silence off") {
        DEBUG("Silent mode OFF");
        silence_ = false;
    } else if (cmd == "clear") {
        Clear();
    } else if (cmd == "help") {
        PrintHelp();
    } else {
        ExecLuaCommand(cmd);
    }
}

void DebugConsole::Print(DebugMessageType type, char const* msg)
{
    Console::Print(type, msg);
    Broadcast(msg);

    if (!silence_ && gExtender) {
        auto debugger = gExtender->GetLuaDebugger();
        if (debugger && debugger->IsDebuggerReady()) {
            debugger->OnLogMessage(type, msg);
        }
    }
}

// PORT (linux): the console is a Unix socket, <state dir>/console.sock (bg3se-console in
// tools/). Each client connection is one console session: lines received are commands,
// exactly as upstream's InputLoop() reads them (`--[[` / `]]--` for multi-line), and
// every Print() is broadcast to the connected clients. Output is enabled only while a
// client is connected, so the log file (CoreLib/Console.cpp) is the record.
void DebugConsole::ConsoleThread()
{
    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof addr.sun_path, "%s", socketPath_.c_str());
    unlink(socketPath_.c_str());
    listenFd_ = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (listenFd_ < 0 || bind(listenFd_, (struct sockaddr*)&addr, sizeof addr) < 0 || listen(listenFd_, 1) < 0) {
        ERR("Console socket %s: %s", socketPath_.c_str(), strerror(errno));
        return;
    }
    chmod(socketPath_.c_str(), 0600);
    DEBUG("Console listening on %s", socketPath_.c_str());

    while (consoleRunning_) {
        int fd = accept4(listenFd_, nullptr, nullptr, SOCK_CLOEXEC);
        if (fd < 0) {
            if (errno == EINTR) continue;
            break;
        }

        {
            std::lock_guard<std::mutex> _(clientsMutex_);
            clients_.push_back(Client{ fd });
        }
        DEBUG("Console client connected; entering %s Lua console.", serverContext_ ? "server" : "client");
        InputLoop(fd);
        DEBUG("Console client left.");
        {
            std::lock_guard<std::mutex> _(clientsMutex_);
            clients_.erase(std::remove_if(clients_.begin(), clients_.end(),
                [fd](Client const& c) { return c.fd == fd; }), clients_.end());
        }
        close(fd);
    }
}

void DebugConsole::InputLoop(int fd)
{
    std::string buf, line;
    char chunk[4096];
    while (consoleRunning_) {
        auto prompt = std::string(serverContext_ ? "S" : "C") + (multiLineMode_ ? " -->> " : " >> ");
        // PORT (linux): a blocking write is fine here and only here -- this is our own
        // console thread, and a client that is not reading has nothing to send us
        // either. Broadcast() is the one that may not block (see its comment).
        (void)!write(fd, prompt.data(), prompt.size());

        auto nl = buf.find('\n');
        while (nl == std::string::npos) {
            ssize_t n = read(fd, chunk, sizeof chunk);
            if (n <= 0) return;
            buf.append(chunk, (size_t)n);
            nl = buf.find('\n');
        }
        line = buf.substr(0, nl);
        buf.erase(0, nl + 1);
        if (!line.empty() && line.back() == '\r') line.pop_back();

        if (!multiLineMode_) {
            if (line == "exit") {
                return;
            }

            if (line == "--[[") {
                multiLineMode_ = true;
                multiLineCommand_.clear();
                continue;
            }

            HandleCommand(line);
        } else {
            if (line == "]]--") {
                multiLineMode_ = false;
                HandleCommand(multiLineCommand_);
            } else {
                multiLineCommand_ += line;
                multiLineCommand_ += '\n';
            }
        }
    }
}

// PORT (linux): this runs on whichever game thread called Print(), so it must never
// block. A console client that stops reading fills the socket's buffer (measured on an
// AF_UNIX stream pair: 278 lines of 96 bytes, 26688 bytes, go in and the next send()
// returns EAGAIN), and a blocking write() here would then hang the printing game thread
// until the client closed the socket -- a client that sends commands without reading the
// replies is enough to do it.
//
// So: MSG_DONTWAIT and drop the line on EAGAIN, counting the drops and reporting them
// to the client when it reads again and to the log once per episode. MSG_NOSIGNAL
// because SIGPIPE's disposition belongs to the game (it ignores it, so a closed peer
// returns EPIPE either way; InputLoop()'s read() is what notices and cleans up).
// A single large line can come back partially sent (measured: 36544 of 65536 bytes),
// which tears the client's view of the stream, so the next complete line is prefixed
// with a newline to put it back on a line boundary.
void DebugConsole::Broadcast(char const* msg)
{
    std::lock_guard<std::mutex> _(clientsMutex_);
    if (clients_.empty()) return;

    for (auto& client : clients_) {
        std::string line;
        if (client.torn) line += '\n';
        if (client.dropped > 0) {
            char note[80];
            snprintf(note, sizeof note, "[%llu console line(s) dropped: client not reading]\n",
                (unsigned long long)client.dropped);
            line += note;
        }
        line += msg;
        line += '\n';

        auto sent = send(client.fd, line.data(), line.size(), MSG_DONTWAIT | MSG_NOSIGNAL);
        if (sent == (ssize_t)line.size()) {
            client.dropped = 0;
            client.torn = false;
            client.reported = false;
            continue;
        }

        if (sent > 0) {
            // partial: the client has half a line, so resynchronise on the next one
            client.torn = true;
        } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            // the client is gone (EPIPE) or the fd is bad; leave it to InputLoop()
            continue;
        }

        client.dropped++;
        if (!client.reported) {
            client.reported = true;
            Console::LocalPrint(DebugMessageType::Warning,
                "Console client is not reading its socket; dropping console output.");
        }
    }
}

void DebugConsole::SetSocketPath(std::string const& path)
{
    socketPath_ = path;
}

void DebugConsole::Create()
{
    Console::Create();
    EnableOutput(getenv("BG3SE_STDERR") != nullptr);

    consoleRunning_ = true;
    serverContext_ = !gExtender->GetConfig().DefaultToClientConsole;

    DEBUG("******************************************************************************");
    DEBUG("*                                                                            *");
    DEBUG("*                     BG3 Script Extender Debug Console                      *");
    DEBUG("*                                                                            *");
    DEBUG("******************************************************************************");
    DEBUG("");
    DEBUG("BG3Ext v%d built on %s (Linux port)", CurrentVersion, BuildDate);

    if (!socketPath_.empty()) {
        consoleThread_ = new std::thread(&DebugConsole::ConsoleThread, this);
    }
}


END_SE()
