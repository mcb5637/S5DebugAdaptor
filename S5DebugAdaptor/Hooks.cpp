#include "pch.h"
#include "Hooks.h"
#include <stdexcept>
#include "luapp/luapp50.h"
#include "shok.h"

namespace lua_debughook {
	struct DebuggerOverrides {
		int(__cdecl* lua_load_debug)(lua_State* L, void* reader, void* data, const char* chunkname, const char* mode);
		int(__cdecl* lua_pcallk_debug)(lua_State* L, int nargs, int nresults, int errfunc, ptrdiff_t* ctx, void* k);
	};
}

lua_debughook::DebuggerOverrides* Overrides = nullptr;
int(__cdecl* lua_load_real)(lua_State* L, void* reader, void* data, const char* chunkname, const char* mode) = nullptr;
int(__cdecl* lua_pcallk_real)(lua_State* L, int nargs, int nresults, int errfunc, ptrdiff_t* ctx, void* k) = nullptr;

LRESULT __stdcall debug_lua::Hooks::WinProcHook(HWND wnd, UINT msg, WPARAM w, LPARAM l)
{
	if (RunCallback)
		RunCallback();
	return 0;
}
void __declspec(naked) winprocasm() {
	__asm {
		push[ebp + 0x14];
		push[ebp + 0x10];
		push[ebp + 0xC];
		push[ebp + 8];
		call debug_lua::Hooks::WinProcHook;

		mov ecx, [ebp + 0xC];
		cmp ecx, debug_lua::Hooks::WM_CHECK_RUN;
		je skip;

		push 0xFFFFFFEB;
		push[ebp + 8];
		mov eax, [0x761370];
		mov eax, [eax];
		call eax;

		push 0x40745C;
		ret;

	skip:
		push 0x40751B;
		ret;
	}
}

int pcall_jumpback = 0;
int __declspec(naked) __cdecl pcall_recovered(lua_State* L, int nargs, int nresults, int errfunc) {
	__asm {
		mov eax, [esp + 0x10];
		sub esp, 8;

		mov ecx, eax;
		mov edx, eax;

		push pcall_jumpback;
		ret;
	};
}
int DoubleErrorFunc(lua::State L) {
	L.PushValue(L.Upvalueindex(1));
	L.PushValue(L.Upvalueindex(2));
	L.PushValue(1);
	L.PCall(1, 1);
	L.PCall(1, 1);
	return 1;
}
int __cdecl debug_lua::Hooks::PCallOverride(lua_State* l, int nargs, int nresults, int errfunc)
{
	lua::State L{ l };
	int ehsi = 0;
	lua::DebugInfo di{};
	if (ErrorCallback) {
		bool valid = L.Debug_GetStack(0, di, lua::DebugInfoOptions::Name | lua::DebugInfoOptions::Source, false);
		if (errfunc != 0) {
			L.PushValue(errfunc);
			if (valid)
				L.PushLightUserdata(&di);
			else
				L.Push();
			L.Push(ErrorCallback, 1);
			L.Push<DoubleErrorFunc>(2);
		}
		else {
			if (valid)
				L.PushLightUserdata(&di);
			else
				L.Push();
			L.Push(ErrorCallback, 1);
		}
		ehsi = L.ToAbsoluteIndex(-nargs - 2);
		L.Insert(ehsi);
		errfunc = ehsi;
	}
	int r = pcall_recovered(l, nargs, nresults, errfunc);
	if (ErrorCallback) {
		L.Remove(ehsi);
	}
	return r;
}

int load_jumpback = 0;
int __declspec(naked) __cdecl load_recovered(lua_State* L, void* reader, void* data, const char* chunkname) {
	__asm {
		mov eax, [esp + 0x10];
		sub esp, 0x14;

		mov ecx, eax;
		mov edx, eax;

		push load_jumpback;
		ret;
	};
}
int __cdecl debug_lua::Hooks::LoadOverride(lua_State* L, void* reader, void* data, const char* chunkname)
{
	int r = load_recovered(L, reader, data, chunkname);
	if (r != static_cast<int>(lua::ErrorCode::Success) && SyntaxCallback)
		SyntaxCallback(L, r);
	return r;
}

