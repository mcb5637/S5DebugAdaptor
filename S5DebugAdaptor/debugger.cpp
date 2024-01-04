#include "pch.h"
#include "debugger.h"
#include <regex>
#include "Hooks.h"

bool debug_lua::operator==(DebugState d, lua_State* l)
{
    return d.L == l;
}

debug_lua::DebugState& debug_lua::Debugger::GetState(lua_State* l)
{
    std::unique_lock lo{ StatesMutex };
    for (DebugState& r : States) {
        if (r.L == l)
            return r;
    }
    throw std::invalid_argument{ "state does not exist" };
}
debug_lua::DebugState& debug_lua::Debugger::GetState(int i)
{
    std::unique_lock lo{ StatesMutex };
    return States.at(i);
}
int debug_lua::Debugger::GetStateIndex(const DebugState& s)
{
    std::unique_lock lo{ StatesMutex };
    for (size_t i = 0; i < States.size(); ++i) {
        if (&s == &States[i])
            return static_cast<int>(i);
    }
    throw std::invalid_argument{ "state does not exist" };
}

void debug_lua::Debugger::OnStateAdded(lua_State* l, const char* name)
{
    DebugState* s;
    {
        std::unique_lock lo{ StatesMutex };
        Hooks::InstallHook();
        Hooks::RunCallback = std::bind(&Debugger::CheckRun, this);
        if (name == nullptr)
            name = States.empty() ? "Main Menu" : "Ingame";
        s = &States.emplace_back(l, name);
        InitializeLua(lua::State{ s->L });
    }
    CheckHooked();
    if (Handler)
        Handler->OnStateOpened(*s);
}

void debug_lua::Debugger::OnStateClosed(lua_State* l)
{
    std::unique_lock lo{ StatesMutex };
    auto i = std::find(States.begin(), States.end(), l);
    if (i == States.end())
        throw std::invalid_argument{ "trying to close a state that does not exist" };
    if (Handler)
        Handler->OnStateClosing(*i, States.size() == 1);
    States.erase(i);
}

void debug_lua::Debugger::OnBreak(lua_State* l)
{
    if (Handler == nullptr)
        return;
    {
        std::unique_lock lo{ StatesMutex };
        auto i = std::find(States.begin(), States.end(), l);
        if (i == States.end())
            throw std::invalid_argument{ "trying to close a state that does not exist" };
    }
    LineFix = true;
    Command(Request::StepOut);
    TranslateRequest(lua::State{ l });
    Re = Request::BreakpointAtLevel;
}

void debug_lua::Debugger::RunInSHoKThread(LuaExecutionTask& t)
{
    std::unique_lock l{ DataMutex };
    Tasks.push_back(&t);
    HasTasks = true;
    Hooks::SendCheckRun();
}

void debug_lua::Debugger::Command(Request r)
{
    Re = r;
    if (r == Request::Pause)
        LineFix = true;
    CheckHooked();
}

void debug_lua::Debugger::RebuildBreakpoints()
{
    BreakpointLookup.clear();
    for (auto& b : Breakpoints) {
        for (auto l : b.Lines) {
            BreakpointLookup[l].push_back(&b);
        }
    }
    CheckHooked();
}

void debug_lua::Debugger::SetPCallEnabled(bool e)
{
    Hooks::ErrorCallback = e ? lua::State::CppToCFunction<ErrorFunc> : nullptr;
}

