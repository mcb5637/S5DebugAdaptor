import * as vscode from "vscode";
import { S5DebugAdapterDescriptorFactory } from "../debugadaptor/debugadaptor.ts";

interface messageContent {
  type: string;
  value: boolean;
}

export class MySidebarProvider implements vscode.WebviewViewProvider {
  private persistentStorageKeyName = "IsS5LuaDebuggerActive";
  private registerdDebugAdapterDescriptorFactory: vscode.Disposable;
  private S5DebugAdapterDescriptorFactoryDisposable: S5DebugAdapterDescriptorFactory;

  public isCheckboxChecked = false;

  constructor(private readonly extensionContext: vscode.ExtensionContext) {
    this.isCheckboxChecked =
      this.extensionContext.globalState.get(this.persistentStorageKeyName) || false;
    this.S5DebugAdapterDescriptorFactoryDisposable = new S5DebugAdapterDescriptorFactory(
      extensionContext,
      this,
    );
    this.registerdDebugAdapterDescriptorFactory =
      vscode.debug.registerDebugAdapterDescriptorFactory(
        "s5lua",
        this.S5DebugAdapterDescriptorFactoryDisposable,
      );
    this.extensionContext.subscriptions.push(this.registerdDebugAdapterDescriptorFactory);
    this.activateOrStopGameSearch(this.isCheckboxChecked);
  }

  public async resolveWebviewView(
    webviewView: vscode.WebviewView,
    context: vscode.WebviewViewResolveContext,
    token: vscode.CancellationToken,
  ) {
    webviewView.webview.options = {
      enableScripts: true,
      localResourceRoots: [this.extensionContext.extensionUri],
    };
    webviewView.webview.html = await this.getHtmlForWebview(webviewView.webview);
    if (this.isCheckboxChecked) {
      webviewView.webview.postMessage({
        command: "setCheckboxToTrue",
        value: this.isCheckboxChecked,
      });
      //this.activateOrStopGameSearch(this.isCheckboxChecked);
    }
    webviewView.webview.onDidReceiveMessage(async (message: messageContent) => {
      switch (message.type) {
        case "toggleChanged": {
          await this.extensionContext.globalState.update(
            this.persistentStorageKeyName,
            message.value,
          );
          this.isCheckboxChecked = message.value;
          this.activateOrStopGameSearch(message.value);
          break;
        }
      }
    });
    webviewView.onDidChangeVisibility(() => {
      if (webviewView.visible) {
        webviewView.webview.postMessage({
          command: "setCheckboxToTrue",
          value: this.isCheckboxChecked,
        });
      }
    });
  }

  public activateOrStopGameSearch(status: boolean) {
    if (this.S5DebugAdapterDescriptorFactoryDisposable.isDebuggingActive) {
      return;
    }
    if (status === true) {
      this.S5DebugAdapterDescriptorFactoryDisposable.searchForGame();
    } else if (status === false) {
      this.S5DebugAdapterDescriptorFactoryDisposable.stopSearchForGame();
    }
  }

  private async getHtmlForWebview(webview: vscode.Webview) {
    const extensionUri = this.extensionContext.extensionUri;
    const rawdata = await vscode.workspace.fs.readFile(
      vscode.Uri.joinPath(extensionUri, "out", "htmlwebview", "index.html"),
    );
    const htmlstring = new TextDecoder("utf-8").decode(rawdata);
    return htmlstring;
  }
}
