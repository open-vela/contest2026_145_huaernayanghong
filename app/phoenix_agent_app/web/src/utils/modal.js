import { showToast } from './toast.js';
import { agentApi } from '../api/agent.js';

let activeTool = null;

export const modal = {
  openToolExec: (name, defaultArgs) => {
    activeTool = name;
    const title = document.getElementById('modalToolTitle');
    const desc = document.getElementById('modalToolDesc');
    const args = document.getElementById('modalToolArgs');
    const resSec = document.getElementById('modalResultSection');
    const resTxt = document.getElementById('modalToolResult');
    const execBtn = document.getElementById('modalExecBtn');
    const modalEl = document.getElementById('toolExecModal');

    if (title) title.innerText = '⚡ 调试端侧具身驱动: ' + name;
    if (desc) desc.innerText = '将直接调度全志 R528 底层硬件驱动，绕过大模型端云决策，用于快速单元排查外设闭环。';
    if (args) args.value = defaultArgs || '{}';
    if (resSec) resSec.style.display = 'none';
    if (resTxt) resTxt.innerText = '';
    if (execBtn) {
      execBtn.disabled = false;
      execBtn.innerText = '🚀 立即触发执行';
    }
    if (modalEl) modalEl.style.display = 'flex';
  },

  closeToolExec: () => {
    const modalEl = document.getElementById('toolExecModal');
    if (modalEl) modalEl.style.display = 'none';
    activeTool = null;
  },

  formatArgs: () => {
    const ta = document.getElementById('modalToolArgs');
    if (!ta) return;
    try {
      const obj = JSON.parse(ta.value.trim() || '{}');
      ta.value = JSON.stringify(obj, null, 2);
    } catch (e) {
      showToast('⚠️ JSON 语法格式不正确');
    }
  },

  executeModalTool: async (onSuccess) => {
    if (!activeTool) return;
    const argsVal = document.getElementById('modalToolArgs')?.value.trim() || '{}';
    const btn = document.getElementById('modalExecBtn');
    const resSec = document.getElementById('modalResultSection');
    const resTxt = document.getElementById('modalToolResult');

    if (btn) {
      btn.disabled = true;
      btn.innerText = '⚙️ 驱动执行中...';
    }

    try {
      const d = await agentApi.executeTool(activeTool, argsVal);
      if (btn) {
        btn.disabled = false;
        btn.innerText = '🚀 立即触发执行';
      }
      if (resSec) resSec.style.display = 'block';
      const output = typeof d.result === 'object' ? JSON.stringify(d.result, null, 2) : (d.result || '{}');
      if (resTxt) resTxt.innerText = output;
      showToast('✅ 驱动调用完成');
      if (onSuccess) onSuccess();
    } catch (e) {
      if (btn) {
        btn.disabled = false;
        btn.innerText = '🚀 立即触发执行';
      }
      showToast('执行异常: ' + e.message);
    }
  }
};
