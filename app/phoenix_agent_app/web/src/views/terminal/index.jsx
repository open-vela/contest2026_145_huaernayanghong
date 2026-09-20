import React, { useState, useEffect, useRef } from 'react';
import { terminalApi } from '../../api/terminal.js';

const MAX_LOG_LINES = 1500;

function parseLogLine(raw) {
  if (!raw) return null;
  let level = 'I';
  if (raw.includes('[E]') || raw.includes('ERROR') || raw.includes('<3>')) level = 'E';
  else if (raw.includes('[W]') || raw.includes('WARN') || raw.includes('<4>')) level = 'W';
  else if (raw.includes('[D]') || raw.includes('DEBUG') || raw.includes('<7>')) level = 'D';
  else if (raw.includes('[V]') || raw.includes('VERBOSE')) level = 'V';
  else if (raw.includes('[I]') || raw.includes('INFO') || raw.includes('<6>')) level = 'I';
  return { raw, level };
}

function matchesFilter(entry, kw, minLvl) {
  if (kw && !entry.raw.toLowerCase().includes(kw)) return false;
  if (minLvl === 'all') return true;
  const order = { 'DEBUG': 1, 'INFO': 2, 'WARN': 3, 'ERROR': 4 };
  const entryVal = { 'D': 1, 'V': 1, 'I': 2, 'W': 3, 'E': 4 }[entry.level] || 2;
  const targetVal = order[minLvl] || 0;
  return entryVal >= targetVal;
}

export function Terminal({ isActive }) {
  const [logs, setLogs] = useState([]);
  const [cursor, setCursor] = useState(0);
  const [minLevel, setMinLevel] = useState('all');
  const [filterKw, setFilterKw] = useState('');
  const [autoScroll, setAutoScroll] = useState(true);
  const [isPaused, setIsPaused] = useState(false);

  const termWindowRef = useRef(null);
  const cursorRef = useRef(cursor);
  cursorRef.current = cursor;
  const isPausedRef = useRef(isPaused);
  isPausedRef.current = isPaused;

  const fetchLogs = async () => {
    if (isPausedRef.current) return;
    try {
      const d = await terminalApi.getLogs(cursorRef.current);
      if (d.cursor !== undefined) {
        setCursor(d.cursor);
      }
      if (d.logs && d.logs.length > 0) {
        const lines = d.logs.split('\n').filter(l => l.trim());
        const newEntries = lines.map(parseLogLine).filter(Boolean);
        setLogs(prev => {
          const merged = [...prev, ...newEntries];
          return merged.length > MAX_LOG_LINES ? merged.slice(merged.length - MAX_LOG_LINES) : merged;
        });
      }
    } catch (e) {}
  };

  useEffect(() => {
    let timer = null;
    if (isActive) {
      fetchLogs();
      timer = setInterval(fetchLogs, 900);
    }
    return () => {
      if (timer) clearInterval(timer);
    };
  }, [isActive]);

  useEffect(() => {
    if (autoScroll && termWindowRef.current) {
      termWindowRef.current.scrollTop = termWindowRef.current.scrollHeight;
    }
  }, [logs, autoScroll]);

  const handleLevelChange = (val) => {
    setMinLevel(val);
    if (val !== 'all') {
      terminalApi.setLevel(val).catch(() => {});
    }
  };

  const handleClearLocal = () => {
    setLogs([]);
  };

  const handleClearDevice = async () => {
    if (!confirm('确认清空端侧循环日志缓冲区？')) return;
    try {
      await terminalApi.clearLogs();
      setLogs([]);
      setCursor(0);
    } catch (e) {
      alert('清空失败');
    }
  };

  const handleExport = () => {
    const text = logs.map(e => e.raw).join('\n');
    const blob = new Blob([text], { type: 'text/plain;charset=utf-8' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = 'phoenix_logs_' + new Date().toISOString().replace(/[:.]/g, '-') + '.log';
    a.click();
    URL.revokeObjectURL(url);
  };

  const filteredLogs = logs.filter(entry => matchesFilter(entry, filterKw.trim().toLowerCase(), minLevel));

  return (
    <div className="card highlight">
      <div className="card-head">
        <div className="card-title">📜 实时系统日志终端 (Live Kernel & App Logs)</div>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <span className="status-badge" style={{ color: isPaused ? 'var(--muted)' : 'var(--accent-sub)' }}>
            {isPaused ? '⏸️ 已暂停' : '● 按需监听中'}
          </span>
          <span style={{ fontSize: '11px', color: 'var(--muted)' }}>Cursor: {cursor} B</span>
        </div>
      </div>

      {/* 日志工具栏 */}
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', flexWrap: 'wrap', gap: '8px', marginBottom: '10px' }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '6px', flex: 1, minWidth: '280px' }}>
          <select value={minLevel} onChange={(e) => handleLevelChange(e.target.value)} style={{ width: 'auto', padding: '6px 10px', fontSize: '12px' }}>
            <option value="all">🔍 显示: 全部级别</option>
            <option value="INFO">🟢 仅 INFO 及以上</option>
            <option value="WARN">🟡 仅 WARN 及以上</option>
            <option value="ERROR">🔴 仅 ERROR 错误</option>
            <option value="DEBUG">🔵 DEBUG 及以上</option>
          </select>
          <input
            type="text"
            placeholder="关键词检索过滤..."
            value={filterKw}
            onChange={(e) => setFilterKw(e.target.value)}
            style={{ padding: '6px 10px', fontSize: '12px', flex: 1 }}
          />
        </div>
        <div style={{ display: 'flex', gap: '6px', flexWrap: 'wrap' }}>
          <button
            className="btn-secondary"
            style={{ padding: '6px 10px', fontSize: '11px', borderColor: autoScroll ? 'var(--accent)' : 'var(--border)' }}
            onClick={() => setAutoScroll(!autoScroll)}
          >
            {autoScroll ? '⏬ 滚屏: 开' : '⏸️ 滚屏: 关'}
          </button>
          <button className="btn-secondary" style={{ padding: '6px 10px', fontSize: '11px' }} onClick={() => setIsPaused(!isPaused)}>
            {isPaused ? '▶️ 继续' : '⏸️ 暂停'}
          </button>
          <button className="btn-secondary" style={{ padding: '6px 10px', fontSize: '11px' }} onClick={handleClearLocal}>
            🧹 清屏
          </button>
          <button className="btn-warn" style={{ padding: '6px 10px', fontSize: '11px' }} onClick={handleClearDevice}>
            🗑️ 清空端侧缓存
          </button>
          <button className="btn-secondary" style={{ padding: '6px 10px', fontSize: '11px' }} onClick={handleExport}>
            💾 导出日志
          </button>
        </div>
      </div>

      {/* 终端控制台 */}
      <div className="terminal-window" ref={termWindowRef}>
        <div className="log-line" style={{ color: 'var(--muted)' }}>
          [SYSTEM] 连接到 Phoenix HoloDesk-S1 实时日志流 (增量游标已对齐)...
        </div>
        {filteredLogs.map((entry, idx) => (
          <div key={idx} className="log-line">
            <span className={`log-lvl-${entry.level}`}>{entry.raw}</span>
          </div>
        ))}
      </div>
    </div>
  );
}
