#pragma once

#include <condition_variable>
#include <mutex>
#include <optional>
#include <tuple>

#include <dap/io.h>
#include <dap/session.h>
#include <dap/protocol.h>

#include "debugger.h"

namespace debug_lua {
	class Adaptor : IDebugEventHandler {
		std::unique_ptr<dap::Session> Session = dap::Session::create();
		Debugger& Dbg;
		bool TerminateDebugger = false;
		bool IsAttached = false, UnderstandsType = false;
		std::condition_variable ConditionTerminate;
		std::mutex MutexTerminate;

		enum class Scope : int {
			None, Local, Upvalue,
		};

	public:
		Adaptor(Debugger& d, const std::shared_ptr<dap::ReaderWriter>& socket);

		std::optional<int> EncodeStackFrame(const DebugState& s, int lvl, Scope sc, int var);
		std::tuple<DebugState&, int, Scope, int> DecodeStackFrame(int f);
		void WaitUntilDisconnected();

		virtual void OnStateOpened(DebugState& s) override;
		virtual void OnStateClosing(DebugState& s, bool lastState) override;
		virtual void OnPaused(DebugState& s, Reason r, std::string_view exceptionText) override;
		virtual void OnLog(std::string_view s) override;
	};
}