int debug_lua::Debugger::EvaluateInContext(std::string_view s, lua::State L, int lvl)
{
    std::string pre = "";
    std::string post = "";
    std::string var = "r";
    auto cb = Hooks::ErrorCallback;
    Hooks::ErrorCallback = nullptr;
    if (lvl >= 0 && L.Debug_IsStackLevelValid(lvl)) {
        std::vector<std::string_view> varstaken{};
        int num = 1;
        while (const char* n = L.Debug_GetLocal(lvl, num)) {
            L.Pop(1);
            std::string_view s{ n };
            if (IsIdentifier(s) && std::find(varstaken.begin(), varstaken.end(), s) == varstaken.end()) {
                varstaken.push_back(s);
                pre.append(std::format("local {} = LuaDebugger.GetLocal({}, {})\r\n", s, lvl+2, num));
                post.append(std::format("LuaDebugger.SetLocal({}, {}, {})\r\n", lvl + 2, num, s));
            }

            ++num;
        }
        lua::DebugInfo i{};
        L.Debug_GetStack(lvl, i, lua::DebugInfoOptions::Line, true);
        num = 1;
        while (const char* n = L.Debug_GetUpvalue(-1, num)) {
            L.Pop(1);
            std::string_view s{ n };
            if (IsIdentifier(s) && std::find(varstaken.begin(), varstaken.end(), s) == varstaken.end()) {
                varstaken.push_back(s);
                pre.append(std::format("local {} = LuaDebugger.GetUpvalue({}, {})\r\n", s, lvl + 2, num));
                post.append(std::format("LuaDebugger.SetUpvalue({}, {}, {})\r\n", lvl + 2, num, s));
            }

            ++num;
        }
        L.Pop(1);
        while (std::find(varstaken.begin(), varstaken.end(), var) != varstaken.end()) {
            var.append("r");
        }
    }
    std::string asstatement = std::format("{0}local {1} = function()\r\n{3}\r\nend\r\n{1} = {{{1}()}}\r\n{2}return unpack({1})", pre, var, post, s);
    std::string asexpresion = std::format("{0}local {1} = function()\r\nreturn {3}\r\nend\r\n{1} = {{{1}()}}\r\n{2}return unpack({1})", pre, var, post, s);
    try {
        int r = L.DoStringT(asexpresion);
        Hooks::ErrorCallback = cb;
        return r;
    }
    catch (const lua::LuaException&) {}
    int r = L.DoStringT(asstatement);
    Hooks::ErrorCallback = cb;
    return r;
}

bool debug_lua::Debugger::IsIdentifier(std::string_view s)
{
    static std::regex reg{ "^[a-zA-Z_][a-zA-Z_0-9]*$", std::regex_constants::ECMAScript | std::regex_constants::optimize };
    return std::regex_match(s.begin(), s.end(), reg);
}

std::string debug_lua::Debugger::OutputString(lua::State L, int n)
{
    std::string r{};
    if (n == 0) {
        r = "nil";
    }
    else if (n == 1) {
        r = L.ToDebugString(-1);
    }
    else {
        int t = L.GetTop() - n;
        r = "(";
        for (int i = t + 1; i <= t + n; ++i) {
            r.append(L.ToDebugString(i));
            if (i < t + n)
                r.append(",\r\n");
        }
        r.append(")");
    }
    return r;
}

void debug_lua::Debugger::CheckRun()
{
    if (!HasTasks)
        return;
    while (true) {
        LuaExecutionTask* t;
        {
            std::unique_lock l{ DataMutex };
            if (Tasks.empty()) {
                HasTasks = false;
                return;
            }
            t = Tasks.front();
            Tasks.pop_front();
        }
        t->Work();
    }
}

void debug_lua::Debugger::CheckHooked()
{
    std::unique_lock lo{ StatesMutex };
    bool h = Re != Request::Resume || !BreakpointLookup.empty();
    for (auto& s : States)
        SetHooked(s, h, LineFix);
}
void debug_lua::Debugger::SetHooked(DebugState& s, bool h, bool imm)
{
    lua::State L{ s.L };
    if (h) {
        auto e = lua::HookEvent::Call | lua::HookEvent::Line | lua::HookEvent::Return;
        if (imm)
            e = e | lua::HookEvent::Count;
        L.Debug_SetHook<Hook>(e, 1);
    }
    else {
        L.Debug_SetHook<Hook>(lua::HookEvent::Count, 50000); // so we can pause in infinite loops
    }
}

