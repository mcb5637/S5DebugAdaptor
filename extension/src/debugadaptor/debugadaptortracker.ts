import * as vscode from "vscode";
import { S5DebugAdapterDescriptorFactory } from "./debugadaptor.ts";

export class S5DebugAdapterDescriptorTrackerFactory implements vscode.DebugAdapterTrackerFactory {
  private parentAdaptorDescriptorFactory: S5DebugAdapterDescriptorFactory;
  constructor(s5DebugAdaptorFactory: S5DebugAdapterDescriptorFactory) {
    this.parentAdaptorDescriptorFactory = s5DebugAdaptorFactory;
  }
  createDebugAdapterTracker(
    session: vscode.DebugSession,
  ): vscode.ProviderResult<vscode.DebugAdapterTracker> {
    let threadExited = false;
    return {
      onWillReceiveMessage: (message) => {
        if (
          this.parentAdaptorDescriptorFactory.isDebuggingActive &&
          session.name === this.parentAdaptorDescriptorFactory.debugConfiguration.name
        ) {
          if (message.event === "terminated") {
            this.parentAdaptorDescriptorFactory.didDebuggerTerminateNormaly = true;
            return;
          }
          if (message.command === "disconnect") {
            this.parentAdaptorDescriptorFactory.didApplicationCrash = true;
            return;
          }
        }
      },
    };
  }
}
