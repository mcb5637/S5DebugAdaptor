#include "pch.h"
#include "debugger.h"
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

void debug_lua::Debugger::CheckRun()
{
    if (!HasTasks)
        return;
    std::unique_lock l{ DataMutex };
    while (!Tasks.empty()) {
        LuaExecutionTask& t = *Tasks.front();
        Tasks.pop_front();
        t.Work();
    }
    HasTasks = false;
}

void debug_lua::Debugger::CheckHooked()
{
    std::unique_lock lo{ StatesMutex };
    bool h = Re != Request::Resume;
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
    L.GetSubTable("LuaDebugger");
    L.Push<Debugger, &Debugger::Log>(*this, 0);
    L.SetTableRaw(-2, "Log");
    L.Pop(1);
}

void debug_lua::Debugger::Hook(lua::State L, lua::ActivationRecord ar)
{
    L.PushLightUserdata(&Debugger::Hook);
    L.GetTableRaw(L.REGISTRYINDEX);
    auto th = static_cast<Debugger*>(L.ToUserdata(-1));
    L.Pop(1);
    auto& s = th->GetState(L.GetState());

    auto ev = L.Debug_GetEventFromAR(ar);

    if (th->LineFix && ev == lua::HookEvent::Count) {
        lua::DebugInfo i{};
        if (!L.Debug_GetStack(0, i, lua::DebugInfoOptions::Line, false))
            return;
        int lvl = L.Debug_GetStackDepth();
        if (th->LineFixLine == i.CurrentLine && th->LineFixLevel == lvl) {
            return;
        }
        th->LineFixLine = i.CurrentLine;
        th->LineFixLevel = lvl;
    }
    if (th->LineFix && ev == lua::HookEvent::Line) {
        th->LineFix = false;
        th->CheckHooked();
    }

    th->TranslateRequest(L);
    if (th->Re == Request::Pause && th->St == Status::Running) {
        th->St = Status::Paused;
        if (th->Handler)
            th->Handler->OnPaused(s, Reason::Pause, "");
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

int debug_lua::Debugger::Log(lua::State L)
{
    if (Handler)
        Handler->OnLog(L.ConvertToString(1));
    return 0;
}
