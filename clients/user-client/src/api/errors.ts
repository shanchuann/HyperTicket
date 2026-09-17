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
  SERVER_BUSY: '服务繁忙，请稍后重试',
  REQUEST_TOO_LARGE: '请求内容过大，请减少提交内容',
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
  VERIFICATION_UNAVAILABLE: '验证服务暂时不可用，请稍后再试',
  VERIFICATION_CHANNEL_UNAVAILABLE: '当前验证方式暂不可用，请选择其他方式',
  VERIFICATION_DELIVERY_FAILED: '验证码发送失败，请检查接收地址后重试',
  VERIFICATION_RATE_LIMITED: '验证码请求过于频繁，请稍后再试',
  VERIFICATION_COOLDOWN: '验证码已发送，请等待 60 秒后再试',
  VERIFICATION_REQUIRED: '请先完成验证码校验',
  INVALID_OR_EXPIRED_CODE: '验证码错误或已过期，请重新获取',
  INVALID_OR_EXPIRED_RESET_TOKEN: '重置凭证已过期，请重新验证',
  ACCOUNT_ALREADY_EXISTS: '该手机号或邮箱已注册',
  CONTACT_ALREADY_IN_USE: '该邮箱已绑定其他账号',
  AUTH_TEMPORARILY_LOCKED: '密码验证失败次数过多，请稍后再试',
  SESSION_UNAVAILABLE: '登录服务暂时不可用，请稍后再试',
};

export const toChineseError = (error: unknown, fallback = '操作失败，请稍后重试') => {
  const raw = error instanceof Error ? error.message : typeof error === 'string' ? error : '';
  if (!raw) return fallback;
  if (messages[raw]) return messages[raw];
  if (/^[A-Z0-9_]+$/.test(raw)) return fallback;
  return /[\u3400-\u9fff]/.test(raw) ? raw : fallback;
};
