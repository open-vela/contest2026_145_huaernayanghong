import React from 'react';

export function Header({ network }) {
  const ssid = network?.ssid || (network?.mode === 'SOFTAP' ? 'SoftAP 独立配网模式' : '已连接局域网');
  const ip = network?.ip || '192.168.4.1';

  return (
    <header className="top-header">
      <div className="brand-title">
        <span>💎 Phoenix HoloDesk-S1</span>
        <span style={{ fontSize: '12px', color: 'var(--muted)', fontWeight: 'normal' }}>
          极客伴侣看板
        </span>
      </div>
      <div className="header-status">
        <div className="status-badge" id="netStatusBadge">
          <span className="pulse-dot"></span>
          <span>{ssid}</span>
        </div>
        <div className="status-badge" style={{ color: 'var(--muted)' }}>
          IP: {ip}
        </div>
      </div>
    </header>
  );
}
