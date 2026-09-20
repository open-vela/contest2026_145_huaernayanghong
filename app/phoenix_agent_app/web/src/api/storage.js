import { api } from './client.js';

export const storageApi = {
  getList: (path) => api.get('/api/sdcard/list?path=' + encodeURIComponent(path)),
  mkdir: (path) => api.post('/api/sdcard/mkdir', { path }),
  uploadText: (path, content) => api.post('/api/sdcard/upload', { path, content }),
  deleteItem: (path) => api.post('/api/sdcard/delete', { path })
};
