import wsClient from './client';
import type {
  AccountSecurityStatus,
  AuthBackendResponse,
  ChallengeResponse,
  PasswordResetGrantResponse,
  VerificationChannel,
  VerificationGrantResponse,
} from '../types';

const CLIENT_ID_KEY = 'hyperticket_client_id';

function createClientId(): string {
  if (typeof crypto.randomUUID === 'function') return crypto.randomUUID();
  const bytes = crypto.getRandomValues(new Uint8Array(16));
  return Array.from(bytes, (byte) => byte.toString(16).padStart(2, '0')).join('');
}

export function getClientId(): string {
  const existing = localStorage.getItem(CLIENT_ID_KEY);
  if (existing) return existing;
  const value = createClientId();
  localStorage.setItem(CLIENT_ID_KEY, value);
  return value;
}

export const authApi = {
  async login(tel: string, password: string): Promise<AuthBackendResponse> {
    return wsClient.send<AuthBackendResponse>({
      type: 1,
      usertel: tel,
      passward: password,  // 后端拼写
      client_id: getClientId(),
    });
  },

  async register(
    tel: string,
    username: string,
    password: string,
    verificationToken: string,
    email?: string,
  ): Promise<AuthBackendResponse> {
    return wsClient.send<AuthBackendResponse>({
      type: 2,
      usertel: tel,
      passward: password,
      username,
      verification_token: verificationToken,
      ...(email ? { email } : {}),
    });
  },

  async requestRegistrationCode(
    tel: string,
    channel: VerificationChannel,
    email?: string,
  ): Promise<ChallengeResponse> {
    return wsClient.send<ChallengeResponse>({
      type: 26,
      usertel: tel,
      channel,
      client_id: getClientId(),
      ...(channel === 'EMAIL' ? { email } : {}),
    });
  },

  async verifyRegistrationCode(challengeId: string, code: string): Promise<VerificationGrantResponse> {
    return wsClient.send<VerificationGrantResponse>({
      type: 27,
      challenge_id: challengeId,
      code,
    });
  },

  async requestPasswordReset(account: string, channel: VerificationChannel): Promise<ChallengeResponse> {
    return wsClient.send<ChallengeResponse>({
      type: 28,
      account,
      channel,
      client_id: getClientId(),
    });
  },

  async verifyPasswordReset(challengeId: string, code: string): Promise<PasswordResetGrantResponse> {
    return wsClient.send<PasswordResetGrantResponse>({
      type: 29,
      challenge_id: challengeId,
      code,
    });
  },

  async confirmPasswordReset(resetToken: string, newPassword: string): Promise<void> {
    await wsClient.send({ type: 30, reset_token: resetToken, new_password: newPassword });
  },

  async getSecurityStatus(token: string): Promise<AccountSecurityStatus> {
    return wsClient.send<AccountSecurityStatus>({ type: 31, token });
  },

  async requestContactVerification(
    token: string,
    password: string,
    channel: VerificationChannel,
    email?: string,
  ): Promise<ChallengeResponse> {
    return wsClient.send<ChallengeResponse>({
      type: 32,
      token,
      passward: password,
      channel,
      client_id: getClientId(),
      ...(channel === 'EMAIL' ? { email } : {}),
    });
  },

  async confirmContactVerification(
    token: string,
    challengeId: string,
    code: string,
  ): Promise<AccountSecurityStatus> {
    return wsClient.send<AccountSecurityStatus>({
      type: 33,
      token,
      challenge_id: challengeId,
      code,
    });
  },

  async logout(token: string): Promise<void> {
    await wsClient.send({ type: 3, token }).catch(() => {});
    localStorage.removeItem('token');
    localStorage.removeItem('user');
  },
};
