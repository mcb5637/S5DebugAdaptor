#include "pch.h"
#include "adaptor.h"
#include "shok.h"
#include "utility.h"

debug_lua::Adaptor::Adaptor(Debugger& d, const std::shared_ptr<dap::ReaderWriter>& socket) : Dbg(d)
{
	Session->registerHandler([&](const dap::InitializeRequest& r) {
		if (r.supportsVariableType.has_value())
			UnderstandsType = *r.supportsVariableType;

		dap::InitializeResponse response;
		response.supportsConfigurationDoneRequest = true;
		response.supportsSetVariable = true;
		response.supportsLoadedSourcesRequest = true;
		response.exceptionBreakpointFilters = dap::array<dap::ExceptionBreakpointsFilter>{};
		{
			dap::ExceptionBreakpointsFilter f{};
			f.filter = "lua_pcall";
			f.label = "lua error (unhandled)";
			f.description = "any lua error ending up in c++ code";
			f.def = true;
			response.exceptionBreakpointFilters->push_back(f);
		}
		{
			dap::ExceptionBreakpointsFilter f{};
			f.filter = "break";
			f.label = "LuaDebugger.Break()";
			f.description = "calls to LuaDebugger.Break()";
			f.def = true;
			response.exceptionBreakpointFilters->push_back(f);
		}
		{
			dap::ExceptionBreakpointsFilter f{};
			f.filter = "load";
			f.label = "syntax error";
			f.description = "syntax errors loading any lua script";
			f.def = true;
			response.exceptionBreakpointFilters->push_back(f);
		}
		{
			dap::ExceptionBreakpointsFilter f{};
			f.filter = "xpcall";
			f.label = "user handled lua error (xpcall, pcall)";
			f.description = "any lua error that gets cought by xpcall/pcall. turn this on, if a triggerfix is spamming error messages";
			f.def = false;
			response.exceptionBreakpointFilters->push_back(f);
		}
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
					frame.name = EnsureUTF8(L.Debug_GetNameForStackFunc(i));
					if (i.What != nullptr && i.What == std::string_view{ "C" }) {
						L.Debug_PushDebugInfoFunc(i);
						frame.name += std::format(" C {}", static_cast<void*>(L.ToCFunction(-1)));
						L.Pop(1);
					}
					else if (i.Source != nullptr && i.Source == std::string_view{ "?" }) {
						frame.name += " unknown lua";
					}
					else if (i.Source != nullptr && i.Source == std::string_view{ "=(tail call)" }) {
						frame.name += " tail call";
					}
					else if (i.Source != nullptr) {
						dap::Source source;
						source.path = Dbg.FindSource(l, i.Source);
						frame.source = source;
					}

					frame.line = i.CurrentLine;
					frame.column = 1;
					auto fid = EncodeStackFrame(l, lvl, Scope::None, 0);
					if (!fid.has_value())
						break;
					frame.id = *fid;

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
				auto [s, lvl, sc, var] = DecodeStackFrame(static_cast<int>(request.frameId));
				dap::ScopesResponse response;
				{
					dap::Scope scope;
					scope.name = "Locals";
					scope.presentationHint = "locals";
					scope.variablesReference = *EncodeStackFrame(s, lvl, Scope::Local, 0);
					response.scopes.push_back(scope);
				}
				{
					dap::Scope scope;
					scope.name = "Upvalues";
					scope.presentationHint = "locals";
					scope.variablesReference = *EncodeStackFrame(s, lvl, Scope::Upvalue, 0);
					response.scopes.push_back(scope);
				}
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
				auto [s, lvl, sc, var] = DecodeStackFrame(static_cast<int>(request.variablesReference));
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

				if (sc == Scope::Local) {
					if (var == 0) {
						int num = 1;
						int tablenum = 1;
						while (const char* n = L.Debug_GetLocal(lvl, num)) {
							dap::Variable currentLineVar;
							currentLineVar.name = EnsureUTF8(n);
							currentLineVar.type = L.TypeName(L.Type(-1));
							if (L.IsTable(-1)) {
								auto ref = EncodeStackFrame(s, lvl, sc, tablenum);
								if (ref.has_value())
									currentLineVar.variablesReference = *ref;
								++tablenum;
							}
							currentLineVar.value = EnsureUTF8(L.ToDebugString<Debugger::ToDebugString_Format>(-1, currentLineVar.variablesReference == 0 ? Dbg.MaxTableExpandLevels : 0));
							L.Pop(1);
							response.variables.push_back(currentLineVar);

							++num;
						}
						L.SetTop(t);
					}
					else {
						int num = 1;
						int tablenum = 1;
						while (const char* n = L.Debug_GetLocal(lvl, num)) {
							if (L.IsTable(-1)) {
								if (tablenum == var) {
									for (auto t : L.Pairs(-1)) {
										dap::Variable currentLineVar;
										currentLineVar.name = EnsureUTF8(L.ToDebugString<Debugger::ToDebugString_Format>(-2));
										currentLineVar.type = L.TypeName(L.Type(-1));
										currentLineVar.value = EnsureUTF8(L.ToDebugString<Debugger::ToDebugString_Format>(-1, Dbg.MaxTableExpandLevels));

										response.variables.push_back(currentLineVar);
									}

									L.Pop(1);
									break;
								}

								++tablenum;
							}
							L.Pop(1);

							++num;
						}
						L.SetTop(t);
					}
				}
				else if (sc == Scope::Upvalue) {
					if (var == 0) {
						int num = 1;
						int tablenum = 1;
						while (const char* n = L.Debug_GetUpvalue(func, num)) {
							dap::Variable currentLineVar;
							currentLineVar.name = EnsureUTF8(n);
							currentLineVar.type = L.TypeName(L.Type(-1));
							if (L.IsTable(-1)) {
								auto ref = EncodeStackFrame(s, lvl, sc, tablenum);
								if (ref.has_value())
									currentLineVar.variablesReference = *ref;
								++tablenum;
							}
							currentLineVar.value = EnsureUTF8(L.ToDebugString<Debugger::ToDebugString_Format>(-1, currentLineVar.variablesReference == 0 ? Dbg.MaxTableExpandLevels : 0));
							L.Pop(1);
							response.variables.push_back(currentLineVar);

							++num;
						}
						L.SetTop(t);
					}
					else {
						int num = 1;
						int tablenum = 1;
						while (const char* n = L.Debug_GetUpvalue(func, num)) {
							if (L.IsTable(-1)) {
								if (tablenum == var) {
									for (auto t : L.Pairs(-1)) {
										dap::Variable currentLineVar;
										currentLineVar.name = EnsureUTF8(L.ToDebugString<Debugger::ToDebugString_Format>(-2));
										currentLineVar.type = L.TypeName(L.Type(-1));
										currentLineVar.value = EnsureUTF8(L.ToDebugString<Debugger::ToDebugString_Format>(-1, Dbg.MaxTableExpandLevels));

										response.variables.push_back(currentLineVar);
									}

									L.Pop(1);
									break;
								}

								++tablenum;
							}
							L.Pop(1);

							++num;
						}
						L.SetTop(t);
					}
				}

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
				auto [s, lvl, sc, var] = DecodeStackFrame(static_cast<int>(request.variablesReference));
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

				if (sc == Scope::Local) {
					int num = 1;
					while (const char* n = L.Debug_GetLocal(lvl, num)) {
						L.Pop(1);
						if (n == request.name) {
							Dbg.EvaluateInContext(request.value, L, lvl);
							L.SetTop(t + 2);
							response.value = EnsureUTF8(L.ToDebugString<Debugger::ToDebugString_Format>(-1));
							L.Debug_SetLocal(lvl, num);
							L.SetTop(t);
							return response;
						}
						++num;
					}
					L.SetTop(t);
				}
				else if (sc == Scope::Upvalue) {
					int num = 1;
					while (const char* n = L.Debug_GetUpvalue(func, num)) {
						L.Pop(1);
						if (n == request.name) {
							Dbg.EvaluateInContext(request.value, L, lvl);
							L.SetTop(t + 2);
							response.value = EnsureUTF8(L.ToDebugString<Debugger::ToDebugString_Format>(-1));
							L.Debug_SetUpvalue(func, num);
							L.SetTop(t);
							return response;
						}
						++num;
					}
					L.SetTop(t);
				}

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
					auto [s, lvl2, _1, _2] = DecodeStackFrame(static_cast<int>(*request.frameId));
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
				r.result = EnsureUTF8(Dbg.OutputString(L, n));
				
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

	Session->registerHandler([&](const dap::SetBreakpointsRequest& request) 
		-> dap::ResponseOrError<dap::SetBreakpointsResponse> {
		auto c = LuaExecutionPackagedTask<dap::SetBreakpointsResponse>{ [this, request]() {
				if (!request.source.path.has_value())
					throw std::invalid_argument{"unknown"};

				dap::SetBreakpointsResponse r;
				const dap::string& p = *request.source.path;
				auto it = std::find_if(Dbg.Breakpoints.begin(), Dbg.Breakpoints.end(), [p](BreakpointFile f) {return p == f.Source.External; });
				BreakpointFile* f;
				if (it == Dbg.Breakpoints.end()) {
					auto* s = Dbg.SearchExternal(p);
					if (s == nullptr)
						throw std::invalid_argument("unknown source");
					f = &Dbg.Breakpoints.emplace_back(*s);
				}
				else {
					f = &(*it);
				}

				f->Lines.clear();
				if (request.breakpoints.has_value()) {
					for (const auto& b : *request.breakpoints) {
						f->Lines.push_back(static_cast<int>(b.line));
						auto& br = r.breakpoints.emplace_back();
						br.line = static_cast<int>(b.line);
						br.verified = true;
					}
				}

				Dbg.RebuildBreakpoints();

				return r;
				} };
		Dbg.RunInSHoKThread(c);
		try {
			return c.Get();
		}
		catch (const std::invalid_argument&) {
			return dap::Error("Unknown source");
		}
		});

	Session->registerHandler([&](const dap::SetExceptionBreakpointsRequest& r)
		-> dap::ResponseOrError<dap::SetExceptionBreakpointsResponse> {
			BreakSettings s = BreakSettings::None;
			for (const auto& f : r.filters) {
				if (f == "lua_pcall")
					s |= BreakSettings::PCall;
				if (f == "break")
					s |= BreakSettings::Break;
				if (f == "load")
					s |= BreakSettings::Syntax;
				if (f == "xpcall")
					s |= BreakSettings::XPCall;
			}
			auto c = LuaExecutionPackagedTask<dap::SetExceptionBreakpointsResponse>{ [&]() {
				Dbg.SetBreakSettings(s);
				return dap::SetExceptionBreakpointsResponse();
				} };
			Dbg.RunInSHoKThread(c);
			try {
				return c.Get();
			}
			catch (const std::invalid_argument&) {
				return dap::Error("Unknown source");
			}
		});

	Session->registerHandler([&](const dap::LoadedSourcesRequest& r) {
		dap::LoadedSourcesResponse res;

		std::unique_lock lo{ Dbg.StatesMutex };
		for (const auto& s : Dbg.GetStates()) {
			for (const auto& src : s.SourcesLoaded) {
				dap::Source sr;
				sr.path = src.External;
				res.sources.push_back(sr);
			}
		}

		return res;
		});

	Session->registerHandler([&](const dap::SourceRequest& request)
		-> dap::ResponseOrError<dap::SourceResponse> {
			if (request.source->sourceReference.has_value() && *request.source->sourceReference != 0) {
				return dap::Error("Unknown source reference '%d'",
					int(*request.source->sourceReference));
			}

			if (request.source.has_value() && request.source->path.has_value()) {
				auto c = LuaExecutionPackagedTask<dap::SourceResponse>{ [this, request]() {
					std::unique_lock lo{ Dbg.StatesMutex };
					auto& stat = Dbg.GetStates();
					std::string_view bbatoload{};
					if (stat.size() > 1 && !stat[1].MapFile.empty()) {
						bbatoload = stat[1].MapFile;
					}
					EnsureBbaLoaded load{ bbatoload };

					BB::CFileStreamEx f{};
					auto file = UTF8ToANSI(*request.source->path);
					if (!f.OpenFile(file.c_str(), BB::IStream::Flags::DefaultRead))
						throw std::invalid_argument{""};

					dap::SourceResponse response;
					
					dap::string s{};
					s.resize(f.GetSize());
					f.Read(s.data(), s.size());
					f.Close();
					if (s.ends_with('\0'))
						s.resize(s.size() - 2);

					response.content = EnsureUTF8(s);
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
		[&](const dap::LaunchRequest&) {
			IsAttached = false;
			return dap::LaunchResponse();
		});

	Session->registerHandler(
		[&](const dap::AttachRequest&) {
			IsAttached = true;
			return dap::AttachResponse();
		});

	Session->registerHandler([&](const dap::DisconnectRequest& request) {
		{
			std::lock_guard<std::mutex> lock(MutexTerminate);
			TerminateDebugger = true;
			Dbg.Handler = nullptr;
			Dbg.Command(Debugger::Request::Resume);
			if (!IsAttached) {
				auto c = LuaExecutionPackagedTask<void>{ [this, request]() {
					std::lock_guard<std::mutex> lock(Dbg.StatesMutex);
					auto& s = Dbg.GetStates();
					if (!s.empty())
						Dbg.EvaluateInContext("Framework.ExitGame()", s[0].L, -1);
					} };
				Dbg.RunInSHoKThread(c);
				c.Get();
			}
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

// encoded: lowest->highest bit: 2 state, 18 bits frame, 2 bits scope, 10 bits variable
constexpr int bitmask(int n) {
	return (1 << n) - 1;
}
constexpr int state_bits = 2;
constexpr int state_mask = bitmask(state_bits);
constexpr int frame_bits = 18;
constexpr int frame_mask = bitmask(frame_bits) << state_bits;
constexpr int scope_bits = 2;
constexpr int scope_mask = bitmask(scope_bits) << frame_bits << state_bits;
constexpr int var_bits = 10;
constexpr int var_mask = bitmask(var_bits) << scope_bits << frame_bits << state_bits;
static_assert(state_bits + frame_bits + scope_bits + var_bits == 32);
std::optional<int> debug_lua::Adaptor::EncodeStackFrame(const DebugState& s, int lvl, Scope sc, int var)
{
	int si = Dbg.GetStateIndex(s);
	if ((si & state_mask) != si)
		return std::nullopt;
	lvl = lvl << state_bits;
	if ((lvl & frame_mask) != lvl)
		return std::nullopt;
	int sco = static_cast<int>(sc) << state_bits << frame_bits;
	var = var << state_bits << frame_bits << scope_bits;
	if ((var & var_mask) != var)
		return std::nullopt;
	return si | lvl | sco | var;
}
std::tuple<debug_lua::DebugState&, int, debug_lua::Adaptor::Scope, int> debug_lua::Adaptor::DecodeStackFrame(int f)
{
	auto& s = Dbg.GetState(f & state_mask);
	int lvl = (f & frame_mask) >> state_bits;
	Scope sc = static_cast<Scope>((f & scope_mask) >> state_bits >> frame_bits);
	int v = (f & var_mask) >> scope_bits >> state_bits >> frame_bits;
	return { s, lvl, sc, v };
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
		{
			dap::TerminatedEvent ev;
			Session->send(ev);
			std::lock_guard<std::mutex> lock(MutexTerminate);
			TerminateDebugger = true;
			Dbg.Handler = nullptr;
		}
		ConditionTerminate.notify_one();
	}
	else {
		for (const auto& src : s.SourcesLoaded) {
			bool found = false;
			for (const auto& st : Dbg.GetStates()) {
				if (&st == &s)
					continue;
				if (std::find(st.SourcesLoaded.begin(), st.SourcesLoaded.end(), src) != st.SourcesLoaded.end()) {
					found = true;
					break;
				}
			}
			if (!found) {
				dap::LoadedSourceEvent le;
				le.reason = "removed";
				le.source.path = src.External;
				Session->send(le);
			}
		}
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
		ev.text = EnsureUTF8(exceptionText);
		break;
	case Reason::Pause:
		ev.reason = "pause";
		ev.description = "pause request";
		break;
	}
	ev.allThreadsStopped = true;
	ev.threadId = reinterpret_cast<int>(s.L);
	ev.preserveFocusHint = false;
	Session->send(ev);
}

void debug_lua::Adaptor::OnLog(std::string_view s)
{
	dap::OutputEvent ev;
	ev.category = "stdout";
	ev.output = EnsureUTF8(s);
	Session->send(ev);
}

void debug_lua::Adaptor::OnSourceAdded(DebugState& s, std::string_view f)
{
	dap::LoadedSourceEvent ev;
	ev.reason = "new";
	ev.source.path = dap::string{ f };
	Session->send(ev);
}

void debug_lua::Adaptor::OnShutdown()
{
	{
		dap::TerminatedEvent ev;
		Session->send(ev);
		std::lock_guard<std::mutex> lock(MutexTerminate);
		TerminateDebugger = true;
		Dbg.Handler = nullptr;
		Dbg.Command(Debugger::Request::Resume);
	}
	ConditionTerminate.notify_one();
}
