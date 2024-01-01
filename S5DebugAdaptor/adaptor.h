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
		bool TerminateDebugger = false;
		std::condition_variable ConditionTerminate;
		std::mutex MutexTerminate;

	public:
		Adaptor(Debugger& d, const std::shared_ptr<dap::ReaderWriter>& socket);

		int64_t EncodeStackFrame(const DebugState& s, int lvl);
		std::pair<DebugState&, int> DecodeStackFrame(int64_t f);
		void WaitUntilDisconnected();

		virtual void OnStateOpened(DebugState& s) override;
		virtual void OnStateClosing(DebugState& s, bool lastState) override;
		virtual void OnPaused(DebugState& s, Reason r, std::string_view exceptionText) override;
		virtual void OnLog(std::string_view s) override;
	};
}