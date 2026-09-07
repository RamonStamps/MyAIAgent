const vscode = require('vscode');
const cp = require('child_process');
const path = require('path');
const fs = require('fs');

let agentProc = null;
let panel = null;
let outputBuffer = '';
let lastExePath = null;
let extContext = null; // store extension context to access secrets

function ensureAgent(exePath) {
  if (agentProc) return true;
  // In test environments spawn the test-agent stub if available to validate IPC.
  if (process.env.MYAAGENT_TEST) {
    try {
      const stubPath = path.join(__dirname, 'test', 'agent_stub.js');
      if (fs.existsSync(stubPath)) {
        agentProc = cp.spawn(process.execPath, [stubPath], { stdio: ['pipe', 'pipe', 'pipe'] });
      } else {
        agentProc = { stdin: { write: () => {} }, kill: () => { agentProc = null; } };
      }
    } catch (e) {
      agentProc = { stdin: { write: () => {} }, kill: () => { agentProc = null; } };
    }
    lastExePath = exePath;
    // continue to attach stdout/stderr handlers below
  }
  if (!process.env.MYAAGENT_TEST) {
    try {
      agentProc = cp.spawn(exePath, [], { stdio: ['pipe', 'pipe', 'pipe'] });
    } catch (e) {
      vscode.window.showErrorMessage('Failed to start MyAIAgent: ' + e.message);
      agentProc = null;
      return false;
    }
    lastExePath = exePath;
  }
  lastExePath = exePath;
  // Stream output: support token markers (TOK:) for token-accurate streaming,
  // otherwise fall back to chunked character streaming.
  agentProc.stdout.on('data', (data) => {
    const text = data.toString();
    // accumulate output for tests to inspect
    outputBuffer += text;
    // If agent emits token markers like `TOK:...\n`, forward tokens explicitly
    const lines = text.split(/\r?\n/);
    let handled = false;
    for (let ln of lines) {
      if (ln.startsWith('TOK:')) {
        handled = true;
        const token = ln.slice(4);
        if (panel) panel.webview.postMessage({ kind: 'token', token });
      }
    }
    if (!handled) {
      const chunkSize = 12;
      for (let i = 0; i < text.length; i += chunkSize) {
        const chunk = text.slice(i, i + chunkSize);
        if (panel) panel.webview.postMessage({ kind: 'output', text: chunk, stream: true });
      }
    }
  });
  agentProc.stderr.on('data', (data) => {
    const text = data.toString();
    outputBuffer += text;
    // stderr likely not tokenized; stream in small chunks
    const chunkSize = 12;
    for (let i = 0; i < text.length; i += chunkSize) {
      const chunk = text.slice(i, i + chunkSize);
      if (panel) panel.webview.postMessage({ kind: 'output', text: chunk, stream: true });
    }
  });
  agentProc.on('exit', (code) => {
    if (panel) panel.webview.postMessage({ kind: 'output', text: '\n[MyAIAgent exited with code ' + code + ']\n' });
    agentProc = null;
    // auto-restart attempt
    setTimeout(() => {
      if (!agentProc && lastExePath) ensureAgent(lastExePath);
    }, 1000);
  });
  return true;
}

// programmatic helpers for tests
function registerTestCommands(context) {
  context.subscriptions.push(vscode.commands.registerCommand('myaagent.getOutputBuffer', async () => {
    return outputBuffer;
  }));

  context.subscriptions.push(vscode.commands.registerCommand('myaagent.getWebviewSnapshot', async () => {
    // ask webview to post its DOM snapshot back to the extension via message
    if (!panel) return null;
    return new Promise((resolve) => {
      const handler = (event) => {
        const msg = event;
        if (msg && msg.kind === 'domSnapshot') {
          resolve(msg.html || '');
        }
      };
      // temporary single-use listener via webview message
      const disposable = panel.webview.onDidReceiveMessage((m) => handler(m), null);
      // request snapshot
      panel.webview.postMessage({ command: 'snapshot' });
      // timeout fallback
      setTimeout(() => { disposable.dispose(); resolve(null); }, 3000);
    });
  }));

  context.subscriptions.push(vscode.commands.registerCommand('myaagent.sendToAgent', async (text) => {
    try {
      if (agentProc && agentProc.stdin && typeof agentProc.stdin.write === 'function') {
        agentProc.stdin.write((text || '') + '\n');
        return { ok: true };
      }
      return { ok: false, error: 'no-agent' };
    } catch (e) { return { ok: false, error: e.message }; }
  }));
}

