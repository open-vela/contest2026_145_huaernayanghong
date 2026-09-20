import { showToast } from '../utils/toast.js';

/**
 * 统一 API 通信客户端
 */
export async function apiFetch(url, options = {}) {
  const { timeout = 15000, showErrorToast = true, ...fetchOptions } = options;
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), timeout);

  try {
    const res = await fetch(url, {
      ...fetchOptions,
      signal: controller.signal
    });
    clearTimeout(timer);

    const contentType = res.headers.get('content-type') || '';
    let data;
    if (contentType.includes('application/json')) {
      data = await res.json();
    } else {
      data = await res.text();
    }

    if (!res.ok) {
      const errorMsg = (typeof data === 'object' && data.error) ? data.error : `HTTP ${res.status}`;
      if (showErrorToast) {
        showToast(`❌ 请求失败: ${errorMsg}`);
      }
      throw new Error(errorMsg);
    }
    return data;
  } catch (err) {
    clearTimeout(timer);
    if (err.name === 'AbortError') {
      if (showErrorToast) showToast('⏱️ 网络请求超时');
      throw new Error('Timeout');
    }
    if (showErrorToast && !err.message.startsWith('HTTP')) {
      showToast(`❌ 网络异常: ${err.message}`);
    }
    throw err;
  }
}

export const api = {
  get: (url, opts) => apiFetch(url, { ...opts, method: 'GET' }),
  post: (url, body, opts) => apiFetch(url, {
    ...opts,
    method: 'POST',
    headers: { 'Content-Type': 'application/json', ...(opts && opts.headers) },
    body: typeof body === 'string' ? body : JSON.stringify(body)
  }),
  postRaw: (url, body, opts) => apiFetch(url, {
    ...opts,
    method: 'POST',
    body
  })
};
