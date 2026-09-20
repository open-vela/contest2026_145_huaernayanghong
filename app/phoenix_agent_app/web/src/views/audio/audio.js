import { audioApi } from '../../api/audio.js';
import { showToast } from '../../utils/toast.js';

let audioPollTimer = null;
let audioLoopbackActive = false;

export const audioView = {
  startPolling: () => {
    if (audioPollTimer) clearInterval(audioPollTimer);
    audioView.pollStatus();
    audioPollTimer = setInterval(audioView.pollStatus, 500);
  },

  stopPolling: () => {
    if (audioPollTimer) {
      clearInterval(audioPollTimer);
      audioPollTimer = null;
    }
  },

  pollStatus: async () => {
    try {
      const d = await audioApi.getStatus();
      if (!d || !d.success) return;

      // 状态徽章
      const badge = document.getElementById('audioStateBadge');
      if (badge) {
        let stateTxt = '● 空闲 (IDLE)';
        let color = 'var(--muted)';
        if (d.state_str === 'RECORDING') {
          stateTxt = '🔴 正在录音 (RECORDING)';
          color = 'var(--accent-danger)';
        } else if (d.state_str === 'PLAYING_REC') {
          stateTxt = '▶️ 正在回放录音 (PLAYING)';
          color = 'var(--accent-sub)';
        } else if (d.state_str === 'PLAYING_TONE') {
          stateTxt = '🔔 正在播放 1kHz 纯音';
          color = 'var(--accent)';
        }
        if (d.loopback_active) {
          stateTxt += ' [耳返开启]';
        }
        badge.innerText = stateTxt;
        badge.style.color = color;
      }

      // 实时能量柱
      const bar = document.getElementById('audioEnergyBar');
      const valTxt = document.getElementById('audioEnergyVal');
      if (bar && valTxt) {
        const energy = Math.min(100, Math.max(0, d.current_energy || 0));
        bar.style.width = energy + '%';
        valTxt.innerText = energy + '%';
      }

      // 已录制时长
      const durTxt = document.getElementById('audioRecordDurTxt');
      if (durTxt) {
        const sec = ((d.record_duration_ms || 0) / 1000).toFixed(1);
        durTxt.innerText = `已录: ${sec}s / 10.0s (${Math.round((d.recorded_bytes || 0) / 1024)} KB)`;
      }

      // 耳返开关
      audioLoopbackActive = !!d.loopback_active;
      const lbBtn = document.getElementById('btnLoopbackToggle');
      if (lbBtn) {
        lbBtn.innerText = audioLoopbackActive ? '关闭耳返' : '开启耳返';
        lbBtn.className = audioLoopbackActive ? 'btn-warn' : 'btn-secondary';
      }

      // 硬件音量
      if (typeof d.volume === 'number') {
        const slider = document.getElementById('audioVolumeSlider');
        const volTxt = document.getElementById('audioVolumeTxt');
        if (slider && document.activeElement !== slider) slider.value = d.volume;
        if (volTxt) volTxt.innerText = d.volume + '%';
      }
    } catch (e) {}
  },

  record: async (durationMs) => {
    showToast(`🔴 开始麦克风录音 (${durationMs / 1000}s)...`);
    try {
      const d = await audioApi.record(durationMs);
      if (d.success) audioView.pollStatus();
      else showToast('⚠️ 启动录音失败: ' + (d.error || '忙'));
    } catch (e) {
      showToast('网络错误: ' + e.message);
    }
  },

  recordStop: async () => {
    showToast('⏹️ 停止录音');
    try {
      await audioApi.recordStop();
      audioView.pollStatus();
    } catch (e) {}
  },

  playRecord: async () => {
    showToast('▶️ 设备端扬声器回放录音...');
    try {
      const d = await audioApi.playRecord();
      if (d.success) audioView.pollStatus();
      else showToast('⚠️ 回放失败: ' + (d.error || '无录音数据'));
    } catch (e) {
      showToast('网络错误: ' + e.message);
    }
  },

  playTone: async (freqHz, durationMs) => {
    showToast(`🔔 播放 ${freqHz}Hz 纯音 (${durationMs / 1000}s)...`);
    try {
      const d = await audioApi.playTone(freqHz, durationMs);
      if (d.success) audioView.pollStatus();
      else showToast('⚠️ 播放失败: ' + (d.error || '忙'));
    } catch (e) {
      showToast('网络错误: ' + e.message);
    }
  },

  playStop: async () => {
    showToast('⏹️ 停止播放');
    try {
      await audioApi.playStop();
      audioView.pollStatus();
    } catch (e) {}
  },

  toggleLoopback: async () => {
    const nextState = !audioLoopbackActive;
    showToast(nextState ? '🎧 开启麦克风实时耳返' : '🔇 关闭耳返');
    try {
      const d = await audioApi.setLoopback(nextState);
      if (d.success) audioView.pollStatus();
    } catch (e) {}
  },

  onVolumeSlide: (val) => {
    const volTxt = document.getElementById('audioVolumeTxt');
    if (volTxt) volTxt.innerText = val + '%';
  },

  setVolume: async (val) => {
    const v = parseInt(val, 10);
    try {
      const d = await audioApi.setVolume(v);
      if (d.success) {
        showToast(`🔊 硬件音量已设为: ${v}%`);
        audioView.pollStatus();
      }
    } catch (e) {}
  },

  downloadWav: () => {
    const url = '/api/audio/download?t=' + Date.now();
    const player = document.getElementById('browserAudioPlayer');
    if (player) {
      player.src = url;
      player.load();
      player.play().catch(() => {});
    }
    showToast('💾 正在下载/加载录音 WAV...');
    const a = document.createElement('a');
    a.href = url;
    a.download = 'phoenix_mic_record.wav';
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
  }
};
