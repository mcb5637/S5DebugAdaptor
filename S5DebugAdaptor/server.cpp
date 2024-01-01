#include "pch.h"
#include "server.h"

debug_lua::Server::Server(Debugger& d) : Dbg(d)
{
   /*std::mutex m{};
    std::condition_variable cv{};
    bool connected = false;*/

    auto onClientConnected =
        [&](const std::shared_ptr<dap::ReaderWriter>& socket) {
        Adaptor a{ Dbg, socket};
        /*connected = true;
        {
            cv.notify_one();
        }*/
        
        a.WaitUntilDisconnected();
        };

    // Error handler
    auto onError = [&](const char* msg) { printf("Server error: %s\n", msg); };

    Srv->start(Port, onClientConnected, onError);

    /*if (!connected) {
        std::unique_lock lk(m);
        cv.wait(lk, [&]() { return connected; });
    }*/
}
