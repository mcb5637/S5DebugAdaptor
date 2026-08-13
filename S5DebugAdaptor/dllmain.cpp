#include "luapp/luapp50.h"
#include "debugger.h"
#include "adaptor.h"
#include "server.h"
#include "framework.h"
#include "shok.h"
#include "winhelpers.h"

static debug_lua::Debugger debugger{};
static std::unique_ptr<debug_lua::Server> serv = nullptr;

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
		serv = std::make_unique<debug_lua::Server>(debugger);
		break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
	default:
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

static DebuggerOrig dbg{};

static int ShutdownDebuggerLua(lua::State L);
extern "C" {
	void __declspec(dllexport) __stdcall AddLuaState(lua_State* L) {
		dbg.Load();
		if (dbg.AddLuaState)
			dbg.AddLuaState(L);
		if (debug_lua::GetEnvVariable("LUADEBUGGER_WAITATTACH").has_value()) {
			while (debugger.Handler == nullptr)
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
		debugger.OnStateAdded(L, nullptr, lua::State::CppToCFunction<ShutdownDebuggerLua>);
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

bool __declspec(dllexport) __stdcall DebuggerSupportsSourceArchive() {
	return true;
}

void __declspec(dllexport) __stdcall SetDisableExecuteLua(bool disable) {
	debugger.DisableExecuteLua = disable;
}

void __declspec(dllexport) __stdcall SetGetSourceFromArchive(BB::CFileSystemMgr::OpenFileStreamWithSourceT f) {
	BB::CFileSystemMgr::OpenFileStreamWithSource = f;
}

int ShutdownDebuggerLua(lua::State L) {
	ShutdownDebugger();
	return 0;
}
