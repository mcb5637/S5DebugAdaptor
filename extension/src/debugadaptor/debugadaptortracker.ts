import * as vscode from 'vscode';
import { S5DebugAdapterDescriptorFactory } from './debugadaptor.ts';

export class S5DebugAdapterDescriptorTrackerFactory implements vscode.DebugAdapterTrackerFactory {
    private parentAdaptorDescriptorFactory: S5DebugAdapterDescriptorFactory
    constructor(s5DebugAdaptorFactory: S5DebugAdapterDescriptorFactory) {
        this.parentAdaptorDescriptorFactory = s5DebugAdaptorFactory
    }
    createDebugAdapterTracker(session: vscode.DebugSession): vscode.ProviderResult<vscode.DebugAdapterTracker> {
        let threadExited = false;
                return {
                    onDidSendMessage: (message) => {
                        // Prüfen, ob es sich um unsere Session handelt
                        if (this.parentAdaptorDescriptorFactory.isDebuggingActive && session.name === this.parentAdaptorDescriptorFactory.debugConfiguration.name) {
                            console.log("OnDidSendMessage of Tracker triggered!")
                            console.log(message.body)
                            return
                            /*
                            // Debugger meldet: Ein einzelner Thread wurde beendet
                            if (message.body?.reason === 'exited' && message.body?.threadId) {
                                threadExited = true;
                                return;
                            }

                            // Debugger meldet: Das gesamte Programm ist beendet (Exited-Event)
                            if (message.event === 'exited') {
                                // Wenn kein Thread-Exit vorausging, lief der Prozess regulär zu Ende
                                if (!threadExited) {
                                    programmRegulaerBeendet = true;
                                }
                            }
                                */
                        }
                    }
                }
    }
}