import React, { useState, useEffect } from 'react';
import { agentApi } from '../api/agent.js';
import { showToast } from '../utils/toast.js';

export function ToolExecModal({ isOpen, tool, onClose, onSuccess }) {
  const [argsText, setArgsText] = useState('{}');
  const [loading, setLoading] = useState(false);
  const [result, setResult] = useState(null);

  useEffect(() => {
    if (tool) {
      setArgsText(tool.defaultArgs || '{}');
      setResult(null);
      setLoading(false);
    }
  }, [tool]);

  if (!isOpen || !tool) return null;

  const handleFormat = () => {
    try {
      const obj = JSON.parse(argsText.trim() || '{}');
      setArgsText(JSON.stringify(obj, null, 2));
    } catch (e) {
      showToast('⚠️ JSON 语法格式不正确');
    }
  };

  const handleExecute = async () => {
    setLoading(true);
    try {
      const d = await agentApi.executeTool(tool.name, argsText);
      setLoading(false);
      setResult(typeof d.result === 'object' ? JSON.stringify(d.result, null, 2) : (d.result || '{}'));
      showToast('✅ 驱动调用完成');
      if (onSuccess) onSuccess();
    } catch (e) {
      setLoading(false);
      showToast('执行异常: ' + e.message);
    }
  };

  return (
    <div className="modal-overlay" style={{ display: 'flex' }}>
      <div className="modal-dialog">
        <div className="modal-head">
          <div className="modal-title">⚡ 调试端侧具身驱动: {tool.name}</div>
          <button className="modal-close" onClick={onClose}>✕</button>
        </div>
        <div style={{ fontSize: '12px', color: 'var(--muted)', marginBottom: '10px' }}>
          将直接调度全志 R528 底层硬件驱动，绕过大模型端云决策，用于快速单元排查外设闭环。
        </div>
        <div style={{ fontSize: '11px', color: '#a2b9d5', marginBottom: '4px', fontWeight: 600 }}>
          输入驱动入参 (JSON 格式):
        </div>
        <textarea
          rows={4}
          style={{ fontFamily: 'var(--font-mono)', fontSize: '12px', marginBottom: '10px' }}
          value={argsText}
          onChange={(e) => setArgsText(e.target.value)}
        />

        <div style={{ display: 'flex', gap: '8px', justifyContent: 'flex-end', marginBottom: '12px' }}>
          <button className="btn-secondary" style={{ padding: '6px 12px', fontSize: '11.5px' }} onClick={handleFormat}>
            📐 格式化 JSON
          </button>
          <button style={{ padding: '6px 14px', fontSize: '11.5px' }} disabled={loading} onClick={handleExecute}>
            {loading ? '⚙️ 驱动执行中...' : '🚀 立即触发执行'}
          </button>
        </div>

        {result !== null && (
          <div style={{ borderTop: '1px solid var(--border-light)', paddingTop: '10px' }}>
            <div style={{ fontSize: '11px', fontWeight: 600, color: 'var(--accent-sub)', marginBottom: '4px' }}>
              📥 硬件驱动返回 (Observation):
            </div>
            <pre style={{
              background: '#05080f',
              padding: '10px',
              borderRadius: '6px',
              fontFamily: 'var(--font-mono)',
              fontSize: '11.5px',
              color: '#8be9fd',
              maxHeight: '180px',
              overflowY: 'auto',
              margin: 0
            }}>
              {result}
            </pre>
          </div>
        )}
      </div>
    </div>
  );
}
