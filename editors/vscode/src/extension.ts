// aimee VS Code extension — Tier 3 (phases 1-5).
//
// Consumes aimee-server's shipped `/v1` surface (no new server protocol):
//  - a stateful `@aimee` chat participant (streams `/v1/responses`, threads
//    previous_response_id so session memory persists);
//  - a server-health status-bar item (`/v1/health`);
//  - command-palette queries (`/v1/memory/recall`, `/v1/kb/search`);
//  - a Language Model Chat Provider registering "aimee" in the native model
//    picker (streams `/v1/chat/completions`);
//  - a docked chat webview panel (aimee: Open chat panel), a thin renderer over
//    the same streamResponses path.

import * as vscode from 'vscode';

function config() {
  const c = vscode.workspace.getConfiguration('aimee');
  // Trim a trailing slash so `${base}/<path>` never doubles up.
  const apiBase = (c.get<string>('apiBase') || 'http://127.0.0.1:8910/v1').replace(/\/+$/, '');
  return {
    apiBase,
    bearerToken: c.get<string>('bearerToken') || '',
    model: c.get<string>('model') || 'aimee',
  };
}

function authHeaders(bearerToken: string): Record<string, string> {
  const h: Record<string, string> = { 'Content-Type': 'application/json' };
  if (bearerToken) {
    h['Authorization'] = `Bearer ${bearerToken}`;
  }
  return h;
}

// Stream the stateful Responses API (`POST /v1/responses`, stream:true) into the
// chat response. `previousResponseId` continues the same server-side aimee
// session so memory persists across @aimee turns (vs. re-sending history).
// Returns the new response id — thread it into the next turn via
// ChatResult.metadata — or undefined on error/cancel.
async function streamResponses(
  prompt: string,
  previousResponseId: string | undefined,
  emit: (text: string) => void,
  token: vscode.CancellationToken,
): Promise<string | undefined> {
  const { apiBase, bearerToken, model } = config();

  const controller = new AbortController();
  const cancel = token.onCancellationRequested(() => controller.abort());

  const body: Record<string, unknown> = { model, input: prompt, stream: true };
  if (previousResponseId) {
    body.previous_response_id = previousResponseId;
  }

  let resp: Response;
  try {
    resp = await fetch(`${apiBase}/responses`, {
      method: 'POST',
      headers: authHeaders(bearerToken),
      body: JSON.stringify(body),
      signal: controller.signal,
    });
  } catch {
    cancel.dispose();
    emit(
      `\n\n**aimee:** could not reach the server at \`${apiBase}\`. Start the loopback ` +
        'listener (`aimee api status`) and set `aimee.apiBase` / `aimee.bearerToken`.\n',
    );
    return undefined;
  }

  if (!resp.ok) {
    cancel.dispose();
    const detail = await resp.text().catch(() => '');
    const hint =
      resp.status === 401 || resp.status === 403
        ? ' Check `aimee.bearerToken` (a `project:`-scoped token still allows chat).'
        : '';
    emit(
      `\n\n**aimee:** server returned ${resp.status}.${hint}${detail ? '\n```\n' + detail + '\n```\n' : '\n'}`,
    );
    return undefined;
  }

  if (!resp.body) {
    cancel.dispose();
    emit('\n\n**aimee:** empty response body from the server.\n');
    return undefined;
  }

  const reader = resp.body.getReader();
  const decoder = new TextDecoder();
  let buffer = '';
  let responseId: string | undefined;
  try {
    for (;;) {
      const { done, value } = await reader.read();
      if (done) {
        break;
      }
      buffer += decoder.decode(value, { stream: true });
      // SSE events are newline-delimited `data: <json>` lines (Responses API:
      // response.created, response.output_text.delta…, response.completed).
      let nl: number;
      while ((nl = buffer.indexOf('\n')) >= 0) {
        const line = buffer.slice(0, nl).trim();
        buffer = buffer.slice(nl + 1);
        if (!line.startsWith('data:')) {
          continue;
        }
        const payload = line.slice('data:'.length).trim();
        if (payload === '[DONE]') {
          return responseId;
        }
        try {
          const evt = JSON.parse(payload);
          if (evt?.type === 'response.output_text.delta' && typeof evt.delta === 'string') {
            emit(evt.delta);
          } else if (typeof evt?.response?.id === 'string') {
            responseId = evt.response.id; // response.created / response.completed
          } else if (evt?.object === 'response' && typeof evt?.id === 'string') {
            responseId = evt.id;
          }
        } catch {
          // Ignore keep-alive comments / partial frames; the next chunk completes them.
        }
      }
    }
  } finally {
    cancel.dispose();
    reader.releaseLock();
  }
  return responseId;
}

