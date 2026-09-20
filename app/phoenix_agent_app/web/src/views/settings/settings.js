import { configApi } from '../../api/config.js';
import { showToast } from '../../utils/toast.js';

function escapeHtml(s) {
  if (!s) return '';
  return String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
}

export const settingsView = {
  loadConfig: async () => {
    try {
      const d = await configApi.getConfig();
      if (!d) return;

      // LLM 字段
      if (d.base_url) document.getElementById('baseUrl').value = d.base_url;
      if (d.model) {
        document.getElementById('modelName').value = d.model;
        const b = document.getElementById('agentModelBadge');
        if (b) b.innerText = d.model + ' / 端云协同';
      }
      if (d.has_key && d.api_key_masked) {
        document.getElementById('apiKey').placeholder = '已配置 (' + d.api_key_masked + ')';
      }
      if (d.prompt) document.getElementById('agentPrompt').value = d.prompt;
      if (d.temperature !== undefined) {
        document.getElementById('tempRange').value = d.temperature;
        document.getElementById('tempTxt').innerText = (d.temperature / 100).toFixed(1);
      }

      // ASR 字段
      if (d.asr_base_url) document.getElementById('asrBaseUrl').value = d.asr_base_url;
      if (d.asr_model) {
        document.getElementById('asrModelName').value = d.asr_model;
        const b = document.getElementById('asrBackendBadge');
        if (b) b.innerText = d.asr_model;
      }
      if (d.has_asr_key && d.asr_api_key_masked) {
        document.getElementById('asrApiKey').placeholder = '已配置 (' + d.asr_api_key_masked + ')';
      }
      if (d.asr_backend) document.getElementById('asrBackendSelect').value = d.asr_backend;

      // TTS 字段
      if (d.tts_base_url) document.getElementById('ttsBaseUrl').value = d.tts_base_url;
      if (d.tts_model) {
        document.getElementById('ttsModelName').value = d.tts_model;
        const b = document.getElementById('ttsBackendBadge');
        if (b) b.innerText = d.tts_model;
      }
      if (d.tts_voice) document.getElementById('ttsVoiceName').value = d.tts_voice;
      if (d.has_tts_key && d.tts_api_key_masked) {
        document.getElementById('ttsApiKey').placeholder = '已配置 (' + d.tts_api_key_masked + ')';
      }
      if (d.tts_backend) document.getElementById('ttsBackendSelect').value = d.tts_backend;
    } catch (e) {}
  },

  applyLlmPreset: (type) => {
    const urlEl = document.getElementById('baseUrl');
    const modelEl = document.getElementById('modelName');
    if (type === 'deepseek') {
      urlEl.value = 'https://api.deepseek.com/v1/chat/completions';
      modelEl.value = 'deepseek-chat';
    } else if (type === 'siliconflow') {
      urlEl.value = 'https://api.siliconflow.cn/v1/chat/completions';
      modelEl.value = 'deepseek-ai/DeepSeek-V3';
    } else if (type === 'ollama') {
      urlEl.value = 'http://192.168.1.100:11434/v1/chat/completions';
      modelEl.value = 'deepseek-r1:8b';
    } else if (type === 'openai') {
      urlEl.value = 'https://api.openai.com/v1/chat/completions';
      modelEl.value = 'gpt-4o-mini';
    }
    showToast('已填入 ' + type + ' 预设');
  },

  applyAsrPreset: (type) => {
    const urlEl = document.getElementById('asrBaseUrl');
    const modelEl = document.getElementById('asrModelName');
    const backendEl = document.getElementById('asrBackendSelect');
    if (type === 'groq') {
      urlEl.value = 'https://api.groq.com/openai/v1/audio/transcriptions';
      modelEl.value = 'whisper-large-v3';
      backendEl.value = 'cloud';
    } else if (type === 'openai') {
      urlEl.value = 'https://api.openai.com/v1/audio/transcriptions';
      modelEl.value = 'whisper-1';
      backendEl.value = 'cloud';
    } else if (type === 'sensevoice') {
      urlEl.value = 'https://api.siliconflow.cn/v1/audio/transcriptions';
      modelEl.value = 'FunAudioLLM/SenseVoiceSmall';
      backendEl.value = 'cloud';
    } else if (type === 'mock') {
      backendEl.value = 'mock';
    }
    showToast('已填入 ASR ' + type + ' 预设');
  },

  applyTtsPreset: (type) => {
    const urlEl = document.getElementById('ttsBaseUrl');
    const modelEl = document.getElementById('ttsModelName');
    const voiceEl = document.getElementById('ttsVoiceName');
    const backendEl = document.getElementById('ttsBackendSelect');
    if (type === 'openai') {
      urlEl.value = 'https://api.openai.com/v1/audio/speech';
      modelEl.value = 'tts-1';
      voiceEl.value = 'alloy';
      backendEl.value = 'cloud';
    } else if (type === 'siliconflow') {
      urlEl.value = 'https://api.siliconflow.cn/v1/audio/speech';
      modelEl.value = 'FunAudioLLM/CosyVoice2-0.5B';
      voiceEl.value = 'alex';
      backendEl.value = 'cloud';
    } else if (type === 'edge') {
      urlEl.value = 'https://api.openai.com/v1/audio/speech';
      modelEl.value = 'tts-1';
      voiceEl.value = 'zh-CN-XiaoxiaoNeural';
      backendEl.value = 'cloud';
    } else if (type === 'mock') {
      backendEl.value = 'mock';
    }
    showToast('已填入 TTS ' + type + ' 预设');
  },

  setPromptTemplate: (type) => {
    const p = document.getElementById('agentPrompt');
    if (!p) return;
    if (type === 'geek') {
      p.value = '你是一个贴心的赛博桌面极客助手，语气温和简短，具备嵌入式硬件感知与具身工具调度能力。';
    } else if (type === 'pet') {
      p.value = '你是一只居住在桌面屏幕里的傲娇赛博猫咪使魔，说话简短毒舌但很黏主人，喜欢被摸头。';
    } else if (type === 'study') {
      p.value = '你是一个严厉专注的学习与考研督导员，随时提醒主人保持专注，合理规划番茄钟。';
    }
  },

  toggleKeyVisibility: (inputId, btnId) => {
    const k = document.getElementById(inputId);
    const btn = document.getElementById(btnId);
    if (!k || !btn) return;
    if (k.type === 'password') {
      k.type = 'text';
      btn.innerText = '🔒 隐藏';
    } else {
      k.type = 'password';
      btn.innerText = '👁️ 显示';
    }
  },

  testLlmPing: async () => {
    const btn = document.getElementById('pingTestBtn');
    const badge = document.getElementById('pingResultBadge');
    const url = document.getElementById('baseUrl')?.value.trim();
    const model = document.getElementById('modelName')?.value.trim();
    const key = document.getElementById('apiKey')?.value.trim();

    if (btn) {
      btn.disabled = true;
      btn.innerText = '⏳ 探测中...';
    }
    if (badge) {
      badge.style.display = 'block';
      badge.innerHTML = '<span style="color:#f1fa8c">正在向目标大模型端点发送轻量 Ping 探测包...</span>';
    }

    try {
      const d = await configApi.testConfig({ base_url: url, model: model, api_key: key });
      if (btn) {
        btn.disabled = false;
        btn.innerText = '⚡ 测试连通性';
      }
      if (badge) {
        if (d.success) {
          badge.innerHTML = `<span style="color:var(--accent-sub); font-weight:bold;">✅ LLM 连通成功！HTTP ${d.http_status || 200} (往返延时: ${d.latency_ms || 0} ms)</span>`;
        } else {
          badge.innerHTML = `<span style="color:var(--accent-danger); font-weight:bold;">❌ LLM 连通失败: ${escapeHtml(d.error || '无法访问服务')} (HTTP ${d.http_status || 0})</span>`;
        }
      }
    } catch (e) {
      if (btn) {
        btn.disabled = false;
        btn.innerText = '⚡ 测试连通性';
      }
      if (badge) badge.innerHTML = `<span style="color:var(--accent-danger);">❌ 网络请求错误: ${e.message}</span>`;
    }
  },

  testAsrPing: async () => {
    const btn = document.getElementById('asrPingBtn');
    const badge = document.getElementById('asrPingResultBadge');
    const url = document.getElementById('asrBaseUrl')?.value.trim();
    const model = document.getElementById('asrModelName')?.value.trim();
    const key = document.getElementById('asrApiKey')?.value.trim();

    if (btn) {
      btn.disabled = true;
      btn.innerText = '⏳ 探测中...';
    }
    if (badge) {
      badge.style.display = 'block';
      badge.innerHTML = '<span style="color:#f1fa8c">正在向 ASR 语音识别端点探测连通性...</span>';
    }

    try {
      const d = await configApi.testConfig({ asr_base_url: url, asr_model: model, asr_api_key: key });
      if (btn) {
        btn.disabled = false;
        btn.innerText = '⚡ 测试连通性';
      }
      if (badge) {
        if (d.asr_success) {
          badge.innerHTML = `<span style="color:var(--accent-sub); font-weight:bold;">✅ ASR 端点连通正常！HTTP ${d.asr_http_status || 200} (延时: ${d.asr_latency_ms || 0} ms)</span>`;
        } else {
          badge.innerHTML = `<span style="color:var(--accent-danger); font-weight:bold;">❌ ASR 探测失败: ${escapeHtml(d.asr_error || '无法访问')} (HTTP ${d.asr_http_status || 0})</span>`;
        }
      }
    } catch (e) {
      if (btn) {
        btn.disabled = false;
        btn.innerText = '⚡ 测试连通性';
      }
      if (badge) badge.innerHTML = `<span style="color:var(--accent-danger);">❌ 请求失败: ${e.message}</span>`;
    }
  },

  testAsrTranscribe: async () => {
    const btn = document.getElementById('asrTranscribeBtn');
    const resBox = document.getElementById('asrTestResult');
    if (btn) {
      btn.disabled = true;
      btn.innerText = '⏳ 正在转写音频...';
    }
    if (resBox) resBox.innerText = '正在调用 ASR 模型进行语音转文字识别...';

    try {
      const d = await configApi.testAsrTranscribe();
      if (btn) {
        btn.disabled = false;
        btn.innerText = '🎙️ 立即执行转写测试';
      }
      if (resBox) {
        if (d.success) {
          resBox.innerText = `[识别成功 (${Math.round((d.wav_bytes || 0) / 1024)} KB)] "${d.text}"`;
          showToast('✅ ASR 语音转文字识别成功！');
        } else {
          resBox.innerText = '❌ 识别失败: ' + (d.error || '未知错误');
          showToast('⚠️ ASR 转写失败');
        }
      }
    } catch (e) {
      if (btn) {
        btn.disabled = false;
        btn.innerText = '🎙️ 立即执行转写测试';
      }
      if (resBox) resBox.innerText = '❌ 网络请求错误: ' + e.message;
    }
  },

  testTtsPing: async () => {
    const btn = document.getElementById('ttsPingBtn');
    const badge = document.getElementById('ttsPingResultBadge');
    const url = document.getElementById('ttsBaseUrl')?.value.trim();
    const model = document.getElementById('ttsModelName')?.value.trim();
    const voice = document.getElementById('ttsVoiceName')?.value.trim();
    const key = document.getElementById('ttsApiKey')?.value.trim();

    if (btn) {
      btn.disabled = true;
      btn.innerText = '⏳ 探测中...';
    }
    if (badge) {
      badge.style.display = 'block';
      badge.innerHTML = '<span style="color:#f1fa8c">正在向 TTS 语音合成端点探测连通性...</span>';
    }

    try {
      const d = await configApi.testConfig({ tts_base_url: url, tts_model: model, tts_voice: voice, tts_api_key: key });
      if (btn) {
        btn.disabled = false;
        btn.innerText = '⚡ 测试连通性';
      }
      if (badge) {
        if (d.tts_success) {
          badge.innerHTML = `<span style="color:var(--accent-sub); font-weight:bold;">✅ TTS 端点连通正常！HTTP ${d.tts_http_status || 200} (延时: ${d.tts_latency_ms || 0} ms)</span>`;
        } else {
          badge.innerHTML = `<span style="color:var(--accent-danger); font-weight:bold;">❌ TTS 探测失败: ${escapeHtml(d.tts_error || '无法访问')} (HTTP ${d.tts_http_status || 0})</span>`;
        }
      }
    } catch (e) {
      if (btn) {
        btn.disabled = false;
        btn.innerText = '⚡ 测试连通性';
      }
      if (badge) badge.innerHTML = `<span style="color:var(--accent-danger);">❌ 请求失败: ${e.message}</span>`;
    }
  },

  testTtsSpeak: async () => {
    const btn = document.getElementById('ttsSpeakBtn');
    const text = document.getElementById('ttsTestInput')?.value.trim();
    const playOnDev = document.getElementById('ttsPlayOnDevice')?.checked;
    const player = document.getElementById('ttsAudioPlayer');

    if (!text) {
      showToast('请输入待合成的文本内容');
      return;
    }

    if (btn) {
      btn.disabled = true;
      btn.innerText = '⏳ 正在合成语音...';
    }

    try {
      const d = await configApi.testTtsSpeak({ text, play_on_device: playOnDev });
      if (btn) {
        btn.disabled = false;
        btn.innerText = '🔊 合成并播放语音';
      }
      if (d.success) {
        showToast(`✅ TTS 合成成功 (${Math.round((d.wav_bytes || 0) / 1024)} KB)`);
        if (player) {
          player.style.display = 'block';
          player.src = (d.audio_url || '/api/audio/tts_download') + '?t=' + Date.now();
          player.load();
          player.play().catch(() => {});
        }
      } else {
        showToast('⚠️ TTS 合成失败: ' + (d.error || ''));
      }
    } catch (e) {
      if (btn) {
        btn.disabled = false;
        btn.innerText = '🔊 合成并播放语音';
      }
      showToast('网络错误: ' + e.message);
    }
  },

  saveAgentConfig: async () => {
    const payload = {
      base_url: document.getElementById('baseUrl')?.value.trim(),
      model: document.getElementById('modelName')?.value.trim(),
      api_key: document.getElementById('apiKey')?.value.trim(),
      prompt: document.getElementById('agentPrompt')?.value.trim(),
      temperature: parseInt(document.getElementById('tempRange')?.value || 70),
      asr_base_url: document.getElementById('asrBaseUrl')?.value.trim(),
      asr_model: document.getElementById('asrModelName')?.value.trim(),
      asr_api_key: document.getElementById('asrApiKey')?.value.trim(),
      asr_backend: document.getElementById('asrBackendSelect')?.value,
      tts_base_url: document.getElementById('ttsBaseUrl')?.value.trim(),
      tts_model: document.getElementById('ttsModelName')?.value.trim(),
      tts_voice: document.getElementById('ttsVoiceName')?.value.trim(),
      tts_api_key: document.getElementById('ttsApiKey')?.value.trim(),
      tts_backend: document.getElementById('ttsBackendSelect')?.value
    };

    try {
      await configApi.saveConfig(payload);
      showToast('✅ LLM、ASR、TTS 全套配置已热加载至开发板！');
      settingsView.loadConfig();
    } catch (e) {
      showToast('保存配置失败: ' + e.message);
    }
  },

  resetWifi: () => {
    if (confirm('确认重置 Wi-Fi 并返回 SoftAP 独立热点配网模式？')) {
      configApi.resetWifi().then(() => {
        alert('设备正在重置网络并开启配网热点...');
      }).catch(() => {
        alert('重置网络失败');
      });
    }
  }
};
