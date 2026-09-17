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
const GATEWAY_TOKEN = process.env.HYPERTICKET_GATEWAY_TOKEN || '';
const MAX_CONNECTIONS = Number.parseInt(process.env.WS_MAX_CONNECTIONS || '1000', 10);
const MAX_CONNECTIONS_PER_IP = Number.parseInt(process.env.WS_MAX_CONNECTIONS_PER_IP || '20', 10);
const MAX_MESSAGE_BYTES = Number.parseInt(process.env.WS_MAX_MESSAGE_BYTES || '65536', 10);
const MAX_PENDING_BYTES = Number.parseInt(process.env.WS_MAX_PENDING_BYTES || '262144', 10);
const MAX_TCP_BUFFER_BYTES = Number.parseInt(process.env.WS_MAX_TCP_BUFFER_BYTES || '262144', 10);
const TCP_CONNECT_TIMEOUT_MS = Number.parseInt(process.env.TCP_CONNECT_TIMEOUT_MS || '5000', 10);
const ALLOWED_ORIGINS = new Set((process.env.WS_ALLOWED_ORIGINS || '')
  .split(',').map(value => value.trim()).filter(Boolean));
const TRUSTED_PROXIES = (process.env.TRUSTED_PROXY_ADDRESSES || '')
  .split(',').map(value => value.trim()).filter(Boolean);

function positiveInteger(value, name) {
  if (!Number.isSafeInteger(value) || value <= 0) throw new Error(`${name} must be a positive integer`);
  return value;
}

for (const [value, name] of [
  [WEB_PORT, 'WEB_PORT'], [TCP_PORT, 'TCP_PORT'], [MAX_CONNECTIONS, 'WS_MAX_CONNECTIONS'],
  [MAX_CONNECTIONS_PER_IP, 'WS_MAX_CONNECTIONS_PER_IP'], [MAX_MESSAGE_BYTES, 'WS_MAX_MESSAGE_BYTES'],
  [MAX_PENDING_BYTES, 'WS_MAX_PENDING_BYTES'], [MAX_TCP_BUFFER_BYTES, 'WS_MAX_TCP_BUFFER_BYTES'],
  [TCP_CONNECT_TIMEOUT_MS, 'TCP_CONNECT_TIMEOUT_MS'],
]) positiveInteger(value, name);

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
    'Referrer-Policy': 'same-origin',
    'X-Frame-Options': 'DENY',
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

function backendHealth(response) {
  let settled = false;
  const probe = net.createConnection({ host: TCP_HOST, port: TCP_PORT });
  const finish = (ok) => {
    if (settled) return;
    settled = true;
    probe.destroy();
    response.writeHead(ok ? 200 : 503, { 'Content-Type': 'application/json; charset=utf-8' });
    response.end(JSON.stringify({ status: ok ? 'ok' : 'unavailable' }));
  };
  probe.setTimeout(Math.min(TCP_CONNECT_TIMEOUT_MS, 2000));
  probe.once('connect', () => finish(true));
  probe.once('timeout', () => finish(false));
  probe.once('error', () => finish(false));
}

