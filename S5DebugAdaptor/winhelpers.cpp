#include "pch.h"
#include "winhelpers.h"
#include <cstdlib>

void debug_lua::ProcessBasicWindowEvents()
{
	MSG msg;
	while (PeekMessageA(&msg, 0, 0, WM_CHARTOITEM, PM_REMOVE)) {
		if (msg.message == WM_QUIT)
			std::exit(0);
		TranslateMessage(&msg);
		DispatchMessageA(&msg);
	}
	while (PeekMessageA(&msg, 0, WM_NCCREATE, WM_NCMBUTTONDBLCLK, PM_REMOVE)) {
		if (msg.message == WM_QUIT)
			std::exit(0);
		TranslateMessage(&msg);
		DispatchMessageA(&msg);
	}
}
