import * as path from 'path';
import * as vscode from 'vscode';
import {
    LanguageClient,
    LanguageClientOptions,
    ServerOptions
} from 'vscode-languageclient/node';

let client: LanguageClient | undefined;

export function activate(context: vscode.ExtensionContext) {
    // 1. Configure and start Language Server
    startLanguageServer(context);

    // 2. Register commands
    context.subscriptions.push(
        vscode.commands.registerCommand('vbc.compile', async () => {
            const editor = vscode.window.activeTextEditor;
            if (!editor || editor.document.languageId !== 'vbc') {
                vscode.window.showWarningMessage('Please open a .vbc file to compile.');
                return;
            }
            const filePath = editor.document.fileName;
            const terminal = vscode.window.createTerminal('VBC Build');
            terminal.show();
            terminal.sendText(`vbc compile "${filePath}"`);
        })
    );

    context.subscriptions.push(
        vscode.commands.registerCommand('vbc.run', async () => {
            const editor = vscode.window.activeTextEditor;
            if (!editor || editor.document.languageId !== 'vbc') {
                vscode.window.showWarningMessage('Please open a .vbc file to run.');
                return;
            }
            const filePath = editor.document.fileName;
            const terminal = vscode.window.createTerminal('VBC Run');
            terminal.show();
            terminal.sendText(`vbc run "${filePath}"`);
        })
    );

    context.subscriptions.push(
        vscode.commands.registerCommand('vbc.debug', async () => {
            const editor = vscode.window.activeTextEditor;
            if (!editor || editor.document.languageId !== 'vbc') {
                vscode.window.showWarningMessage('Please open a .vbc file to debug.');
                return;
            }
            const filePath = editor.document.fileName;
            const exePath = filePath.replace(/\.vbc$/i, '.exe');

            // Compile first with debug symbols
            const terminal = vscode.window.createTerminal('VBC Debug Build');
            terminal.sendText(`vbc compile "${filePath}" -o "${exePath}"`);

            // Start debugging session using native debugger
            setTimeout(() => {
                vscode.debug.startDebugging(undefined, {
                    name: 'VBC Debug',
                    type: 'cppvsdbg',
                    request: 'launch',
                    program: exePath,
                    cwd: path.dirname(filePath),
                    stopAtEntry: false
                });
            }, 1000);
        })
    );

    context.subscriptions.push(
        vscode.commands.registerCommand('vbc.restartLsp', async () => {
            if (client) {
                await client.stop();
                client = undefined;
            }
            startLanguageServer(context);
            vscode.window.showInformationMessage('VBC Language Server restarted.');
        })
    );
}

function startLanguageServer(context: vscode.ExtensionContext) {
    // Server options: launch 'vbc lsp' (or dotnet run fallback)
    const serverOptions: ServerOptions = {
        command: 'vbc',
        args: ['lsp']
    };

    const clientOptions: LanguageClientOptions = {
        documentSelector: [{ scheme: 'file', language: 'vbc' }],
        synchronize: {
            fileEvents: vscode.workspace.createFileSystemWatcher('**/*.vbc')
        }
    };

    client = new LanguageClient(
        'vbcLanguageServer',
        'VBC Language Server',
        serverOptions,
        clientOptions
    );

    client.start().catch(err => {
        console.error('Failed to start VBC Language Server:', err);
    });
}

export function deactivate(): Thenable<void> | undefined {
    if (!client) {
        return undefined;
    }
    return client.stop();
}
