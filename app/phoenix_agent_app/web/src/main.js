// 1. 样式引入
import './styles/variables.css';
import './styles/base.css';
import './styles/components.css';
import './styles/views/agent.css';
import './styles/views/terminal.css';

// 2. HTML 模版引入
import headerHtml from './components/Header.html?raw';
import modalsHtml from './components/Modals.html?raw';
import overviewHtml from './views/overview/overview.html?raw';
import agentHtml from './views/agent/agent.html?raw';
import settingsHtml from './views/settings/settings.html?raw';
import storageHtml from './views/storage/storage.html?raw';
import terminalHtml from './views/terminal/terminal.html?raw';
import audioHtml from './views/audio/audio.html?raw';

// 3. 视图控制器与工具
import { overviewView } from './views/overview/overview.js';
import { agentView } from './views/agent/agent.js';
import { settingsView } from './views/settings/settings.js';
import { storageView } from './views/storage/storage.js';
import { terminalView } from './views/terminal/terminal.js';
import { audioView } from './views/audio/audio.js';
import { modal } from './utils/modal.js';
import { autoExpandTextarea } from './utils/helpers.js';
import { systemApi } from './api/system.js';

// 当前路由视图
let currentView = 'overview';
let statusPollTimer = null;

// Tab 切换逻辑
function switchView(viewId) {
  currentView = viewId;
  document.querySelectorAll('.view-panel').forEach(p => p.classList.remove('active'));
  document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));

  const activePanel = document.getElementById('view-' + viewId);
  if (activePanel) activePanel.classList.add('active');

  const activeTab = document.querySelector(`.tab-btn[data-view="${viewId}"]`);
  if (activeTab) activeTab.classList.add('active');

  // 按需轮询控制
  if (viewId === 'terminal') {
    terminalView.startPolling();
  } else {
    terminalView.stopPolling();
  }

  if (viewId === 'audio') {
    audioView.startPolling();
  } else {
    audioView.stopPolling();
  }

  if (viewId === 'storage') {
    storageView.sdcardRefreshList();
  }

  if (viewId === 'settings') {
    settingsView.loadConfig();
  }

  if (viewId === 'agent') {
    agentView.refreshAgentTools();
    agentView.refreshAgentMemory();
  }
}

// 避让高频轮询（Agent 推理时保护嵌入式 TCP 连接池）
function pausePolling() {
  terminalView.stopPolling();
  if (statusPollTimer) {
    clearInterval(statusPollTimer);
    statusPollTimer = null;
  }
}

function resumePolling() {
  if (currentView === 'terminal') {
    terminalView.startPolling();
  }
  if (!statusPollTimer) {
    statusPollTimer = setInterval(() => {
      overviewView.refreshStatus(storageView.updateStorageInfo);
    }, 2000);
  }
}

// 初始化挂载 DOM
function mountApp() {
  const appContainer = document.getElementById('app');
  if (!appContainer) return;

  appContainer.innerHTML = `
    ${headerHtml}
    <main class="main-wrap">
      <div class="view-panel active" id="view-overview">${overviewHtml}</div>
      <div class="view-panel" id="view-agent">${agentHtml}</div>
      <div class="view-panel" id="view-settings">${settingsHtml}</div>
      <div class="view-panel" id="view-storage">${storageHtml}</div>
      <div class="view-panel" id="view-terminal">${terminalHtml}</div>
      <div class="view-panel" id="view-audio">${audioHtml}</div>
    </main>
    ${modalsHtml}
  `;

  bindEvents();
}

