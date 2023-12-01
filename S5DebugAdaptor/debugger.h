#pragma once

#include "luapp/luapp50.h"

namespace debug_lua {
	class DebugState {
	public:
		lua_State* L;
		const char* Name;
	};

	struct IDebugEventHandler {
		virtual void OnStateOpened(DebugState& s) = 0;
	};

	bool operator==(DebugState d, lua_State* l);

	class Debugger {
	public:
		enum class Status : int {
			Running,
			Paused,
		};
		enum class Request : int {
			Pause,
			Resume,
			StepLine,
			StepIn,
			StepOut,
		};

	private:
		std::vector<DebugState> States;
	public:
		IDebugEventHandler* Handler = nullptr;
		Status St = Status::Running;
		Request Re = Request::Resume;

		inline const std::vector<DebugState> GetStates() const {
			return States;
		}
		DebugState& GetState(lua_State* l);
		void OnStateAdded(lua_State* l);
		void OnStateClosed(lua_State* l);
	};
}
