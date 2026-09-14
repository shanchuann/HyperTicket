const messages: Record<string, string> = {
  INVALID_INPUT: '提交的信息不完整或格式不正确',
  UNKNOWN_TYPE: '当前操作暂不受支持',
  UNAUTHORIZED: '登录状态已失效，请重新登录',
  DB_UNAVAILABLE: '数据服务暂时不可用，请稍后重试',
  DB_QUERY: '读取数据失败，请稍后重试',
  DB_RESULT: '数据响应异常，请稍后重试',
  DB_INSERT: '保存失败，该信息可能已经存在',
  DB_UPDATE: '更新失败，请稍后重试',
  DB_BEGIN: '服务繁忙，请稍后重试',
  RATE_LIMITED: '操作过于频繁，请稍后再试',
  USER_NOT_FOUND: '手机号或密码错误',
  PASSWD_ERROR: '手机号或密码错误',
  INVALID_CREDENTIALS: '手机号或密码错误',
  WEAK_PASSWORD: '密码强度不足，请包含大小写字母和数字',
  BLACKLISTED: '该账号已被禁用，请联系管理员',
  TICKET_NOT_FOUND: '该票务不存在或已被删除',
  TICKET_OFFLINE: '该场次已经下架',
  NO_TICKET: '当前余票不足，请减少数量或选择其他场次',
  ORDER_NOT_FOUND: '未找到对应订单',
  ORDER_CANNOT_CANCEL: '该订单当前无法取消',
  ORDER_QUEUE_UNAVAILABLE: '下单队列暂时繁忙，请稍后重试',
  SEAT_TAKEN: '所选座位已被其他用户锁定',
};

export const toChineseError = (error: unknown, fallback = '操作失败，请稍后重试') => {
  const raw = error instanceof Error ? error.message : typeof error === 'string' ? error : '';
  if (!raw) return fallback;
  if (messages[raw]) return messages[raw];
  if (/^[A-Z0-9_]+$/.test(raw)) return fallback;
  return /[\u3400-\u9fff]/.test(raw) ? raw : fallback;
};
