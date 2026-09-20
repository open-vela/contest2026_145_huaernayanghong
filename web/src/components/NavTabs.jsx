import React from 'react';

const TABS = [
  { id: 'overview', label: '📊 系统监控 & 概览' },
  { id: 'agent', label: '🤖 灵眸具身工坊 (ReAct)' },
  { id: 'settings', label: '⚙️ 大模型 & 系统配置' },
  { id: 'storage', label: '📁 TF卡文件管理' },
  { id: 'terminal', label: '📜 实时系统日志终端' },
  { id: 'audio', label: '🎙️ 声学实验室 (Audio Lab)' },
];

export function NavTabs({ activeTab, onChangeTab }) {
  return (
    <nav className="nav-tabs">
      {TABS.map(tab => (
        <button
          key={tab.id}
          className={`tab-btn ${activeTab === tab.id ? 'active' : ''}`}
          onClick={() => onChangeTab(tab.id)}
        >
          {tab.label}
        </button>
      ))}
    </nav>
  );
}
