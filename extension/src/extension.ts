import * as vscode from "vscode";
import { MySidebarProvider } from "./webview/webview.ts";

export function activate(context: vscode.ExtensionContext) {
  const provider = new MySidebarProvider(context);
  const registeredWebViewProvider = vscode.window.registerWebviewViewProvider(
    "S5LuaDebuggerAdaptor-toggle-view",
    provider,
  );
  context.subscriptions.push(registeredWebViewProvider);
}

export function deactivate() {}
