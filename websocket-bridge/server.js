const fs = require('fs');
const http = require('http');
const net = require('net');
const path = require('path');
const WebSocket = require('ws');

const WEB_PORT = Number.parseInt(process.env.WEB_PORT || process.env.WS_PORT || '8080', 10);
const TCP_HOST = process.env.TCP_HOST || '127.0.0.1';
const TCP_PORT = Number.parseInt(process.env.TCP_PORT || '7000', 10);
const WEB_ROOT = process.env.WEB_ROOT || '';
const ADMIN_ROOT = process.env.ADMIN_ROOT || '';

const mimeTypes = {
  '.css': 'text/css; charset=utf-8',
  '.html': 'text/html; charset=utf-8',
  '.ico': 'image/x-icon',
  '.jpg': 'image/jpeg',
  '.jpeg': 'image/jpeg',
  '.js': 'text/javascript; charset=utf-8',
  '.json': 'application/json; charset=utf-8',
  '.png': 'image/png',
  '.svg': 'image/svg+xml',
  '.webp': 'image/webp',
  '.woff': 'font/woff',
  '.woff2': 'font/woff2',
};

function sendFile(response, root, requestPath) {
  if (!root) return false;
  const normalized = path.posix.normalize(`/${requestPath}`).replace(/^\/+/, '');
  const candidate = path.resolve(root, normalized);
  const resolvedRoot = path.resolve(root);
  if (candidate !== resolvedRoot && !candidate.startsWith(`${resolvedRoot}${path.sep}`)) return false;

  let target = candidate;
  try {
    if (!fs.statSync(target).isFile()) return false;
  } catch (_) {
    return false;
  }

  response.writeHead(200, {
    'Content-Type': mimeTypes[path.extname(target).toLowerCase()] || 'application/octet-stream',
    'Cache-Control': path.basename(target) === 'index.html'
      ? 'no-cache'
      : (requestPath.startsWith('assets/') ? 'public, max-age=31536000, immutable' : 'public, max-age=3600'),
    'X-Content-Type-Options': 'nosniff',
  });
  fs.createReadStream(target).pipe(response);
  return true;
}

function serveApplication(response, root, requestPath) {
  if (sendFile(response, root, requestPath)) return;
  if (sendFile(response, root, 'index.html')) return;
  response.writeHead(404, { 'Content-Type': 'text/plain; charset=utf-8' });
  response.end('Not found');
}

const server = http.createServer((request, response) => {
  const requestUrl = new URL(request.url || '/', 'http://localhost');
  if (requestUrl.pathname === '/healthz') {
    response.writeHead(200, { 'Content-Type': 'application/json; charset=utf-8' });
    response.end(JSON.stringify({ status: 'ok' }));
    return;
  }

  let pathname;
  try {
    pathname = decodeURIComponent(requestUrl.pathname);
  } catch (_) {
    response.writeHead(400, { 'Content-Type': 'text/plain; charset=utf-8' });
    response.end('Bad request');
    return;
  }

  if (pathname === '/admin') {
    response.writeHead(308, { Location: '/admin/' });
    response.end();
    return;
  }
  if (pathname.startsWith('/admin/')) {
    serveApplication(response, ADMIN_ROOT, pathname.slice('/admin/'.length));
    return;
  }
  serveApplication(response, WEB_ROOT, pathname.slice(1));
});

const wss = new WebSocket.Server({ server, path: '/ws' });

function clientAddress(request) {
  const forwarded = request.headers['x-forwarded-for'];
  const value = Array.isArray(forwarded) ? forwarded[0] : forwarded;
  return (value ? value.split(',')[0].trim() : request.socket.remoteAddress || '').slice(0, 64);
}

function safeLogMessage(raw) {
  try {
    const value = JSON.parse(raw);
    for (const key of [
      'passward', 'password', 'new_password', 'token', 'admin_token',
      'reset_token', 'verification_token', 'code',
    ]) {
      if (Object.prototype.hasOwnProperty.call(value, key)) value[key] = '[REDACTED]';
    }
    return JSON.stringify(value).slice(0, 240);
  } catch (_) {
    return '[non-json message]';
  }
}

wss.on('connection', (ws, request) => {
  const clientIp = clientAddress(request);
  console.log(`[Bridge] WebSocket connected: ${clientIp}`);

  const tcp = new net.Socket();
  let tcpReady = false;
  let sendQueue = [];

  tcp.connect(TCP_PORT, TCP_HOST, () => {
    tcpReady = true;
    for (const message of sendQueue) tcp.write(`${message}\n`);
    sendQueue = [];
  });

  let tcpBuffer = '';
  tcp.on('data', chunk => {
    tcpBuffer += chunk.toString();
    const lines = tcpBuffer.split('\n');
    tcpBuffer = lines.pop();
    for (const line of lines) {
      const message = line.trim();
      if (message && ws.readyState === WebSocket.OPEN) ws.send(message);
    }
  });

  tcp.on('error', error => {
    console.error(`[Bridge] TCP error: ${error.message}`);
    if (ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify({ status: 'ERR', reason: '票务服务暂时不可用，请稍后重试' }));
      ws.close();
    }
  });
  tcp.on('close', () => {
    if (ws.readyState === WebSocket.OPEN) ws.close();
  });

  ws.on('message', data => {
    const raw = data.toString().trim();
    if (!raw) return;
    let message;
    try {
      const payload = JSON.parse(raw);
      payload._gateway_client_ip = clientIp;
      message = JSON.stringify(payload);
    } catch (_) {
      ws.send(JSON.stringify({ status: 'ERR', reason: 'JSON_PARSE' }));
      return;
    }
    console.log(`[Bridge] Frontend -> Backend: ${safeLogMessage(message)}`);
    if (tcpReady) tcp.write(`${message}\n`);
    else sendQueue.push(message);
  });

  ws.on('close', () => tcp.destroy());
  ws.on('error', error => {
    console.error(`[Bridge] WebSocket error: ${error.message}`);
    tcp.destroy();
  });
});

wss.on('error', error => {
  console.error(`[Bridge] Server error: ${error.message}`);
});

server.listen(WEB_PORT, '0.0.0.0', () => {
  console.log(`[Web] HTTP and WebSocket server listening on 0.0.0.0:${WEB_PORT}`);
  console.log(`[Bridge] Forwarding /ws to ${TCP_HOST}:${TCP_PORT}`);
});

function shutdown() {
  wss.close(() => server.close(() => process.exit(0)));
}
process.on('SIGINT', shutdown);
process.on('SIGTERM', shutdown);