// 绑定各视图事件
function bindEvents() {
  // 1. Tab 路由
  document.querySelectorAll('.tab-btn[data-view]').forEach(btn => {
    btn.addEventListener('click', () => switchView(btn.dataset.view));
  });

  // 2. Overview 视图事件
  document.getElementById('volRange')?.addEventListener('input', (e) => {
    document.getElementById('volTxt').innerText = e.target.value + '%';
  });
  document.getElementById('volRange')?.addEventListener('change', overviewView.syncHardwareCtrl);

  document.getElementById('brightRange')?.addEventListener('input', (e) => {
    document.getElementById('brightTxt').innerText = e.target.value + '%';
  });
  document.getElementById('brightRange')?.addEventListener('change', overviewView.syncHardwareCtrl);

  document.getElementById('btnTriggerProactive')?.addEventListener('click', overviewView.triggerProactiveDemo);

  document.querySelectorAll('[data-cartridge]').forEach(btn => {
    btn.addEventListener('click', () => overviewView.switchCartridge(btn.dataset.cartridge));
  });

  document.getElementById('btnPetFamiliar')?.addEventListener('click', () => overviewView.doCartridgeAction('pet'));
  document.getElementById('btnAddMemo')?.addEventListener('click', overviewView.addMemo);
  document.getElementById('memoInput')?.addEventListener('keydown', (e) => {
    if (e.key === 'Enter') overviewView.addMemo();
  });

  document.getElementById('btnTogglePomo')?.addEventListener('click', () => overviewView.doCartridgeAction('pomo_toggle'));
  document.getElementById('btnKnockFish')?.addEventListener('click', () => overviewView.doCartridgeAction('knock_fish'));

  document.getElementById('btnSyncWeather')?.addEventListener('click', overviewView.syncWeatherFetch);
  document.getElementById('btnSaveWeatherCity')?.addEventListener('click', overviewView.saveWeatherCity);
  document.querySelectorAll('[data-city]').forEach(el => {
    el.addEventListener('click', () => overviewView.setWeatherCity(el.dataset.city));
  });

  document.getElementById('btnClearDoneTodos')?.addEventListener('click', overviewView.clearDoneTodos);
  document.getElementById('btnAddTodo')?.addEventListener('click', overviewView.addTodo);
  document.getElementById('todoTitleInput')?.addEventListener('keydown', (e) => {
    if (e.key === 'Enter') overviewView.addTodo();
  });
  document.getElementById('todoTimeInput')?.addEventListener('keydown', (e) => {
    if (e.key === 'Enter') overviewView.addTodo();
  });

  // 3. Agent 视图事件
  document.getElementById('btnClearAgentMemory')?.addEventListener('click', agentView.clearAgentMemory);
  document.querySelectorAll('[data-quick-prompt]').forEach(chip => {
    chip.addEventListener('click', () => agentView.setAndSendPrompt(chip.dataset.quickPrompt));
  });

  const promptInput = document.getElementById('agentPromptInput');
  if (promptInput) {
    promptInput.addEventListener('keydown', (e) => {
      if (e.key === 'Enter' && !e.shiftKey) {
        e.preventDefault();
        agentView.sendAgentChat(pausePolling, resumePolling);
      }
    });
    promptInput.addEventListener('input', () => autoExpandTextarea(promptInput));
  }

  document.getElementById('sendAgentBtn')?.addEventListener('click', () => {
    agentView.sendAgentChat(pausePolling, resumePolling);
  });
  document.getElementById('btnRefreshTools')?.addEventListener('click', agentView.refreshAgentTools);
  document.getElementById('memoryHeaderBar')?.addEventListener('click', agentView.toggleMemoryDrawer);

  // 4. Settings 视图事件
  document.querySelectorAll('[data-llm-preset]').forEach(el => {
    el.addEventListener('click', () => settingsView.applyLlmPreset(el.dataset.llmPreset));
  });
  document.querySelectorAll('[data-asr-preset]').forEach(el => {
    el.addEventListener('click', () => settingsView.applyAsrPreset(el.dataset.asrPreset));
  });
  document.querySelectorAll('[data-tts-preset]').forEach(el => {
    el.addEventListener('click', () => settingsView.applyTtsPreset(el.dataset.ttsPreset));
  });
  document.querySelectorAll('[data-prompt-template]').forEach(el => {
    el.addEventListener('click', () => settingsView.setPromptTemplate(el.dataset.promptTemplate));
  });

  document.getElementById('toggleKeyBtn')?.addEventListener('click', () => settingsView.toggleKeyVisibility('apiKey', 'toggleKeyBtn'));
  document.getElementById('toggleAsrKeyBtn')?.addEventListener('click', () => settingsView.toggleKeyVisibility('asrApiKey', 'toggleAsrKeyBtn'));
  document.getElementById('toggleTtsKeyBtn')?.addEventListener('click', () => settingsView.toggleKeyVisibility('ttsApiKey', 'toggleTtsKeyBtn'));

  document.getElementById('pingTestBtn')?.addEventListener('click', settingsView.testLlmPing);
  document.getElementById('asrPingBtn')?.addEventListener('click', settingsView.testAsrPing);
  document.getElementById('asrTranscribeBtn')?.addEventListener('click', settingsView.testAsrTranscribe);
  document.getElementById('ttsPingBtn')?.addEventListener('click', settingsView.testTtsPing);
  document.getElementById('ttsSpeakBtn')?.addEventListener('click', settingsView.testTtsSpeak);

  document.getElementById('tempRange')?.addEventListener('input', (e) => {
    const val = (e.target.value / 100).toFixed(1);
    const txt = document.getElementById('tempTxt');
    if (txt) txt.innerText = val;
  });

  document.getElementById('btnSaveConfig')?.addEventListener('click', settingsView.saveAgentConfig);
  document.getElementById('btnResetWifi')?.addEventListener('click', settingsView.resetWifi);

  // 5. Storage 视图事件
  document.getElementById('btnSdcardGoUp')?.addEventListener('click', storageView.sdcardGoUp);
  document.getElementById('btnSdcardRefresh')?.addEventListener('click', storageView.sdcardRefreshList);
  document.querySelectorAll('[data-quick-path]').forEach(btn => {
    btn.addEventListener('click', () => storageView.sdcardGoPath(btn.dataset.quickPath));
  });
  document.getElementById('btnSdcardMkdir')?.addEventListener('click', storageView.sdcardMkdir);
  document.getElementById('btnSelectLocalFile')?.addEventListener('click', () => {
    document.getElementById('localFileInput')?.click();
  });
  document.getElementById('localFileInput')?.addEventListener('change', (e) => {
    storageView.handleLocalFileSelected(e.target);
  });
  document.getElementById('btnUploadText')?.addEventListener('click', storageView.sdcardUploadText);

  // 6. Terminal 视图事件
  document.getElementById('logLevelSelect')?.addEventListener('change', (e) => {
    terminalView.changeLogLevel(e.target.value);
  });
  document.getElementById('logFilterInput')?.addEventListener('input', terminalView.applyLogFilter);
  document.getElementById('autoScrollBtn')?.addEventListener('click', terminalView.toggleAutoScroll);
  document.getElementById('pauseLogBtn')?.addEventListener('click', terminalView.toggleLogPause);
  document.getElementById('btnClearLocalLogs')?.addEventListener('click', terminalView.clearLocalLogs);
  document.getElementById('btnClearDeviceLogs')?.addEventListener('click', terminalView.clearDeviceLogs);
  document.getElementById('btnExportLogs')?.addEventListener('click', terminalView.exportLogs);

  // 7. Audio 视图事件
  document.getElementById('btnRecStart5')?.addEventListener('click', () => audioView.record(5000));
  document.getElementById('btnRecStart10')?.addEventListener('click', () => audioView.record(10000));
  document.getElementById('btnRecStop')?.addEventListener('click', audioView.recordStop);
  document.getElementById('btnPlayRec')?.addEventListener('click', audioView.playRecord);
  document.getElementById('btnDownloadWav')?.addEventListener('click', audioView.downloadWav);
  document.getElementById('btnTone1s')?.addEventListener('click', () => audioView.playTone(1000, 1000));
  document.getElementById('btnTone3s')?.addEventListener('click', () => audioView.playTone(1000, 3000));
  document.getElementById('btnToneStop')?.addEventListener('click', audioView.playStop);
  document.getElementById('btnLoopbackToggle')?.addEventListener('click', audioView.toggleLoopback);

  const volSlider = document.getElementById('audioVolumeSlider');
  if (volSlider) {
    volSlider.addEventListener('input', (e) => audioView.onVolumeSlide(e.target.value));
    volSlider.addEventListener('change', (e) => audioView.setVolume(e.target.value));
  }
  document.querySelectorAll('[data-vol-preset]').forEach(btn => {
    btn.addEventListener('click', () => audioView.setVolume(btn.dataset.volPreset));
  });

  // 8. Modals 事件
  document.getElementById('btnCloseToolModal')?.addEventListener('click', modal.closeToolExec);
  document.getElementById('btnFormatModalArgs')?.addEventListener('click', modal.formatArgs);
  document.getElementById('modalExecBtn')?.addEventListener('click', () => {
    modal.executeModalTool(() => overviewView.refreshStatus(storageView.updateStorageInfo));
  });

  // 页面可见性挂起策略：离开浏览器前台时降频轮询
  document.addEventListener('visibilitychange', () => {
    if (document.hidden) {
      if (statusPollTimer) clearInterval(statusPollTimer);
      statusPollTimer = setInterval(() => {
        overviewView.refreshStatus(storageView.updateStorageInfo);
      }, 5000);
    } else {
      if (statusPollTimer) clearInterval(statusPollTimer);
      statusPollTimer = setInterval(() => {
        overviewView.refreshStatus(storageView.updateStorageInfo);
      }, 2000);
      overviewView.refreshStatus(storageView.updateStorageInfo);
    }
  });
}

// 启动应用
function init() {
  mountApp();

  // 首次拉取与轮询
  overviewView.refreshStatus(storageView.updateStorageInfo);
  statusPollTimer = setInterval(() => {
    overviewView.refreshStatus(storageView.updateStorageInfo);
  }, 2000);

  // 同步客户端时间
  systemApi.syncTime(Math.floor(Date.now() / 1000)).catch(() => {});
}

// DOM 就绪启动
if (document.readyState === 'loading') {
  document.addEventListener('DOMContentLoaded', init);
} else {
  init();
}