// The previous_response_id we stored on the most recent @aimee response turn,
// so a follow-up continues the same server-side session.
function previousResponseId(context: vscode.ChatContext): string | undefined {
  for (let i = context.history.length - 1; i >= 0; i--) {
    const turn = context.history[i];
    if (turn instanceof vscode.ChatResponseTurn) {
      const id = (turn.result?.metadata as { aimeeResponseId?: unknown } | undefined)?.aimeeResponseId;
      return typeof id === 'string' ? id : undefined;
    }
  }
  return undefined;
}

// Unary POST to a /v1 path. Returns the parsed JSON, or throws an Error whose
// message is suitable for showErrorMessage (unreachable / non-2xx / bad JSON).
async function postV1(path: string, body: unknown): Promise<any> {
  const { apiBase, bearerToken } = config();
  let resp: Response;
  try {
    resp = await fetch(`${apiBase}${path}`, {
      method: 'POST',
      headers: authHeaders(bearerToken),
      body: JSON.stringify(body),
    });
  } catch {
    throw new Error(`could not reach aimee-server at ${apiBase} (run \`aimee api status\`)`);
  }
  const text = await resp.text();
  if (!resp.ok) {
    const hint = resp.status === 401 || resp.status === 403 ? ' — check aimee.bearerToken' : '';
    throw new Error(`server returned ${resp.status}${hint}: ${text.slice(0, 400)}`);
  }
  try {
    return JSON.parse(text);
  } catch {
    throw new Error('server returned a non-JSON response');
  }
}

// Run a /v1 query command: prompt for input, POST it, and show the JSON envelope
// in the shared output channel. Kept generic so memory recall and KB search
// share one code path (their response shapes differ; raw-but-pretty avoids
// guessing nested fields that may evolve).
async function runQueryCommand(
  out: vscode.OutputChannel,
  opts: { promptTitle: string; placeHolder: string; path: string; bodyFor: (q: string) => unknown },
): Promise<void> {
  const query = await vscode.window.showInputBox({
    title: opts.promptTitle,
    placeHolder: opts.placeHolder,
    ignoreFocusOut: true,
  });
  if (!query) {
    return;
  }
  try {
    const result = await postV1(opts.path, opts.bodyFor(query));
    out.clear();
    out.appendLine(`# ${opts.promptTitle}`);
    out.appendLine(`query: ${query}`);
    out.appendLine('');
    out.appendLine(JSON.stringify(result, null, 2));
    out.show(true);
  } catch (err) {
    void vscode.window.showErrorMessage(`aimee: ${(err as Error).message}`);
  }
}

async function fetchHealth(): Promise<string | null> {
  const { apiBase, bearerToken } = config();
  try {
    const resp = await fetch(`${apiBase}/health`, { headers: authHeaders(bearerToken) });
    if (!resp.ok) {
      return null;
    }
    const obj: any = await resp.json();
    return typeof obj?.status === 'string' ? obj.status : 'ok';
  } catch {
    return null;
  }
}

function makeStatusBar(context: vscode.ExtensionContext): { refresh: () => void } {
  const item = vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Right, 100);
  item.command = 'aimee.showServerStatus';
  context.subscriptions.push(item);

  const refresh = () => {
    void fetchHealth().then((status) => {
      if (status) {
        item.text = '$(sparkle) aimee';
        item.tooltip = `aimee-server: ${status} (${config().apiBase})`;
      } else {
        item.text = '$(sparkle) aimee: offline';
        item.tooltip = `aimee-server unreachable at ${config().apiBase}. Run \`aimee api status\`.`;
      }
      item.show();
    });
  };

  refresh();
  const timer = setInterval(refresh, 30_000);
  context.subscriptions.push({ dispose: () => clearInterval(timer) });
  return { refresh };
}

// ── Language Model Chat Provider (Tier 3, phase 4) ──────────────────────────
// Registers "aimee" in VS Code's native model picker so it can be selected as
// the model for any chat (not just @aimee). Stateless: VS Code passes the full
// message history each call, so this maps to /v1/chat/completions (vs. the
// stateful /v1/responses the @aimee participant uses).

const AIMEE_MODEL_INFO: vscode.LanguageModelChatInformation = {
  id: 'aimee',
  name: 'aimee',
  family: 'aimee',
  version: '1',
  maxInputTokens: 128000,
  maxOutputTokens: 8192,
  capabilities: { imageInput: false, toolCalling: false },
};

function requestMessageText(msg: vscode.LanguageModelChatRequestMessage): string {
  let text = '';
  for (const part of msg.content) {
    if (part instanceof vscode.LanguageModelTextPart) {
      text += part.value;
    }
  }
  return text;
}

function toOpenAIMessages(
  messages: readonly vscode.LanguageModelChatRequestMessage[],
): Array<{ role: string; content: string }> {
  return messages.map((m) => ({
    role: m.role === vscode.LanguageModelChatMessageRole.Assistant ? 'assistant' : 'user',
    content: requestMessageText(m),
  }));
}

