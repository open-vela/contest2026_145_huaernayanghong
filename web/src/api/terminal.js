import { api } from './client.js';

export const terminalApi = {
  getLogs: (cursor) => api.get('/api/logs?cursor=' + cursor, { showErrorToast: false }),
  clearLogs: () => api.post('/api/logs/clear', {}),
  setLevel: (level) => api.post('/api/logs/level', { level })
};
