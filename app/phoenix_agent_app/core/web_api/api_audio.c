/**
 * @file api_audio.c
 * @brief Phoenix HoloDesk-S1 Audio Lab REST API Handlers Implementation
 * @author OpenVela Contest 2026 Team 145
 */

#include "web_api.h"
#include "../audio_test_service.h"
#include "../../hal/hal_actuator.h"
#include "../../harness/asr_provider.h"
#include "../../harness/tts_provider.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int handle_audio_status(const http_req_t *req, http_resp_t *resp)
{
    (void)req;
    audio_test_status_t st;
    audio_test_get_status(&st);

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        http_resp_error(resp, 500, "JSON alloc failed");
        return -1;
    }

    const char *state_str = "IDLE";
    switch (st.state) {
        case AUDIO_TEST_STATE_RECORDING:   state_str = "RECORDING"; break;
        case AUDIO_TEST_STATE_PLAYING_REC: state_str = "PLAYING_REC"; break;
        case AUDIO_TEST_STATE_PLAYING_TONE:state_str = "PLAYING_TONE"; break;
        default: break;
    }

    cJSON_AddBoolToObject(root, "success", true);
    cJSON_AddNumberToObject(root, "state", (int)st.state);
    cJSON_AddStringToObject(root, "state_str", state_str);
    cJSON_AddNumberToObject(root, "record_duration_ms", (double)st.record_duration_ms);
    cJSON_AddNumberToObject(root, "recorded_bytes", (double)st.recorded_bytes);
    cJSON_AddNumberToObject(root, "current_energy", st.current_energy);
    cJSON_AddNumberToObject(root, "volume", st.volume_pct);
    cJSON_AddBoolToObject(root, "has_recording", (st.recorded_bytes > 0));
    cJSON_AddBoolToObject(root, "loopback_active", st.is_loopback);

    http_resp_json_obj(resp, 200, root);
    return 0;
}

int handle_audio_record(const http_req_t *req, http_resp_t *resp)
{
    const char *action = "start";

    if (req->json) {
        cJSON *j_act = cJSON_GetObjectItem(req->json, "action");
        if (j_act && cJSON_IsString(j_act)) {
            action = j_act->valuestring;
        }
    } else {
        char val[32];
        if (http_req_get_query_param(req, "action", val, sizeof(val))) {
            if (strcmp(val, "stop") == 0) action = "stop";
        }
    }

    if (strcmp(action, "stop") == 0) {
        audio_test_record_stop();
    } else {
        int ret = audio_test_record_start();
        if (ret != 0) {
            http_resp_error(resp, 400, "Audio service busy or record start failed");
            return 0;
        }
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "success", true);
    cJSON_AddStringToObject(root, "action", action);
    http_resp_json_obj(resp, 200, root);
    return 0;
}

int handle_audio_play(const http_req_t *req, http_resp_t *resp)
{
    const char *action = "start";
    const char *type = "recording";
    uint32_t duration_ms = 1000;
    uint32_t freq_hz = 1000;

    if (req->json) {
        cJSON *j_act = cJSON_GetObjectItem(req->json, "action");
        if (j_act && cJSON_IsString(j_act)) {
            action = j_act->valuestring;
        }
        cJSON *j_type = cJSON_GetObjectItem(req->json, "type");
        if (j_type && cJSON_IsString(j_type)) {
            type = j_type->valuestring;
        }
        cJSON *j_dur = cJSON_GetObjectItem(req->json, "duration_ms");
        if (j_dur && cJSON_IsNumber(j_dur) && j_dur->valueint > 0) {
            duration_ms = (uint32_t)j_dur->valueint;
        }
        cJSON *j_freq = cJSON_GetObjectItem(req->json, "freq_hz");
        if (j_freq && cJSON_IsNumber(j_freq) && j_freq->valueint > 0) {
            freq_hz = (uint32_t)j_freq->valueint;
        }
    } else {
        char val[32];
        if (http_req_get_query_param(req, "action", val, sizeof(val))) {
            if (strcmp(val, "stop") == 0) action = "stop";
        }
        if (http_req_get_query_param(req, "type", val, sizeof(val))) {
            if (strcmp(val, "tone") == 0) type = "tone";
        }
    }

    if (strcmp(action, "stop") == 0) {
        audio_test_play_stop();
    } else {
        int ret = 0;
        if (strcmp(type, "tone") == 0) {
            ret = audio_test_play_tone(freq_hz, duration_ms);
        } else {
            ret = audio_test_play_record();
        }
        if (ret != 0) {
            http_resp_error(resp, 400, "Audio service busy or play start failed");
            return 0;
        }
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "success", true);
    cJSON_AddStringToObject(root, "action", action);
    cJSON_AddStringToObject(root, "type", type);
    http_resp_json_obj(resp, 200, root);
    return 0;
}

