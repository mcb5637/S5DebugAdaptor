# S5DebugAdaptor
##### by [mcb](https://github.com/mcb5637)

Allows to attach visual studio code studio to settlers hok.

you need 3 things for this to work:
- LuaDebugger.dll needs to be in each of your bin/ directories.  
  if you have a CppLogic LuaDebugger.dll, rename this debugger dll to LuaDebuggerOrig.dll.
- s5luadebug-0.0.1.vsix needs to be installed as an extension into vs code.
- you need to copy the launch.json onto your vsc workspace into a .vscode/ folder.  
  modify the program and args to match the shok installation you want to debug.

then either start shok manually and attach to it or let vsc launch it and attach to it.



# VSCode Extension
##### by [schmeling65](https://github.com/schmeling65)



A VSCode extension based on Typescript. Allows to connect with the DAP-Server of the S5DebugAdaptor.




## Building

### Requirements

- NodeJS v22.21.1 used in development
- Visual Studio Code v1.131.0 used in development


### Steps


1. Clone the Repo
2. Go into the `extension` directory
3. Run the command `npm run startpackagepipeline`
5. Output: A vsix-file.



### updating
- make sure npm is installed
- `npm install`
- `npm update`
- check if it still builds



## Installation

Go to the directory your vsix-file is located in.
Use the CLI command of VSCode.
```
code --install-extension S5LuaDebuggerAdaptor-0.0.1.vsix
```