const server = http.createServer((request, response) => {
  const requestUrl = new URL(request.url || '/', 'http://localhost');
  if (requestUrl.pathname === '/healthz') {
    backendHealth(response);
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

const wss = new WebSocket.Server({
  server,
  path: '/ws',
  maxPayload: MAX_MESSAGE_BYTES,
  perMessageDeflate: false,
  verifyClient: ({ origin }, done) => {
    if (ALLOWED_ORIGINS.size === 0 || ALLOWED_ORIGINS.has(origin)) done(true);
    else done(false, 403, 'Origin not allowed');
  },
});

const proxyBlockList = new net.BlockList();
for (const entry of TRUSTED_PROXIES) {
  const parts = entry.split('/');
  if (parts.length > 2) throw new Error(`Invalid trusted proxy address: ${entry}`);
  const [address, prefixText] = parts;
  const type = net.isIPv6(address) ? 'ipv6' : (net.isIPv4(address) ? 'ipv4' : '');
  if (!type) throw new Error(`Invalid trusted proxy address: ${entry}`);
  if (prefixText === undefined) proxyBlockList.addAddress(address, type);
  else {
    const prefix = Number(prefixText);
    const maxPrefix = type === 'ipv4' ? 32 : 128;
    if (!Number.isInteger(prefix) || prefix < 0 || prefix > maxPrefix) {
      throw new Error(`Invalid trusted proxy prefix: ${entry}`);
    }
    proxyBlockList.addSubnet(address, prefix, type);
  }
}

function remoteAddress(request) {
  const value = request.socket.remoteAddress || '';
  return value.startsWith('::ffff:') ? value.slice(7) : value;
}

function isTrustedProxy(address) {
  if (!address || TRUSTED_PROXIES.length === 0) return false;
  const type = net.isIPv6(address) ? 'ipv6' : (net.isIPv4(address) ? 'ipv4' : '');
  return type ? proxyBlockList.check(address, type) : false;
}

function clientAddress(request) {
  const remote = remoteAddress(request);
  const forwarded = request.headers['x-forwarded-for'];
  const value = Array.isArray(forwarded) ? forwarded[0] : forwarded;
  if (isTrustedProxy(remote) && value) return value.split(',')[0].trim().slice(0, 64);
  return remote.slice(0, 64);
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

const connectionsByIp = new Map();
const tcpConnections = new Set();

wss.on('connection', (ws, request) => {
  const clientIp = clientAddress(request);
  const currentForIp = connectionsByIp.get(clientIp) || 0;
  if (wss.clients.size > MAX_CONNECTIONS || currentForIp >= MAX_CONNECTIONS_PER_IP) {
    ws.close(1013, 'Connection limit reached');
    return;
  }
  connectionsByIp.set(clientIp, currentForIp + 1);
  console.log(`[Bridge] WebSocket connected: ${clientIp}`);

  const tcp = new net.Socket();
  tcpConnections.add(tcp);
  let tcpReady = false;
  let sendQueue = [];
  let sendQueueBytes = 0;
  let closed = false;

  const cleanup = () => {
    if (closed) return;
    closed = true;
    tcpConnections.delete(tcp);
    const remaining = (connectionsByIp.get(clientIp) || 1) - 1;
    if (remaining > 0) connectionsByIp.set(clientIp, remaining);
    else connectionsByIp.delete(clientIp);
  };

  tcp.setTimeout(TCP_CONNECT_TIMEOUT_MS, () => tcp.destroy(new Error('TCP connect timeout')));
  tcp.connect(TCP_PORT, TCP_HOST, () => {
    tcp.setTimeout(0);
    tcpReady = true;
    for (const message of sendQueue) {
      if (!tcp.write(`${message}\n`)) ws.pause();
    }
    sendQueue = [];
    sendQueueBytes = 0;
  });
  tcp.on('drain', () => ws.resume());

  let tcpBuffer = '';
  tcp.on('data', chunk => {
    tcpBuffer += chunk.toString();
    if (Buffer.byteLength(tcpBuffer) > MAX_TCP_BUFFER_BYTES) {
      tcp.destroy(new Error('Backend response exceeded buffer limit'));
      return;
    }
    const lines = tcpBuffer.split('\n');
    tcpBuffer = lines.pop();
    for (const line of lines) {
      const message = line.trim();
      if (message && ws.readyState === WebSocket.OPEN) {
        if (ws.bufferedAmount + Buffer.byteLength(message) > MAX_PENDING_BYTES) {
          ws.close(1013, 'Client is too slow');
          tcp.destroy();
          return;
        }
        ws.send(message);
      }
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
    cleanup();
    if (ws.readyState === WebSocket.OPEN) ws.close();
  });

  ws.on('message', data => {
    if (Buffer.byteLength(data) > MAX_MESSAGE_BYTES) {
      ws.close(1009, 'Message too large');
      return;
    }
    const raw = data.toString().trim();
    if (!raw) return;
    let message;
    try {
      const payload = JSON.parse(raw);
      payload._gateway_client_ip = clientIp;
      if (GATEWAY_TOKEN) payload._gateway_token = GATEWAY_TOKEN;
      message = JSON.stringify(payload);
    } catch (_) {
      ws.send(JSON.stringify({ status: 'ERR', reason: 'JSON_PARSE' }));
      return;
    }
    console.log(`[Bridge] Frontend -> Backend: ${safeLogMessage(message)}`);
    if (tcpReady) {
      if (!tcp.write(`${message}\n`)) ws.pause();
    } else {
      const messageBytes = Buffer.byteLength(message) + 1;
      if (sendQueueBytes + messageBytes > MAX_PENDING_BYTES) {
        ws.close(1013, 'Backend connection is not ready');
        tcp.destroy();
        return;
      }
      sendQueue.push(message);
      sendQueueBytes += messageBytes;
    }
  });

  ws.on('close', () => { cleanup(); tcp.destroy(); });
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
  for (const client of wss.clients) client.close(1001, 'Server shutting down');
  for (const tcp of tcpConnections) tcp.destroy();
  wss.close(() => server.close(() => process.exit(0)));
}
process.on('SIGINT', shutdown);
process.on('SIGTERM', shutdown);
