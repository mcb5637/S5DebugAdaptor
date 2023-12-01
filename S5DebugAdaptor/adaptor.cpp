#include "pch.h"
#include "adaptor.h"

debug_lua::Adaptor::Adaptor(Debugger& d, const std::shared_ptr<dap::ReaderWriter>& socket) : Dbg(d)
{
	Session->registerHandler([](const dap::InitializeRequest&) {
		dap::InitializeResponse response;
		response.supportsConfigurationDoneRequest = true;
		return response;
		});

	Session->registerSentHandler(
		[&](const dap::ResponseOrError<dap::InitializeResponse>&) {
			Session->send(dap::InitializedEvent());
		});

	Session->registerHandler([&](const dap::ThreadsRequest&) {
		dap::ThreadsResponse response;
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
			try {
				auto& l = Dbg.GetState(reinterpret_cast<lua_State*>(int(request.threadId)));
				lua::State L{ l.L };
				int lvl = 0;
				lua50::DebugInfo i{};
				dap::StackTraceResponse response;
				while (L.Debug_GetStack(lvl, i, lua50::DebugInfoOptions::Line | lua50::DebugInfoOptions::Name |
					lua50::DebugInfoOptions::Source, false)) {

					dap::Source source;
					source.name = i.ShortSrc;
					source.path = i.Source;

					dap::StackFrame frame;
					frame.line = i.CurrentLine;
					frame.column = 1;
					frame.name = i.Name;
					frame.id = EncodeStackFrame(l, lvl);
					frame.source = source;

					response.stackFrames.push_back(frame);
				}
				response.totalFrames = response.stackFrames.size();
				return response;
			}
			catch (const std::invalid_argument& ia) {
				return dap::Error("Unknown threadId '%d'", int(request.threadId));
			}
			catch (const lua::LuaException& e) {
				return dap::Error("Lua error: '%s'", e.what());
			}
		});

	Session->registerHandler([&](const dap::ScopesRequest& request)
		-> dap::ResponseOrError<dap::ScopesResponse> {
			try {
				auto sl = DecodeStackFrame(request.frameId);

				dap::Scope scope;
				scope.name = "Locals";
				scope.presentationHint = "locals";
				scope.variablesReference = request.frameId;

				dap::ScopesResponse response;
				response.scopes.push_back(scope);
				return response;
			}
			catch (const std::invalid_argument& ia) {
				return dap::Error("Unknown frameId '%d'", int(request.frameId));
			}
			catch (const lua::LuaException& e) {
				return dap::Error("Lua error: '%s'", e.what());
			}
		});

	Session->registerHandler([&](const dap::VariablesRequest& request)
		-> dap::ResponseOrError<dap::VariablesResponse> {
			try {
				auto sl = DecodeStackFrame(request.variablesReference);
				lua::State L{ sl.first.L };
				lua50::DebugInfo i{};
				dap::VariablesResponse response;

				if (!L.Debug_GetStack(sl.second, i, lua50::DebugInfoOptions::Source, true)) {
					throw std::invalid_argument{ "invalid stack lvl" };
				}

				if (i.What == std::string_view{ "C" }) {
					L.Pop(1);
					return response;
				}

				int num = 0;
				const char* n = L.Debug_GetLocal(sl.second, num);
				while (n != nullptr) {
					dap::Variable currentLineVar;
					currentLineVar.name = n;
					currentLineVar.type = L.TypeName(L.Type(-1));
					currentLineVar.value = L.ToDebugString(-1);
					L.Pop(1);
					response.variables.push_back(currentLineVar);

					++num;
					n = L.Debug_GetLocal(sl.second, num);
				}

				num = 0;
				n = L.Debug_GetUpvalue(-1, num);
				while (n != nullptr) {
					dap::Variable currentLineVar;
					currentLineVar.name = n;
					currentLineVar.type = L.TypeName(L.Type(-1));
					currentLineVar.value = L.ToDebugString(-1);
					L.Pop(1);
					response.variables.push_back(currentLineVar);

					++num;
					n = L.Debug_GetUpvalue(-1, num);
				}
				L.Pop(1);

				return response;
			}
			catch (const std::invalid_argument& ia) {
				return dap::Error("Unknown variablesReference '%d'", int(request.variablesReference));
			}
			catch (const lua::LuaException& e) {
				return dap::Error("Lua error: '%s'", e.what());
			}
		});

	Session->registerHandler([&](const dap::PauseRequest&) {
		Dbg.Re = Debugger::Request::Pause;
		return dap::PauseResponse();
		});

	Session->registerHandler([&](const dap::ContinueRequest&) {
		Dbg.Re = Debugger::Request::Resume;
		return dap::ContinueResponse();
		});

	Session->registerHandler([&](const dap::NextRequest&) {
		Dbg.Re = Debugger::Request::StepLine;
		return dap::NextResponse();
		});

	Session->registerHandler([&](const dap::StepInRequest&) {
		Dbg.Re = Debugger::Request::StepIn;
		return dap::StepInResponse();
		});

	Session->registerHandler([&](const dap::StepOutRequest&) {
		Dbg.Re = Debugger::Request::StepOut;
		return dap::StepOutResponse();
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
			/*if (request.sourceReference != sourceReferenceId) {
				return dap::Error("Unknown source reference '%d'",
					int(request.sourceReference));
			}*/

			dap::SourceResponse response;
			//response.content = sourceContent; TODO
			return response;
		});

	Session->registerHandler(
		[&](const dap::LaunchRequest&) { return dap::LaunchResponse(); });

	Session->registerHandler([&](const dap::DisconnectRequest& request) {
		std::unique_lock<std::mutex> lock(mutex);
		terminate = true;
		cv.notify_one();
		return dap::DisconnectResponse();
		});

	Session->registerHandler([&](const dap::ConfigurationDoneRequest&) {
		return dap::ConfigurationDoneResponse();
		});

	Session->bind(socket);

	/*in = dap::file(stdin, false);
	out = dap::file(stdout, false);
	Session->bind(in, out);*/
}

int64_t debug_lua::Adaptor::EncodeStackFrame(const DebugState& s, int lvl)
{
	return static_cast<int64_t>(reinterpret_cast<int>(s.L)) << 32 | static_cast<int64_t>(lvl);
}
std::pair<debug_lua::DebugState&, int> debug_lua::Adaptor::DecodeStackFrame(int64_t f)
{
	int lvl = static_cast<int>(f);
	lua_State* s = reinterpret_cast<lua_State*>(static_cast<int>(f >> 32));
	return std::pair<debug_lua::DebugState&, int>{ Dbg.GetState(s), lvl };
}

void debug_lua::Adaptor::WaitUntilDisconnected()
{
	std::unique_lock<std::mutex> lock(mutex);
	cv.wait(lock);
}

void debug_lua::Adaptor::OnStateOpened(DebugState& s)
{

}