void debug_lua::Debugger::WaitForRequest()
{
    do {
        CheckRun();
    } while (Re == Request::Pause);
}
void debug_lua::Debugger::TranslateRequest(lua::State L)
{
    std::unique_lock l{ DataMutex };
    if (Re == Request::StepIn) {
        Re = Request::StepToLevel;
        StepToLevel = L.Debug_GetStackDepth() + 1;
    }
    else if (Re == Request::StepLine) {
        Re = Request::StepToLevel;
        StepToLevel = L.Debug_GetStackDepth();
    }
    else if (Re == Request::StepOut) {
        Re = Request::StepToLevel;
        StepToLevel = L.Debug_GetStackDepth() - 1;
    }
}

void debug_lua::Debugger::InitializeLua(lua::State L)
{
    L.PushLightUserdata(&Debugger::Hook);
    L.PushLightUserdata(this);
    L.SetTableRaw(L.REGISTRYINDEX);

    std::array<lua::FuncReference, 6> lib{ {
        lua::FuncReference::GetRef<Debugger, &Debugger::Log>(*this, "Log"),
        lua::FuncReference::GetRef<Debugger, &Debugger::IsDebuggerAttached>(*this, "IsDebuggerAttached"),
        lua::FuncReference::GetRef<Debugger, &Debugger::SetLocal>(*this, "SetLocal"),
        lua::FuncReference::GetRef<Debugger, &Debugger::GetLocal>(*this, "GetLocal"),
        lua::FuncReference::GetRef<Debugger, &Debugger::SetUpvalue>(*this, "SetUpvalue"),
        lua::FuncReference::GetRef<Debugger, &Debugger::GetUpvalue>(*this, "GetUpvalue"),
        } };
    L.RegisterGlobalLib(lib, "LuaDebugger");
}

void debug_lua::Debugger::Hook(lua::State L, lua::ActivationRecord ar)
{
    L.PushLightUserdata(&Debugger::Hook);
    L.GetTableRaw(L.REGISTRYINDEX);
    auto th = static_cast<Debugger*>(L.ToUserdata(-1));
    L.Pop(1);
    auto& s = th->GetState(L.GetState());

    int line = -1;
    bool checkBreakpoint = false;

    if (th->LineFix && ar.Matches(lua::HookEvent::Count)) {
        lua::DebugInfo i{};
        if (!L.Debug_GetStack(0, i, lua::DebugInfoOptions::Line, false))
            return;
        int lvl = L.Debug_GetStackDepth();
        if (th->LineFixLine == i.CurrentLine && th->LineFixLevel == lvl) {
            return;
        }
        th->LineFixLine = i.CurrentLine;
        th->LineFixLevel = lvl;
        line = i.CurrentLine;
        checkBreakpoint = true;
    }
    if (ar.Matches(lua::HookEvent::Line)) {
        if (th->LineFix) {
            th->LineFix = false;
            th->LineFixLevel = 0;
            th->LineFixLine = -1;
            th->CheckHooked();
        }
        line = ar.Line();
        checkBreakpoint = true;
    }

    th->TranslateRequest(L);
    if (th->Re == Request::Pause && th->St == Status::Running) {
        th->St = Status::Paused;
        if (th->Handler)
            th->Handler->OnPaused(s, Reason::Pause, "");
    }
    else if (checkBreakpoint && !th->BreakpointLookup.empty() && th->St == Status::Running) {
        auto it = th->BreakpointLookup.find(line);
        if (it != th->BreakpointLookup.end()) {
            auto dinf = L.Debug_GetInfoFromAR(ar, lua::DebugInfoOptions::Source);
            if (dinf.Source != nullptr) {
                auto it2 = std::find_if(it->second.begin(), it->second.end(), [dinf](BreakpointFile* f) {return dinf.Source == f->Filename; });
                if (it2 != it->second.end()) {
                    th->Re = Request::Pause;
                    th->St = Status::Paused;
                    if (th->Handler)
                        th->Handler->OnPaused(s, Reason::Breakpoint, "");
                }
            }
        }
    }

    if (th->Re == Request::StepToLevel || th->Re == Request::BreakpointAtLevel) {
        int lvl = L.Debug_GetStackDepth();
        if (th->StepToLevel == lvl) {
            th->Re = Request::Pause;
            th->St = Status::Paused;
            if (th->Handler)
                th->Handler->OnPaused(s, th->Re == Request::BreakpointAtLevel ? Reason::Breakpoint : Reason::Step, "");
        }
    }

    th->WaitForRequest();
    th->TranslateRequest(L);
    th->St = Status::Running;
}

