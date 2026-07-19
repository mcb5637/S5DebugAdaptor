cmake_minimum_required(VERSION 4.0)
project(S5DebugAdaptor)

enable_language(CXX ASM)

set(CMAKE_CXX_STANDARD 20)

find_package(uni-algo CONFIG REQUIRED)
find_package(cppdap REQUIRED)

add_compile_options(-Wno-invalid-offsetof -Wno-microsoft-cast)

add_library(LuaDebugger SHARED
        S5DebugAdaptor/adaptor.cpp
        S5DebugAdaptor/debugger.cpp
        S5DebugAdaptor/dllmain.cpp
        S5DebugAdaptor/Hooks.cpp
        S5DebugAdaptor/server.cpp
        S5DebugAdaptor/shok.cpp
        S5DebugAdaptor/utility.cpp
        S5DebugAdaptor/winhelpers.cpp
        S5DebugAdaptor/luapp/src50/luapp50_d.cpp
        S5DebugAdaptor/Resource.rc
)
target_include_directories(LuaDebugger
        PRIVATE
        S5DebugAdaptor/luapp/include
        S5DebugAdaptor/luapp/lua/lua50
)
target_link_libraries(LuaDebugger PRIVATE uni-algo::uni-algo cppdap::cppdap "${CMAKE_CURRENT_SOURCE_DIR}/S5DebugAdaptor/luapp/lua/lua50/S5Lua5.lib")
