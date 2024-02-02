#include "pch.h"
#include "debugger.h"
#include <regex>
#include <thread>
#include "Hooks.h"
#include "shok.h"
#include "winhelpers.h"
#include "utility.h"

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
        Hooks::RunCallback = std::bind(&Debugger::RunCallback, this);
        if (name == nullptr)
            name = States.empty() ? "Main Menu" : "Ingame";
        bool isingame = !States.empty();
        s = &States.emplace_back(l, name);
        InitializeLua(lua::State{ s->L }, !isingame);
        if (isingame) {
            Framework::CMain* ma = *Framework::CMain::GlobalObj;
            Framework::MapInfo* mapinf = nullptr;
            if (ma->ToDo == Framework::CMain::NextMode::LoadSaveSP) {
                Framework::SavegameSystem* sa = Framework::SavegameSystem::GlobalObj();
                auto* ci = ma->CampagnInfoHandler.GetCampagnInfo(&sa->CurrentSave->MapData);
                mapinf = ci->GetMapInfoByName(sa->CurrentSave->MapData.MapName.c_str());
            }
            else if (ma->ToDo == Framework::CMain::NextMode::RestartMapSP || ma->ToDo == Framework::CMain::NextMode::StartMapSP
                || ma->ToDo == Framework::CMain::NextMode::StartMapMP) {
                auto* ci = ma->CampagnInfoHandler.GetCampagnInfo(&ma->CurrentMap);
                mapinf = ci->GetMapInfoByName(ma->CurrentMap.MapName.c_str());
            }
            if (mapinf != nullptr) {
                if (mapinf->IsExternalmap) {
                    BB::CFileSystemMgr* mng = *BB::CFileSystemMgr::GlobalObj;
                    s->MapFile = mapinf->MapFilePath;
                    s->MapScriptFile = "Maps\\ExternalMap\\MapScript.lua";
                }
                else {
                    s->MapScriptFile = mapinf->MapFilePath;
                    s->MapScriptFile.append("\\MapScript.lua");
                }
                MapJustOpened = true;
            }
        }
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
    if ((Brk & BreakSettings::Break) == BreakSettings::None)
        return;
    if (Evaluating)
        return;
    DebugState* s = nullptr;
    {
        std::unique_lock lo{ StatesMutex };
        auto i = std::find(States.begin(), States.end(), l);
        if (i == States.end())
            throw std::invalid_argument{ "trying to break a state that does not exist" };
        s = &*i;
    }
    /*Command(Request::StepOut);
    TranslateRequest(lua::State{ l });
    Re = Request::BreakpointAtLevel;*/

    St = Status::Paused;
    Re = Request::Pause;

    Handler->OnPaused(*s, Reason::Breakpoint, "");

    LineFix = true;
    CheckHooked();
    WaitForRequest();
    TranslateRequest(lua::State{l});
    St = Status::Running;
}

void debug_lua::Debugger::OnSourceLoaded(lua_State* L, const char* filename)
{
    std::unique_lock lo{ StatesMutex };
    auto i = std::find(States.begin(), States.end(), L);
    if (i == States.end())
        throw std::invalid_argument{ "trying to break a state that does not exist" };
    DoAddSource(*i, filename);
}

