#include "pch.h"
#include "adaptor.h"
#include "shok.h"

debug_lua::Adaptor::Adaptor(Debugger& d, const std::shared_ptr<dap::ReaderWriter>& socket) : Dbg(d)
{
	Session->registerHandler([](const dap::InitializeRequest&) {
		dap::InitializeResponse response;
		response.supportsConfigurationDoneRequest = true;
		response.supportsSetVariable = true;
		return response;
		});

	Session->registerSentHandler(
		[&](const dap::ResponseOrError<dap::InitializeResponse>&) {
			Session->send(dap::InitializedEvent());
		});

	Session->registerHandler([&](const dap::ThreadsRequest&) {
		dap::ThreadsResponse response;
		std::unique_lock l{ Dbg.StatesMutex };
		for (const auto& s : Dbg.GetStates()) {
			dap::Thread thread;
			thread.id = reinterpret_cast<int>(s.L);
			thread.name = s.Name;
			response.threads.push_back(thread);
		}
		return response;
		});

	Session->registerHandler(
		[&](const dap::StackTraceRequest& request)
		-> dap::ResponseOrError<dap::StackTraceResponse> {
			auto c = LuaExecutionPackagedTask<dap::StackTraceResponse>{ [this, request]() {
				auto& l = Dbg.GetState(reinterpret_cast<lua_State*>(int(request.threadId)));
				lua::State L{ l.L };
				int lvl = 0;
				lua::DebugInfo i{};
				dap::StackTraceResponse response;
				while (L.Debug_GetStack(lvl, i, lua::DebugInfoOptions::Line | lua::DebugInfoOptions::Name |
					lua::DebugInfoOptions::Source, false)) {

					dap::StackFrame frame;
					frame.name = L.Debug_GetNameForStackFunc(lvl);
					if (i.What != nullptr && i.What == std::string_view{ "C" }) {
						frame.name += " C code";
					}
					else if (i.Source != nullptr && i.Source == std::string_view{ "?" }) {
						frame.name += " unknown lua";
					}
					else {
						dap::Source source;
						source.name = i.ShortSrc;
						source.path = i.Source;
						frame.source = source;
					}

					frame.line = i.CurrentLine;
					frame.column = 1;
					frame.id = EncodeStackFrame(l, lvl);
					auto [ds, dl] = DecodeStackFrame(frame.id);

					response.stackFrames.push_back(frame);
					++lvl;
				}
				response.totalFrames = response.stackFrames.size();
				return response;
			} };
			Dbg.RunInSHoKThread(c);
			try {
				return c.Get();
			}
			catch (const std::invalid_argument&) {
				return dap::Error("Unknown threadId '%d'", int(request.threadId));
			}
			catch (const lua::LuaException& e) {
				return dap::Error("Lua error: '%s'", e.what());
			}
		});

	Session->registerHandler([&](const dap::ScopesRequest& request)
		-> dap::ResponseOrError<dap::ScopesResponse> {
			auto c = LuaExecutionPackagedTask<dap::ScopesResponse>{ [this, request]() {
				auto sl = DecodeStackFrame(request.frameId);

				dap::Scope scope;
				scope.name = "Locals";
				scope.presentationHint = "locals";
				scope.variablesReference = request.frameId;

				dap::ScopesResponse response;
				response.scopes.push_back(scope);
				return response;
				} };
			Dbg.RunInSHoKThread(c);
			try {
				return c.Get();
			}
			catch (const std::invalid_argument&) {
				return dap::Error("Unknown frameId '%d'", int(request.frameId));
			}
			catch (const lua::LuaException& e) {
				return dap::Error("Lua error: '%s'", e.what());
			}
		});

	Session->registerHandler([&](const dap::VariablesRequest& request)
		-> dap::ResponseOrError<dap::VariablesResponse> {
			auto c = LuaExecutionPackagedTask<dap::VariablesResponse>{ [this, request]() {
				auto [s, lvl] = DecodeStackFrame(request.variablesReference);
				lua::State L{ s.L };
				lua::DebugInfo i{};
				dap::VariablesResponse response;

				int t = L.GetTop();

				if (!L.Debug_GetStack(lvl, i, lua::DebugInfoOptions::Source, true)) {
					L.SetTop(t);
					throw std::invalid_argument{ "invalid stack lvl" };
				}
				int func = L.ToAbsoluteIndex(-1);

				if (i.What != nullptr && i.What == std::string_view{ "C" }) {
					L.SetTop(t);
					return response;
				}

				int num = 1;
				while (const char* n = L.Debug_GetLocal(lvl, num)) {
					dap::Variable currentLineVar;
					currentLineVar.name = n;
					currentLineVar.type = L.TypeName(L.Type(-1));
					currentLineVar.value = L.ToDebugString(-1);
					L.Pop(1);
					response.variables.push_back(currentLineVar);

					++num;
				}
				L.SetTop(t+1);

				num = 1;
				while (const char* n = L.Debug_GetUpvalue(func, num)) {
					dap::Variable currentLineVar;
					currentLineVar.name = n;
					currentLineVar.type = L.TypeName(L.Type(-1));
					currentLineVar.value = L.ToDebugString(-1);
					L.Pop(1);
					response.variables.push_back(currentLineVar);

					++num;
				}
				L.SetTop(t);

				return response;
				} };
			Dbg.RunInSHoKThread(c);
			try {
				return c.Get();
			}
			catch (const std::invalid_argument&) {
				return dap::Error("Unknown variablesReference '%d'", int(request.variablesReference));
			}
			catch (const lua::LuaException& e) {
				return dap::Error("Lua error: '%s'", e.what());
			}
		});

	Session->registerHandler([&](const dap::SetVariableRequest& request)
		-> dap::ResponseOrError<dap::SetVariableResponse> {
			auto c = LuaExecutionPackagedTask<dap::SetVariableResponse>{ [this, request]() {
				auto [s, lvl] = DecodeStackFrame(request.variablesReference);
				lua::State L{ s.L };
				lua::DebugInfo i{};
				dap::SetVariableResponse response;

				int t = L.GetTop();
				if (!L.Debug_GetStack(lvl, i, lua::DebugInfoOptions::Source, true)) {
					L.SetTop(t);
					throw std::invalid_argument{ "invalid stack lvl" };
				}
				int func = L.ToAbsoluteIndex(-1);

				if (i.What != nullptr && i.What == std::string_view{ "C" }) {
					L.SetTop(t);
					throw std::invalid_argument{ "c func" };
				}

				int num = 1;
				while (const char* n = L.Debug_GetLocal(lvl, num)) {
					L.Pop(1);
					if (n == request.name) {
						Dbg.EvaluateInContext(request.value, L, lvl);
						L.SetTop(t + 2);
						response.value = L.ToDebugString(-1);
						L.Debug_SetLocal(lvl, num);
						L.SetTop(t);
						return response;
					}
					++num;
				}
				L.SetTop(t + 1);

				num = 1;
				while (const char* n = L.Debug_GetUpvalue(func, num)) {
					L.Pop(1);
					if (n == request.name) {
						Dbg.EvaluateInContext(request.value, L, lvl);
						L.SetTop(t + 2);
						response.value = L.ToDebugString(-1);
						L.Debug_SetUpvalue(func, num);
						L.SetTop(t);
						return response;
					}
					++num;
				}
				L.SetTop(t);

				throw std::invalid_argument{ "variable not found" };
				} };
			Dbg.RunInSHoKThread(c);
			try {
				return c.Get();
			}
			catch (const std::invalid_argument&) {
				return dap::Error("Unknown variablesReference '%d'", int(request.variablesReference));
			}
			catch (const lua::LuaException& e) {
				return dap::Error("Lua error: '%s'", e.what());
			}
		});

	Session->registerHandler([&](const dap::EvaluateRequest& request)
		-> dap::ResponseOrError<dap::EvaluateResponse> {
			auto c = LuaExecutionPackagedTask<dap::EvaluateResponse>{ [this, request]() {
				lua::State L;
				int lvl;
				if (request.frameId.has_value()) {
					auto [s, lvl2] = DecodeStackFrame(*request.frameId);
					L = s.L;
					lvl = lvl2;
				}
				else {
					lvl = 0;
					L = Dbg.GetStates().back().L;
				}
				
				dap::EvaluateResponse r{};
				int t = L.GetTop();

				int n = Dbg.EvaluateInContext(request.expression, L, lvl);
				r.result = Dbg.OutputString(L, n);
				
				L.SetTop(t);
				return r;
				} };
			Dbg.RunInSHoKThread(c);
			try {
				return c.Get();
			}
			catch (const lua::LuaException& e) {
				return dap::Error("Lua error: '%s'", e.what());
			}
		});

	Session->registerHandler([&](const dap::PauseRequest&) {
		auto c = LuaExecutionPackagedTask<dap::PauseResponse>{ [this]() {
			Dbg.Command(Debugger::Request::Pause);
			return dap::PauseResponse{};
			} };
		Dbg.RunInSHoKThread(c);
		return c.Get();
		});

	Session->registerHandler([&](const dap::ContinueRequest&) {
		auto c = LuaExecutionPackagedTask<dap::ContinueResponse>{ [this]() {
			Dbg.Command(Debugger::Request::Resume);
			return dap::ContinueResponse{};
			} };
		Dbg.RunInSHoKThread(c);
		return c.Get();
		});

	Session->registerHandler([&](const dap::NextRequest&) {
		auto c = LuaExecutionPackagedTask<dap::NextResponse>{ [this]() {
			Dbg.Command(Debugger::Request::StepLine);
			return dap::NextResponse{};
			} };
		Dbg.RunInSHoKThread(c);
		return c.Get();
		});

	Session->registerHandler([&](const dap::StepInRequest&) {
		auto c = LuaExecutionPackagedTask<dap::StepInResponse>{ [this]() {
			Dbg.Command(Debugger::Request::StepIn);
			return dap::StepInResponse{};
			} };
		Dbg.RunInSHoKThread(c);
		return c.Get();
		});

	Session->registerHandler([&](const dap::StepOutRequest&) {
		auto c = LuaExecutionPackagedTask<dap::StepOutResponse>{ [this]() {
			Dbg.Command(Debugger::Request::StepOut);
			return dap::StepOutResponse{};
			} };
		Dbg.RunInSHoKThread(c);
		return c.Get();
		});

	Session->registerHandler([&](const dap::SetBreakpointsRequest& request) {
		dap::SetBreakpointsResponse response;

		/*auto breakpoints = request.breakpoints.value({});
		if (request.source.sourceReference.value(0) == sourceReferenceId) {
			debugger.clearBreakpoints();
			response.breakpoints.resize(breakpoints.size());
			for (size_t i = 0; i < breakpoints.size(); i++) {
				debugger.addBreakpoint(breakpoints[i].line);
				response.breakpoints[i].verified = breakpoints[i].line < numSourceLines;
			}
		}
		else {
			response.breakpoints.resize(breakpoints.size());
		}*/
		// TODO
		return response;
		});

	Session->registerHandler([&](const dap::SetExceptionBreakpointsRequest&) {
		return dap::SetExceptionBreakpointsResponse(); // TODO
		});

	Session->registerHandler([&](const dap::SourceRequest& request)
		-> dap::ResponseOrError<dap::SourceResponse> {
			if (request.source->sourceReference.has_value() && *request.source->sourceReference != 0) {
				return dap::Error("Unknown source reference '%d'",
					int(*request.source->sourceReference));
			}

			if (request.source.has_value() && request.source->path.has_value()) {
				auto c = LuaExecutionPackagedTask<dap::SourceResponse>{ [this, request]() {

					BB::CFileStreamEx f{};
					if (!f.OpenFile(request.source->path->c_str(), BB::IStream::Flags::DefaultRead))
						throw std::invalid_argument{""};

					dap::SourceResponse response;
					
					dap::string s{};
					s.resize(f.GetSize());
					f.Read(s.data(), s.size());
					f.Close();

					response.content = s;
					return response;
					} };
				Dbg.RunInSHoKThread(c);
				try {
					return c.Get();
				}
				catch (const std::invalid_argument&) {
					return dap::Error("could not locate source");
				}
				catch (const lua::LuaException& e) {
					return dap::Error("Lua error: '%s'", e.what());
				}
				catch (const BB::CException& bbe) {
					char msg[200]{};
					bbe.CopyMessage(msg, 200 - 1);
					return dap::Error("%s: %s", typeid(bbe).name(), msg);
				}
			}

			return dap::Error("Unknown source reference '%d'",
				int(*request.source->sourceReference));
		});

	Session->registerHandler(
		[&](const dap::LaunchRequest&) { return dap::LaunchResponse(); });

	Session->registerHandler([&](const dap::DisconnectRequest& request) {
		{
			std::lock_guard<std::mutex> lock(MutexTerminate);
			TerminateDebugger = true;
			Dbg.Handler = nullptr;
			Dbg.Command(Debugger::Request::Resume);
		}
		ConditionTerminate.notify_one();
		return dap::DisconnectResponse();
		});

	Session->registerHandler([&](const dap::ConfigurationDoneRequest&) {
		return dap::ConfigurationDoneResponse();
		});

	Dbg.Handler = this;
	Session->bind(socket);
}

