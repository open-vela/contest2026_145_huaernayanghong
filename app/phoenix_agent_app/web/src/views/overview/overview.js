import { systemApi } from '../../api/system.js';
import { showToast } from '../../utils/toast.js';

function escapeHtml(s) {
  if (!s) return '';
  return String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
}

export const overviewView = {
  refreshStatus: async (updateStorageCallback) => {
    try {
      const d = await systemApi.getStatus();
      if (!d) return;

      // 1. 网络状态
      if (d.network) {
        const ssidEl = document.getElementById('netSsidTxt');
        const ipEl = document.getElementById('netIpTxt');
        if (ssidEl) ssidEl.innerText = d.network.ssid || (d.network.mode === 'SOFTAP' ? 'SoftAP 独立配网模式' : '已连接局域网');
        if (ipEl) ipEl.innerText = 'IP: ' + (d.network.ip || '192.168.4.1');
      }

      // 2. 硬件遥测
      if (d.telemetry) {
        const t = d.telemetry;
        const cpuLoad = document.getElementById('cpuLoadVal');
        const cpuFreq = document.getElementById('cpuFreqVal');
        const memUsed = document.getElementById('memUsedVal');
        const memFree = document.getElementById('memFreeVal');
        const temp = document.getElementById('tempVal');
        const fps = document.getElementById('fpsBadge');
        const uptime = document.getElementById('uptimeVal');
        const osVer = document.getElementById('osVerVal');

        if (cpuLoad) cpuLoad.innerText = (t.cpu_load_pct || 0) + ' %';
        if (cpuFreq) cpuFreq.innerText = (t.cpu_freq_mhz || 1008) + ' MHz';
        if (memUsed) memUsed.innerText = (t.mem_used_pct || 0) + ' %';
        if (memFree) memFree.innerText = 'Free: ' + Math.round((t.mem_free_kb || 0) / 1024) + ' MB';
        if (temp) temp.innerText = (t.temp_c || 42) + ' °C';
        if (fps) fps.innerText = (t.fps || 60) + ' FPS';

        const upSec = t.uptime || 0;
        const h = Math.floor(upSec / 3600);
        const m = Math.floor((upSec % 3600) / 60);
        const s = upSec % 60;
        if (uptime) uptime.innerText = `${h}h ${m}m ${s}s`;
        if (osVer && t.os) osVer.innerText = t.os;
      }

      // 3. 当前卡带
      if (d.active_cartridge) {
        const names = { familiar: '使魔萌宠', memo: '灵感外脑', clock: '番茄时钟', zen: '赛博木鱼', home: '主页', agent: '灵眸AI' };
        const hint = document.getElementById('activeCartridgeHint');
        if (hint) hint.innerText = '当前真机屏幕: ' + (names[d.active_cartridge] || d.active_cartridge);
        document.querySelectorAll('[id^="pill-cartridge-"]').forEach(p => {
          p.classList.toggle('active', p.id === 'pill-cartridge-' + d.active_cartridge);
        });
      }

      // 4. 卡带状态数据
      const c = d.cartridges || {};
      const s = d.stats || {};
      const aff = document.getElementById('affinityVal');
      const memoCount = document.getElementById('memoCountTag');
      const merit = document.getElementById('meritVal');
      const pomoRemain = document.getElementById('pomoRemain');
      const pomoStatus = document.getElementById('pomoStatusTag');
      const tokens = document.getElementById('tokensVal');

      if (c.familiar && c.familiar.affinity !== undefined && aff) aff.innerText = c.familiar.affinity;
      if (c.memo && c.memo.count !== undefined && memoCount) memoCount.innerText = c.memo.count + ' 条';
      if (c.zen && (c.zen.total_merit !== undefined || s.merit !== undefined) && merit) {
        merit.innerText = c.zen.total_merit || s.merit || 0;
      }
      if (c.clock && pomoRemain && pomoStatus) {
        const sec = c.clock.remain_s || 0;
        const mm = String(Math.floor(sec / 60)).padStart(2, '0');
        const ss = String(sec % 60).padStart(2, '0');
        pomoRemain.innerText = c.clock.is_pomodoro ? (mm + ':' + ss) : '25:00';
        pomoStatus.innerText = c.clock.is_pomodoro ? '🍅 专注中' : '待机';
      }
      if (tokens) tokens.innerText = s.total_tokens || 0;

      // 5. 存储状态更新回调
      if (d.storage && updateStorageCallback) {
        updateStorageCallback(d.storage);
      }

      // 6. 天气与待办看板刷新
      overviewView.refreshWeather();
      overviewView.refreshTodos();
    } catch (e) {
      // 静默轮询错误
    }
  },

  refreshWeather: async () => {
    try {
      const d = await systemApi.getWeatherStatus();
      if (!d || !d.success) return;

      const tempEl = document.getElementById('weatherTempTxt');
      const condEl = document.getElementById('weatherCondTxt');
      const rangeEl = document.getElementById('weatherRangeTxt');
      const humiEl = document.getElementById('weatherHumiTxt');
      const badgeEl = document.getElementById('weatherSyncBadge');
      const inputEl = document.getElementById('weatherCityInput');
      const timeEl = document.getElementById('weatherUpdatedTime');

      if (tempEl) tempEl.innerText = d.is_valid ? (d.temp_c + '°C') : '--°C';
      if (condEl) condEl.innerText = (d.city || '上海') + ' · ' + (d.condition || '晴');
      if (rangeEl) rangeEl.innerText = d.is_valid ? `${d.temp_min}° ~ ${d.temp_max}°C` : '-- ~ --°C';
      if (humiEl) humiEl.innerText = d.is_valid ? `相对湿度: ${d.humidity}%` : '相对湿度: --%';
      if (badgeEl) {
        badgeEl.innerText = d.is_fetching ? '同步中...' : (d.is_valid ? '实时同步' : '未连接');
        badgeEl.style.color = d.is_fetching ? 'var(--accent-warn)' : 'var(--accent)';
      }
      if (inputEl && !inputEl.matches(':focus') && d.city) {
        inputEl.value = d.city;
      }
      if (timeEl && d.update_time > 0) {
        const dt = new Date(d.update_time * 1000);
        timeEl.innerText = '更新于 ' + dt.toLocaleTimeString();
      }
    } catch (e) {}
  },

  saveWeatherCity: async () => {
    const city = document.getElementById('weatherCityInput')?.value.trim();
    if (!city) { showToast('请输入城市名'); return; }
    try {
      await systemApi.setWeatherConfig(city);
      showToast('🌤️ 城市已变更为 [' + city + '], 硬件屏幕正同步拉取');
      overviewView.refreshWeather();
    } catch (e) {
      showToast('设置失败: ' + e.message);
    }
  },

  setWeatherCity: (cityName) => {
    const el = document.getElementById('weatherCityInput');
    if (el) el.value = cityName;
    overviewView.saveWeatherCity();
  },

  syncWeatherFetch: () => {
    showToast('🔄 正在向气象服务器获取最新数据...');
    overviewView.saveWeatherCity();
  },

  refreshTodos: async () => {
    try {
      const d = await systemApi.getTodoList();
      if (!d || !d.success) return;

      const container = document.getElementById('todoListContainer');
      const badge = document.getElementById('todoProgressBadge');
      if (badge) {
        badge.innerText = `${d.done_count || 0}/${d.total_count || 0} 达成`;
      }
      if (!container) return;

      const todos = d.todos || [];
      if (todos.length === 0) {
        container.innerHTML = '<div style="font-size:12px; color:var(--muted); text-align:center; padding:16px;">暂无待办事项，在下方输入框添加吧！</div>';
        return;
      }

      let html = '';
      todos.forEach(it => {
        const isDone = it.done;
        html += `
          <div style="display:flex; align-items:center; gap:8px; background:var(--card-inner); padding:8px 12px; border-radius:6px; border:1px solid ${isDone ? 'rgba(255,255,255,0.04)' : '#1e3352'}; transition:all 0.2s;">
            <input type="checkbox" ${isDone ? 'checked' : ''} data-todo-toggle="${it.id}" style="cursor:pointer; width:16px; height:16px;">
            <span style="flex:1; font-size:13px; color:${isDone ? 'var(--muted)' : 'var(--text)'}; text-decoration:${isDone ? 'line-through' : 'none'}; font-weight:${isDone ? 'normal' : '600'};">
              ${escapeHtml(it.title)}
            </span>
            <span style="font-size:11px; color:${isDone ? 'var(--muted)' : 'var(--accent-warn)'}; background:#141f33; padding:2px 6px; border-radius:4px;">
              ${escapeHtml(it.time || '今日')}
            </span>
            <button class="btn-secondary" style="padding:2px 6px; font-size:11px; color:#ff5555; border-color:rgba(255,85,85,0.2);" data-todo-delete="${it.id}">✕</button>
          </div>
        `;
      });
      container.innerHTML = html;

      // 事件委托绑定
      container.querySelectorAll('[data-todo-toggle]').forEach(el => {
        el.addEventListener('change', () => overviewView.toggleTodo(el.dataset.todoToggle));
      });
      container.querySelectorAll('[data-todo-delete]').forEach(el => {
        el.addEventListener('click', () => overviewView.deleteTodo(el.dataset.todoDelete));
      });
    } catch (e) {}
  },

  toggleTodo: async (id) => {
    try {
      await systemApi.toggleTodo(id);
      showToast('📝 待办状态已更新');
      overviewView.refreshTodos();
    } catch (e) {
      showToast('更新待办失败: ' + e.message);
    }
  },

  addTodo: async () => {
    const titleInput = document.getElementById('todoTitleInput');
    const timeInput = document.getElementById('todoTimeInput');
    const title = titleInput?.value.trim();
    const time = timeInput?.value.trim();
    if (!title) { showToast('请输入待办事项内容'); return; }

    try {
      const d = await systemApi.addTodo(title, time || '今日');
      if (d.success) {
        if (titleInput) titleInput.value = '';
        if (timeInput) timeInput.value = '';
        showToast('➕ 待办添加成功，已推送到真机桌面');
        overviewView.refreshTodos();
      } else {
        showToast('添加失败: ' + (d.error || ''));
      }
    } catch (e) {
      showToast('添加失败: ' + e.message);
    }
  },

  deleteTodo: async (id) => {
    try {
      await systemApi.deleteTodo(id);
      showToast('🗑️ 待办已删除');
      overviewView.refreshTodos();
    } catch (e) {
      showToast('删除失败: ' + e.message);
    }
  },

  clearDoneTodos: async () => {
    try {
      const d = await systemApi.clearDoneTodos();
      showToast(`🧹 已清理 ${d.cleared_count || 0} 条已完成待办`);
      overviewView.refreshTodos();
    } catch (e) {
      showToast('清理失败: ' + e.message);
    }
  },

  switchCartridge: async (id) => {
    try {
      await systemApi.switchCartridge(id);
      showToast('📺 开发板真机屏幕已切换至: ' + id);
      overviewView.refreshStatus();
    } catch (e) {
      showToast('卡带切换失败');
    }
  },

  doCartridgeAction: async (act) => {
    try {
      await systemApi.doCartridgeAction(act);
      overviewView.refreshStatus();
    } catch (e) {}
  },

  addMemo: async () => {
    const inp = document.getElementById('memoInput');
    const txt = inp?.value.trim();
    if (!txt) return;

    try {
      await systemApi.addMemo(txt);
      const lastMemo = document.getElementById('lastMemoBox');
      if (lastMemo) lastMemo.innerText = txt;
      if (inp) inp.value = '';
      showToast('💡 灵感已推入端侧外脑！');
      overviewView.refreshStatus();
    } catch (e) {
      showToast('保存灵感失败');
    }
  },

  syncHardwareCtrl: () => {
    const vol = parseInt(document.getElementById('volRange')?.value || 80);
    const bright = parseInt(document.getElementById('brightRange')?.value || 90);
    systemApi.setHardwareConfig(vol, bright).catch(() => {});
  },

  triggerProactiveDemo: async () => {
    try {
      await systemApi.triggerProactive();
      showToast('⚡ 已向端侧触发主动式交互演练！');
    } catch (e) {
      showToast('触发主动交互失败');
    }
  }
};
