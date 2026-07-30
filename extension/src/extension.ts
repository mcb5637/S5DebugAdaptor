import * as vscode from "vscode";
import { MySidebarProvider } from "./webview/webview.ts";
import { S5DebugProvider } from "./debugprovider/debugprovider.ts";

export function activate(context: vscode.ExtensionContext) {
  const provider = new MySidebarProvider(context);
  const registeredWebViewProvider = vscode.window.registerWebviewViewProvider(
    "S5LuaDebuggerAdaptor-toggle-view",
    provider,
  );
  context.subscriptions.push(registeredWebViewProvider);
  const debugProvider = new S5DebugProvider();
  context.subscriptions.push(
    vscode.debug.registerDebugConfigurationProvider("s5lua", debugProvider),
  );
}

export function deactivate() {}
