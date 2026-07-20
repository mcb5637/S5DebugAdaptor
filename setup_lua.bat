
set link="https://sourceforge.net/projects/luabinaries/files/5.0.3/Windows%%20Libraries/lua5_0_3_Win32_dll8_lib.zip/download"
set out="S5DebugAdaptor\luapp\lua\lua50"
set temparch="lua.zip"
if exist "%out%\" (
    echo lua50 already exists
) else (
    curl -o %temparch% -L %link%
    mkdir %out%
    7z x %temparch% -o%out%
    del %temparch%
    move /Y %out%\include\* %out%
    del %out%\include\
    move /Y \s5_lua\S5Lua5.lib %out%\S5Lua5.lib
)
