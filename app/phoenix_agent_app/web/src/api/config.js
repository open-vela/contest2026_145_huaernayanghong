import { api } from './client.js';

export const configApi = {
  getConfig: () => api.get('/api/config'),
  saveConfig: (payload) => api.post('/api/config', payload),
  testConfig: (payload) => api.post('/api/config/test', payload, { showErrorToast: false, timeout: 15000 }),
  testAsrTranscribe: () => api.post('/api/audio/asr_test', {}, { showErrorToast: false, timeout: 20000 }),
  testTtsSpeak: (payload) => api.post('/api/audio/tts_test', payload, { showErrorToast: false, timeout: 20000 }),
  resetWifi: () => api.post('/api/wifi/reset', {})
};
