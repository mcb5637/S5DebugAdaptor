#pragma once

#include <condition_variable>
#include <mutex>

#include <dap/io.h>
#include <dap/session.h>
#include <dap/protocol.h>

#include "debugger.h"

namespace debug_lua {
	class Adaptor : IDebugEventHandler {
		std::unique_ptr<dap::Session> Session = dap::Session::create();
		Debugger& Dbg;
		bool terminate = false;
		std::condition_variable cv;
		std::mutex mutex;
		std::shared_ptr<dap::Reader> in;
		std::shared_ptr<dap::Writer> out;

	public:
		Adaptor(Debugger& d, const std::shared_ptr<dap::ReaderWriter>& socket);

		int64_t EncodeStackFrame(const DebugState& s, int lvl);
		std::pair<DebugState&, int> DecodeStackFrame(int64_t f);
		void WaitUntilDisconnected();

		virtual void OnStateOpened(DebugState& s) override;
	};
}