const aimeeChatModelProvider: vscode.LanguageModelChatProvider = {
  async provideLanguageModelChatInformation(_options, _token) {
    return [AIMEE_MODEL_INFO];
  },

  async provideLanguageModelChatResponse(_model, messages, _options, progress, token) {
    const { apiBase, bearerToken, model } = config();
    const controller = new AbortController();
    const cancel = token.onCancellationRequested(() => controller.abort());

    let resp: Response;
    try {
      resp = await fetch(`${apiBase}/chat/completions`, {
        method: 'POST',
        headers: authHeaders(bearerToken),
        body: JSON.stringify({ model, messages: toOpenAIMessages(messages), stream: true }),
        signal: controller.signal,
      });
    } catch {
      cancel.dispose();
      throw new Error(`aimee: could not reach the server at ${apiBase} (run \`aimee api status\`)`);
    }

    if (!resp.ok || !resp.body) {
      cancel.dispose();
      const detail = await resp.text().catch(() => '');
      throw new Error(`aimee: server returned ${resp.status}${detail ? ': ' + detail.slice(0, 300) : ''}`);
    }

    const reader = resp.body.getReader();
    const decoder = new TextDecoder();
    let buffer = '';
    try {
      for (;;) {
        const { done, value } = await reader.read();
        if (done) {
          break;
        }
        buffer += decoder.decode(value, { stream: true });
        let nl: number;
        while ((nl = buffer.indexOf('\n')) >= 0) {
          const line = buffer.slice(0, nl).trim();
          buffer = buffer.slice(nl + 1);
          if (!line.startsWith('data:')) {
            continue;
          }
          const payload = line.slice('data:'.length).trim();
          if (payload === '[DONE]') {
            return;
          }
          try {
            const obj = JSON.parse(payload);
            const delta: string | undefined = obj?.choices?.[0]?.delta?.content;
            if (delta) {
              progress.report(new vscode.LanguageModelTextPart(delta));
            }
          } catch {
            // partial frame / keep-alive
          }
        }
      }
    } finally {
      cancel.dispose();
      reader.releaseLock();
    }
  },

  async provideTokenCount(_model, text, _token) {
    const s =
      typeof text === 'string'
        ? text
        : text.content
            .map((p) => (p instanceof vscode.LanguageModelTextPart ? p.value : ''))
            .join('');
    // Rough heuristic (~4 chars/token); aimee-server owns real tokenization.
    return Math.ceil(s.length / 4);
  },
};

// ── Docked chat panel (Tier 3, phase 5) ─────────────────────────────────────
// A webview chat docked beside the editor. The streaming + session logic stays
// in the (type-checked) extension host via streamResponses; the webview is a
// thin renderer that posts {type:'send'} and receives {start|delta|done}.

function nonce(): string {
  let s = '';
  const chars = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789';
  for (let i = 0; i < 32; i++) {
    s += chars.charAt(Math.floor(Math.random() * chars.length));
  }
  return s;
}

function chatPanelHtml(cspSource: string, scriptNonce: string): string {
  return `<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta http-equiv="Content-Security-Policy" content="default-src 'none'; style-src ${cspSource} 'unsafe-inline'; script-src 'nonce-${scriptNonce}';">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<style>
  body { font-family: var(--vscode-font-family); color: var(--vscode-foreground); margin: 0; padding: 0; display: flex; flex-direction: column; height: 100vh; }
  #log { flex: 1; overflow-y: auto; padding: 10px; }
  .msg { margin: 8px 0; padding: 8px 10px; border-radius: 6px; white-space: pre-wrap; word-wrap: break-word; }
  .user { background: var(--vscode-input-background); }
  .aimee { background: var(--vscode-editor-inactiveSelectionBackground); }
  .role { font-size: 11px; opacity: 0.7; margin-bottom: 2px; }
  #bar { display: flex; gap: 6px; padding: 8px; border-top: 1px solid var(--vscode-panel-border); }
  #input { flex: 1; background: var(--vscode-input-background); color: var(--vscode-input-foreground); border: 1px solid var(--vscode-input-border); border-radius: 4px; padding: 6px; font-family: inherit; resize: none; }
  #send { background: var(--vscode-button-background); color: var(--vscode-button-foreground); border: none; border-radius: 4px; padding: 6px 14px; cursor: pointer; }
  #send:disabled { opacity: 0.5; cursor: default; }
</style>
</head>
<body>
<div id="log"></div>
<div id="bar">
  <textarea id="input" rows="2" placeholder="Message aimee… (Enter to send, Shift+Enter for newline)"></textarea>
  <button id="send">Send</button>
</div>
<script nonce="${scriptNonce}">
  const vscode = acquireVsCodeApi();
  const log = document.getElementById('log');
  const input = document.getElementById('input');
  const sendBtn = document.getElementById('send');
  let current = null;

  function add(role, cls) {
    const el = document.createElement('div');
    el.className = 'msg ' + cls;
    const r = document.createElement('div');
    r.className = 'role';
    r.textContent = role;
    const body = document.createElement('div');
    el.appendChild(r);
    el.appendChild(body);
    log.appendChild(el);
    log.scrollTop = log.scrollHeight;
    return body;
  }

  function send() {
    const text = input.value.trim();
    if (!text) return;
    add('you', 'user').textContent = text;
    input.value = '';
    sendBtn.disabled = true;
    vscode.postMessage({ type: 'send', text: text });
  }

  sendBtn.addEventListener('click', send);
  input.addEventListener('keydown', (e) => {
    if (e.key === 'Enter' && !e.shiftKey) { e.preventDefault(); send(); }
  });

  window.addEventListener('message', (event) => {
    const m = event.data;
    if (m.type === 'start') { current = add('aimee', 'aimee'); }
    else if (m.type === 'delta' && current) { current.textContent += m.text; log.scrollTop = log.scrollHeight; }
    else if (m.type === 'done') { current = null; sendBtn.disabled = false; input.focus(); }
  });
  input.focus();
</script>
</body>
</html>`;
}

