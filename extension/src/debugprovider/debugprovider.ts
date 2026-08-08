import * as vscode from "vscode";

export class S5DebugProvider implements vscode.DebugConfigurationProvider {
  // Generiert die Standard-Konfiguration für die launch.json
  provideDebugConfigurations(
    folder: vscode.WorkspaceFolder | undefined,
    token?: vscode.CancellationToken,
  ): vscode.ProviderResult<vscode.DebugConfiguration[]> {
    return [
      {
        type: "s5lua",
        name: "S5LuaDebugger",
        request: "attach",
        //program: '${file}'
      },
    ];
  }

  resolveDebugConfiguration(
    folder: vscode.WorkspaceFolder | undefined,
    config: vscode.DebugConfiguration,
    token?: vscode.CancellationToken,
  ): vscode.ProviderResult<vscode.DebugConfiguration> {
    // Falls die Config leer ist (F5 ohne launch.json)
    if (!config.type && !config.request && !config.name) {
      config.type = "s5lua";
      config.name = "Dynamischer Start";
      config.request = "attach";
    }
    return config;
  }
}
