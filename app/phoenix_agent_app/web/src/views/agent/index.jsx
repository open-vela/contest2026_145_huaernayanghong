import React, { useState, useEffect, useRef } from 'react';
import { agentApi } from '../../api/agent.js';
import { showToast } from '../../utils/toast.js';
import { copyText, formatBytes } from '../../utils/helpers.js';

export function Agent({ onOpenToolModal, pausePolling, resumePolling }) {
  const [messages, setMessages] = useState([]);
  const [prompt, setPrompt] = useState('');
  const [loading, setLoading] = useState(false);
  const [onlineState, setOnlineState] = useState('IDLE');

  // 工具池
  const [tools, setTools] = useState([]);

  // 两级记忆
  const [memoryOpen, setMemoryOpen] = useState(false);
  const [memoryData, setMemoryData] = useState({ history_count: 0, total_bytes: 0, context_summary: '', messages: [] });

  const timelineRef = useRef(null);

  const loadTools = async () => {
    try {
      const d = await agentApi.getTools();
      if (d && d.success) {
        setTools(d.tools || []);
      }
    } catch (e) {}
  };

  const loadMemory = async () => {
    try {
      const d = await agentApi.getMemory();
      if (d && d.success) {
        setMemoryData(d);
      }
    } catch (e) {}
  };

  useEffect(() => {
    loadTools();
    loadMemory();
  }, []);

  useEffect(() => {
    if (timelineRef.current) {
      timelineRef.current.scrollTop = timelineRef.current.scrollHeight;
    }
  }, [messages, loading]);

  const handleSendChat = async (textToSend) => {
    const text = (textToSend || prompt).trim();
    if (!text) return;

    setPrompt('');
    setLoading(true);
    setOnlineState('THINKING');

    // 追加用户消息
    const userMsg = {
      id: Date.now() + '_user',
      role: 'user',
      content: text,
      time: new Date().toLocaleTimeString()
    };
    setMessages(prev => [...prev, userMsg]);

    if (pausePolling) pausePolling();

    try {
      const d = await agentApi.chat(text);
      setLoading(false);
      if (resumePolling) resumePolling();

      const agentMsg = {
        id: Date.now() + '_agent',
        role: 'agent',
        state: d.state || 'IDLE',
        answer: d.answer || (d.error ? `⚠️ ${d.error}` : '(灵眸已处理指令，未生成文本答复)'),
        reasoning_content: d.reasoning_content,
        action: d.action,
        metrics: d.metrics
      };

      setMessages(prev => [...prev, agentMsg]);
      setOnlineState(d.state || 'IDLE');
      loadMemory();
    } catch (e) {
      setLoading(false);
      if (resumePolling) resumePolling();
      setOnlineState('ERROR');
      showToast('Agent 调度异常: ' + e.message);
    }
  };

  const handleClearMemory = async () => {
    if (!confirm('确认清空 Agent 当前会话历史与前情摘要？设备将重置为初始纯净状态。')) return;
    try {
      await agentApi.clearMemory();
      showToast('✅ Agent 会话记忆已清空！');
      setMessages([]);
      loadMemory();
    } catch (e) {
      showToast('清空失败');
    }
  };

  return (
    <div className="card highlight">
      <div className="card-head">
        <div className="card-title">🤖 具身灵眸 Agent 极客工作台 (ReAct 对话流)</div>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <span className="status-badge" id="agentModelBadge">DeepSeek-Chat / 端云协同</span>
          <span className="status-badge" style={{
            background: '#091b2c',
            color: onlineState === 'ERROR' ? '#ff5555' : (onlineState === 'THINKING' ? '#f1fa8c' : 'var(--accent-sub)')
          }}>
            ● {onlineState}
          </span>
          <button className="btn-secondary" style={{ padding: '4px 10px', fontSize: '11px' }} onClick={handleClearMemory}>
            🧹 清空会话
          </button>
        </div>
      </div>

      {/* 现代化 ReAct 对话时间线 */}
      <div className="chat-timeline" ref={timelineRef}>
        {messages.length === 0 && (
          <div className="chat-welcome-card">
            <div className="chat-welcome-title">✨ Phoenix HoloDesk-S1 具身数字生命体已就绪</div>
            <div className="chat-welcome-desc">
              采用 DeepSeek 端云协同大模型驱动，具备 ReAct 自主决策闭环与全志 R528-S3 板载外设驱动调用能力。<br />
              支持自然语言实时多轮对话、深度思维链胶囊折叠、端侧具身动作卡片与两级长程记忆滚动。
            </div>
          </div>
        )}

        {messages.map(m => {
          if (m.role === 'user') {
            return (
              <div key={m.id} className="chat-msg user">
                <div className="chat-bubble">
                  <div className="chat-sender-row" style={{ marginBottom: '4px', paddingBottom: '3px' }}>
                    <span className="chat-sender-name" style={{ color: '#9bc0e7' }}>👤 开发者</span>
                    <span style={{ fontSize: '10px', color: 'rgba(255,255,255,0.4)' }}>{m.time}</span>
                  </div>
                  <div style={{ whiteSpace: 'pre-wrap' }}>{m.content}</div>
                </div>
              </div>
            );
          }

          const lat = m.metrics?.latency_ms || 0;
          const pTokens = m.metrics?.prompt_tokens || 0;
          const cTokens = m.metrics?.completion_tokens || 0;
          const tTokens = m.metrics?.total_tokens || 0;

          return (
            <div key={m.id} className="chat-msg agent">
              <div className="chat-bubble">
                <div className="chat-sender-row">
                  <span className="chat-sender-name">🤖 灵眸 (HoloDesk-S1)</span>
                  <div className="chat-sender-pills">
                    <span className="status-badge" style={{ fontSize: '10px', padding: '2px 8px' }}>STATE: {m.state}</span>
                    <span className="status-badge" style={{ background: '#132338', color: 'var(--accent-sub)', fontSize: '10px', padding: '2px 8px' }}>
                      {lat} ms
                    </span>
                  </div>
                </div>

                {/* 思维链 */}
                {m.reasoning_content && (
                  <details className="think-pill" open>
                    <summary>🧠 DeepSeek 思维链 (Thinking Process) ▾</summary>
                    <div className="think-pill-content">{m.reasoning_content}</div>
                  </details>
                )}

                {/* 具身动作卡片 */}
                {m.action?.tool_name && (
                  <div className="tool-action-card">
                    <div className="tool-action-head">
                      <span className="tool-action-title">🛠️ 具身动作调用: {m.action.tool_name}</span>
                      <span className="status-badge" style={{ background: '#132338', color: '#f1fa8c', fontSize: '10px' }}>
                        ✅ 硬件驱动已闭环
                      </span>
                    </div>
                    <details className="tool-action-details">
                      <summary>查看参数与硬件反馈详情 (Arguments & Observation) ▾</summary>
                      <div style={{ fontSize: '10.5px', color: 'var(--muted)', marginTop: '4px' }}>入参 Arguments:</div>
                      <pre className="tool-action-pre" style={{ color: '#50fa7b' }}>
                        {typeof m.action.arguments === 'string' ? m.action.arguments : JSON.stringify(m.action.arguments, null, 2)}
                      </pre>
                      <div style={{ fontSize: '10.5px', color: 'var(--muted)' }}>反馈 Observation:</div>
                      <pre className="tool-action-pre" style={{ color: '#8be9fd' }}>
                        {typeof m.action.observation === 'string' ? m.action.observation : JSON.stringify(m.action.observation, null, 2)}
                      </pre>
                    </details>
                  </div>
                )}

                <div className="chat-answer-text">{m.answer}</div>

                <div className="chat-metrics-bar">
                  <span>⚡ 往返耗时: <b>{lat}</b>ms | 📊 Tokens: {tTokens} (P:{pTokens} C:{cTokens})</span>
                  <button className="chat-copy-btn" onClick={() => copyText(m.answer)}>📋 复制</button>
                </div>
              </div>
            </div>
          );
        })}

        {/* 思考中卡片 */}
        {loading && (
          <div className="chat-msg agent">
            <div className="chat-bubble">
              <div className="chat-sender-row">
                <span className="chat-sender-name">🤖 灵眸正在认知决策...</span>
                <span className="status-badge" style={{ fontSize: '10px', padding: '2px 8px', color: 'var(--accent-warn)' }}>
                  EXECUTING
                </span>
              </div>
              <div className="stepper-box">
                <div className="step-item active"><span className="step-dot-pulse"></span> 🧠 端云协同认知推理 (DeepSeek ReAct)...</div>
                <div className="step-item"><span className="step-dot-wait"></span> 🛠️ 具身硬件工具调度与闭环驱动...</div>
                <div className="step-item"><span className="step-dot-wait"></span> 💬 生成拟人化具身表达...</div>
              </div>
            </div>
          </div>
        )}
      </div>

      {/* 对话输入中枢 (Dock) */}
      <div className="agent-dock">
        {/* 快捷意图芯片 */}
        <div style={{ display: 'flex', flexWrap: 'wrap', gap: '6px' }}>
          {[
            { prompt: '帮我敲3下赛博木鱼积攒功德', label: '📿 敲木鱼 (功德+3)' },
            { prompt: '检查一下开发板当前的内存和CPU负载', label: '🩺 硬件健康遥测' },
            { prompt: '开启一个25分钟的专注番茄钟', label: '⏱️ 25分钟番茄专注' },
            { prompt: '切换眼球微表情为开心状态', label: '😊 灵眸开心表情' },
            { prompt: '用一句话介绍你的具身智能与端侧能力', label: '🤖 具身人设自我介绍' },
          ].map(item => (
            <span
              key={item.label}
              className="chip"
              onClick={() => handleSendChat(item.prompt)}
            >
              {item.label}
            </span>
          ))}
        </div>

        {/* 弹性自适应输入行 */}
        <div className="agent-dock-input-row">
          <textarea
            rows={1}
            placeholder="输入自然语言指令，调度具身智能与端侧工具闭环 (Enter 发送，Shift+Enter 换行)..."
            value={prompt}
            onChange={(e) => setPrompt(e.target.value)}
            onKeyDown={(e) => {
              if (e.key === 'Enter' && !e.shiftKey) {
                e.preventDefault();
                handleSendChat();
              }
            }}
          />
          <button
            style={{ whiteSpace: 'nowrap', padding: '10px 18px', height: '42px' }}
            disabled={loading}
            onClick={() => handleSendChat()}
          >
            {loading ? '🧠 调度中...' : '🚀 调度 Agent'}
          </button>
        </div>

        <div className="agent-dock-hint">
          <span>💡 提示：按 <b>Enter</b> 发送，<b>Shift + Enter</b> 换行。端侧硬件调用与思维链透明可见。</span>
          <span style={{ color: '#637996' }}>ReAct 闭环就绪</span>
        </div>
      </div>

      {/* 工具池 Playground */}
      <div style={{ marginTop: '16px' }}>
        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '10px' }}>
          <div style={{ fontSize: '13px', fontWeight: 'bold', color: 'var(--accent)' }}>
            🛠️ 端侧具身工具池与单步直调 (Tool Playground)
          </div>
          <button className="btn-secondary" style={{ margin: 0, padding: '4px 10px', fontSize: '11px' }} onClick={loadTools}>
            🔄 刷新工具
          </button>
        </div>
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(260px, 1fr))', gap: '12px' }}>
          {tools.length === 0 ? (
            <div style={{ color: 'var(--muted)', fontSize: '12px' }}>正在加载已注册端侧工具...</div>
          ) : (
            tools.map(t => {
              let defaultArgs = '{}';
              if (t.name === 'knock_wooden_fish') defaultArgs = '{"count": 1}';
              else if (t.name === 'manage_pomodoro') defaultArgs = '{"action":"start","minutes":25}';
              else if (t.name === 'set_eye_emotion') defaultArgs = '{"emotion":"happy"}';
              else if (t.name === 'launch_app') defaultArgs = '{"app_name":"赛博木鱼"}';

              return (
                <div key={t.name} className="tool-item-card">
                  <div>
                    <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '4px' }}>
                      <b style={{ color: 'var(--accent)', fontSize: '12.5px' }}>{t.name}</b>
                      <span className="status-badge" style={{ fontSize: '10px', padding: '2px 8px' }}>C Driver</span>
                    </div>
                    <div style={{ fontSize: '11.5px', color: '#a2b9d5', lineHeight: '1.4', marginBottom: '8px' }}>
                      {t.description || ''}
                    </div>
                  </div>
                  <div style={{ display: 'flex', gap: '6px' }}>
                    <button
                      className="btn-secondary"
                      style={{ margin: 0, padding: '6px 10px', fontSize: '11px', flex: 1 }}
                      onClick={() => onOpenToolModal({ name: t.name, defaultArgs })}
                    >
                      ⚡ 单步直调测试
                    </button>
                  </div>
                </div>
              );
            })
          )}
        </div>
      </div>

      {/* 两级记忆自省 */}
      <div style={{ marginTop: '16px', background: '#090d16', border: '1px solid var(--border)', borderRadius: '8px', padding: '12px' }}>
        <div
          style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', cursor: 'pointer' }}
          onClick={() => setMemoryOpen(!memoryOpen)}
        >
          <div style={{ fontSize: '12px', fontWeight: 'bold', color: '#c0d2e8', display: 'flex', alignItems: 'center', gap: '6px' }}>
            <span>🧠 两级长程记忆自省 (Two-Tier Memory)</span>
            <span className="status-badge">{memoryData.history_count || 0} 轮会话</span>
            <span style={{ fontSize: '11px', color: 'var(--muted)' }}>{formatBytes(memoryData.total_bytes || 0)}</span>
          </div>
          <span style={{ fontSize: '11px', color: 'var(--accent)' }}>
            {memoryOpen ? '▲ 收起记忆详情' : '▼ 展开记忆详情'}
          </span>
        </div>

        {memoryOpen && (
          <div style={{ marginTop: '10px', borderTop: '1px solid var(--border-light)', paddingTop: '8px' }}>
            <div style={{ fontSize: '11px', color: 'var(--muted)', marginBottom: '4px' }}>前情增量摘要卡片 (context_summary):</div>
            <div style={{
              background: '#05080f',
              padding: '8px',
              borderRadius: '6px',
              fontSize: '11.5px',
              color: memoryData.context_summary ? '#f1fa8c' : 'var(--muted)',
              marginBottom: '8px',
              wordBreak: 'break-all'
            }}>
              {memoryData.context_summary || '(暂无前情摘要，记忆仍在近程滑动窗口内)'}
            </div>
            <div style={{ fontSize: '11px', color: 'var(--muted)', marginBottom: '4px' }}>滑动窗口上下文消息:</div>
            <div style={{ maxHeight: '160px', overflowY: 'auto', fontSize: '11.5px', display: 'flex', flexDirection: 'column', gap: '6px' }}>
              {!memoryData.messages || memoryData.messages.length === 0 ? (
                <div style={{ color: 'var(--muted)' }}>(暂无历史会话)</div>
              ) : (
                memoryData.messages.map((m, idx) => {
                  const roleColor = m.role === 'user' ? '#00e5ff' : (m.role === 'tool' ? '#50fa7b' : '#ffb86c');
                  let snippet = m.content || '';
                  if (snippet.length > 100) snippet = snippet.substring(0, 100) + '...';

                  return (
                    <div key={idx} style={{ background: '#05080f', padding: '6px 10px', borderRadius: '6px', borderLeft: `3px solid ${roleColor}` }}>
                      <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '2px' }}>
                        <b style={{ color: roleColor, fontSize: '11px' }}>{m.role?.toUpperCase()}</b>
                        {m.tool_name && <span style={{ color: '#f1fa8c', fontSize: '10px' }}>🛠️ {m.tool_name}</span>}
                      </div>
                      <div style={{ color: '#c9d1d9' }}>{snippet}</div>
                    </div>
                  );
                })
              )}
            </div>
          </div>
        )}
      </div>
    </div>
  );
}