void debug_lua::Debugger::OnShutdown(std::function<void()> cb)
{
    struct S : LuaExecutionTask {
        Debugger& D;
        std::function<void()> Cb;
        S(Debugger& d, std::function<void()> cb) : D(d), Cb(cb) {}
        virtual void Work() override {
            if (D.Handler)
                D.Handler->OnShutdown();
            Cb();
            delete this;
        }
    };
    auto c = new S{*this, cb};
    RunInSHoKThread(*c);
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

void debug_lua::Debugger::SetBreakSettings(BreakSettings s)
{
    Brk = s;
    Hooks::ErrorCallback = (s & (BreakSettings::PCall | BreakSettings::XPCall)) != BreakSettings::None ? lua::State::CppToCFunction<ErrorFunc> : nullptr;
    Hooks::SyntaxCallback = (s & BreakSettings::Syntax) != BreakSettings::None ? SyntaxErrorFunc : nullptr;
}

int debug_lua::Debugger::EvaluateInContext(std::string_view s, lua::State L, int lvl)
{
    std::string pre = "";
    std::string post = "";
    std::string var = "r";
    VarOverrideReset over{ Evaluating, true };
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
        return L.DoStringT(asexpresion, "from console");
    }
    catch (const lua::LuaException&) {}
    return L.DoStringT(asstatement, "from console");
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
        r = L.ToDebugString<ToDebugString_Format>(-1, MaxTableExpandLevels);
    }
    else {
        int t = L.GetTop() - n;
        r = "(";
        for (int i = t + 1; i <= t + n; ++i) {
            r.append(L.ToDebugString<ToDebugString_Format>(i));
            if (i < t + n)
                r.append(",\r\n");
        }
        r.append(")");
    }
    return r;
}

std::string debug_lua::Debugger::ToDebugString_Format::LuaFuncSourceFormat(lua::State L, int index, const lua::DebugInfo& d)
{
    L.PushLightUserdata(&Debugger::Hook);
    L.GetTableRaw(L.REGISTRYINDEX);
    auto th = static_cast<Debugger*>(L.ToUserdata(-1));
    L.Pop(1);
    auto src = d.Source == nullptr ? "" : th->FindSource(th->GetState(L.GetState()), d.Source);
    return std::format("{}:{}", src, d.LineDefined);
}

std::string debug_lua::Debugger::TranslateSourceString(const DebugState& s, std::string_view src)
{
    if (src == MapScript) {
        if (!s.MapScriptFile.empty())
            src = s.MapScriptFile;
    }
    return ANSIToUTF8(src);
}

debug_lua::Source* debug_lua::Debugger::SearchInternal(std::string_view i)
{
    std::unique_lock lo{ StatesMutex };
    for (DebugState& r : States) {
        for (auto& s : r.SourcesLoaded) {
            if (s.Internal == i)
                return &s;
        }
    }
    return nullptr;
}
debug_lua::Source* debug_lua::Debugger::SearchExternal(std::string_view e)
{
    std::unique_lock lo{ StatesMutex };
    for (DebugState& r : States) {
        for (auto& s : r.SourcesLoaded) {
            if (s.External == e)
                return &s;
        }
    }
    return nullptr;
}
std::string debug_lua::Debugger::FindSource(const DebugState& s, std::string_view i)
{
    auto sr = SearchInternal(i);
    if (sr != nullptr) {
        return sr->External;
    }
    return TranslateSourceString(s, i);
}

void debug_lua::Debugger::RunCallback()
{
    CheckRun();
    if (MapJustOpened) {
        std::unique_lock lo{ StatesMutex };
        for (size_t i = 1; i < States.size(); ++i) {
            CheckSourcesLoaded(States[i]);
        }
        MapJustOpened = false;
    }
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
        auto e = lua::HookEvent::Line;
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
    CheckRun();
    HadForeground = GetForegroundWindow() == *shok::MainWindowHandle;
    while (Re == Request::Pause)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds{ 10 });
        ProcessBasicWindowEvents();
        CheckRun();
    }
    if (HadForeground)
        SetForegroundWindow(*shok::MainWindowHandle);
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

void debug_lua::Debugger::InitializeLua(lua::State L, bool mainmenu)
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
    if (mainmenu)
        shok::AddGlobalToNotSerialize("LuaDebugger");
}