int debug_lua::Debugger::ErrorFunc(lua::State L)
{
    L.PushLightUserdata(&Debugger::Hook);
    L.GetTableRaw(L.REGISTRYINDEX);
    auto th = static_cast<Debugger*>(L.ToUserdata(-1));
    L.Pop(1);
    auto& s = th->GetState(L.GetState());

    if (!th->Handler)
        return 1;

    th->St = Status::Paused;
    th->Re = Request::Pause;
    
    th->Handler->OnPaused(s, Reason::Exception, L.ToStringView(1));

    th->WaitForRequest();
    th->TranslateRequest(L);
    th->St = Status::Running;
    return 1;
}

int debug_lua::Debugger::Log(lua::State L)
{
    if (Handler) {
        auto s = OutputString(L, L.GetTop());
        s = "Log: " + s + "\r\n";
        Handler->OnLog(s);
    }
    return 0;
}

int debug_lua::Debugger::GetLocal(lua::State L)
{
    int lvl = L.CheckInt(1);
    lua::DebugInfo i{};
    if (!L.Debug_GetStack(lvl, i, lua::DebugInfoOptions::Source, false))
        throw lua::LuaException{ "invalid stack level" };
    if (i.What != nullptr && i.What == std::string_view{ "C" })
        throw lua::LuaException{ "not allowed to access locals of c functions" };
    L.Push(L.Debug_GetLocal(lvl, L.CheckInt(2)));
    return 2;
}
int debug_lua::Debugger::SetLocal(lua::State L)
{
    int lvl = L.CheckInt(1);
    lua::DebugInfo i{};
    if (!L.Debug_GetStack(lvl, i, lua::DebugInfoOptions::Source, false))
        throw lua::LuaException{ "invalid stack level" };
    if (i.What != nullptr && i.What == std::string_view{ "C" })
        throw lua::LuaException{ "not allowed to access locals of c functions" };
    L.CheckAny(3);
    L.PushValue(3);
    L.Debug_SetLocal(lvl, L.CheckInt(2));
    return 0;
}
int debug_lua::Debugger::GetUpvalue(lua::State L)
{
    int lvl = L.CheckInt(1);
    lua::DebugInfo i{};
    if (!L.Debug_GetStack(lvl, i, lua::DebugInfoOptions::Source, true))
        throw lua::LuaException{ "invalid stack level" };
    if (i.What != nullptr && i.What == std::string_view{ "C" })
        throw lua::LuaException{ "not allowed to access locals of c functions" };
    L.Push(L.Debug_GetUpvalue(-1, L.CheckInt(2)));
    return 2;
}
int debug_lua::Debugger::SetUpvalue(lua::State L)
{
    int lvl = L.CheckInt(1);
    lua::DebugInfo i{};
    if (!L.Debug_GetStack(lvl, i, lua::DebugInfoOptions::Source, true))
        throw lua::LuaException{ "invalid stack level" };
    if (i.What != nullptr && i.What == std::string_view{ "C" })
        throw lua::LuaException{ "not allowed to access locals of c functions" };
    L.CheckAny(3);
    L.PushValue(3);
    L.Debug_SetLocal(-2, L.CheckInt(2));
    return 0;
}
int debug_lua::Debugger::IsDebuggerAttached(lua::State L)
{
    L.Push(Handler != nullptr);
    return 1;
}
