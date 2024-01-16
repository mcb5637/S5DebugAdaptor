#include "pch.h"
#include "server.h"
#include "shok.h"

debug_lua::Server::Server(Debugger& d) : Dbg(d)
{
    auto onClientConnected =
        [&](const std::shared_ptr<dap::ReaderWriter>& socket) {
        Adaptor a{ Dbg, socket};
        a.WaitUntilDisconnected();
        };

    // Error handler
    auto onError = [&](const char* msg) { printf("Server error: %s\n", msg); };

    Srv->start(Port, onClientConnected, onError);
}
