let toastTimer = null;

export function showToast(txt, duration = 2800) {
  const el = document.getElementById('toastMsg');
  if (!el) return;
  el.innerText = txt;
  el.style.display = 'block';
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => {
    el.style.display = 'none';
  }, duration);
}
