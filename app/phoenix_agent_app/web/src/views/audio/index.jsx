import React, { useState, useEffect, useRef } from 'react';
import { audioApi } from '../../api/audio.js';
import { showToast } from '../../utils/toast.js';

export function Audio({ isActive }) {
  const [audioState, setAudioState] = useState({
    state_str: 'IDLE',
    current_energy: 0,
    record_duration_ms: 0,
    recorded_bytes: 0,
    loopback_active: false,
    volume: 80
  });
  const [audioPlayerSrc, setAudioPlayerSrc] = useState('');
  const [isPlayingWeb, setIsPlayingWeb] = useState(false);
  const [webPlayTime, setWebPlayTime] = useState(0);
  const [webDuration, setWebDuration] = useState(0);
  const [transcription, setTranscription] = useState('');
  const [isTranscribing, setIsTranscribing] = useState(false);
  const playerRef = useRef(null);
  const prevRecordedBytesRef = useRef(0);

  const pollStatus = async () => {
    try {
      const d = await audioApi.getStatus();
      if (d && d.success) {
        setAudioState(d);
        // 若检测到产生新的录音，自动同步网页音频源
        if (d.recorded_bytes > 0 && d.recorded_bytes !== prevRecordedBytesRef.current) {
          prevRecordedBytesRef.current = d.recorded_bytes;
          setAudioPlayerSrc('/api/audio/download?t=' + Date.now());
        }
      }
    } catch (e) {}
  };

  useEffect(() => {
    let timer = null;
    if (isActive) {
      pollStatus();
      timer = setInterval(pollStatus, 500);
    }
    return () => {
      if (timer) clearInterval(timer);
    };
  }, [isActive]);

  const handleRecord = async (durationMs) => {
    showToast(`🔴 开始麦克风录音 (${durationMs / 1000}s)...`);
    try {
      const d = await audioApi.record(durationMs);
      if (d.success) pollStatus();
      else showToast('⚠️ 启动录音失败: ' + (d.error || '忙'));
    } catch (e) {
      showToast('网络错误: ' + e.message);
    }
  };

  const handleRecordStop = async () => {
    showToast('⏹️ 停止录音');
    try {
      await audioApi.recordStop();
      pollStatus();
    } catch (e) {}
  };

  // 网页端直接在线播放/暂停
  const handleTogglePlayWeb = () => {
    if (!audioState.recorded_bytes && (!playerRef.current || !playerRef.current.src)) {
      showToast('⚠️ 暂无录音数据，请先录音');
      return;
    }
    if (!audioPlayerSrc) {
      const url = '/api/audio/download?t=' + Date.now();
      setAudioPlayerSrc(url);
    }

    if (playerRef.current) {
      if (isPlayingWeb) {
        playerRef.current.pause();
        setIsPlayingWeb(false);
      } else {
        playerRef.current.play().then(() => {
          setIsPlayingWeb(true);
        }).catch((err) => {
          showToast('⚠️ 播放受阻: ' + err.message);
        });
      }
    }
  };

  // 语音识别转写 ASR 查看
  const handleTranscribe = async () => {
    if (!audioState.recorded_bytes) {
      showToast('⚠️ 暂无录音数据，请先录音');
      return;
    }
    setIsTranscribing(true);
    showToast('📝 正在请求 ASR 语音识别转写...');
    try {
      const d = await audioApi.asrTranscribe();
      if (d && d.success) {
        setTranscription(d.text || '(未识别到清晰语音)');
        showToast('✅ ASR 语音识别转写完成！');
      } else {
        showToast('⚠️ 语音转写失败: ' + (d?.error || '服务暂不可用'));
      }
    } catch (e) {
      showToast('网络错误: ' + e.message);
    } finally {
      setIsTranscribing(false);
    }
  };

  const handlePlayRecord = async () => {
    showToast('▶️ 触发设备端扬声器回放...');
    try {
      const d = await audioApi.playRecord();
      if (d.success) pollStatus();
      else showToast('⚠️ 回放失败: ' + (d.error || '无录音数据或硬件忙'));
    } catch (e) {
      showToast('网络错误: ' + e.message);
    }
  };

  const handlePlayTone = async (freqHz, durationMs) => {
    showToast(`🔔 播放 ${freqHz}Hz 纯音 (${durationMs / 1000}s)...`);
    try {
      const d = await audioApi.playTone(freqHz, durationMs);
      if (d.success) pollStatus();
      else showToast('⚠️ 播放失败: ' + (d.error || '忙'));
    } catch (e) {
      showToast('网络错误: ' + e.message);
    }
  };

  const handlePlayStop = async () => {
    showToast('⏹️ 停止播放');
    try {
      await audioApi.playStop();
      pollStatus();
    } catch (e) {}
  };

  const handleToggleLoopback = async () => {
    const nextState = !audioState.loopback_active;
    showToast(nextState ? '🎧 开启麦克风实时耳返' : '🔇 关闭耳返');
    try {
      const d = await audioApi.setLoopback(nextState);
      if (d.success) pollStatus();
    } catch (e) {}
  };

  const handleSetVolume = async (val) => {
    const v = parseInt(val, 10);
    try {
      const d = await audioApi.setVolume(v);
      if (d.success) {
        showToast(`🔊 硬件音量已设为: ${v}%`);
        pollStatus();
      }
    } catch (e) {}
  };

  const handleDownloadWav = () => {
    const url = '/api/audio/download?t=' + Date.now();
    setAudioPlayerSrc(url);
    if (playerRef.current) {
      playerRef.current.load();
    }
    showToast('💾 正在下载 WAV 录音文件...');
    const a = document.createElement('a');
    a.href = url;
    a.download = 'phoenix_mic_record.wav';
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
  };

  const formatTime = (sec) => {
    if (!sec || isNaN(sec)) return '00:00';
    const m = Math.floor(sec / 60);
    const s = Math.floor(sec % 60);
    return `${m < 10 ? '0' : ''}${m}:${s < 10 ? '0' : ''}${s}`;
  };

  // 状态显示
  let stateTxt = '● 状态: 空闲 (IDLE)';
  let stateColor = 'var(--muted)';
  if (audioState.state_str === 'RECORDING') {
    stateTxt = '🔴 正在录音 (RECORDING)';
    stateColor = 'var(--accent-danger)';
  } else if (audioState.state_str === 'PLAYING_REC') {
    stateTxt = '▶️ 正在回放录音 (PLAYING)';
    stateColor = 'var(--accent-sub)';
  } else if (audioState.state_str === 'PLAYING_TONE') {
    stateTxt = '🔔 正在播放 1kHz 纯音';
    stateColor = 'var(--accent)';
  }
  if (audioState.loopback_active) {
    stateTxt += ' [耳返开启]';
  }

  const energy = Math.min(100, Math.max(0, audioState.current_energy || 0));
  const recordSec = ((audioState.record_duration_ms || 0) / 1000).toFixed(1);
  const hasRecording = (audioState.recorded_bytes || 0) > 0;

  return (
    <div className="card highlight">
      <div className="card-head">
        <div className="card-title">🎙️ 全志 R528-S3 声学实验室 (Audio Diagnostics Lab)</div>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <span className="status-badge" style={{ color: stateColor }}>
            {stateTxt}
          </span>
        </div>
      </div>
      <div style={{ fontSize: '12px', color: 'var(--muted)', marginBottom: '16px' }}>
        硬件节点: 拾音 <code style={{ color: 'var(--accent)', fontFamily: 'var(--font-mono)' }}>/dev/audio/pcm0c</code> | 放音 <code style={{ color: 'var(--accent-sub)', fontFamily: 'var(--font-mono)' }}>/dev/audio/pcm0p</code> (16kHz 16-bit Mono 标准语音管线)
      </div>

      <div className="grid-2">
        {/* 录音测试卡片 */}
        <div className="card" style={{ background: 'var(--card-inner)', borderColor: '#20314f', marginBottom: 0 }}>
          <div style={{ fontSize: '14px', fontWeight: 600, color: 'var(--accent)', marginBottom: '12px', display: 'flex', justifyContent: 'space-between' }}>
            <span>🎤 麦克风录音与能量拾音分析</span>
            <span style={{ fontSize: '12px', color: 'var(--muted)' }}>
              已录: {recordSec}s / 10.0s ({Math.round((audioState.recorded_bytes || 0) / 1024)} KB)
            </span>
          </div>

          {/* 实时能量音量柱 */}
          <div style={{ marginBottom: '14px' }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '11px', color: 'var(--muted)', marginBottom: '4px' }}>
              <span>实时输入能量 (RMS)</span>
              <span style={{ fontFamily: 'var(--font-mono)', color: 'var(--accent)' }}>{energy}%</span>
            </div>
            <div style={{ background: '#070a12', border: '1px solid #1c2a44', borderRadius: '6px', height: '18px', position: 'relative', overflow: 'hidden' }}>
              <div style={{
                width: `${energy}%`,
                height: '100%',
                background: 'linear-gradient(90deg, #00ff88 0%, #f1fa8c 70%, #ff5555 100%)',
                transition: 'width 0.1s linear'
              }} />
            </div>
          </div>

          {/* 录音控制按钮组 */}
          <div style={{ display: 'flex', gap: '8px', flexWrap: 'wrap', marginBottom: '14px' }}>
            <button className="btn-warn" style={{ flex: 1, minWidth: '110px' }} onClick={() => handleRecord(5000)}>
              🔴 录音 5 秒
            </button>
            <button className="btn-warn" style={{ flex: 1, minWidth: '110px' }} onClick={() => handleRecord(10000)}>
              🔴 录音 10 秒
            </button>
            <button className="btn-secondary" style={{ flex: 1, minWidth: '90px' }} onClick={handleRecordStop}>
              ⏹️ 停止录音
            </button>
          </div>

          {/* 录音查看、Web试听与数据操作 */}
          <div style={{ borderTop: '1px solid var(--border-light)', paddingTop: '12px' }}>
            <div style={{ fontSize: '12px', fontWeight: 600, color: 'var(--text)', marginBottom: '8px', display: 'flex', justifyContent: 'space-between' }}>
              <span>🎧 录音数据查看与 Web 在线试听</span>
              {hasRecording && (
                <span style={{ fontSize: '11px', color: '#00ff88', fontWeight: 'normal' }}>
                  ● 录音已就绪 (免板载音响)
                </span>
              )}
            </div>

            {/* 无音响贴心提示条 */}
            <div style={{
              background: hasRecording ? '#0a1d33' : '#0c1220',
              border: `1px solid ${hasRecording ? '#1f487a' : '#182436'}`,
              borderRadius: '6px',
              padding: '8px 10px',
              marginBottom: '10px',
              fontSize: '11px',
              color: hasRecording ? '#9ecaff' : 'var(--muted)',
              lineHeight: 1.4
            }}>
              {hasRecording ? (
                <>💡 <strong>录音已同步</strong>：开发板未接扬声器时，可直接点击下方<strong>「网页直接播放」</strong>试听，或使用<strong>「ASR 转文字」</strong>直接查看内容。</>
              ) : (
                <>💡 设备端或 Web 端启动录音后，数据会自动同步至此处，可直接在浏览器播放或查看文本，无需依赖板载音响。</>
              )}
            </div>

            {/* 主要交互按钮组 */}
            <div style={{ display: 'flex', gap: '8px', flexWrap: 'wrap', marginBottom: '10px' }}>
              <button
                className={isPlayingWeb ? 'btn-warn' : 'btn-primary'}
                style={{ flex: '1 1 120px', fontWeight: 600 }}
                onClick={handleTogglePlayWeb}
                disabled={!hasRecording}
              >
                {isPlayingWeb ? '⏸️ 网页暂停播放' : '▶️ 网页直接播放'}
              </button>
              <button
                className="btn-warn"
                style={{ flex: '1 1 120px' }}
                onClick={handleTranscribe}
                disabled={!hasRecording || isTranscribing}
              >
                {isTranscribing ? '⏳ 正在识别...' : '📝 语音转文字 (ASR)'}
              </button>
              <button
                className="btn-secondary"
                style={{ flex: '1 1 90px' }}
                onClick={handleDownloadWav}
                disabled={!hasRecording}
              >
                💾 下载 WAV
              </button>
            </div>

            {/* 板载扬声器辅助选项 */}
            <div style={{ display: 'flex', gap: '8px', marginBottom: '10px' }}>
              <button
                className="btn-secondary"
                style={{ flex: 1, fontSize: '11px', color: 'var(--muted)', borderColor: '#1d2c46' }}
                onClick={handlePlayRecord}
                disabled={!hasRecording}
                title="若开发板外接了喇叭/功放，可由此测试板载播放"
              >
                🔊 板载扬声器回放 (需外接喇叭)
              </button>
            </div>

            {/* 原生浏览器音频控件 (用于即时试听与精准拖动进度) */}
            <audio
              ref={playerRef}
              controls
              src={audioPlayerSrc}
              onPlay={() => setIsPlayingWeb(true)}
              onPause={() => setIsPlayingWeb(false)}
              onEnded={() => { setIsPlayingWeb(false); setWebPlayTime(0); }}
              onTimeUpdate={(e) => setWebPlayTime(e.target.currentTime)}
              onLoadedMetadata={(e) => setWebDuration(e.target.duration)}
              style={{ width: '100%', height: '36px', outline: 'none', borderRadius: '6px' }}
            />

            {/* ASR 语音识别转写文本结果展示区 */}
            {transcription && (
              <div style={{
                background: '#071224',
                border: '1px solid #1c3d6c',
                borderRadius: '6px',
                padding: '10px 12px',
                marginTop: '10px'
              }}>
                <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '6px' }}>
                  <span style={{ fontSize: '12px', fontWeight: 600, color: 'var(--accent)' }}>
                    📝 ASR 语音识别结果 (直接查看):
                  </span>
                  <button
                    className="btn-secondary"
                    style={{ padding: '2px 8px', fontSize: '11px' }}
                    onClick={() => {
                      navigator.clipboard?.writeText(transcription);
                      showToast('📋 已复制识别文本');
                    }}
                  >
                    📋 复制文本
                  </button>
                </div>
                <div style={{
                  fontSize: '13px',
                  color: '#e6edf3',
                  background: '#0a1a33',
                  padding: '8px 10px',
                  borderRadius: '4px',
                  borderLeft: '3px solid var(--accent)',
                  lineHeight: 1.5,
                  wordBreak: 'break-all'
                }}>
                  "{transcription}"
                </div>
              </div>
            )}
          </div>
        </div>

        {/* 放音与耳返测试卡片 */}
        <div className="card" style={{ background: 'var(--card-inner)', borderColor: '#20314f', marginBottom: 0 }}>
          <div style={{ fontSize: '14px', fontWeight: 600, color: 'var(--accent-sub)', marginBottom: '12px' }}>
            🔊 扬声器放音、蜂鸣纯音与实时耳返
          </div>

          {/* 1000Hz 纯音测试 */}
          <div style={{ marginBottom: '14px' }}>
            <div style={{ fontSize: '12px', color: 'var(--muted)', marginBottom: '6px' }}>
              DAC / 功放通道通断检测 (1000Hz 正弦纯音):
            </div>
            <div style={{ display: 'flex', gap: '8px', flexWrap: 'wrap' }}>
              <button className="btn-secondary" style={{ flex: 1, minWidth: '110px' }} onClick={() => handlePlayTone(1000, 1000)}>
                🔔 纯音 1 秒 (1kHz)
              </button>
              <button className="btn-secondary" style={{ flex: 1, minWidth: '110px' }} onClick={() => handlePlayTone(1000, 3000)}>
                🔔 纯音 3 秒 (1kHz)
              </button>
              <button className="btn-secondary" style={{ flex: 1, minWidth: '90px' }} onClick={handlePlayStop}>
                ⏹️ 停止放音
              </button>
            </div>
          </div>

          {/* 实时耳返回环开关 */}
          <div style={{ background: '#0b1120', border: '1px solid #1f355a', borderRadius: '8px', padding: '12px', marginBottom: '14px' }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
              <div>
                <div style={{ fontSize: '13px', fontWeight: 600, color: 'var(--text)' }}>🎧 实时麦克风耳返 (Loopback)</div>
                <div style={{ fontSize: '11px', color: 'var(--muted)' }}>麦克风输入直通扬声器输出，零延迟检验端侧音频通路</div>
              </div>
              <button
                className={audioState.loopback_active ? 'btn-warn' : 'btn-secondary'}
                style={{ padding: '6px 14px', fontSize: '12px' }}
                onClick={handleToggleLoopback}
              >
                {audioState.loopback_active ? '关闭耳返' : '开启耳返'}
              </button>
            </div>
          </div>

          {/* 主硬件音量控制 */}
          <div style={{ borderTop: '1px solid var(--border-light)', paddingTop: '12px' }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '12px', marginBottom: '6px' }}>
              <span>硬件主音量调节</span>
              <span style={{ fontFamily: 'var(--font-mono)', color: 'var(--accent)' }}>{audioState.volume}%</span>
            </div>
            <input
              type="range"
              min="0"
              max="100"
              value={audioState.volume}
              onChange={(e) => setAudioState(prev => ({ ...prev, volume: parseInt(e.target.value) }))}
              onMouseUp={(e) => handleSetVolume(e.target.value)}
              onTouchEnd={(e) => handleSetVolume(e.target.value)}
              style={{ width: '100%', marginBottom: '8px' }}
            />
            <div style={{ display: 'flex', gap: '6px' }}>
              {[0, 50, 80, 100].map(v => (
                <button
                  key={v}
                  className="btn-secondary"
                  style={{ flex: 1, padding: '4px', fontSize: '11px' }}
                  onClick={() => handleSetVolume(v)}
                >
                  {v === 0 ? '静音 (0%)' : `${v}%`}
                </button>
              ))}
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}
