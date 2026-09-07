# MyAIAgent VS Code Integration

This minimal VS Code extension launches the local `MyAIAgent` executable and provides a simple chat webview to send `chat` commands and receive output.

Installation (developer):
1. Open this folder in VS Code (`vscode-extension`).
2. Run `npm install` to install dev dependencies (if you plan to publish).
3. Press F5 to run the extension in the Extension Development Host.

Configuration:
- `myaagent.executablePath` — Path to the `MyAIAgent` executable. Defaults to `${workspaceFolder}/MyAIAgent.exe`.

Usage:
- Run the command **MyAIAgent: Open Chat** from the Command Palette.
- Type messages and press Send to forward them to `MyAIAgent` as `chat <message>`.

Extra actions available in the chat view:
- **Compile Current File** — sends `compile <current-file>` to the agent.
- **Complete Current File** — sends `complete <current-file> 1 1` (cursor integration coming soon).
- **List Models** — sends `listmodels` to the agent and shows installed Ollama models.
- **Set Model** — set an Ollama model by name using the text box + button.

Developer notes:
- To test locally, open this folder in VS Code and press F5 to run the Extension Development Host.
- Ensure `myaagent.executablePath` points to the `MyAIAgent.exe` built in the workspace.

Notes:
- This is a minimal integration. For production use, add authentication, robust parsing, process supervision, and streaming handling.
