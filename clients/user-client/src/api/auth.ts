import wsClient from './client';
import type {
  AuthBackendResponse,
  ChallengeResponse,
  ResetGrantResponse,
  SecurityStatusResponse,
  VerificationChannel,
  VerificationGrantResponse,
} from '../types';

const getClientId = () => {
  const key = 'hyperticket_client_id';
  const existing = localStorage.getItem(key);
  if (existing) return existing;
  const id = typeof crypto.randomUUID === 'function'
    ? crypto.randomUUID()
    : `client-${Date.now()}-${Math.random().toString(16).slice(2)}`;
  localStorage.setItem(key, id);
  return id;
};

export const authApi = {
  async login(tel: string, password: string): Promise<AuthBackendResponse> {
    return wsClient.send<AuthBackendResponse>({
      type: 1,
      usertel: tel,
      passward: password,  // 后端拼写
    });
  },

  async register(tel: string, username: string, password: string, verificationToken: string): Promise<AuthBackendResponse> {
    return wsClient.send<AuthBackendResponse>({
      type: 2,
      usertel: tel,
      passward: password,
      username,
      verification_token: verificationToken,
    });
  },

  requestRegistrationCode(tel: string, email: string, channel: VerificationChannel) {
    return wsClient.send<ChallengeResponse>({
      type: 26,
      usertel: tel,
      ...(channel === 'EMAIL' ? { email } : {}),
      channel,
      client_id: getClientId(),
    });
  },

  verifyRegistrationCode(challengeId: string, code: string) {
    return wsClient.send<VerificationGrantResponse>({ type: 27, challenge_id: challengeId, code });
  },

  requestPasswordReset(account: string, channel: VerificationChannel) {
    return wsClient.send<ChallengeResponse>({
      type: 28,
      account,
      channel,
      client_id: getClientId(),
    });
  },

  verifyPasswordReset(challengeId: string, code: string) {
    return wsClient.send<ResetGrantResponse>({ type: 29, challenge_id: challengeId, code });
  },

  confirmPasswordReset(resetToken: string, newPassword: string) {
    return wsClient.send({ type: 30, reset_token: resetToken, new_password: newPassword });
  },

  securityStatus(token: string) {
    return wsClient.send<SecurityStatusResponse>({ type: 31, token });
  },

  requestContactVerification(token: string, password: string, channel: VerificationChannel, email = '') {
    return wsClient.send<ChallengeResponse>({
      type: 32,
      token,
      passward: password,
      channel,
      ...(channel === 'EMAIL' ? { email } : {}),
      client_id: getClientId(),
    });
  },

  confirmContactVerification(token: string, challengeId: string, code: string) {
    return wsClient.send({ type: 33, token, challenge_id: challengeId, code });
  },

  async logout(token: string): Promise<void> {
    await wsClient.send({ type: 3, token }).catch(() => {});
    localStorage.removeItem('token');
    localStorage.removeItem('user');
  },
};
