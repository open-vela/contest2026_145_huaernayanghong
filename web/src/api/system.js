import { api } from './client.js';

export const systemApi = {
  getStatus: () => api.get('/api/status'),
  getWeatherStatus: () => api.get('/api/weather/status'),
  setWeatherConfig: (city) => api.post('/api/weather/config', { city }),
  getTodoList: () => api.get('/api/todo/list'),
  toggleTodo: (id) => api.post('/api/todo/toggle', { id }),
  addTodo: (title, time) => api.post('/api/todo/add', { title, time }),
  deleteTodo: (id) => api.post('/api/todo/delete', { id }),
  clearDoneTodos: () => api.post('/api/todo/clear_done', {}),
  switchCartridge: (id) => api.post('/api/cartridge/switch', { id }),
  doCartridgeAction: (action) => api.post('/api/action', { action }),
  addMemo: (content) => api.post('/api/memo/add', { content }),
  setHardwareConfig: (volume, brightness) => api.post('/api/config', { volume, brightness }),
  triggerProactive: () => api.post('/api/proactive', {}),
  syncTime: (timestamp) => api.post('/api/system/time', { timestamp })
};
