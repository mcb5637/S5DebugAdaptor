// The module 'vscode' contains the VS Code extensibility API
// Import the module and reference it with the alias vscode in your code below
import { spawn } from 'child_process';
import * as vscode from 'vscode';

// This method is called when your extension is activated
// Your extension is activated the very first time the command is executed
export function activate(context: vscode.ExtensionContext) {

	context.subscriptions.push(vscode.debug.registerDebugAdapterDescriptorFactory('s5lua', new S5DebugAdapterDescriptorFactory()));
}

// This method is called when your extension is deactivated
export function deactivate() { }

class S5DebugAdapterDescriptorFactory implements vscode.DebugAdapterDescriptorFactory {
	async createDebugAdapterDescriptor(session: vscode.DebugSession, executable: vscode.DebugAdapterExecutable | undefined): Promise<vscode.ProviderResult<vscode.DebugAdapterDescriptor>> {
		let conf = session.configuration;
		if (conf.request === "launch" && conf.program) {
			let del = 1;
			if (conf.attachDelay) {
				del = conf.attachDelay;
			}
			spawn(conf.program, conf.args);
			await this.sleep(del * 1000);
		}
		return new vscode.DebugAdapterServer(19021);
	}
	sleep(ms: number) {
		return new Promise(resolve => setTimeout(resolve, ms));
	}
}
