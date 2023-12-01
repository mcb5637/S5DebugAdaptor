#pragma once

#include <dap/io.h>
#include <dap/network.h>
#include <dap/protocol.h>
#include <dap/session.h>

#include "adaptor.h"

namespace debug_lua {
	class Server
	{
		std::unique_ptr<dap::net::Server> Srv = dap::net::Server::create();
		static constexpr int Port = 19021;
		Debugger& Dbg;

	public:
		Server(Debugger& d);
	};
}