int64_t debug_lua::Adaptor::EncodeStackFrame(const DebugState& s, int lvl)
{
	return static_cast<int64_t>(lvl) << 32 | static_cast<int64_t>(reinterpret_cast<int>(s.L));
}
std::pair<debug_lua::DebugState&, int> debug_lua::Adaptor::DecodeStackFrame(int64_t f)
{
	int lvl = static_cast<int>(f >> 32);
	lua_State* s = reinterpret_cast<lua_State*>(static_cast<int>(f));
	return std::pair<debug_lua::DebugState&, int>{ Dbg.GetState(s), lvl };
}

void debug_lua::Adaptor::WaitUntilDisconnected()
{
	std::unique_lock<std::mutex> lock(MutexTerminate);
	ConditionTerminate.wait(lock);
}

void debug_lua::Adaptor::OnStateOpened(DebugState& s)
{
	dap::ThreadEvent ev;
	ev.threadId = reinterpret_cast<int>(s.L);
	ev.reason = "started";
	Session->send(ev);
}

void debug_lua::Adaptor::OnStateClosing(DebugState& s, bool lastState)
{
	dap::ThreadEvent ev;
	ev.threadId = reinterpret_cast<int>(s.L);
	ev.reason = "exited";
	Session->send(ev);
	if (lastState) {
		dap::TerminatedEvent ev;
		Session->send(ev);
	}
}

void debug_lua::Adaptor::OnPaused(DebugState& s, Reason r, std::string_view exceptionText)
{
	dap::StoppedEvent ev;
	switch (r) {
	case Reason::Step:
		ev.reason = "step";
		ev.description = "step request";
		break;
	case Reason::Breakpoint:
		ev.reason = "breakpoint";
		ev.description = "hit breakpoint";
		break;
	case Reason::Exception:
		ev.reason = "exception";
		ev.description = "exception thrown";
		ev.text = dap::string{ exceptionText };
		break;
	case Reason::Pause:
		ev.reason = "pause";
		ev.description = "pause request";
		break;
	}
	ev.allThreadsStopped = true;
	ev.threadId = reinterpret_cast<int>(s.L);
	Session->send(ev);
}

void debug_lua::Adaptor::OnLog(std::string_view s)
{
	dap::OutputEvent ev;
	ev.category = "stdout";
	ev.output = dap::string{ s };
	Session->send(ev);
}