int __cdecl debug_lua::Hooks::LoadOverride_Dbg(lua_State* L, void* reader, void* data, const char* chunkname, const char* mode)
{
	int r = lua_load_real(L, reader, data, chunkname, mode);
	if (r != static_cast<int>(lua::ErrorCode::Success) && SyntaxCallback)
		SyntaxCallback(L, r);
	return r;
}

int __cdecl debug_lua::Hooks::PCallOverride_Dbg(lua_State* l, int nargs, int nresults, int errfunc, ptrdiff_t* ctx, void* k)
{
	lua::State L{ l };
	int ehsi = 0;
	lua::DebugInfo di{};
	if (ErrorCallback) {
		bool valid = L.Debug_GetStack(0, di, lua::DebugInfoOptions::Name | lua::DebugInfoOptions::Source, false);
		if (errfunc != 0) {
			L.PushValue(errfunc);
			if (valid)
				L.PushLightUserdata(&di);
			else
				L.Push();
			L.Push(ErrorCallback, 1);
			L.Push<DoubleErrorFunc>(2);
		}
		else {
			if (valid)
				L.PushLightUserdata(&di);
			else
				L.Push();
			L.Push(ErrorCallback, 1);
		}
		ehsi = L.ToAbsoluteIndex(-nargs - 2);
		L.Insert(ehsi);
		errfunc = ehsi;
	}
	int r = lua_pcallk_real(l, nargs, nresults, errfunc, ctx, k);
	if (ErrorCallback) {
		L.Remove(ehsi);
	}
	return r;
}

std::function<void()> debug_lua::Hooks::RunCallback{};
int(*debug_lua::Hooks::ErrorCallback)(lua_State* L) = nullptr;
void(*debug_lua::Hooks::SyntaxCallback)(lua_State* L, int err) = nullptr;
bool Hooked = false;
void debug_lua::Hooks::InstallHook()
{
	if (Hooked)
		return;
	Hooked = true;
	SaveVirtualProtect vp{ reinterpret_cast<void*>(0x407451), 0x40745C - 0x407451 };
	WriteJump(reinterpret_cast<void*>(0x407451), &winprocasm, reinterpret_cast<void*>(0x40745C));
	
	auto handle = GetModuleHandle("S5Lua5");
	if (!handle)
		return;

	auto getover = reinterpret_cast<lua_debughook::DebuggerOverrides&(*)()>(GetProcAddress(handle, "?GetOverrides@lua_debughook@@YAAAUDebuggerOverrides@1@XZ"));
	if (getover != nullptr) {
		Overrides = &getover();
		lua_pcallk_real = reinterpret_cast<int(__cdecl*)(lua_State * L, int nargs, int nresults, int errfunc, ptrdiff_t * ctx, void* k)>(GetProcAddress(handle, "?lua_pcallk_real@lua_debughook@@YAHPAUlua_State@@HHHHP6AH0HH@Z@Z"));
		lua_load_real = reinterpret_cast<int(__cdecl *)(lua_State * L, void* reader, void* data, const char* chunkname, const char* mode)>(GetProcAddress(handle, "?lua_load_real@lua_debughook@@YAHPAUlua_State@@P6APBD0PAXPAI@Z1PBD4@Z"));

		Overrides->lua_load_debug = &LoadOverride_Dbg;
		Overrides->lua_pcallk_debug = &PCallOverride_Dbg;

		return;
	}

	int pcall = reinterpret_cast<int>(GetProcAddress(handle, "lua_pcall"));
	if (*reinterpret_cast<uint32_t*>(pcall) != 0x1024448B) {
		MessageBoxA(0, "lua_pcall code mismatch, exception breakpoints will not work!", nullptr, 0);
		return;
	}
	pcall_jumpback = pcall + 7;

	SaveVirtualProtect vp2{ reinterpret_cast<void*>(pcall), static_cast<size_t>(pcall_jumpback - pcall) };
	WriteJump(reinterpret_cast<void*>(pcall), &PCallOverride, reinterpret_cast<void*>(pcall_jumpback));

	int load = reinterpret_cast<int>(GetProcAddress(handle, "lua_load"));
	if (*reinterpret_cast<uint32_t*>(load) != 0x1024448B) {
		MessageBoxA(0, "lua_load code mismatch, syntax breakpoints will not work!", nullptr, 0);
		return;
	}
	load_jumpback = load + 7;

	SaveVirtualProtect vp3{ reinterpret_cast<void*>(load), static_cast<size_t>(load_jumpback - load) };
	WriteJump(reinterpret_cast<void*>(load), &LoadOverride, reinterpret_cast<void*>(load_jumpback));
}

