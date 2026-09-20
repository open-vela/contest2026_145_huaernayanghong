import { showToast } from './toast.js';

export function copyText(txt) {
  if (navigator.clipboard && window.isSecureContext) {
    navigator.clipboard.writeText(txt).then(() => {
      showToast('📋 已复制到剪贴板');
    }).catch(() => {
      fallbackCopy(txt);
    });
  } else {
    fallbackCopy(txt);
  }
}

function fallbackCopy(txt) {
  const ta = document.createElement('textarea');
  ta.value = txt;
  ta.style.position = 'fixed';
  ta.style.opacity = '0';
  document.body.appendChild(ta);
  ta.focus();
  ta.select();
  try {
    document.execCommand('copy');
    showToast('📋 已复制到剪贴板');
  } catch (e) {
    showToast('❌ 复制失败，请手动选择复制');
  }
  document.body.removeChild(ta);
}

export function autoExpandTextarea(el) {
  el.style.height = 'auto';
  el.style.height = Math.min(el.scrollHeight, 140) + 'px';
}

export function formatBytes(bytes) {
  if (bytes === 0) return '0 B';
  const k = 1024;
  const sizes = ['B', 'KB', 'MB', 'GB'];
  const i = Math.floor(Math.log(bytes) / Math.log(k));
  return parseFloat((bytes / Math.pow(k, i)).toFixed(1)) + ' ' + sizes[i];
}
