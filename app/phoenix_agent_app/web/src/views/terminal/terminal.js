import { terminalApi } from '../../api/terminal.js';

function escapeHtml(s) {
  if (!s) return '';
  return String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
}

let logCursor = 0;
let logIsPaused = false;
let logAutoScroll = true;
let logLinesData = [];
let logPollTimer = null;
const MAX_LOG_LINES = 1500;

function parseLogLine(raw) {
  if (!raw) return null;
  let level = 'I';
  if (raw.includes('[E]') || raw.includes('ERROR') || raw.includes('<3>')) level = 'E';
  else if (raw.includes('[W]') || raw.includes('WARN') || raw.includes('<4>')) level = 'W';
  else if (raw.includes('[D]') || raw.includes('DEBUG') || raw.includes('<7>')) level = 'D';
  else if (raw.includes('[V]') || raw.includes('VERBOSE')) level = 'V';
  else if (raw.includes('[I]') || raw.includes('INFO') || raw.includes('<6>')) level = 'I';
  return { raw: raw, level: level };
}

function renderLogEntry(entry) {
  const div = document.createElement('div');
  div.className = 'log-line';
  div.innerHTML = `<span class="log-lvl-${entry.level}">${escapeHtml(entry.raw)}</span>`;
  return div;
}

function matchesFilter(entry, kw, minLvl) {
  if (kw && !entry.raw.toLowerCase().includes(kw)) return false;
  if (minLvl === 'all') return true;
  const order = { 'DEBUG': 1, 'INFO': 2, 'WARN': 3, 'ERROR': 4 };
  const entryVal = { 'D': 1, 'V': 1, 'I': 2, 'W': 3, 'E': 4 }[entry.level] || 2;
  const targetVal = order[minLvl] || 0;
  return entryVal >= targetVal;
}

export const terminalView = {
  startPolling: () => {
    if (!logPollTimer) {
      terminalView.fetchLogs();
      logPollTimer = setInterval(terminalView.fetchLogs, 900);
    }
  },

  stopPolling: () => {
    if (logPollTimer) {
      clearInterval(logPollTimer);
      logPollTimer = null;
    }
  },

  fetchLogs: async () => {
    if (logIsPaused) return;
    try {
      const d = await terminalApi.getLogs(logCursor);
      if (d.cursor !== undefined) {
        logCursor = d.cursor;
        const cTag = document.getElementById('logCursorTag');
        if (cTag) cTag.innerText = 'Cursor: ' + logCursor + ' B';
      }
      if (d.logs && d.logs.length > 0) {
        const lines = d.logs.split('\n');
        const term = document.getElementById('terminalWindow');
        const kw = document.getElementById('logFilterInput')?.value.trim().toLowerCase() || '';
        const minLvl = document.getElementById('logLevelSelect')?.value || 'all';

        for (let line of lines) {
          if (!line.trim()) continue;
          const entry = parseLogLine(line);
          logLinesData.push(entry);
          if (logLinesData.length > MAX_LOG_LINES) logLinesData.shift();

          if (term && matchesFilter(entry, kw, minLvl)) {
            term.appendChild(renderLogEntry(entry));
          }
        }

        if (term) {
          while (term.children.length > MAX_LOG_LINES) {
            term.removeChild(term.firstChild);
          }
          if (logAutoScroll) term.scrollTop = term.scrollHeight;
        }
      }
    } catch (e) {}
  },

  applyLogFilter: () => {
    const term = document.getElementById('terminalWindow');
    if (!term) return;
    term.innerHTML = '';
    const kw = document.getElementById('logFilterInput')?.value.trim().toLowerCase() || '';
    const minLvl = document.getElementById('logLevelSelect')?.value || 'all';
    const frag = document.createDocumentFragment();

    for (let entry of logLinesData) {
      if (matchesFilter(entry, kw, minLvl)) {
        frag.appendChild(renderLogEntry(entry));
      }
    }
    term.appendChild(frag);
    if (logAutoScroll) term.scrollTop = term.scrollHeight;
  },

  changeLogLevel: (val) => {
    terminalView.applyLogFilter();
    if (val !== 'all') {
      terminalApi.setLevel(val).catch(() => {});
    }
  },

  toggleAutoScroll: () => {
    logAutoScroll = !logAutoScroll;
    const btn = document.getElementById('autoScrollBtn');
    if (btn) {
      btn.innerText = logAutoScroll ? '⏬ 滚屏: 开' : '⏸️ 滚屏: 关';
      btn.style.borderColor = logAutoScroll ? 'var(--accent)' : 'var(--border)';
    }
  },

  toggleLogPause: () => {
    logIsPaused = !logIsPaused;
    const btn = document.getElementById('pauseLogBtn');
    const badge = document.getElementById('logStreamBadge');
    if (btn && badge) {
      if (logIsPaused) {
        btn.innerText = '▶️ 继续';
        badge.innerText = '⏸️ 已暂停';
        badge.style.color = 'var(--muted)';
      } else {
        btn.innerText = '⏸️ 暂停';
        badge.innerText = '● 实时监听中';
        badge.style.color = 'var(--accent-sub)';
      }
    }
  },

  clearLocalLogs: () => {
    logLinesData = [];
    const term = document.getElementById('terminalWindow');
    if (term) term.innerHTML = '<div class="log-line" style="color:var(--muted)">[UI] 视图已清空</div>';
  },

  clearDeviceLogs: async () => {
    if (!confirm('确认清空端侧循环日志缓冲区？')) return;
    try {
      await terminalApi.clearLogs();
      terminalView.clearLocalLogs();
      logCursor = 0;
      const cTag = document.getElementById('logCursorTag');
      if (cTag) cTag.innerText = 'Cursor: 0 B';
    } catch (e) {
      alert('清空失败');
    }
  },

  exportLogs: () => {
    const text = logLinesData.map(e => e.raw).join('\n');
    const blob = new Blob([text], { type: 'text/plain;charset=utf-8' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = 'phoenix_logs_' + new Date().toISOString().replace(/[:.]/g, '-') + '.log';
    a.click();
    URL.revokeObjectURL(url);
  }
};
