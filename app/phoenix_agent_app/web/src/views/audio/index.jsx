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
  const playerRef = useRef(null);

  const pollStatus = async () => {
    try {
      const d = await audioApi.getStatus();
      if (d && d.success) {
        setAudioState(d);
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

  const handlePlayRecord = async () => {
    showToast('▶️ 设备端扬声器回放录音...');
    try {
      const d = await audioApi.playRecord();
      if (d.success) pollStatus();
      else showToast('⚠️ 回放失败: ' + (d.error || '无录音数据'));
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
      playerRef.current.play().catch(() => {});
    }
    showToast('💾 正在下载/加载录音 WAV...');
    const a = document.createElement('a');
    a.href = url;
    a.download = 'phoenix_mic_record.wav';
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
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

          {/* 录音回放与下载 */}
          <div style={{ borderTop: '1px solid var(--border-light)', paddingTop: '12px' }}>
            <div style={{ fontSize: '12px', fontWeight: 600, color: 'var(--text)', marginBottom: '8px' }}>
              🎧 录音数据操作与试听
            </div>
            <div style={{ display: 'flex', gap: '8px', flexWrap: 'wrap', marginBottom: '10px' }}>
              <button className="btn-primary" style={{ flex: 1, minWidth: '130px' }} onClick={handlePlayRecord}>
                ▶️ 设备端扬声器回放
              </button>
              <button className="btn-secondary" style={{ flex: 1, minWidth: '130px' }} onClick={handleDownloadWav}>
                💾 下载 WAV 录音文件
              </button>
            </div>
            <audio ref={playerRef} controls src={audioPlayerSrc} style={{ width: '100%', height: '36px', outline: 'none', borderRadius: '6px' }} />
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
