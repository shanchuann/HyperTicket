const assert = require('assert');
const { spawn } = require('child_process');
const net = require('net');
const path = require('path');
const WebSocket = require('ws');

const bridgePort = 18080;
const backendPort = 17000;
const repositoryRoot = path.resolve(__dirname, '..');

const backend = net.createServer(socket => {
  let buffer = '';
  socket.on('error', error => {
    if (error.code !== 'ECONNRESET') console.error(error);
  });
  socket.on('data', chunk => {
    buffer += chunk.toString();
    const lines = buffer.split('\n');
    buffer = lines.pop();
    for (const line of lines) {
      if (!line.trim()) continue;
      const request = JSON.parse(line);
      socket.write(`${JSON.stringify({
        status: 'OK',
        type: request.type,
        hasGatewayIp: Boolean(request._gateway_client_ip),
        clientIp: request._gateway_client_ip,
        gatewayToken: request._gateway_token,
      })}\n`);
    }
  });
});

function listen(server, port) {
  return new Promise((resolve, reject) => {
    server.once('error', reject);
    server.listen(port, '127.0.0.1', resolve);
  });
}

function waitForBridge(child) {
  return new Promise((resolve, reject) => {
    const timeout = setTimeout(() => reject(new Error('Bridge did not start')), 5000);
    child.stdout.on('data', chunk => {
      if (chunk.toString().includes('HTTP and WebSocket server listening')) {
        clearTimeout(timeout);
        resolve();
      }
    });
    child.once('exit', code => reject(new Error(`Bridge exited early with ${code}`)));
  });
}

function websocketRoundTrip() {
  return new Promise((resolve, reject) => {
    const socket = new WebSocket(`ws://127.0.0.1:${bridgePort}/ws`, {
      origin: 'http://allowed.test',
      headers: { 'x-forwarded-for': '203.0.113.55' },
    });
    socket.once('open', () => socket.send(JSON.stringify({ type: 4 })));
    socket.once('message', raw => {
      try {
        const response = JSON.parse(raw.toString());
        assert.equal(response.status, 'OK');
        assert.equal(response.type, 4);
        assert.equal(response.hasGatewayIp, true);
        assert.equal(response.clientIp, '127.0.0.1');
        assert.equal(response.gatewayToken, 'bridge-test-token');
        socket.close();
        resolve();
      } catch (error) {
        reject(error);
      }
    });
    socket.once('error', reject);
  });
}

function rejectedOrigin() {
  return new Promise((resolve, reject) => {
    const socket = new WebSocket(`ws://127.0.0.1:${bridgePort}/ws`, {
      origin: 'http://blocked.test',
    });
    socket.once('open', () => reject(new Error('Blocked origin connected')));
    socket.once('unexpected-response', (_, response) => {
      try {
        assert.equal(response.statusCode, 403);
        resolve();
      } catch (error) {
        reject(error);
      }
    });
    socket.once('error', () => {});
  });
}

function rejectsInvalidProxyPrefix() {
  return new Promise((resolve, reject) => {
    const child = spawn(process.execPath, ['server.js'], {
      cwd: __dirname,
      env: {
        ...process.env,
        WEB_PORT: String(bridgePort + 1),
        TCP_PORT: String(backendPort),
        TRUSTED_PROXY_ADDRESSES: '127.0.0.1/33',
      },
      stdio: ['ignore', 'ignore', 'pipe'],
    });
    let stderr = '';
    const timeout = setTimeout(() => {
      child.kill('SIGKILL');
      reject(new Error('Bridge accepted an invalid trusted proxy prefix'));
    }, 3000);
    child.stderr.on('data', chunk => { stderr += chunk.toString(); });
    child.once('exit', code => {
      clearTimeout(timeout);
      try {
        assert.notEqual(code, 0);
        assert.match(stderr, /Invalid trusted proxy prefix/);
        resolve();
      } catch (error) {
        reject(error);
      }
    });
  });
}

async function main() {
  await listen(backend, backendPort);
  const bridge = spawn(process.execPath, ['server.js'], {
    cwd: __dirname,
    env: {
      ...process.env,
      WEB_PORT: String(bridgePort),
      TCP_HOST: '127.0.0.1',
      TCP_PORT: String(backendPort),
      WEB_ROOT: path.join(repositoryRoot, 'clients', 'user-client', 'dist'),
      ADMIN_ROOT: path.join(repositoryRoot, 'clients', 'admin-client', 'dist'),
      HYPERTICKET_GATEWAY_TOKEN: 'bridge-test-token',
      WS_ALLOWED_ORIGINS: 'http://allowed.test',
      TRUSTED_PROXY_ADDRESSES: '',
    },
    stdio: ['ignore', 'pipe', 'pipe'],
  });

  try {
    await waitForBridge(bridge);
    const health = await fetch(`http://127.0.0.1:${bridgePort}/healthz`);
    assert.equal(health.status, 200);
    assert.deepEqual(await health.json(), { status: 'ok' });

    const user = await fetch(`http://127.0.0.1:${bridgePort}/customer`);
    assert.equal(user.status, 200);
    assert.match(await user.text(), /<div id="root"><\/div>/);

    const admin = await fetch(`http://127.0.0.1:${bridgePort}/admin/`);
    assert.equal(admin.status, 200);
    assert.match(await admin.text(), /\/admin\/assets\//);

    await websocketRoundTrip();
    await rejectedOrigin();
    await rejectsInvalidProxyPrefix();
    console.log('Web image smoke test passed');
  } finally {
    bridge.kill('SIGTERM');
    backend.close();
  }
}

main().catch(error => {
  console.error(error);
  process.exitCode = 1;
});