function activate(context) {
  extContext = context;
  let disposable = vscode.commands.registerCommand('myaagent.chat', async function () {
    const config = vscode.workspace.getConfiguration('myaagent');
    const exePath = config.get('executablePath') || path.join(vscode.workspace.rootPath || '.', 'MyAIAgent.exe');

    if (!ensureAgent(exePath)) return;
    // create or reveal panel
    if (panel) {
      panel.reveal();
      return;
    }

    panel = vscode.window.createWebviewPanel('myaagentChat', 'MyAIAgent Chat', vscode.ViewColumn.One, { enableScripts: true });
    panel.webview.html = getWebviewContent();

    panel.webview.onDidReceiveMessage(async message => {
      const config = vscode.workspace.getConfiguration('myaagent');
      const exePath = config.get('executablePath') || path.join(vscode.workspace.rootPath || '.', 'MyAIAgent.exe');
      if (!ensureAgent(exePath)) return;

      if (message.command === 'send') {
        const user = message.text || '';
        agentProc.stdin.write('chat ' + user.replace(/\r?\n/g, ' ') + '\n');
      } else if (message.command === 'stop') {
        if (agentProc) {
          agentProc.kill();
          agentProc = null;
        }
      } else if (message.command === 'compile_current') {
        const editor = vscode.window.activeTextEditor;
        if (editor) {
          const file = editor.document.uri.fsPath;
          agentProc.stdin.write('compile ' + file + '\n');
        } else vscode.window.showInformationMessage('No active editor');
      } else if (message.command === 'complete_current') {
        const editor = vscode.window.activeTextEditor;
        if (editor) {
          // ensure file is saved so the agent can read it
          if (editor.document.isDirty) await editor.document.save();
          const file = editor.document.uri.fsPath;
          const pos = editor.selection.active;
          const line = pos.line + 1; // convert to 1-based
          const col = pos.character + 1;
          agentProc.stdin.write('complete ' + file + ' ' + line + ' ' + col + '\n');
        } else vscode.window.showInformationMessage('No active editor');
      } else if (message.command === 'saveKey') {
        // use SecretStorage for secure key storage
        try {
          const provider = (message.provider || 'default').replace(/[^a-z0-9\-_.]/gi, '_');
          const keyName = 'myaagent.key.' + provider;
          await extContext.secrets.store(keyName, message.key || '');
          // maintain an index of providers in globalState for listing
          const idxKey = 'myaagent.keysIndex';
          let idx = extContext.globalState.get(idxKey, []);
          if (!idx.includes(provider)) { idx.push(provider); await extContext.globalState.update(idxKey, idx); }
          panel.webview.postMessage({ kind: 'output', text: '[Saved key for ' + provider + ']\n' });
        } catch (e) {
          vscode.window.showErrorMessage('Failed to save key: ' + e.message);
        }
      } else if (message.command === 'listKeys') {
        try {
          const idxKey = 'myaagent.keysIndex';
          let idx = extContext.globalState.get(idxKey, []);
          panel.webview.postMessage({ kind: 'output', text: '[keys]\n' + idx.join('\n') + '\n' });
        } catch (e) {
          vscode.window.showErrorMessage('Failed to list keys: ' + e.message);
        }
        } else if (message.command === 'useKey') {
          try {
            const provider = (message.provider || 'default').replace(/[^a-z0-9\-_.]/gi, '_');
            const keyName = 'myaagent.key.' + provider;
            const keyVal = await extContext.secrets.get(keyName);
            if (!keyVal) {
              panel.webview.postMessage({ kind: 'output', text: '[no key stored for ' + provider + ']\n' });
            } else {
              // send key to agent via stdin using setkey command (agent will store it in keys/)
              if (agentProc) agentProc.stdin.write('setkey ' + provider + ' ' + keyVal.replace(/\r?\n/g,' ') + '\n');
              panel.webview.postMessage({ kind: 'output', text: '[Injected key for ' + provider + ' to agent]\n' });
            }
          } catch (e) {
            vscode.window.showErrorMessage('Failed to use key: ' + e.message);
          }
      } else if (message.command === 'run_exe') {
        const exe = message.path || message.text;
        if (exe) agentProc.stdin.write('run ' + exe + '\n');
      } else if (message.command === 'domSnapshot') {
        // messages from webview DOM snapshot
        // forward to tests via outputBuffer as well
        if (message.html) outputBuffer += '\n[DOM_SNAPSHOT]\n' + message.html + '\n[/DOM_SNAPSHOT]\n';
      } else if (message.command === 'listmodels') {
        agentProc.stdin.write('listmodels\n');
      } else if (message.command === 'setmodel') {
        if (message.model) agentProc.stdin.write('setmodel ' + message.model + '\n');
      }
    }, undefined, context.subscriptions);

    panel.onDidDispose(() => { panel = null; }, null, context.subscriptions);
  });

  context.subscriptions.push(disposable);

  // register test helper commands once
  registerTestCommands(context);

  // expose key management commands for tests and external callers
  context.subscriptions.push(vscode.commands.registerCommand('myaagent.saveKey', async (provider, key) => {
    try {
      const prov = (provider || 'default').replace(/[^a-z0-9\-_.]/gi, '_');
      const keyName = 'myaagent.key.' + prov;
      await extContext.secrets.store(keyName, key || '');
      const idxKey = 'myaagent.keysIndex';
      let idx = extContext.globalState.get(idxKey, []);
      if (!idx.includes(prov)) { idx.push(prov); await extContext.globalState.update(idxKey, idx); }
      return { ok: true, provider: prov };
    } catch (e) { return { ok: false, error: e.message }; }
  }));

  context.subscriptions.push(vscode.commands.registerCommand('myaagent.listKeys', async () => {
    try {
      const idxKey = 'myaagent.keysIndex';
      let idx = extContext.globalState.get(idxKey, []);
      return idx;
    } catch (e) { return []; }
  }));

  context.subscriptions.push(vscode.commands.registerCommand('myaagent.useKey', async (provider) => {
    try {
      const prov = (provider || 'default').replace(/[^a-z0-9\-_.]/gi, '_');
      const keyName = 'myaagent.key.' + prov;
      const keyVal = await extContext.secrets.get(keyName);
      if (!keyVal) return { ok: false, error: 'no-key' };
      if (agentProc) agentProc.stdin.write('setkey ' + prov + ' ' + keyVal.replace(/\r?\n/g,' ') + '\n');
      return { ok: true, provider: prov };
    } catch (e) { return { ok: false, error: e.message }; }
  }));
}

