import * as vscode from "vscode";
import { checkProcess } from "./findprocess.ts";
import { S5DebugAdapterDescriptorTrackerFactory } from "./debugadaptortracker.ts";

type timeoutTrigger = NodeJS.Timeout | undefined;

export class S5DebugAdapterDescriptorFactory implements vscode.DebugAdapterDescriptorFactory {
  private searchIntervall: timeoutTrigger = undefined;
  private debugAdaptorTracker: S5DebugAdapterDescriptorTrackerFactory;
  private debugAdaptorTrackerDisposable: vscode.Disposable;

  public debugConfiguration: vscode.DebugConfiguration;
  public isDebuggingActive: boolean = false;

  constructor(extensionContext: vscode.ExtensionContext) {
    this.debugConfiguration = {
      type: "s5lua",
      request: "launch",
      name: "Webview Debug Session",
    };
    this.debugAdaptorTracker = new S5DebugAdapterDescriptorTrackerFactory(this);
    this.debugAdaptorTrackerDisposable = vscode.debug.registerDebugAdapterTrackerFactory('*', this.debugAdaptorTracker)
	  extensionContext.subscriptions.push(this.debugAdaptorTrackerDisposable);
    extensionContext.subscriptions.push(
      vscode.debug.onDidTerminateDebugSession((session) => {
        if (this.isDebuggingActive && session.name === this.debugConfiguration.name) {
          console.log("onDidTerminateDebugSession triggered!")
			    /* Auswertung der Abbruch-Ursache
                if (programmRegulaerBeendet) {
                    console.log('Das Programm lief regulär zu Ende.');
                    meinCallback('REGULAR_EXIT');
                } else {
                    console.log('Das Debugging wurde manuell vom Nutzer abgebrochen (Stopp-Button).');
                    meinCallback('MANUAL_STOP');
                }
          */
        }
      }),
    );
  }

  public async createDebugAdapterDescriptor(
    session: vscode.DebugSession,
    executable: vscode.DebugAdapterExecutable | undefined,
  ): Promise<vscode.ProviderResult<vscode.DebugAdapterDescriptor>> {
    /*
		let conf = session.configuration;
		if (conf.request === "launch" && conf.program) {
			let del = 1;
			if (conf.attachDelay) {
				del = conf.attachDelay;
			}
			spawn(conf.program, conf.args);
			await this.sleep(del * 1000);
		}
		*/

    return new vscode.DebugAdapterServer(19021);
  }

  private setDebuggingStatus(status: boolean) {
	  this.isDebuggingActive = status
  }

  private async startDebugging() {
    console.log("Searching...")
    if (this.isDebuggingActive && (await checkProcess("settlershok.exe"))) {
      const workspaceFolder = vscode.workspace.workspaceFolders?.[0];
      const didStart = await vscode.debug.startDebugging(workspaceFolder, this.debugConfiguration);
      if (didStart) {
        clearInterval(this.searchIntervall);
        this.setDebuggingStatus(true)
        return;
      }
      setTimeout(() => {this.startDebugging()}, 2000);
    }
  }

  public searchForGame() {
    this.searchIntervall = setTimeout(() => {this.startDebugging()}, 2000);
  }

  public stopSearchForGame() {
    clearInterval(this.searchIntervall);
    this.searchIntervall = undefined;
  }
}
