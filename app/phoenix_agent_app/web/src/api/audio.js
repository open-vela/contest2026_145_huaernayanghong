import { api } from './client.js';

export const audioApi = {
  getStatus: () => api.get('/api/audio/status', { showErrorToast: false }),
  record: (duration_ms) => api.post('/api/audio/record', { action: 'start', duration_ms }),
  recordStop: () => api.post('/api/audio/record', { action: 'stop' }),
  playRecord: () => api.post('/api/audio/play', { action: 'start', type: 'recording' }),
  playTone: (freq_hz, duration_ms) => api.post('/api/audio/play', { action: 'start', type: 'tone', freq_hz, duration_ms }),
  playStop: () => api.post('/api/audio/play', { action: 'stop' }),
  setLoopback: (enable) => api.post('/api/audio/loopback', { enable }),
  setVolume: (volume) => api.post('/api/audio/volume', { volume })
};