function deactivate() {
  if (agentProc) {
    agentProc.kill();
    agentProc = null;
  }
}

function getWebviewContent() {
  return `<!doctype html>
<html>
<head>
<meta charset="utf-8" />
<style>
body{font-family: sans-serif; margin:0; padding:10px}
#out{height:60vh; overflow:auto; border:1px solid #ddd; padding:8px; white-space:pre-wrap}
#input{width:100%; box-sizing:border-box}
.row{margin-top:8px}
.key-area{display:flex; gap:8px; align-items:center}
.key-area input, .key-area textarea{font-family:monospace}
</style>
</head>
<body>
<h3>MyAIAgent Chat</h3>
<div id="out"></div>
<textarea id="input" rows="3" placeholder="Type a message..."></textarea>
<div class="row">
  <button id="send">Send</button>
  <button id="compile">Compile Current File</button>
  <button id="complete">Complete Current File</button>
  <button id="listmodels">List Models</button>
  <input id="modelname" placeholder="model name" style="width:200px" />
  <button id="setmodel">Set Model</button>
  <button id="stop">Stop Agent</button>
</div>
<h4>Key Management</h4>
  <div class="key-area">
  <input id="provider" placeholder="provider (e.g., ollama)" />
  <textarea id="keyval" rows="2" cols="40" placeholder="paste api key here"></textarea>
  <button id="savekey">Save Key</button>
  <button id="listkeys">List Keys</button>
  <button id="usekey">Use Key</button>
</div>
<script>
const vscode = acquireVsCodeApi();
const out = document.getElementById('out');
const input = document.getElementById('input');
const send = document.getElementById('send');
const stop = document.getElementById('stop');
const provider = document.getElementById('provider');
const keyval = document.getElementById('keyval');

// simple streaming queue to animate characters for smoother UX
let streamQueue = [];
let streaming = false;
function enqueueStream(text) {
  for (let ch of text) streamQueue.push(ch);
  if (!streaming) drainStream();
}
function drainStream() {
  streaming = true;
  if (streamQueue.length === 0) { streaming = false; return; }
  out.textContent += streamQueue.shift();
  out.scrollTop = out.scrollHeight;
  // small delay to simulate token/char typing
  setTimeout(drainStream, 8);
}

send.addEventListener('click', () => {
  const text = input.value.trim();
  if (!text) return;
  out.textContent += '\n> ' + text + '\n';
  vscode.postMessage({ command: 'send', text });
  input.value = '';
});
document.getElementById('compile').addEventListener('click', () => { vscode.postMessage({ command: 'compile_current' }); });
document.getElementById('complete').addEventListener('click', () => { vscode.postMessage({ command: 'complete_current' }); });
document.getElementById('listmodels').addEventListener('click', () => { vscode.postMessage({ command: 'listmodels' }); });
document.getElementById('setmodel').addEventListener('click', () => { const m = document.getElementById('modelname').value.trim(); if (m) vscode.postMessage({ command: 'setmodel', model: m }); });
stop.addEventListener('click', () => { vscode.postMessage({ command: 'stop' }); });

document.getElementById('savekey').addEventListener('click', () => {
  const prov = provider.value.trim();
  const key = keyval.value.trim();
  if (!prov || !key) { alert('Provide provider and key'); return; }
  vscode.postMessage({ command: 'saveKey', provider: prov, key });
  provider.value = '';
  keyval.value = '';
});
document.getElementById('listkeys').addEventListener('click', () => { vscode.postMessage({ command: 'listKeys' }); });
document.getElementById('usekey').addEventListener('click', () => {
  const prov = provider.value.trim();
  if (!prov) { alert('Provide provider to use'); return; }
  vscode.postMessage({ command: 'useKey', provider: prov });
});

window.addEventListener('message', event => {
  const msg = event.data;
  if (msg.kind === 'output') {
    if (msg.stream) enqueueStream(msg.text);
    else { out.textContent += msg.text; out.scrollTop = out.scrollHeight; }
  } else if (msg.command === 'snapshot') {
    // respond with DOM snapshot
    try {
      const html = document.documentElement.outerHTML;
      vscode.postMessage({ command: 'domSnapshot', kind: 'domSnapshot', html });
    } catch (e) {
      vscode.postMessage({ command: 'domSnapshot', kind: 'domSnapshot', html: '' });
    }
  }
});
</script>
</body>
</html>`;
}

module.exports = { activate, deactivate };