function openChatPanel(context: vscode.ExtensionContext): void {
  const panel = vscode.window.createWebviewPanel('aimee.chatPanel', 'aimee', vscode.ViewColumn.Beside, {
    enableScripts: true,
    retainContextWhenHidden: true,
  });
  const scriptNonce = nonce();
  panel.webview.html = chatPanelHtml(panel.webview.cspSource, scriptNonce);

  let prevId: string | undefined;
  let inFlight: vscode.CancellationTokenSource | undefined;
  panel.onDidDispose(() => inFlight?.cancel(), null, context.subscriptions);

  panel.webview.onDidReceiveMessage(
    async (msg: { type?: string; text?: string }) => {
      if (msg?.type !== 'send' || typeof msg.text !== 'string' || !msg.text.trim()) {
        return;
      }
      inFlight?.cancel();
      inFlight = new vscode.CancellationTokenSource();
      panel.webview.postMessage({ type: 'start' });
      const newId = await streamResponses(
        msg.text,
        prevId,
        (delta) => void panel.webview.postMessage({ type: 'delta', text: delta }),
        inFlight.token,
      );
      if (newId) {
        prevId = newId; // keep the panel's aimee session stateful
      }
      void panel.webview.postMessage({ type: 'done' });
    },
    null,
    context.subscriptions,
  );
}

export function activate(context: vscode.ExtensionContext): void {
  const status = makeStatusBar(context);

  const handler: vscode.ChatRequestHandler = async (request, chatContext, stream, token) => {
    const newId = await streamResponses(
      request.prompt,
      previousResponseId(chatContext),
      (text) => stream.markdown(text),
      token,
    );
    return newId ? { metadata: { aimeeResponseId: newId } } : {};
  };

  const participant = vscode.chat.createChatParticipant('aimee.chat', handler);
  context.subscriptions.push(participant);

  // Register aimee in the native model picker where the API is available
  // (newer VS Code); the participant + commands work without it on older builds.
  if (typeof vscode.lm?.registerLanguageModelChatProvider === 'function') {
    context.subscriptions.push(
      vscode.lm.registerLanguageModelChatProvider('aimee', aimeeChatModelProvider),
    );
  }

  const out = vscode.window.createOutputChannel('aimee');
  context.subscriptions.push(out);

  context.subscriptions.push(
    vscode.commands.registerCommand('aimee.showServerStatus', async () => {
      const s = await fetchHealth();
      status.refresh();
      void vscode.window.showInformationMessage(
        s ? `aimee-server: ${s} (${config().apiBase})` : `aimee-server unreachable at ${config().apiBase}.`,
      );
    }),
    vscode.commands.registerCommand('aimee.searchMemory', () =>
      runQueryCommand(out, {
        promptTitle: 'aimee — Recall from memory',
        placeHolder: 'What should aimee recall? (task hint)',
        path: '/memory/recall',
        bodyFor: (q) => ({ task_hint: q, limit_tokens: 1024 }),
      }),
    ),
    vscode.commands.registerCommand('aimee.searchKnowledgeBase', () =>
      runQueryCommand(out, {
        promptTitle: 'aimee — Search knowledge base',
        placeHolder: 'Knowledge base query',
        path: '/kb/search',
        bodyFor: (q) => ({ query: q }),
      }),
    ),
    vscode.commands.registerCommand('aimee.openChat', () => openChatPanel(context)),
  );
}

export function deactivate(): void {
  // Subscriptions are disposed by VS Code via context.subscriptions.
}
