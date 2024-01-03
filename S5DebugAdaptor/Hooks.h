#pragma once
#include <initializer_list>
#include <cstdint>
#include <functional>
using byte = uint8_t;
struct lua_State;

namespace debug_lua {
	class Hooks
	{
		static constexpr UINT WM_CHECK_RUN = 0x7002;

		static LRESULT __stdcall WinProcHook(HWND wnd, UINT msg, WPARAM w, LPARAM l);
		static int __cdecl PCallOverride(lua_State* L, int nargs, int nresults, int errfunc);
	public:
		static void InstallHook();

		static std::function<void()> RunCallback;
		static int(*ErrorCallback)(lua_State* L);

		static void SendCheckRun();


		static void RedirectCall(void* call, void* redirect);
		static void WriteJump(void* adr, void* toJump, void* nextvalid);
		static void WriteJump(void* adr, void* toJump, void* nextvalid, byte* backup);
		static void RestoreJumpBackup(void* adr, byte* backup);
		static void WriteNops(void* adr, int num);
		static void WriteNops(void* adr, void* nextvalid);


		// allows read/write/execute of the memory location pointed to until it goes out of scope.
		// using more than one at the same time works as expected, cause the destructors are called in reverse order.
		// use always as stack variable!
		struct SaveVirtualProtect {
			SaveVirtualProtect(void* adr, size_t size);
			~SaveVirtualProtect();
			SaveVirtualProtect(size_t size, std::initializer_list<void*> adrs);
		private:
			void* Adr;
			size_t Size;
			unsigned long Prev;
		};

	};
}
