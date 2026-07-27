import * as vscode from "vscode";
import { checkProcess } from "./findprocess.ts";
import { S5DebugAdapterDescriptorTrackerFactory } from "./debugadaptortracker.ts";
import { MySidebarProvider } from "../webview/webview.ts";

type timeoutTrigger = NodeJS.Timeout | undefined;

export class S5DebugAdapterDescriptorFactory implements vscode.DebugAdapterDescriptorFactory {
  private parentWebview: MySidebarProvider;
  private searchIntervall: timeoutTrigger = undefined;
  private debugAdaptorTracker: S5DebugAdapterDescriptorTrackerFactory;
  private debugAdaptorTrackerDisposable: vscode.Disposable;
  private customDisconnect: vscode.Disposable;
  private didClickOnDisconnectInVSCode: boolean = false;

  public debugConfiguration: vscode.DebugConfiguration;
  public isDebuggingActive: boolean = false;
  public didDebuggerTerminateNormaly: boolean = false;
  public didApplicationCrash: boolean = false;

  constructor(extensionContext: vscode.ExtensionContext, parent: MySidebarProvider) {
    this.debugConfiguration = {
      type: "s5lua",
      request: "attach",
      name: "Webview Debug Session",
    };

    this.parentWebview = parent;
    this.debugAdaptorTracker = new S5DebugAdapterDescriptorTrackerFactory(this);
    this.debugAdaptorTrackerDisposable = vscode.debug.registerDebugAdapterTrackerFactory(
      "*",
      this.debugAdaptorTracker,
    );
    extensionContext.subscriptions.push(this.debugAdaptorTrackerDisposable);
    extensionContext.subscriptions.push(
      vscode.debug.onDidTerminateDebugSession((session) => {
        if (this.isDebuggingActive && session.name === this.debugConfiguration.name) {
          this.setDebuggingStatus(false);
          if (this.didClickOnDisconnectInVSCode) {
          } else if (this.didDebuggerTerminateNormaly) {
            this.QueuenextGameSearchIfAutoQueueOn();
          } else if (this.didApplicationCrash) {
            this.QueuenextGameSearchIfAutoQueueOn();
          } else {
            throw new Error(
              "Es ist nicht gecrasht, kein normaler Abbruch, und Debugger wurde nicht disconnectet?",
            );
          }
          this.resetUpExitVariables();
          return;
        }
      }),
    );
    this.customDisconnect = vscode.commands.registerCommand(
      "workbench.action.debug.disconnect",
      async () => {
        const activeSession = vscode.debug.activeDebugSession;
        if (activeSession && activeSession.type === "s5lua") {
          this.didClickOnDisconnectInVSCode = true;
          await vscode.debug.stopDebugging(activeSession);
        } else {
          if (activeSession) {
            await vscode.debug.stopDebugging(activeSession);
          }
        }
      },
    );
    extensionContext.subscriptions.push(this.customDisconnect);
  }

  public async createDebugAdapterDescriptor(
    session: vscode.DebugSession,
    executable: vscode.DebugAdapterExecutable | undefined,
  ): Promise<vscode.ProviderResult<vscode.DebugAdapterDescriptor>> {
    return new vscode.DebugAdapterServer(19021);
  }

  private resetUpExitVariables() {
    this.didApplicationCrash = false;
    this.didDebuggerTerminateNormaly = false;
    this.didClickOnDisconnectInVSCode = false;
  }

  private setDebuggingStatus(status: boolean) {
    this.isDebuggingActive = status;
  }

  private async startDebugging() {
    console.log("Searching...");
    if (!this.isDebuggingActive && (await checkProcess("settlershok.exe"))) {
      const workspaceFolder = vscode.workspace.workspaceFolders?.[0];
      const didStart = await vscode.debug.startDebugging(workspaceFolder, this.debugConfiguration);
      if (didStart) {
        clearInterval(this.searchIntervall);
        this.setDebuggingStatus(true);
        return;
      }
    }
    setTimeout(() => {
      this.startDebugging();
    }, 2000);
  }

  public searchForGame() {
    this.searchIntervall = setTimeout(() => {
      this.startDebugging();
    }, 2000);
  }

  public stopSearchForGame() {
    clearInterval(this.searchIntervall);
    this.searchIntervall = undefined;
  }

  private QueuenextGameSearchIfAutoQueueOn() {
    this.parentWebview.activateOrStopGameSearch(this.parentWebview.isCheckboxChecked);
  }
}