int handle_audio_loopback(const http_req_t *req, http_resp_t *resp)
{
    bool enable = false;

    if (req->json) {
        cJSON *j_en = cJSON_GetObjectItem(req->json, "enable");
        if (j_en && cJSON_IsBool(j_en)) {
            enable = cJSON_IsTrue(j_en);
        }
    } else {
        char val[16];
        if (http_req_get_query_param(req, "enable", val, sizeof(val))) {
            enable = (strcmp(val, "1") == 0 || strcmp(val, "true") == 0);
        }
    }

    int ret = audio_test_set_loopback(enable);
    if (ret != 0) {
        http_resp_error(resp, 400, "Failed to toggle loopback");
        return 0;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "success", true);
    cJSON_AddBoolToObject(root, "loopback_active", enable);
    http_resp_json_obj(resp, 200, root);
    return 0;
}

int handle_audio_volume(const http_req_t *req, http_resp_t *resp)
{
    if (req->method == HTTP_METHOD_POST) {
        int vol = -1;
        if (req->json) {
            cJSON *j_vol = cJSON_GetObjectItem(req->json, "volume");
            if (j_vol && cJSON_IsNumber(j_vol)) {
                vol = j_vol->valueint;
            }
        } else {
            char val[16];
            if (http_req_get_query_param(req, "volume", val, sizeof(val))) {
                vol = atoi(val);
            }
        }

        if (vol >= 0 && vol <= 100) {
            hal_actuator_set_volume((uint8_t)vol);
        } else {
            http_resp_error(resp, 400, "Volume must be between 0 and 100");
            return 0;
        }
    }

    audio_test_status_t st;
    audio_test_get_status(&st);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "success", true);
    cJSON_AddNumberToObject(root, "volume", st.volume_pct);
    http_resp_json_obj(resp, 200, root);
    return 0;
}

int handle_audio_download(const http_req_t *req, http_resp_t *resp)
{
    (void)req;
    size_t wav_len = 0;
    const uint8_t *wav_buf = audio_test_get_wav_data(&wav_len);

    if (!wav_buf || wav_len == 0) {
        http_resp_error(resp, 404, "No recorded audio available");
        return 0;
    }

    http_resp_file_stream(resp, 200, "audio/wav", "phoenix_mic_record.wav", wav_buf, wav_len);
    return 0;
}

/* 暂存最近一次 TTS 合成的音频供下载与网页端即时试听 */
static uint8_t s_tts_cache_buf[128 * 1024];
static size_t s_tts_cache_len = 0;

int handle_audio_asr_test(const http_req_t *req, http_resp_t *resp)
{
    (void)req;
    size_t wav_len = 0;
    const uint8_t *wav_buf = audio_test_get_wav_data(&wav_len);

    /* 若尚未录音，自动构造一段带 44 字节 WAV 头的简短音频用于测试 ASR 通路 */
    uint8_t dummy_wav[1024];
    if (!wav_buf || wav_len == 0) {
        memset(dummy_wav, 0x55, sizeof(dummy_wav));
        wav_buf = dummy_wav;
        wav_len = sizeof(dummy_wav);
    }

    char recognized_text[256] = {0};
    int ret = phoenix_asr_transcribe(wav_buf, wav_len, recognized_text, sizeof(recognized_text));
    if (ret != 0) {
        http_resp_error(resp, 500, "ASR 转写失败或服务不可达");
        return 0;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "success", true);
    cJSON_AddStringToObject(root, "text", recognized_text);
    cJSON_AddNumberToObject(root, "wav_bytes", (double)wav_len);
    http_resp_json_obj(resp, 200, root);
    return 0;
}

int handle_audio_tts_test(const http_req_t *req, http_resp_t *resp)
{
    const char *text = "你好，我是桌面灵眸数字生命体！";
    bool play_on_device = true;

    if (req->json) {
        cJSON *j_text = cJSON_GetObjectItem(req->json, "text");
        if (j_text && cJSON_IsString(j_text) && strlen(j_text->valuestring) > 0) {
            text = j_text->valuestring;
        }
        cJSON *j_play = cJSON_GetObjectItem(req->json, "play_on_device");
        if (j_play && cJSON_IsBool(j_play)) {
            play_on_device = cJSON_IsTrue(j_play);
        }
    }

    s_tts_cache_len = 0;
    int ret = phoenix_tts_synthesize(text, s_tts_cache_buf, sizeof(s_tts_cache_buf), &s_tts_cache_len);
    if (ret != 0 || s_tts_cache_len == 0) {
        http_resp_error(resp, 500, "TTS 语音合成失败");
        return 0;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "success", true);
    cJSON_AddStringToObject(root, "text", text);
    cJSON_AddNumberToObject(root, "wav_bytes", (double)s_tts_cache_len);
    cJSON_AddBoolToObject(root, "played_on_device", play_on_device);
    cJSON_AddStringToObject(root, "audio_url", "/api/audio/tts_download");
    http_resp_json_obj(resp, 200, root);
    return 0;
}

int handle_audio_tts_download(const http_req_t *req, http_resp_t *resp)
{
    (void)req;
    if (s_tts_cache_len == 0) {
        http_resp_error(resp, 404, "No synthesized TTS audio available");
        return 0;
    }
    http_resp_file_stream(resp, 200, "audio/wav", "phoenix_tts_sample.wav", s_tts_cache_buf, s_tts_cache_len);
    return 0;
}

