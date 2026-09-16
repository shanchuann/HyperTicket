/**
 * HyperTicket WebSocket Bridge Server
 * 将前端 WebSocket 请求转发到后端 TCP 服务（端口 7000）
 *
 * 前端 <--WebSocket(8080)--> 桥接 <--TCP(7000)--> HyperTicket 后端
 */

const WebSocket = require('ws');
const net = require('net');

const WS_PORT = parseInt(process.env.WS_PORT || '8080');
const TCP_HOST = process.env.TCP_HOST || '127.0.0.1';
const TCP_PORT = parseInt(process.env.TCP_PORT || '7000');

const wss = new WebSocket.Server({ port: WS_PORT });

function clientAddress(req) {
  const forwarded = req.headers['x-forwarded-for'];
  const value = Array.isArray(forwarded) ? forwarded[0] : forwarded;
  return (value ? value.split(',')[0].trim() : req.socket.remoteAddress || '').slice(0, 64);
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

console.log(`[Bridge] WebSocket server listening on ws://localhost:${WS_PORT}`);
console.log(`[Bridge] Forwarding to TCP ${TCP_HOST}:${TCP_PORT}`);

wss.on('connection', (ws, req) => {
  const clientIp = clientAddress(req);
  console.log(`[Bridge] New WebSocket client: ${clientIp}`);

  const tcp = new net.Socket();
  let tcpReady = false;
  let sendQueue = [];

  tcp.connect(TCP_PORT, TCP_HOST, () => {
    tcpReady = true;
    console.log(`[Bridge] TCP connected to backend`);
    // 发送队列中积压的消息
    for (const msg of sendQueue) {
      tcp.write(msg + '\n');
    }
    sendQueue = [];
  });

  // TCP → WebSocket：后端响应转发给浏览器
  let tcpBuf = '';
  tcp.on('data', (chunk) => {
    tcpBuf += chunk.toString();
    const lines = tcpBuf.split('\n');
    tcpBuf = lines.pop(); // 保留未结束的行
    for (const line of lines) {
      const trimmed = line.trim();
      if (trimmed && ws.readyState === WebSocket.OPEN) {
        console.log(`[Bridge] Backend → Frontend: ${safeLogMessage(trimmed)}`);
        ws.send(trimmed);
      }
    }
  });

  tcp.on('error', (err) => {
    console.error(`[Bridge] TCP error: ${err.message}`);
    if (ws.readyState === WebSocket.OPEN) {
      // 必须与后端协议一致（status/reason）——前端 client.ts 只识别这两个字段，
      // 否则 reason 为 undefined，用户只能看到笼统的"请求失败"
      ws.send(JSON.stringify({ status: 'ERR', reason: '票务服务暂时不可用，请稍后重试' }));
      ws.close();
    }
  });

  tcp.on('close', () => {
    console.log(`[Bridge] TCP connection closed`);
    if (ws.readyState === WebSocket.OPEN) ws.close();
  });

  // WebSocket → TCP：前端消息转发给后端
  ws.on('message', (data) => {
    const msg = data.toString().trim();
    if (!msg) return;
    let forwardedMessage;
    try {
      const payload = JSON.parse(msg);
      // The bridge owns this internal field; any browser-provided value is overwritten.
      payload._gateway_client_ip = clientIp;
      forwardedMessage = JSON.stringify(payload);
    } catch (_) {
      ws.send(JSON.stringify({ status: 'ERR', reason: 'JSON_PARSE' }));
      return;
    }
    console.log(`[Bridge] Frontend → Backend: ${safeLogMessage(forwardedMessage)}`);
    if (tcpReady) {
      tcp.write(forwardedMessage + '\n');
    } else {
      sendQueue.push(forwardedMessage);
    }
  });

  ws.on('close', () => {
    console.log(`[Bridge] WebSocket client disconnected: ${clientIp}`);
    tcp.destroy();
  });

  ws.on('error', (err) => {
    console.error(`[Bridge] WebSocket error: ${err.message}`);
    tcp.destroy();
  });
});

wss.on('error', (err) => {
  console.error(`[Bridge] Server error: ${err.message}`);
  process.exit(1);
});

// 优雅退出
process.on('SIGINT', () => {
  console.log('\n[Bridge] Shutting down...');
  wss.close(() => process.exit(0));
});
