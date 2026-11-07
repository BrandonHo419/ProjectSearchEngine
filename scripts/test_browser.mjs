// Node 24 + installed Chrome/Chromium; no npm dependencies.
// Test a temporary copy so the published package never contains the harness.
import { spawn } from 'node:child_process';
import { mkdtemp, cp, mkdir, readFile, rm } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { setTimeout as delay } from 'node:timers/promises';

const root = fileURLToPath(new URL('../', import.meta.url));
const site = resolve(process.argv[2] ?? '_site');
const temp = await mkdtemp(join(tmpdir(), 'search-browser-'));
const children = [];
let socket;
const pending = new Map();
let sequence = 0;
const deadline = Date.now() + 60000;
async function waitFor(fn) {
  while (Date.now() < deadline) {
    try {
      const result = await fn();
      if (result) return result;
    } catch (error) {
      // Navigation can destroy the current evaluation context between polls.
      if (!['Inspected target navigated or closed',
            'Execution context was destroyed.',
            'Cannot find context with specified id'].includes(error.message)) throw error;
    }
    await delay(100);
  }
  throw new Error('Browser checks timed out.');
}
function start(command, args, cwd = root) {
  const child = spawn(command, args, { cwd, stdio: ['ignore', 'pipe', 'pipe'] });
  child.on('error', error => { child.startError = error; });
  children.push(child);
  return child;
}
function alive(child) {
  if (child.startError) throw child.startError;
  if (child.exitCode !== null) throw new Error(`Process exited: ${child.exitCode}`);
}
function send(method, params = {}) {
  const id = ++sequence;
  return new Promise((resolve, reject) => {
    const timeout = setTimeout(() => { pending.delete(id); reject(new Error(`${method} timed out`)); }, 10000);
    pending.set(id, { resolve, reject, timeout });
    socket.send(JSON.stringify({ id, method, params }));
  });
}
try {
  const web = join(temp, 'web');
  const prefix = join(web, 'ProjectSearchEngine');
  await cp(site, prefix, { recursive: true });
  await mkdir(join(prefix, 'tests'));
  await cp(join(root, 'tests/browser_tests.html'), join(prefix, 'tests/browser_tests.html'));
  // Python selects a free port and prints it before serving.
  const server = start(process.env.PYTHON ?? 'python3', ['-u', '-c',
    'import http.server; s=http.server.ThreadingHTTPServer(("127.0.0.1",0),http.server.SimpleHTTPRequestHandler); print(s.server_port,flush=True); s.serve_forever()',
  ], web);
  let portText = '';
  server.stdout.on('data', data => { portText += data; });
  server.stderr.resume();
  const port = await waitFor(() => { alive(server); return portText.match(/^\d+\r?\n/)?.[0].trim(); });
  const profile = join(temp, 'profile');
  const chrome = start(process.env.CHROME_BIN ?? 'google-chrome', [
    '--headless', '--disable-gpu', '--no-first-run', '--no-default-browser-check',
    '--remote-debugging-port=0', `--user-data-dir=${profile}`, 'about:blank',
    ...(process.env.CHROME_NO_SANDBOX === '1' ? ['--no-sandbox'] : []),
  ]);
  chrome.stdout.resume();
  chrome.stderr.resume();
  const debugPort = await waitFor(async () => {
    alive(chrome);
    try { return (await readFile(join(profile, 'DevToolsActivePort'), 'utf8')).split('\n')[0]; }
    catch (error) { if (error.code !== 'ENOENT') throw error; }
  });
  const tabs = await (await fetch(`http://127.0.0.1:${debugPort}/json/list`)).json();
  socket = new WebSocket(tabs.find(tab => tab.type === 'page').webSocketDebuggerUrl);
  await new Promise((resolve, reject) => {
    socket.addEventListener('open', resolve, { once: true });
    socket.addEventListener('error', reject, { once: true });
  });
  socket.addEventListener('message', event => {
    const message = JSON.parse(event.data);
    const request = pending.get(message.id);
    if (!request) return;
    pending.delete(message.id);
    clearTimeout(request.timeout);
    if (message.error) request.reject(new Error(message.error.message));
    else request.resolve(message.result);
  });
  await send('Page.navigate', { url: `http://127.0.0.1:${port}/ProjectSearchEngine/` });
  await waitFor(async () => {
    const response = await send('Runtime.evaluate', {
      expression: 'location.pathname === "/ProjectSearchEngine/demo/" && document.querySelector("#engine-status")?.dataset.state === "ready"',
      returnByValue: true,
    });
    return response.result.value === true;
  });
  await send('Page.navigate', { url: `http://127.0.0.1:${port}/ProjectSearchEngine/tests/browser_tests.html` });
  const result = await waitFor(async () => {
    alive(chrome);
    const response = await send('Runtime.evaluate', {
      expression: 'JSON.stringify({state:document.querySelector("#output")?.dataset.state,text:document.querySelector("#output")?.textContent})',
      returnByValue: true,
    });
    const output = JSON.parse(response.result.value ?? '{}');
    if (output.state === 'failed') throw new Error(output.text);
    return output.state === 'passed' ? output.text : null;
  });
  console.log(`${result} Packaged site verified under /ProjectSearchEngine/.`);
} finally {
  socket?.close();
  for (const request of pending.values()) clearTimeout(request.timeout);
  for (const child of children.reverse()) {
    if (child.exitCode === null && !child.startError) {
      child.kill();
      await Promise.race([new Promise(resolve => child.once('exit', resolve)), delay(2000)]);
      if (child.exitCode === null) child.kill('SIGKILL');
    }
  }
  await rm(temp, { recursive: true, force: true, maxRetries: 5, retryDelay: 200 });
}
