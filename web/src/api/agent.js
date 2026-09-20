import { api } from './client.js';

export const agentApi = {
  chat: (prompt) => api.post('/api/v1/agent/chat', { prompt }, { showErrorToast: false, timeout: 60000 }),
  getTools: () => api.get('/api/v1/agent/tools'),
  executeTool: (name, args) => api.post('/api/v1/agent/tool/execute', { name, arguments: args }),
  getMemory: () => api.get('/api/v1/agent/memory'),
  clearMemory: () => api.post('/api/v1/agent/memory/clear', {})
};
