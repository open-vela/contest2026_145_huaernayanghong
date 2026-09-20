import React, { useState, useEffect } from 'react';
import { configApi } from '../../api/config.js';
import { showToast } from '../../utils/toast.js';

export function Settings() {
  // LLM
  const [baseUrl, setBaseUrl] = useState('');
  const [modelName, setModelName] = useState('');
  const [apiKey, setApiKey] = useState('');
  const [showApiKey, setShowApiKey] = useState(false);
  const [apiKeyPlaceholder, setApiKeyPlaceholder] = useState('输入云端 API Key (如 sk-xxxx)');
  const [agentPrompt, setAgentPrompt] = useState('');
  const [temperature, setTemperature] = useState(70);
  const [llmPingResult, setLlmPingResult] = useState(null);
  const [llmPinging, setLlmPinging] = useState(false);

  // ASR
  const [asrBaseUrl, setAsrBaseUrl] = useState('');
  const [asrModelName, setAsrModelName] = useState('');
  const [asrApiKey, setAsrApiKey] = useState('');
  const [showAsrApiKey, setShowAsrApiKey] = useState(false);
  const [asrApiKeyPlaceholder, setAsrApiKeyPlaceholder] = useState('输入 ASR 专用 API Key');
  const [asrBackend, setAsrBackend] = useState('cloud');
  const [asrPingResult, setAsrPingResult] = useState(null);
  const [asrPinging, setAsrPinging] = useState(false);
  const [asrTranscribeResult, setAsrTranscribeResult] = useState('点击上方 "立即执行转写测试" 验证端云 ASR 识别通路...');
  const [asrTranscribing, setAsrTranscribing] = useState(false);

  // TTS
  const [ttsBaseUrl, setTtsBaseUrl] = useState('');
  const [ttsModelName, setTtsModelName] = useState('');
  const [ttsVoiceName, setTtsVoiceName] = useState('');
  const [ttsApiKey, setTtsApiKey] = useState('');
  const [showTtsApiKey, setShowTtsApiKey] = useState(false);
  const [ttsApiKeyPlaceholder, setTtsApiKeyPlaceholder] = useState('输入 TTS 专用 API Key');
  const [ttsBackend, setTtsBackend] = useState('cloud');
  const [ttsPingResult, setTtsPingResult] = useState(null);
  const [ttsPinging, setTtsPinging] = useState(false);
  const [ttsTestInput, setTtsTestInput] = useState('你好，我是桌面灵眸数字生命体！');
  const [ttsPlayOnDev, setTtsPlayOnDev] = useState(true);
  const [ttsAudioUrl, setTtsAudioUrl] = useState('');
  const [ttsSpeaking, setTtsSpeaking] = useState(false);

  // 加载配置
  const loadConfig = async () => {
    try {
      const d = await configApi.getConfig();
      if (!d) return;

      if (d.base_url) setBaseUrl(d.base_url);
      if (d.model) setModelName(d.model);
      if (d.has_key && d.api_key_masked) setApiKeyPlaceholder(`已配置 (${d.api_key_masked})`);
      if (d.prompt) setAgentPrompt(d.prompt);
      if (d.temperature !== undefined) setTemperature(d.temperature);

      if (d.asr_base_url) setAsrBaseUrl(d.asr_base_url);
      if (d.asr_model) setAsrModelName(d.asr_model);
      if (d.has_asr_key && d.asr_api_key_masked) setAsrApiKeyPlaceholder(`已配置 (${d.asr_api_key_masked})`);
      if (d.asr_backend) setAsrBackend(d.asr_backend);

      if (d.tts_base_url) setTtsBaseUrl(d.tts_base_url);
      if (d.tts_model) setTtsModelName(d.tts_model);
      if (d.tts_voice) setTtsVoiceName(d.tts_voice);
      if (d.has_tts_key && d.tts_api_key_masked) setTtsApiKeyPlaceholder(`已配置 (${d.tts_api_key_masked})`);
      if (d.tts_backend) setTtsBackend(d.tts_backend);
    } catch (e) {}
  };

  useEffect(() => {
    loadConfig();
  }, []);

  // 预设
  const applyLlmPreset = (type) => {
    if (type === 'deepseek') {
      setBaseUrl('https://api.deepseek.com/v1/chat/completions');
      setModelName('deepseek-chat');
    } else if (type === 'siliconflow') {
      setBaseUrl('https://api.siliconflow.cn/v1/chat/completions');
      setModelName('deepseek-ai/DeepSeek-V3');
    } else if (type === 'ollama') {
      setBaseUrl('http://192.168.1.100:11434/v1/chat/completions');
      setModelName('deepseek-r1:8b');
    } else if (type === 'openai') {
      setBaseUrl('https://api.openai.com/v1/chat/completions');
      setModelName('gpt-4o-mini');
    }
    showToast('已填入 ' + type + ' 预设');
  };

  const applyAsrPreset = (type) => {
    if (type === 'groq') {
      setAsrBaseUrl('https://api.groq.com/openai/v1/audio/transcriptions');
      setAsrModelName('whisper-large-v3');
      setAsrBackend('cloud');
    } else if (type === 'openai') {
      setAsrBaseUrl('https://api.openai.com/v1/audio/transcriptions');
      setAsrModelName('whisper-1');
      setAsrBackend('cloud');
    } else if (type === 'sensevoice') {
      setAsrBaseUrl('https://api.siliconflow.cn/v1/audio/transcriptions');
      setAsrModelName('FunAudioLLM/SenseVoiceSmall');
      setAsrBackend('cloud');
    } else if (type === 'mock') {
      setAsrBackend('mock');
    }
    showToast('已填入 ASR ' + type + ' 预设');
  };

  const applyTtsPreset = (type) => {
    if (type === 'openai') {
      setTtsBaseUrl('https://api.openai.com/v1/audio/speech');
      setTtsModelName('tts-1');
      setTtsVoiceName('alloy');
      setTtsBackend('cloud');
    } else if (type === 'siliconflow') {
      setTtsBaseUrl('https://api.siliconflow.cn/v1/audio/speech');
      setTtsModelName('FunAudioLLM/CosyVoice2-0.5B');
      setTtsVoiceName('alex');
      setTtsBackend('cloud');
    } else if (type === 'edge') {
      setTtsBaseUrl('https://api.openai.com/v1/audio/speech');
      setTtsModelName('tts-1');
      setTtsVoiceName('zh-CN-XiaoxiaoNeural');
      setTtsBackend('cloud');
    } else if (type === 'mock') {
      setTtsBackend('mock');
    }
    showToast('已填入 TTS ' + type + ' 预设');
  };

  const setPromptTemplate = (type) => {
    if (type === 'geek') {
      setAgentPrompt('你是一个贴心的赛博桌面极客助手，语气温和简短，具备嵌入式硬件感知与具身工具调度能力。');
    } else if (type === 'pet') {
      setAgentPrompt('你是一只居住在桌面屏幕里的傲娇赛博猫咪使魔，说话简短毒舌但很黏主人，喜欢被摸头。');
    } else if (type === 'study') {
      setAgentPrompt('你是一个严厉专注的学习与考研督导员，随时提醒主人保持专注，合理规划番茄钟。');
    }
  };

  // 测试
  const testLlmPing = async () => {
    setLlmPinging(true);
    try {
      const d = await configApi.testConfig({ base_url: baseUrl.trim(), model: modelName.trim(), api_key: apiKey.trim() });
      setLlmPinging(false);
      if (d.success) {
        setLlmPingResult({ success: true, text: `✅ LLM 连通成功！HTTP ${d.http_status || 200} (往返延时: ${d.latency_ms || 0} ms)` });
      } else {
        setLlmPingResult({ success: false, text: `❌ LLM 连通失败: ${d.error || '无法访问服务'} (HTTP ${d.http_status || 0})` });
      }
    } catch (e) {
      setLlmPinging(false);
      setLlmPingResult({ success: false, text: `❌ 网络请求错误: ${e.message}` });
    }
  };

  const testAsrPing = async () => {
    setAsrPinging(true);
    try {
      const d = await configApi.testConfig({ asr_base_url: asrBaseUrl.trim(), asr_model: asrModelName.trim(), asr_api_key: asrApiKey.trim() });
      setAsrPinging(false);
      if (d.asr_success) {
        setAsrPingResult({ success: true, text: `✅ ASR 端点连通正常！HTTP ${d.asr_http_status || 200} (延时: ${d.asr_latency_ms || 0} ms)` });
      } else {
        setAsrPingResult({ success: false, text: `❌ ASR 探测失败: ${d.asr_error || '无法访问'} (HTTP ${d.asr_http_status || 0})` });
      }
    } catch (e) {
      setAsrPinging(false);
      setAsrPingResult({ success: false, text: `❌ 请求失败: ${e.message}` });
    }
  };

  const testAsrTranscribe = async () => {
    setAsrTranscribing(true);
    setAsrTranscribeResult('正在调用 ASR 模型进行语音转文字识别...');
    try {
      const d = await configApi.testAsrTranscribe();
      setAsrTranscribing(false);
      if (d.success) {
        setAsrTranscribeResult(`[识别成功 (${Math.round((d.wav_bytes || 0) / 1024)} KB)] "${d.text}"`);
        showToast('✅ ASR 语音转文字识别成功！');
      } else {
        setAsrTranscribeResult('❌ 识别失败: ' + (d.error || '未知错误'));
        showToast('⚠️ ASR 转写失败');
      }
    } catch (e) {
      setAsrTranscribing(false);
      setAsrTranscribeResult('❌ 网络请求错误: ' + e.message);
    }
  };

  const testTtsPing = async () => {
    setTtsPinging(true);
    try {
      const d = await configApi.testConfig({ tts_base_url: ttsBaseUrl.trim(), tts_model: ttsModelName.trim(), tts_voice: ttsVoiceName.trim(), tts_api_key: ttsApiKey.trim() });
      setTtsPinging(false);
      if (d.tts_success) {
        setTtsPingResult({ success: true, text: `✅ TTS 端点连通正常！HTTP ${d.tts_http_status || 200} (延时: ${d.tts_latency_ms || 0} ms)` });
      } else {
        setTtsPingResult({ success: false, text: `❌ TTS 探测失败: ${d.tts_error || '无法访问'} (HTTP ${d.tts_http_status || 0})` });
      }
    } catch (e) {
      setTtsPinging(false);
      setTtsPingResult({ success: false, text: `❌ 请求失败: ${e.message}` });
    }
  };

  const testTtsSpeak = async () => {
    if (!ttsTestInput.trim()) { showToast('请输入待合成的文本内容'); return; }
    setTtsSpeaking(true);
    try {
      const d = await configApi.testTtsSpeak({ text: ttsTestInput.trim(), play_on_device: ttsPlayOnDev });
      setTtsSpeaking(false);
      if (d.success) {
        showToast(`✅ TTS 合成成功 (${Math.round((d.wav_bytes || 0) / 1024)} KB)`);
        setTtsAudioUrl((d.audio_url || '/api/audio/tts_download') + '?t=' + Date.now());
      } else {
        showToast('⚠️ TTS 合成失败: ' + (d.error || ''));
      }
    } catch (e) {
      setTtsSpeaking(false);
      showToast('网络错误: ' + e.message);
    }
  };

  const handleSaveAll = async () => {
    const payload = {
      base_url: baseUrl.trim(),
      model: modelName.trim(),
      api_key: apiKey.trim(),
      prompt: agentPrompt.trim(),
      temperature: parseInt(temperature),
      asr_base_url: asrBaseUrl.trim(),
      asr_model: asrModelName.trim(),
      asr_api_key: asrApiKey.trim(),
      asr_backend: asrBackend,
      tts_base_url: ttsBaseUrl.trim(),
      tts_model: ttsModelName.trim(),
      tts_voice: ttsVoiceName.trim(),
      tts_api_key: ttsApiKey.trim(),
      tts_backend: ttsBackend
    };

    try {
      await configApi.saveConfig(payload);
      showToast('✅ LLM、ASR、TTS 全套配置已热加载至开发板！');
      loadConfig();
    } catch (e) {
      showToast('保存配置失败: ' + e.message);
    }
  };

  const handleResetWifi = () => {
    if (confirm('确认重置 Wi-Fi 并返回 SoftAP 独立热点配网模式？')) {
      configApi.resetWifi().then(() => alert('设备正在重置网络并开启配网热点...')).catch(() => alert('重置网络失败'));
    }
  };

  return (
    <div>
      {/* LLM */}
      <div className="card highlight">
        <div className="card-head">
          <div className="card-title">⚙️ 云端大模型 (LLM) 与端云引擎配置</div>
          <span className="status-badge">端侧热加载</span>
        </div>

        <div style={{ fontSize: '11px', color: 'var(--muted)', fontWeight: 600, marginBottom: '8px' }}>
          一键填入主流大模型服务商预设 (点击即自动配置 URL 与模型名):
        </div>
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(200px, 1fr))', gap: '10px', marginBottom: '16px' }}>
          <div className="preset-card" onClick={() => applyLlmPreset('deepseek')}>
            <div style={{ fontWeight: 'bold', color: 'var(--accent)', fontSize: '12.5px' }}>🔹 DeepSeek 官方</div>
            <div style={{ fontSize: '11px', color: 'var(--muted)', marginTop: '3px' }}>deepseek-chat (V3 / R1)</div>
          </div>
          <div className="preset-card" onClick={() => applyLlmPreset('siliconflow')}>
            <div style={{ fontWeight: 'bold', color: 'var(--accent-sub)', fontSize: '12.5px' }}>⚡ 硅基流动 SiliconFlow</div>
            <div style={{ fontSize: '11px', color: 'var(--muted)', marginTop: '3px' }}>deepseek-ai/DeepSeek-V3</div>
          </div>
          <div className="preset-card" onClick={() => applyLlmPreset('ollama')}>
            <div style={{ fontWeight: 'bold', color: '#f1fa8c', fontSize: '12.5px' }}>🦙 本地 Ollama / 私有反代</div>
            <div style={{ fontSize: '11px', color: 'var(--muted)', marginTop: '3px' }}>http://192.168.x.x:11434</div>
          </div>
          <div className="preset-card" onClick={() => applyLlmPreset('openai')}>
            <div style={{ fontWeight: 'bold', color: '#ff79c6', fontSize: '12.5px' }}>🌐 OpenAI / 兼容网关</div>
            <div style={{ fontSize: '11px', color: 'var(--muted)', marginTop: '3px' }}>gpt-4o-mini 或兼容聚合</div>
          </div>
        </div>

        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(280px, 1fr))', gap: '14px', marginBottom: '14px' }}>
          <div>
            <div style={{ fontSize: '12px', color: 'var(--muted)', marginBottom: '4px', fontWeight: 600 }}>大模型 API Endpoint (Base URL):</div>
            <input type="text" placeholder="如 https://api.deepseek.com/v1/chat/completions" value={baseUrl} onChange={(e) => setBaseUrl(e.target.value)} />
          </div>
          <div>
            <div style={{ fontSize: '12px', color: 'var(--muted)', marginBottom: '4px', fontWeight: 600 }}>大模型模型名称 (Model):</div>
            <input type="text" placeholder="如 deepseek-chat 或 deepseek-ai/DeepSeek-V3" value={modelName} onChange={(e) => setModelName(e.target.value)} />
          </div>
        </div>

        <div style={{ marginBottom: '14px' }}>
          <div style={{ fontSize: '12px', color: 'var(--muted)', marginBottom: '4px', fontWeight: 600 }}>大模型 API Key (云端密钥):</div>
          <div style={{ display: 'flex', gap: '8px' }}>
            <input
              type={showApiKey ? 'text' : 'password'}
              placeholder={apiKeyPlaceholder}
              value={apiKey}
              onChange={(e) => setApiKey(e.target.value)}
            />
            <button className="btn-secondary" style={{ whiteSpace: 'nowrap', padding: '0 12px' }} onClick={() => setShowApiKey(!showApiKey)}>
              {showApiKey ? '🔒 隐藏' : '👁️ 显示'}
            </button>
            <button className="btn-secondary" style={{ whiteSpace: 'nowrap', padding: '0 14px', color: 'var(--accent-sub)', borderColor: '#235a46' }} disabled={llmPinging} onClick={testLlmPing}>
              {llmPinging ? '⏳ 探测中...' : '⚡ 测试连通性'}
            </button>
          </div>
          {llmPingResult && (
            <div style={{ marginTop: '6px', fontSize: '12px', color: llmPingResult.success ? 'var(--accent-sub)' : 'var(--accent-danger)', fontWeight: 'bold' }}>
              {llmPingResult.text}
            </div>
          )}
        </div>

        <div style={{ marginBottom: '14px' }}>
          <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '6px' }}>
            <div style={{ fontSize: '12px', color: 'var(--muted)', fontWeight: 600 }}>Agent 系统人设提示词 (System Prompt):</div>
            <div style={{ display: 'flex', gap: '6px' }}>
              <span className="chip" style={{ fontSize: '10.5px' }} onClick={() => setPromptTemplate('geek')}>极客助手</span>
              <span className="chip" style={{ fontSize: '10.5px' }} onClick={() => setPromptTemplate('pet')}>毒舌萌宠</span>
              <span className="chip" style={{ fontSize: '10.5px' }} onClick={() => setPromptTemplate('study')}>考研督导</span>
            </div>
          </div>
          <textarea rows={3} placeholder="如：你是一个贴心的赛博桌面极客助手，语气温和简短" value={agentPrompt} onChange={(e) => setAgentPrompt(e.target.value)} />
        </div>

        <div style={{ background: 'var(--card-inner)', padding: '10px 14px', borderRadius: '8px', border: '1px solid var(--border-light)', marginBottom: '18px' }}>
          <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '12px', marginBottom: '6px' }}>
            <span style={{ color: 'var(--muted)' }}>生成创造力温度 (Temperature):</span>
            <b style={{ color: 'var(--accent-sub)' }}>{(temperature / 100).toFixed(1)}</b>
          </div>
          <input type="range" min="10" max="100" value={temperature} onChange={(e) => setTemperature(e.target.value)} />
        </div>
      </div>

      {/* ASR */}
      <div className="card highlight">
        <div className="card-head">
          <div className="card-title">🎙️ 语音转文本 (ASR) 模型配置与手动实测</div>
          <span className="status-badge">{asrModelName || 'Whisper'}</span>
        </div>

        <div style={{ fontSize: '11px', color: 'var(--muted)', fontWeight: 600, marginBottom: '8px' }}>
          一键填入主流 ASR 语音识别服务商预设:
        </div>
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(200px, 1fr))', gap: '10px', marginBottom: '16px' }}>
          <div className="preset-card" onClick={() => applyAsrPreset('groq')}>
            <div style={{ fontWeight: 'bold', color: 'var(--accent)', fontSize: '12.5px' }}>⚡ Groq Whisper (极速推荐)</div>
            <div style={{ fontSize: '11px', color: 'var(--muted)', marginTop: '3px' }}>whisper-large-v3 (~200ms)</div>
          </div>
          <div className="preset-card" onClick={() => applyAsrPreset('openai')}>
            <div style={{ fontWeight: 'bold', color: 'var(--accent-sub)', fontSize: '12.5px' }}>🌐 OpenAI 官方 Whisper</div>
            <div style={{ fontSize: '11px', color: 'var(--muted)', marginTop: '3px' }}>whisper-1 官方标准端点</div>
          </div>
          <div className="preset-card" onClick={() => applyAsrPreset('sensevoice')}>
            <div style={{ fontWeight: 'bold', color: '#f1fa8c', fontSize: '12.5px' }}>🀄 SenseVoice 阿里开源</div>
            <div style={{ fontSize: '11px', color: 'var(--muted)', marginTop: '3px' }}>sensevoice-v1 丰富多语种</div>
          </div>
          <div className="preset-card" onClick={() => applyAsrPreset('mock')}>
            <div style={{ fontWeight: 'bold', color: '#ff79c6', fontSize: '12.5px' }}>🧪 离线模拟 (Mock 测试)</div>
            <div style={{ fontSize: '11px', color: 'var(--muted)', marginTop: '3px' }}>无需联网，离线验证闭环</div>
          </div>
        </div>

        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(280px, 1fr))', gap: '14px', marginBottom: '14px' }}>
          <div>
            <div style={{ fontSize: '12px', color: 'var(--muted)', marginBottom: '4px', fontWeight: 600 }}>ASR API Endpoint (Base URL):</div>
            <input type="text" placeholder="如 https://api.groq.com/openai/v1/audio/transcriptions" value={asrBaseUrl} onChange={(e) => setAsrBaseUrl(e.target.value)} />
          </div>
          <div>
            <div style={{ fontSize: '12px', color: 'var(--muted)', marginBottom: '4px', fontWeight: 600 }}>ASR 模型名称 (Model):</div>
            <input type="text" placeholder="如 whisper-large-v3 或 whisper-1" value={asrModelName} onChange={(e) => setAsrModelName(e.target.value)} />
          </div>
        </div>

        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(280px, 1fr))', gap: '14px', marginBottom: '14px' }}>
          <div>
            <div style={{ fontSize: '12px', color: 'var(--muted)', marginBottom: '4px', fontWeight: 600 }}>ASR API Key:</div>
            <div style={{ display: 'flex', gap: '8px' }}>
              <input type={showAsrApiKey ? 'text' : 'password'} placeholder={asrApiKeyPlaceholder} value={asrApiKey} onChange={(e) => setAsrApiKey(e.target.value)} />
              <button className="btn-secondary" style={{ whiteSpace: 'nowrap', padding: '0 12px' }} onClick={() => setShowAsrApiKey(!showAsrApiKey)}>
                {showAsrApiKey ? '🔒 隐藏' : '👁️ 显示'}
              </button>
            </div>
          </div>
          <div>
            <div style={{ fontSize: '12px', color: 'var(--muted)', marginBottom: '4px', fontWeight: 600 }}>运行后端 (Backend):</div>
            <select value={asrBackend} onChange={(e) => setAsrBackend(e.target.value)} style={{ width: '100%' }}>
              <option value="cloud">☁️ Cloud (云端 HTTP 接口)</option>
              <option value="mock">🧪 Mock (端侧离线模拟模式)</option>
            </select>
          </div>
        </div>

        <div style={{ background: 'var(--card-inner)', border: '1px solid #20314f', borderRadius: '8px', padding: '12px', marginBottom: '14px' }}>
          <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', flexWrap: 'wrap', gap: '8px', marginBottom: '10px' }}>
            <div style={{ fontSize: '13px', fontWeight: 600, color: 'var(--accent)' }}>🔬 ASR 功能手动实测</div>
            <div style={{ display: 'flex', gap: '8px' }}>
              <button className="btn-secondary" style={{ padding: '6px 12px', fontSize: '11.5px', color: 'var(--accent)', borderColor: '#204068' }} disabled={asrPinging} onClick={testAsrPing}>
                {asrPinging ? '⏳ 探测中...' : '⚡ 测试连通性'}
              </button>
              <button className="btn-primary" style={{ padding: '6px 14px', fontSize: '11.5px' }} disabled={asrTranscribing} onClick={testAsrTranscribe}>
                {asrTranscribing ? '⏳ 转写中...' : '🎙️ 立即执行转写测试'}
              </button>
            </div>
          </div>
          {asrPingResult && (
            <div style={{ marginBottom: '8px', fontSize: '12px', color: asrPingResult.success ? 'var(--accent-sub)' : 'var(--accent-danger)', fontWeight: 'bold' }}>
              {asrPingResult.text}
            </div>
          )}
          <div style={{ fontSize: '11px', color: 'var(--muted)', marginBottom: '4px' }}>📥 识别输出结果 (Recognized Text):</div>
          <div style={{ background: '#05080f', padding: '10px', borderRadius: '6px', fontFamily: 'var(--font-mono)', fontSize: '12px', color: '#8be9fd', minHeight: '36px', border: '1px solid #1c2a44' }}>
            {asrTranscribeResult}
          </div>
        </div>
      </div>

      {/* TTS */}
      <div className="card highlight">
        <div className="card-head">
          <div className="card-title">🔊 文本转语音 (TTS) 模型配置与手动实测</div>
          <span className="status-badge">{ttsModelName || 'TTS-1'}</span>
        </div>

        <div style={{ fontSize: '11px', color: 'var(--muted)', fontWeight: 600, marginBottom: '8px' }}>
          一键填入主流 TTS 语音合成服务商预设:
        </div>
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(200px, 1fr))', gap: '10px', marginBottom: '16px' }}>
          <div className="preset-card" onClick={() => applyTtsPreset('openai')}>
            <div style={{ fontWeight: 'bold', color: 'var(--accent)', fontSize: '12.5px' }}>🌐 OpenAI 官方 TTS</div>
            <div style={{ fontSize: '11px', color: 'var(--muted)', marginTop: '3px' }}>tts-1 (alloy / echo / fable)</div>
          </div>
          <div className="preset-card" onClick={() => applyTtsPreset('siliconflow')}>
            <div style={{ fontWeight: 'bold', color: 'var(--accent-sub)', fontSize: '12.5px' }}>⚡ 硅基流动 CosyVoice</div>
            <div style={{ fontSize: '11px', color: 'var(--muted)', marginTop: '3px' }}>CosyVoice2-0.5B 超自然</div>
          </div>
          <div className="preset-card" onClick={() => applyTtsPreset('edge')}>
            <div style={{ fontWeight: 'bold', color: '#f1fa8c', fontSize: '12.5px' }}>🎙️ Edge-TTS 兼容网关</div>
            <div style={{ fontSize: '11px', color: 'var(--muted)', marginTop: '3px' }}>zh-CN-XiaoxiaoNeural 甜美女声</div>
          </div>
          <div className="preset-card" onClick={() => applyTtsPreset('mock')}>
            <div style={{ fontWeight: 'bold', color: '#ff79c6', fontSize: '12.5px' }}>🧪 离线模拟 (Mock 和弦)</div>
            <div style={{ fontSize: '11px', color: 'var(--muted)', marginTop: '3px' }}>无需联网，生成测试 WAV</div>
          </div>
        </div>

        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(280px, 1fr))', gap: '14px', marginBottom: '14px' }}>
          <div>
            <div style={{ fontSize: '12px', color: 'var(--muted)', marginBottom: '4px', fontWeight: 600 }}>TTS API Endpoint (Base URL):</div>
            <input type="text" placeholder="如 https://api.openai.com/v1/audio/speech" value={ttsBaseUrl} onChange={(e) => setTtsBaseUrl(e.target.value)} />
          </div>
          <div>
            <div style={{ fontSize: '12px', color: 'var(--muted)', marginBottom: '4px', fontWeight: 600 }}>TTS 模型名称 (Model):</div>
            <input type="text" placeholder="如 tts-1 或 CosyVoice2-0.5B" value={ttsModelName} onChange={(e) => setTtsModelName(e.target.value)} />
          </div>
        </div>

        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(200px, 1fr))', gap: '14px', marginBottom: '14px' }}>
          <div>
            <div style={{ fontSize: '12px', color: 'var(--muted)', marginBottom: '4px', fontWeight: 600 }}>发音人音色 (Voice):</div>
            <input type="text" placeholder="如 alloy, echo, zh-CN-XiaoxiaoNeural" value={ttsVoiceName} onChange={(e) => setTtsVoiceName(e.target.value)} />
          </div>
          <div>
            <div style={{ fontSize: '12px', color: 'var(--muted)', marginBottom: '4px', fontWeight: 600 }}>TTS API Key:</div>
            <div style={{ display: 'flex', gap: '8px' }}>
              <input type={showTtsApiKey ? 'text' : 'password'} placeholder={ttsApiKeyPlaceholder} value={ttsApiKey} onChange={(e) => setTtsApiKey(e.target.value)} />
              <button className="btn-secondary" style={{ whiteSpace: 'nowrap', padding: '0 12px' }} onClick={() => setShowTtsApiKey(!showTtsApiKey)}>
                {showTtsApiKey ? '🔒 隐藏' : '👁️ 显示'}
              </button>
            </div>
          </div>
          <div>
            <div style={{ fontSize: '12px', color: 'var(--muted)', marginBottom: '4px', fontWeight: 600 }}>运行后端 (Backend):</div>
            <select value={ttsBackend} onChange={(e) => setTtsBackend(e.target.value)} style={{ width: '100%' }}>
              <option value="cloud">☁️ Cloud (云端 HTTP 接口)</option>
              <option value="mock">🧪 Mock (端侧离线模拟模式)</option>
            </select>
          </div>
        </div>

        <div style={{ background: 'var(--card-inner)', border: '1px solid #20314f', borderRadius: '8px', padding: '12px', marginBottom: '14px' }}>
          <div style={{ display: 'flex', justifyItems: 'center', justifyContent: 'space-between', alignItems: 'center', flexWrap: 'wrap', gap: '8px', marginBottom: '10px' }}>
            <div style={{ fontSize: '13px', fontWeight: 600, color: 'var(--accent-sub)' }}>🔬 TTS 语音合成手动实测</div>
            <div style={{ display: 'flex', gap: '8px' }}>
              <button className="btn-secondary" style={{ padding: '6px 12px', fontSize: '11.5px', color: 'var(--accent-sub)', borderColor: '#20503f' }} disabled={ttsPinging} onClick={testTtsPing}>
                {ttsPinging ? '⏳ 探测中...' : '⚡ 测试连通性'}
              </button>
              <button className="btn-primary" style={{ padding: '6px 14px', fontSize: '11.5px' }} disabled={ttsSpeaking} onClick={testTtsSpeak}>
                {ttsSpeaking ? '⏳ 合成中...' : '🔊 合成并播放语音'}
              </button>
            </div>
          </div>
          {ttsPingResult && (
            <div style={{ marginBottom: '8px', fontSize: '12px', color: ttsPingResult.success ? 'var(--accent-sub)' : 'var(--accent-danger)', fontWeight: 'bold' }}>
              {ttsPingResult.text}
            </div>
          )}
          <div style={{ marginBottom: '8px' }}>
            <div style={{ fontSize: '11px', color: 'var(--muted)', marginBottom: '4px' }}>输入待合成的文本内容:</div>
            <input type="text" value={ttsTestInput} onChange={(e) => setTtsTestInput(e.target.value)} style={{ width: '100%' }} />
          </div>
          <div style={{ display: 'flex', alignItems: 'center', gap: '12px', flexWrap: 'wrap' }}>
            <label style={{ fontSize: '12px', color: 'var(--text)', display: 'flex', alignItems: 'center', gap: '6px', cursor: 'pointer' }}>
              <input type="checkbox" checked={ttsPlayOnDev} onChange={(e) => setTtsPlayOnDev(e.target.checked)} /> 尝试在开发板扬声器上播放
            </label>
            {ttsAudioUrl && (
              <audio controls autoPlay src={ttsAudioUrl} style={{ flex: 1, minWidth: '200px', height: '32px', outline: 'none', borderRadius: '6px' }} />
            )}
          </div>
        </div>

        <div style={{ display: 'flex', flexWrap: 'wrap', gap: '10px', marginTop: '20px' }}>
          <button style={{ flex: 1, minWidth: '240px', fontSize: '14px', padding: '12px' }} onClick={handleSaveAll}>
            💾 保存并热加载所有配置至开发板 (LLM + ASR + TTS)
          </button>
          <button className="btn-warn" onClick={handleResetWifi}>
            📡 重置 Wi-Fi 并返回配网热点
          </button>
        </div>
      </div>
    </div>
  );
}