void debug_lua::Debugger::CheckSourcesLoaded(DebugState& s)
{
    if (s.SourcesLoaded.size() > 2) // modloader, userscript
        return;
    lua::State L{ s.L };
    int t = L.GetTop();
    L.PushGlobalTable();
    std::set<const void*> tablesDone{};
    CheckSourcesLoadedRecursive(s, -1, tablesDone);
    L.SetTop(t);
}
void debug_lua::Debugger::CheckSourcesLoadedRecursive(DebugState& s, int idx, std::set<const void*>& tablesDone)
{
    lua::State L{ s.L };
    if (!L.CheckStack(3))
        return;
    const void* p = L.ToPointer(idx);
    if (tablesDone.find(p) != tablesDone.end())
        return;
    tablesDone.insert(p);
    for (const auto t : L.Pairs(idx)) {
        if (t == lua::LType::Function)
            CheckSourcesLoadedFunc(s, -2);
        if (L.IsFunction(-1))
            CheckSourcesLoadedFunc(s, -1);
        if (t == lua::LType::Table)
            CheckSourcesLoadedRecursive(s, -2, tablesDone);
        if (L.IsTable(-1))
            CheckSourcesLoadedRecursive(s, -1, tablesDone);
    }
}
void debug_lua::Debugger::CheckSourcesLoadedFunc(DebugState& s, int idx)
{
    lua::State L{ s.L };
    if (L.IsCFunction(idx))
        return;
    L.PushValue(idx);
    lua::DebugInfo i = L.Debug_GetInfoForFunc(lua::DebugInfoOptions::Source);
    auto src = i.Source == nullptr ? "" : std::string_view{ i.Source };
    if (std::find_if(s.SourcesLoaded.begin(), s.SourcesLoaded.end(), [src](const Source& s) { return s.Internal == src; }) == s.SourcesLoaded.end()) {
        DoAddSource(s, src);
    }
}

void debug_lua::Debugger::DoAddSource(DebugState& s, std::string_view src)
{
    auto& f = s.SourcesLoaded.emplace_back(std::string(src), TranslateSourceString(s, src));
    if (Handler)
        Handler->OnSourceAdded(s, f.External);
}

void debug_lua::Debugger::Hook(lua::State L, lua::ActivationRecord ar)
{
    L.PushLightUserdata(&Debugger::Hook);
    L.GetTableRaw(L.REGISTRYINDEX);
    auto th = static_cast<Debugger*>(L.ToUserdata(-1));
    L.Pop(1);
    auto& s = th->GetState(L.GetState());

    if (th->Evaluating)
        return;

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
                std::string_view src = dinf.Source;
                auto it2 = std::find_if(it->second.begin(), it->second.end(), [src](BreakpointFile* f) {return src == f->Source.Internal; });
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
        if (th->StepToLevel >= lvl) {
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

    if (!th->Handler)
        return 1;
    if (th->Evaluating)
        return 1;

    BreakSettings tocheck = BreakSettings::PCall;
    if (L.IsLightUserdata(L.Upvalueindex(1))) {
        auto* di = static_cast<lua::DebugInfo*>(L.ToUserdata(L.Upvalueindex(1)));
        if (di->What == std::string_view("C") && di->NameWhat == std::string_view("global") && (di->Name == std::string_view("xpcall") || di->Name == std::string_view("pcall")))
            tocheck = BreakSettings::XPCall;
    }
    if ((tocheck & th->Brk) == BreakSettings::None)
        return 1;

    auto& s = th->GetState(L.GetState());

    th->St = Status::Paused;
    th->Re = Request::Pause;
    
    th->Handler->OnPaused(s, Reason::Exception, L.ToStringView(1));

    th->WaitForRequest();
    th->TranslateRequest(L);
    th->St = Status::Running;
    return 1;
}

void debug_lua::Debugger::SyntaxErrorFunc(lua_State* l, int err)
{
    lua::State L{ l };
    L.PushLightUserdata(&Debugger::Hook);
    L.GetTableRaw(L.REGISTRYINDEX);
    auto th = static_cast<Debugger*>(L.ToUserdata(-1));
    L.Pop(1);

    if (!th->Handler)
        return;
    if (th->Evaluating)
        return;

    auto& s = th->GetState(L.GetState());

    th->St = Status::Paused;
    th->Re = Request::Pause;

    auto msg = std::format("{}: {}", L.ErrorCodeFormat(static_cast<lua::ErrorCode>(err)), L.ToStringView(-1));
    th->Handler->OnPaused(s, Reason::Exception, msg);

    th->WaitForRequest();
    th->TranslateRequest(L);
    th->St = Status::Running;
    return;
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
