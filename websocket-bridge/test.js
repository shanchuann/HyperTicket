const WebSocket = require('ws');

function req(payload) {
  return new Promise((resolve, reject) => {
    const ws = new WebSocket('ws://localhost:8080');
    ws.on('open', () => ws.send(JSON.stringify(payload)));
    ws.on('message', (d) => { ws.close(); resolve(JSON.parse(d.toString())); });
    ws.on('error', reject);
    setTimeout(() => { ws.close(); reject(new Error('timeout')); }, 8000);
  });
}

async function main() {
  const tel = '19900009999';
  const pass = 'Front123';
  const user = '前端验证用户';

  console.log('1. 查看票务');
  const tickets = await req({ type: 4 });
  console.log('   status:', tickets.status, '| 票务数:', tickets.num);
  const t = tickets.arr?.[0];
  if (t) console.log('   第一条:', t.title, '@ 剩余:', t.num, '张');

  console.log('\n2. 注册');
  const reg = await req({ type: 2, usertel: tel, passward: pass, username: user });
  console.log('   status:', reg.status, reg.reason || '');

  console.log('\n3. 登录');
  const login = await req({ type: 1, usertel: tel, passward: pass });
  console.log('   status:', login.status, '| 用户:', login.username);
  const token = login.token;

  if (token && t && parseInt(t.num) > 0) {
    console.log('\n4. 预订票务 (index=' + t.tk_id + ')');
    const order = await req({ type: 5, token, index: parseInt(t.tk_id) });
    console.log('   status:', order.status, order.reason || '');

    console.log('\n5. 查看我的订单');
    const orders = await req({ type: 6, token });
    console.log('   status:', orders.status, '| 订单:', JSON.stringify(orders.arr));
  } else if (parseInt(t?.num || '0') <= 0) {
    console.log('\n   票务已售罄，跳过预订测试');
  }

  console.log('\n✓ 端对端测试完成');
}

main().catch(e => { console.error('✗ 失败:', e.message); process.exit(1); });
