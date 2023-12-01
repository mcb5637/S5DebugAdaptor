#include "pch.h"
#include "debugger.h"
#include "Hooks.h"

bool debug_lua::operator==(DebugState d, lua_State* l)
{
    return d.L == l;
}

debug_lua::DebugState& debug_lua::Debugger::GetState(lua_State* l)
{
    for (DebugState& r : States) {
        if (r.L == l)
            return r;
    }
    throw std::invalid_argument{ "state does not exist" };
}

void debug_lua::Debugger::OnStateAdded(lua_State* l)
{
    Hooks::InstallHook();
    States.emplace_back(l, States.empty() ? "Main Menu" : "Ingame");
    if (Handler)
        Handler->OnStateOpened(GetState(l));
    Hooks::SendCheckRun();
}

void debug_lua::Debugger::OnStateClosed(lua_State* l)
{
    auto i = std::find(States.begin(), States.end(), l);
    if (i == States.end())
        throw std::invalid_argument{ "trying to close a state that does not exist" };
    States.erase(i);
}
