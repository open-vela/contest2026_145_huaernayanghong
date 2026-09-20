---
name: openvela-audio-pipeline
description: "OpenVela / 全志 R528-S3 (Gemini-S1) 音频链路开发、录音拾音与播放调试、nxplayer/nxrecorder 工具及语音管线实践指南。"
---

# OpenVela 音频链路与录放音调试开发指南

本指南涵盖 OpenVela 在全志 R528-S3 平台上的音频驱动框架、测试工具（`nxplayer`、`nxrecorder`、`nxlooper`）使用方法、端侧 16kHz 语音前处理与工程代码集成规范。

---

## 1. 音频驱动框架与设备节点

OpenVela 采用标准的 NuttX 音频分层模型（Upper-Half / Lower-Half）：
* **Upper-Half**：提供标准字符设备接口与 `ioctl` 命令管理。
* **Lower-Half**：由 `sunxi_alsa.c` 桥接全志底层 `aw-alsa-lib` 库与 R528 Codec/I2S DMA 硬件传输（详见驱动开发指南：[openvela-audio-driver](file:///Volumes/LaCie/OpenVela/contest2026_145_huaernayanghong/.agents/skills/openvela-audio-driver/SKILL.md)）。

### 1.1 核心设备节点
* **音频播放输出**：`/dev/audio/pcm0p`（PCM Playback）
* **麦克风录音采集**：`/dev/audio/pcm0c`（PCM Capture）
* **混音与全局控制**：`/dev/audio/dsp`

---

## 2. 终端调试工具全套实战 (NSH)

### 2.1 音频播放：`nxplayer`
用于测试 DAC、功放与喇叭声道输出：

```bash
# 启动 nxplayer
nxplayer

# 在 nxplayer 交互命令行中操作：
nxplayer> device pcm0p                     # 指定默认输出设备
nxplayer> volume 85                        # 设置音量百分比 (0~100)
nxplayer> tone 1000 2                      # 发出 1000Hz 纯蜂鸣音 2 秒 (快速排查硬件通断)
nxplayer> playraw /data/sounds/test.pcm 1 16 44100  # 播放原始 PCM (单声道, 16位, 44.1kHz)
nxplayer> play /data/sounds/welcome.mp3    # 播放 MP3 压缩音频
nxplayer> stop                             # 停止播放
nxplayer> quit                             # 退出
```

### 2.2 麦克风录音：`nxrecorder`
用于测试双麦拾音、录音采样质量与底噪：

```bash
# 启动 nxrecorder
nxrecorder

# 在 nxrecorder 交互命令行中操作：
nxrecorder> device pcm0c                   # 指定默认录音输入设备
nxrecorder> recordraw /data/rec_16k.pcm 1 16 16000   # 录制语音标准 PCM (单声道, 16位, 16kHz)
# 说话测试中...
nxrecorder> stop                           # 停止录音
nxrecorder> quit                           # 退出

# 立即回放刚刚录制的声音以检验效果：
nxplayer
nxplayer> device pcm0p
nxplayer> playraw /data/rec_16k.pcm 1 16 16000
```

### 2.3 实时音频回环：`nxlooper`
测试麦克风直通喇叭的硬件延迟与失真：
```bash
nxlooper
nxlooper> device pcm0p
nxlooper> device pcm0c
nxlooper> loopback 1 16 16000             # 实时耳返/回环
nxlooper> stop
nxlooper> quit
```

### 2.4 驱动自动化单元测试：`cmocka_driver_audio`
```bash
# -a 1: 仅录音; -a 2: 仅放音; -a 3: 录音后立即回放; -p: 文件路径
cmocka_driver_audio -a 3 -p /data/autotest.pcm
```

---

## 3. 端侧语音识别 (ASR) 音频参数标准

端侧语音交互（如离线唤醒词检测、在线大模型 ASR 转写）对音频流有严格的数据格式要求：

| 参数项 | 推荐标准配置 | 规范原因 |
| :--- | :--- | :--- |
| **采样率 (Sample Rate)** | `16000 Hz` (16kHz) | 绝大多数语音识别模型（Whisper、SenseVoice、PocketSphinx）的标准训练输入 |
| **采样位深 (Bit Depth)** | `16-bit` (S16_LE) | 兼顾动态范围与低内存开销（双字节有符号整型） |
| **声道数 (Channels)** | `1 (Mono 单声道)` | 降低传输带宽与端侧运算量；双麦需经过降噪算法合成为主声道 |
| **帧大小 (Chunk Size)** | `320 采样点 / 640 字节` | 对应 20ms 音频片断，契合端侧音频算法的滑动窗处理周期 |

---

## 4. 工程代码集成示例 (`voice_pipeline.c`)

在 `app/phoenix_agent_app/voice/voice_pipeline.c` 中，音频流的采集与分发模型如下：

```c
#include <fcntl.h>
#include <unistd.h>
#include <nuttx/audio/audio.h>

#define AUDIO_DEV_REC "/dev/audio/pcm0c"
#define FRAME_SIZE_BYTES 640 /* 16kHz, 16bit, 20ms */

int pcm_fd = open(AUDIO_DEV_REC, O_RDONLY);
if (pcm_fd < 0) {
    // 错误处理: 检查 CONFIG_AUDIO 宏与权限
}

uint8_t buffer[FRAME_SIZE_BYTES];
while (is_recording) {
    ssize_t bytes_read = read(pcm_fd, buffer, sizeof(buffer));
    if (bytes_read > 0) {
        // 1. 推入端侧循环队列 ring_buffer
        // 2. 计算音量能量（RMS/VAD 检测）
        // 3. 通知 UI 动态波形刷新或分发给网络 ASR
    }
}
close(pcm_fd);
```

---

## 5. 常见问题排查

1. **播放声音断续或有爆音 (Buffer Underrun)**：
   - 检查写入线程的优先级。音频写入线程优先级应不低于 `100`；
   - 增大音频输出缓冲区或使用双缓冲驱动，防止 CPU 渲染 UI 时卡顿音频供给。
2. **录音文件全为静音或爆音杂音**：
   - 使用 `tinymix` 或驱动参数检查 Codec 的 Mic Bias 及模拟增益（Gain），确保麦克风供电开启；
   - 确认麦克风采样极性及 PCB 差分对走线无反接。
