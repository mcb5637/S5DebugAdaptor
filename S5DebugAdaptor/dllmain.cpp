// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include "luapp/luapp50.h"
#include "debugger.h"
#include "adaptor.h"
#include "server.h"

debug_lua::Debugger debugger{};
//std::unique_ptr<debug_lua::Adaptor> adap = nullptr;
std::unique_ptr<debug_lua::Server> serv = nullptr;

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
		//adap = std::make_unique<debug_lua::Adaptor>(debugger);
		serv = std::make_unique<debug_lua::Server>(debugger);
		break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

struct DebuggerOrig {
	bool loaded = false;
	HMODULE debugger = nullptr;
	void(__stdcall* AddLuaState)(lua_State* L) = nullptr;
	void(__stdcall* RemoveLuaState)(lua_State* L) = nullptr;
	void(__stdcall* NewFile)(lua_State* L, const char* filename, const char* filedata, size_t len) = nullptr;
	void(__stdcall* Show)() = nullptr;
	void(__stdcall* Hide)() = nullptr;
	void(__stdcall* Break)(lua_State* L) = nullptr;
	void(__stdcall* ShowExecuteLine)() = nullptr;


	void Load() {
		if (loaded)
			return;
		loaded = true;
		debugger = LoadLibrary("LuaDebuggerOld.dll");
		if (debugger) {
			AddLuaState = reinterpret_cast<void(__stdcall*)(lua_State*)>(GetProcAddress(debugger, "_AddLuaState@4"));
			RemoveLuaState = reinterpret_cast<void(__stdcall*)(lua_State*)>(GetProcAddress(debugger, "_RemoveLuaState@4"));
			NewFile = reinterpret_cast<void(__stdcall*)(lua_State*, const char*, const char*, size_t)>(GetProcAddress(debugger, "_NewFile@16"));
			Show = reinterpret_cast<void(__stdcall*)()>(GetProcAddress(debugger, "_Show@0"));
			Hide = reinterpret_cast<void(__stdcall*)()>(GetProcAddress(debugger, "_Hide@0"));
			Break = reinterpret_cast<void(__stdcall*)(lua_State*)>(GetProcAddress(debugger, "_Break@4"));
			ShowExecuteLine = reinterpret_cast<void(__stdcall*)()>(GetProcAddress(debugger, "_ShowExecuteLine@0"));
		}
	}
};

DebuggerOrig dbg{};

//std::unique_ptr<debug_lua::Server> server = nullptr;

extern "C" {
	void __declspec(dllexport) __stdcall AddLuaState(lua_State* L) {
		dbg.Load();
		if (dbg.AddLuaState)
			dbg.AddLuaState(L);
		debugger.OnStateAdded(L, nullptr);
	}

	void __declspec(dllexport) __stdcall RemoveLuaState(lua_State* L) {
		if (dbg.RemoveLuaState)
			dbg.RemoveLuaState(L);
		debugger.OnStateClosed(L);
		if (debugger.GetStates().empty())
			serv = nullptr;
	}

	void __declspec(dllexport) __stdcall NewFile(lua_State* L, const char* filename, const char* filedata, size_t len) {
		if (dbg.NewFile)
			dbg.NewFile(L, filename, filedata, len);
		if (filename)
			debugger.OnSourceLoaded(L, filename);
	}

	void __declspec(dllexport) __stdcall Show() {
		if (dbg.Show)
			dbg.Show();
	}

	void __declspec(dllexport) __stdcall Hide() {
		if (dbg.Hide)
			dbg.Hide();
	}

	void __declspec(dllexport) __stdcall Break(lua_State* L) {
		if (dbg.Break)
			dbg.Break(L);
		debugger.OnBreak(L);
	}

	void __declspec(dllexport) __stdcall ShowExecuteLine() {
		if (dbg.ShowExecuteLine)
			dbg.ShowExecuteLine();
	}
}

bool __declspec(dllexport) __stdcall HasRealDebugger() {
	return true;
}

bool __declspec(dllexport) __stdcall DebuggerAttached() {
	return debugger.Handler != nullptr;
}

void __declspec(dllexport) __stdcall ShutdownDebugger() {
	debugger.OnShutdown([]() {
		serv = nullptr;
		});
}
