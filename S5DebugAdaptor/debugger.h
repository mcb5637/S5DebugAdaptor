#pragma once

#include <mutex>
#include <condition_variable>
#include <future>
#include <map>

#include "luapp/luapp50.h"

namespace debug_lua {
	class DebugState {
	public:
		lua_State* L;
		const char* Name;
	};

	enum class Reason : int {
		Step,
		Breakpoint,
		Exception,
		Pause,
	};

	struct IDebugEventHandler {
		virtual void OnStateOpened(DebugState& s) = 0;
		virtual void OnStateClosing(DebugState& s, bool lastState) = 0;
		virtual void OnPaused(DebugState& s, Reason r, std::string_view exceptionText) = 0;
		virtual void OnLog(std::string_view s) = 0;
	};

	bool operator==(DebugState d, lua_State* l);

	class LuaExecutionTask {
		friend class Debugger;
	protected:
		virtual void Work() = 0;
	};
	template<class R, class... A>
	class LuaExecutionPackagedTask : public LuaExecutionTask {
		std::packaged_task<R(A...)> Task;

	public:
		template<class C>
		LuaExecutionPackagedTask(C&& c) : Task(std::forward<C>(c)) {}

		R Get() {
			auto f = Task.get_future();
			return f.get();
		}

	protected:
		virtual void Work() override {
			Task();
		}
	};

	struct BreakpointFile {
		std::string Filename;
		std::vector<int> Lines;
	};

	class Debugger {
	public:
		enum class Status : int {
			Running,
			Paused,
		};
		enum class Request : int {
			Resume,
			Pause,
			StepLine,
			StepIn,
			StepOut,
			StepToLevel,
			BreakpointAtLevel,
		};

	private:
		std::vector<DebugState> States;

		std::mutex DataMutex;
		std::list<LuaExecutionTask*> Tasks;
		bool HasTasks = false, LineFix = false;
		int LineFixLine = -1, LineFixLevel = 0;
		std::map<int, std::vector<BreakpointFile*>> BreakpointLookup;

	public:
		IDebugEventHandler* Handler = nullptr;
		Status St = Status::Running;
		Request Re = Request::Resume;
		int StepToLevel = 0;
		std::vector<BreakpointFile> Breakpoints; // call RebuildBreakpoints after modifying, otherwise you get dangling pointers!

		std::mutex StatesMutex;

		inline const std::vector<DebugState> GetStates() const {
			return States;
		}
		DebugState& GetState(lua_State* l);
		void OnStateAdded(lua_State* l, const char* name);
		void OnStateClosed(lua_State* l);
		void OnBreak(lua_State* l);

		// remember to Get the task
		void RunInSHoKThread(LuaExecutionTask& t);
		void Command(Request r);
		void RebuildBreakpoints();
		void SetPCallEnabled(bool e);

		int EvaluateInContext(std::string_view s, lua::State L, int lvl);
		std::string OutputString(lua::State L, int n);

	private:
		bool IsIdentifier(std::string_view s);
		void CheckRun();
		void CheckHooked();
		void SetHooked(DebugState& s, bool h, bool imm);
		void WaitForRequest();
		void TranslateRequest(lua::State L);
		void InitializeLua(lua::State L);

		static void Hook(lua::State L, lua::ActivationRecord ar);
		static int ErrorFunc(lua::State L);

		int Log(lua::State L);
		int GetLocal(lua::State L);
		int SetLocal(lua::State L);
		int GetUpvalue(lua::State L);
		int SetUpvalue(lua::State L);
		int IsDebuggerAttached(lua::State L);
	};
}
