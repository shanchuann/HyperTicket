/**
 * 后端错误码 → 中文用户提示（集中管理，避免各页面各自维护）
 */
const ERROR_MAP: Record<string, string> = {
  // ── 通用 ────────────────────────────────────────────────────────────────────
  INVALID_INPUT:      '请求参数有误，请检查填写内容',
  UNKNOWN_TYPE:       '请求类型不支持',
  RATE_LIMITED:       '操作过于频繁，请稍后再试',
  SERVER_BUSY:        '服务繁忙，请稍后重试',
  REQUEST_TOO_LARGE:  '请求内容过大，请减少提交内容',

  // ── 认证 ────────────────────────────────────────────────────────────────────
  UNAUTHORIZED:             '登录已过期，请重新登录',
  INVALID_CREDENTIALS:      '手机号或密码错误',
  USER_NOT_FOUND:           '手机号或密码错误',
  PASSWD_ERROR:             '手机号或密码错误',
  BLACKLISTED:              '该账号已被禁用，请联系客服',
  WEAK_PASSWORD:            '密码强度不足，需包含大小写字母和数字',
  AUTH_TEMPORARILY_LOCKED:  '验证失败次数过多，请稍后再试',
  SESSION_UNAVAILABLE:      '登录服务暂时不可用，请稍后再试',
  VERIFICATION_REQUIRED:    '请先完成账号验证',
  VERIFICATION_UNAVAILABLE: '验证码服务暂时不可用',
  VERIFICATION_CHANNEL_UNAVAILABLE: '当前验证方式暂不可用，请选择邮箱验证',
  VERIFICATION_RATE_LIMITED: '验证码请求过于频繁，请稍后再试',
  VERIFICATION_COOLDOWN:    '验证码刚刚发送，请稍后再试',
  VERIFICATION_DELIVERY_FAILED: '验证码发送失败，请检查地址后重试',
  INVALID_OR_EXPIRED_CODE:  '验证码错误或已过期',
  INVALID_OR_EXPIRED_RESET_TOKEN: '重置授权已过期，请重新验证',
  ACCOUNT_ALREADY_EXISTS:   '手机号或邮箱已被注册',
  CONTACT_ALREADY_IN_USE:   '该邮箱已绑定其他账号',
  CONTACT_UPDATE_FAILED:    '联系方式更新失败，请稍后重试',

  // ── 数据库 ──────────────────────────────────────────────────────────────────
  DB_UNAVAILABLE:     '服务暂时不可用，请稍后再试',
  DB_QUERY:           '查询失败，请稍后重试',
  DB_RESULT:          '数据读取失败，请稍后重试',
  DB_INSERT:          '写入失败，该记录可能已存在',
  DB_UPDATE:          '更新失败，请稍后重试',
  DB_BEGIN:           '服务繁忙，事务启动失败',

  // ── 票务 ────────────────────────────────────────────────────────────────────
  TICKET_NOT_FOUND:   '票务不存在或已下架',
  TICKET_OFFLINE:     '该票务已停止销售',
  NO_TICKET:          '该票务已售罄',

  // ── 订单 ────────────────────────────────────────────────────────────────────
  ORDER_NOT_FOUND:      '订单不存在或无权操作',
  ORDER_CANNOT_CANCEL:  '仅"已确认"状态的订单可以取消',
  SEAT_TAKEN:           '该座位刚刚被他人抢占，请重新选择',

  // ── 管理员 ──────────────────────────────────────────────────────────────────
  ADMIN_UNAUTHORIZED:           '管理员身份验证失败，请重新登录',
  ADMIN_INVALID_CREDENTIALS:    '用户名或密码错误',
  ALREADY_IN_STATE:             '用户已处于该状态，无需重复操作',
  PASSWORD_TOO_WEAK:            '密码需 6–16 位，且包含大小写字母和数字，不能与默认密码相同',
  PASSWORD_SAME_AS_OLD:         '新密码不能与旧密码相同',
};

/**
 * 将后端错误码或任意消息转换为可读中文。
 * 如果 reason 本身已是中文（含中文字符），直接返回。
 * 如果映射表找不到，返回「操作失败，请稍后重试」而非裸露的英文码。
 */
export function translateError(reason: string | undefined | null, fallback = '操作失败，请稍后重试'): string {
  if (!reason) return fallback;
  // Already Chinese
  if (/[一-鿿]/.test(reason)) return reason;
  return ERROR_MAP[reason] ?? fallback;
}

/** 从 catch 块的 unknown 值中提取并翻译错误信息 */
export function catchError(err: unknown, fallback?: string): string {
  const raw = err instanceof Error ? err.message : String(err ?? '');
  return translateError(raw, fallback);
}