void debug_lua::Hooks::SendCheckRun()
{
	PostMessage(*reinterpret_cast<HWND*>(0x84ECC4), WM_CHECK_RUN, 0, 0);
}


debug_lua::Hooks::SaveVirtualProtect::SaveVirtualProtect(void* adr, size_t size)
{
	Adr = adr;
	Size = size;
	Prev = PAGE_EXECUTE_READWRITE;
	VirtualProtect(adr, size, PAGE_EXECUTE_READWRITE, &Prev);
}
debug_lua::Hooks::SaveVirtualProtect::SaveVirtualProtect(size_t size, std::initializer_list<void*> adrs) : SaveVirtualProtect(std::min<void*>(adrs), reinterpret_cast<int>(std::max<void*>(adrs)) - reinterpret_cast<int>(std::min<void*>(adrs)) + size)
{
}
debug_lua::Hooks::SaveVirtualProtect::~SaveVirtualProtect()
{
	VirtualProtect(Adr, Size, Prev, &Prev);
}

void debug_lua::Hooks::RedirectCall(void* call, void* redirect) {
	byte* opcode = static_cast<byte*>(call);
	if (*opcode == 0xFF && opcode[1] == 0x15) { // call by address
		*opcode = 0xE8; // call
		opcode[5] = 0x90; // nop
	}
	if (*opcode != 0xE8) { // call
		throw std::logic_error{ "trying to redirect something thats not a call" };
	}
	int* adr = reinterpret_cast<int*>(opcode + 1);
	*adr = reinterpret_cast<int>(redirect) - reinterpret_cast<int>(adr + 1); // address relative to next instruction
}
void debug_lua::Hooks::WriteJump(void* adr, void* toJump, void* nextvalid)
{
	if (reinterpret_cast<int>(nextvalid) - reinterpret_cast<int>(adr) < 5)
		throw std::logic_error{ "not enough space for jump" };
	byte* opcode = reinterpret_cast<byte*>(adr);
	*opcode = 0xE9; // jmp
	int* a = reinterpret_cast<int*>(opcode + 1);
	*a = reinterpret_cast<int>(toJump) - reinterpret_cast<int>(a + 1); // address relative to next instruction
	WriteNops(a + 1, nextvalid); // fill the rest with nops
}
void debug_lua::Hooks::WriteJump(void* adr, void* toJump, void* nextvalid, byte* backup)
{
	if (reinterpret_cast<int>(nextvalid) - reinterpret_cast<int>(adr) < 5)
		throw std::logic_error{ "not enough space for jump" };
	for (byte* a = static_cast<byte*>(adr); a < static_cast<byte*>(nextvalid); ++a) {
		*backup = *a;
		++backup;
	}
	WriteJump(adr, toJump, nextvalid);
}
void debug_lua::Hooks::RestoreJumpBackup(void* adr, byte* backup)
{
	byte* a = static_cast<byte*>(adr);
	for (int i = 0; i < 5; ++i) { // the jump
		*a = *backup;
		++a;
		++backup;
	}
	while (*a == 0x90) { // the nop padding
		*a = *backup;
		++a;
		++backup;
	}
}

void debug_lua::Hooks::WriteNops(void* adr, int num)
{
	byte* a = reinterpret_cast<byte*>(adr);
	for (int i = 0; i < num; ++i)
		a[i] = 0x90;
}
void debug_lua::Hooks::WriteNops(void* adr, void* nextvalid)
{
	for (byte* a = static_cast<byte*>(adr); a < static_cast<byte*>(nextvalid); ++a)
		*a = 0x90;
}
