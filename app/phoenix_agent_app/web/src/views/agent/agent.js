import { agentApi } from '../../api/agent.js';
import { showToast } from '../../utils/toast.js';
import { copyText, formatBytes, autoExpandTextarea } from '../../utils/helpers.js';
import { modal } from '../../utils/modal.js';

function escapeHtml(s) {
  if (!s) return '';
  return String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
}

let memoryDrawerOpen = false;

export const agentView = {
  handleInputKey: (e) => {
    if (e.key === 'Enter' && !e.shiftKey) {
      e.preventDefault();
      agentView.sendAgentChat();
    }
  },

  setAndSendPrompt: (txt) => {
    const inp = document.getElementById('agentPromptInput');
    if (inp) {
      inp.value = txt;
      autoExpandTextarea(inp);
    }
    agentView.sendAgentChat();
  },

  appendUserChatBubble: (prompt) => {
    const welcome = document.getElementById('agentWelcomeCard');
    if (welcome) welcome.style.display = 'none';

    const tl = document.getElementById('agentChatTimeline');
    if (!tl) return;
    const div = document.createElement('div');
    div.className = 'chat-msg user';
    div.innerHTML = `
      <div class="chat-bubble">
        <div class="chat-sender-row" style="margin-bottom:4px; padding-bottom:3px;">
          <span class="chat-sender-name" style="color:#9bc0e7">👤 开发者</span>
          <span style="font-size:10px; color:rgba(255,255,255,0.4);">${new Date().toLocaleTimeString()}</span>
        </div>
        <div style="white-space:pre-wrap;">${escapeHtml(prompt)}</div>
      </div>
    `;
    tl.appendChild(div);
    tl.scrollTop = tl.scrollHeight;
  },

  showAgentThinkingCard: () => {
    const tl = document.getElementById('agentChatTimeline');
    if (!tl) return;
    let card = document.getElementById('agentThinkingCard');
    if (!card) {
      card = document.createElement('div');
      card.id = 'agentThinkingCard';
      card.className = 'chat-msg agent';
      card.innerHTML = `
        <div class="chat-bubble">
          <div class="chat-sender-row">
            <span class="chat-sender-name">🤖 灵眸正在认知决策...</span>
            <span class="status-badge" style="font-size:10px; padding:2px 8px; color:var(--accent-warn);">EXECUTING</span>
          </div>
          <div class="stepper-box">
            <div class="step-item active"><span class="step-dot-pulse"></span> 🧠 端云协同认知推理 (DeepSeek ReAct)...</div>
            <div class="step-item"><span class="step-dot-wait"></span> 🛠️ 具身硬件工具调度与闭环驱动...</div>
            <div class="step-item"><span class="step-dot-wait"></span> 💬 生成拟人化具身表达...</div>
          </div>
        </div>
      `;
      tl.appendChild(card);
    }
    card.style.display = 'flex';
    tl.scrollTop = tl.scrollHeight;
  },

  removeAgentThinkingCard: () => {
    const card = document.getElementById('agentThinkingCard');
    if (card) card.remove();
  },

  appendAgentResponseBubble: (d) => {
    const tl = document.getElementById('agentChatTimeline');
    if (!tl) return;
    const div = document.createElement('div');
    div.className = 'chat-msg agent';

    const stateStr = d.state || 'IDLE';
    const lat = d.metrics ? d.metrics.latency_ms : 0;
    const pTokens = d.metrics ? d.metrics.prompt_tokens : 0;
    const cTokens = d.metrics ? d.metrics.completion_tokens : 0;
    const tTokens = d.metrics ? d.metrics.total_tokens : 0;

    // 1. Thinking Pill
    let thinkingHtml = '';
    if (d.reasoning_content && d.reasoning_content.trim().length > 0) {
      thinkingHtml = `
        <details class="think-pill" open>
          <summary>🧠 DeepSeek 思维链 (Thinking Process) ▾</summary>
          <div class="think-pill-content">${escapeHtml(d.reasoning_content)}</div>
        </details>
      `;
    }

    // 2. Embodied Action Card
    let actionHtml = '';
    if (d.action && d.action.tool_name) {
      const argsStr = typeof d.action.arguments === 'string' ? d.action.arguments : JSON.stringify(d.action.arguments, null, 2);
      const obsStr = typeof d.action.observation === 'string' ? d.action.observation : JSON.stringify(d.action.observation, null, 2);
      actionHtml = `
        <div class="tool-action-card">
          <div class="tool-action-head">
            <span class="tool-action-title">🛠️ 具身动作调用: ${escapeHtml(d.action.tool_name)}</span>
            <span class="status-badge" style="background:#132338; color:#f1fa8c; font-size:10px;">✅ 硬件驱动已闭环</span>
          </div>
          <details class="tool-action-details">
            <summary>查看参数与硬件反馈详情 (Arguments & Observation) ▾</summary>
            <div style="font-size:10.5px; color:var(--muted); margin-top:4px;">入参 Arguments:</div>
            <pre class="tool-action-pre" style="color:#50fa7b;">${escapeHtml(argsStr)}</pre>
            <div style="font-size:10.5px; color:var(--muted);">反馈 Observation:</div>
            <pre class="tool-action-pre" style="color:#8be9fd;">${escapeHtml(obsStr)}</pre>
          </details>
        </div>
      `;
    }

    // 3. Final Answer
    const answerText = d.answer || '(灵眸已处理指令，未生成文本答复)';

    div.innerHTML = `
      <div class="chat-bubble">
        <div class="chat-sender-row">
          <span class="chat-sender-name">🤖 灵眸 (HoloDesk-S1)</span>
          <div class="chat-sender-pills">
            <span class="status-badge" style="font-size:10px; padding:2px 8px;">STATE: ${escapeHtml(stateStr)}</span>
            <span class="status-badge" style="background:#132338; color:var(--accent-sub); font-size:10px; padding:2px 8px;">${lat} ms</span>
          </div>
        </div>

        ${thinkingHtml}
        ${actionHtml}

        <div class="chat-answer-text">${escapeHtml(answerText)}</div>

        <div class="chat-metrics-bar">
          <span>⚡ 往返耗时: <b>${lat}</b>ms | 📊 Tokens: ${tTokens} (P:${pTokens} C:${cTokens})</span>
          <button class="chat-copy-btn" data-copy-answer="${escapeHtml(answerText)}">📋 复制</button>
        </div>
      </div>
    `;

    tl.appendChild(div);
    tl.scrollTop = tl.scrollHeight;

    div.querySelector('[data-copy-answer]')?.addEventListener('click', () => {
      copyText(answerText);
    });
  },

  sendAgentChat: async (pausePollingCallback, resumePollingCallback) => {
    const inp = document.getElementById('agentPromptInput');
    const prompt = inp?.value.trim();
    if (!prompt) return;

    inp.value = '';
    autoExpandTextarea(inp);

    const btn = document.getElementById('sendAgentBtn');
    if (btn) {
      btn.disabled = true;
      btn.innerText = '🧠 调度中...';
    }

    const stateBadge = document.getElementById('agentOnlineStateBadge');
    if (stateBadge) {
      stateBadge.innerText = '⏳ THINKING / EXECUTING';
      stateBadge.style.color = '#f1fa8c';
    }

    agentView.appendUserChatBubble(prompt);
    agentView.showAgentThinkingCard();

    if (pausePollingCallback) pausePollingCallback();

    try {
      const d = await agentApi.chat(prompt);
      if (btn) {
        btn.disabled = false;
        btn.innerText = '🚀 调度 Agent';
      }
      agentView.removeAgentThinkingCard();
      if (resumePollingCallback) resumePollingCallback();

      if (d.error && d.error.indexOf('思考中') !== -1) {
        showToast(d.error);
        if (stateBadge) {
          stateBadge.innerText = '⏳ THINKING';
          stateBadge.style.color = '#f1fa8c';
        }
        agentView.appendAgentResponseBubble(d);
        return;
      }

      if (stateBadge) {
        stateBadge.innerText = '● ' + (d.state || 'IDLE');
        stateBadge.style.color = 'var(--accent-sub)';
      }

      agentView.appendAgentResponseBubble(d);
      agentView.refreshAgentMemory();
    } catch (e) {
      if (btn) {
        btn.disabled = false;
        btn.innerText = '🚀 调度 Agent';
      }
      agentView.removeAgentThinkingCard();
      if (resumePollingCallback) resumePollingCallback();
      if (stateBadge) {
        stateBadge.innerText = '❌ 请求异常';
        stateBadge.style.color = '#ff5555';
      }
      showToast('Agent 调度异常: ' + e.message);
    }
  },

  refreshAgentTools: async () => {
    try {
      const d = await agentApi.getTools();
      const grid = document.getElementById('agentToolsGrid');
      if (!grid) return;

      if (!d.success || !d.tools || d.tools.length === 0) {
        grid.innerHTML = '<div style="color:var(--muted); font-size:12px;">暂无已注册具身工具</div>';
        return;
      }

      let html = '';
      d.tools.forEach(t => {
        let defaultArgs = '{}';
        if (t.name === 'knock_wooden_fish') defaultArgs = '{"count": 1}';
        else if (t.name === 'manage_pomodoro') defaultArgs = '{"action":"start","minutes":25}';
        else if (t.name === 'set_eye_emotion') defaultArgs = '{"emotion":"happy"}';
        else if (t.name === 'launch_app') defaultArgs = '{"app_name":"赛博木鱼"}';

        html += `
          <div class="tool-item-card">
            <div>
              <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:4px;">
                <b style="color:var(--accent); font-size:12.5px;">${escapeHtml(t.name)}</b>
                <span class="status-badge" style="font-size:10px; padding:2px 8px;">C Driver</span>
              </div>
              <div style="font-size:11.5px; color:#a2b9d5; line-height:1.4; margin-bottom:8px;">${escapeHtml(t.description || '')}</div>
            </div>
            <div style="display:flex; gap:6px;">
              <button class="btn-secondary" style="margin:0; padding:6px 10px; font-size:11px; flex:1;" data-tool-exec="${escapeHtml(t.name)}" data-tool-args="${escapeHtml(defaultArgs)}">⚡ 单步直调测试</button>
            </div>
          </div>
        `;
      });
      grid.innerHTML = html;

      grid.querySelectorAll('[data-tool-exec]').forEach(el => {
        el.addEventListener('click', () => {
          modal.openToolExec(el.dataset.toolExec, el.dataset.toolArgs);
        });
      });
    } catch (e) {
      const grid = document.getElementById('agentToolsGrid');
      if (grid) grid.innerHTML = '<div style="color:#ff5555; font-size:12px;">拉取工具清单失败</div>';
    }
  },

  toggleMemoryDrawer: () => {
    memoryDrawerOpen = !memoryDrawerOpen;
    const drawer = document.getElementById('memoryDrawer');
    const toggleTxt = document.getElementById('memToggleTxt');
    if (drawer) drawer.style.display = memoryDrawerOpen ? 'block' : 'none';
    if (toggleTxt) toggleTxt.innerText = memoryDrawerOpen ? '▲ 收起记忆详情' : '▼ 展开记忆详情';
    if (memoryDrawerOpen) agentView.refreshAgentMemory();
  },

  refreshAgentMemory: async () => {
    try {
      const d = await agentApi.getMemory();
      if (!d.success) return;

      const countBadge = document.getElementById('memCountBadge');
      const watermarkBadge = document.getElementById('memWatermarkBadge');
      if (countBadge) countBadge.innerText = (d.history_count || 0) + ' 轮会话';
      if (watermarkBadge) watermarkBadge.innerText = formatBytes(d.total_bytes || 0);

      const sumBox = document.getElementById('memSummaryBox');
      if (sumBox) {
        if (d.context_summary && d.context_summary.trim().length > 0) {
          sumBox.innerText = d.context_summary;
          sumBox.style.color = '#f1fa8c';
        } else {
          sumBox.innerText = '(暂无前情摘要，记忆仍在近程滑动窗口内)';
          sumBox.style.color = 'var(--muted)';
        }
      }

      const msgList = document.getElementById('memMessagesList');
      if (!msgList) return;
      if (!d.messages || d.messages.length === 0) {
        msgList.innerHTML = '<div style="color:var(--muted)">(暂无历史会话)</div>';
        return;
      }

      let mHtml = '';
      d.messages.forEach(m => {
        let roleColor = m.role === 'user' ? '#00e5ff' : (m.role === 'tool' ? '#50fa7b' : '#ffb86c');
        let contentSnippet = escapeHtml(m.content || '');
        if (contentSnippet.length > 100) contentSnippet = contentSnippet.substring(0, 100) + '...';

        mHtml += `
          <div style="background:#05080f; padding:6px 10px; border-radius:6px; border-left:3px solid ${roleColor};">
            <div style="display:flex; justify-content:space-between; margin-bottom:2px;">
              <b style="color:${roleColor}; font-size:11px;">${m.role.toUpperCase()}</b>
              ${m.tool_name ? '<span style="color:#f1fa8c; font-size:10px;">🛠️ ' + escapeHtml(m.tool_name) + '</span>' : ''}
            </div>
            <div style="color:#c9d1d9;">${contentSnippet}</div>
          </div>
        `;
      });
      msgList.innerHTML = mHtml;
    } catch (e) {}
  },

  clearAgentMemory: async () => {
    if (!confirm('确认清空 Agent 当前会话历史与前情摘要？设备将重置为初始纯净状态。')) return;
    try {
      await agentApi.clearMemory();
      showToast('✅ Agent 会话记忆已清空！');
      agentView.refreshAgentMemory();
      const tl = document.getElementById('agentChatTimeline');
      if (tl) {
        tl.innerHTML = `
          <div class="chat-welcome-card" id="agentWelcomeCard">
            <div class="chat-welcome-title">✨ 灵眸具身智能已重置就绪</div>
            <div class="chat-welcome-desc">
              历史上下文已清空。您可以直接在下方输入自然语言指令，或点击快捷芯片唤醒端侧具身能力。
            </div>
          </div>
        `;
      }
    } catch (e) {
      showToast('清空失败');
    }
  }
};
