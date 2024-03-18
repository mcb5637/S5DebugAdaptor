# S5DebugAdaptor

allows to attach visual studio code studio to settlers hok.

you need 3 things for this to work:
- LuaDebugger.dll needs to be in each of your bin/ directories.  
  if you have a CppLogic LuaDebugger.dll, rename this debugger dll to LuaDebuggerOrig.dll.
- s5luadebug-0.0.1.vsix needs to be installed as an extension into vs code.
- you need to copy the launch.json onto your vsc workspace into a .vscode/ folder.  
  modify the program and args to match the shok installation you want to debug.

then either start shok manually and attach to it or let vsc launch it and attach to it.
