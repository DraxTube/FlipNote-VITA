#include "audio.h"
#include <psp2/audio.h>
#include <psp2/audioin.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void audio_init(AudioContext *audio) {
    memset(audio, 0, sizeof(AudioContext));
    
    audio->bgm_volume = 1.0f;
    audio->se_volume = 1.0f;
    audio->metronome_interval = 4;
    
    // Inizializza porta audio
    audio->audio_port = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_MAIN,
                                             AUDIO_BUFFER_SAMPLES,
                                             AUDIO_SAMPLE_RATE,
                                             SCE_AUDIO_OUT_MODE_MONO);
    
    if (audio->audio_port >= 0) {
        audio->audio_initialized = true;
        sceAudioOutSetVolume(audio->audio_port, SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH,
                            (int[]){SCE_AUDIO_OUT_MAX_VOL, SCE_AUDIO_OUT_MAX_VOL});
    }
    
    // Alloca buffer registrazione
    audio->record_buffer = (int16_t *)calloc(MAX_RECORDING_SAMPLES, sizeof(int16_t));
    audio->record_max_samples = MAX_RECORDING_SAMPLES;
    
    // Genera effetti sonori built-in
    audio_generate_click(&audio->sound_effects[0]);
    audio_generate_beep(&audio->sound_effects[1], 880.0f, 0.1f);
    audio_generate_drum(&audio->sound_effects[2]);
    audio_generate_snap(&audio->sound_effects[3]);
    
    for (int i = 0; i < MAX_SOUND_EFFECTS; i++) {
        audio->se_enabled[i] = true;
    }
}

void audio_free(AudioContext *audio) {
    if (audio->audio_port >= 0) {
        sceAudioOutReleasePort(audio->audio_port);
    }
    
    if (audio->record_buffer) {
        free(audio->record_buffer);
    }
    
    for (int i = 0; i < MAX_SOUND_EFFECTS; i++) {
        if (audio->sound_effects[i].data) {
            free(audio->sound_effects[i].data);
        }
    }
    
    if (audio->bgm.data) {
        free(audio->bgm.data);
    }
}

void audio_generate_click(SoundClip *clip) {
    int samples = AUDIO_SAMPLE_RATE / 20; // 50ms
    clip->data = (int16_t *)malloc(samples * sizeof(int16_t));
    clip->sample_count = samples;
    clip->sample_rate = AUDIO_SAMPLE_RATE;
    
    for (int i = 0; i < samples; i++) {
        float t = (float)i / AUDIO_SAMPLE_RATE;
        float envelope = 1.0f - (float)i / samples;
        float wave = sinf(2.0f * M_PI * 1000.0f * t) * 0.5f +
                     sinf(2.0f * M_PI * 2000.0f * t) * 0.3f;
        clip->data[i] = (int16_t)(wave * envelope * 16000);
    }
}

void audio_generate_beep(SoundClip *clip, float frequency, float duration) {
    int samples = (int)(AUDIO_SAMPLE_RATE * duration);
    clip->data = (int16_t *)malloc(samples * sizeof(int16_t));
    clip->sample_count = samples;
    clip->sample_rate = AUDIO_SAMPLE_RATE;
    
    for (int i = 0; i < samples; i++) {
        float t = (float)i / AUDIO_SAMPLE_RATE;
        float envelope = 1.0f;
        if (i > samples - samples/10) {
            envelope = (float)(samples - i) / (samples/10);
        }
        clip->data[i] = (int16_t)(sinf(2.0f * M_PI * frequency * t) * envelope * 12000);
    }
}

void audio_generate_drum(SoundClip *clip) {
    int samples = AUDIO_SAMPLE_RATE / 10; // 100ms
    clip->data = (int16_t *)malloc(samples * sizeof(int16_t));
    clip->sample_count = samples;
    clip->sample_rate = AUDIO_SAMPLE_RATE;
    
    for (int i = 0; i < samples; i++) {
        float t = (float)i / AUDIO_SAMPLE_RATE;
        float envelope = expf(-t * 30.0f);
        float freq = 150.0f * expf(-t * 20.0f);
        float noise = ((float)(rand() % 2000 - 1000)) / 1000.0f;
        float wave = sinf(2.0f * M_PI * freq * t) * 0.7f + noise * 0.3f;
        clip->data[i] = (int16_t)(wave * envelope * 16000);
    }
}

void audio_generate_snap(SoundClip *clip) {
    int samples = AUDIO_SAMPLE_RATE / 25; // 40ms
    clip->data = (int16_t *)malloc(samples * sizeof(int16_t));
    clip->sample_count = samples;
    clip->sample_rate = AUDIO_SAMPLE_RATE;
    
    for (int i = 0; i < samples; i++) {
        float t = (float)i / AUDIO_SAMPLE_RATE;
        float envelope = expf(-t * 50.0f);
        float noise = ((float)(rand() % 2000 - 1000)) / 1000.0f;
        clip->data[i] = (int16_t)(noise * envelope * 20000);
    }
}

void audio_start_recording(AudioContext *audio) {
    int port = sceAudioInOpenPort(SCE_AUDIO_IN_PORT_TYPE_VOICE,
                                   AUDIO_BUFFER_SAMPLES,
                                   AUDIO_SAMPLE_RATE,
                                   SCE_AUDIO_IN_PARAM_FORMAT_S16_MONO);
    if (port >= 0) {
        audio->is_recording = true;
        audio->record_samples = 0;
    }
}

void audio_stop_recording(AudioContext *audio) {
    audio->is_recording = false;
    sceAudioInReleasePort(SCE_AUDIO_IN_PORT_TYPE_VOICE);
    
    // Copia registrazione in SE slot
    if (audio->record_samples > 0 && audio->sound_effects[3].data) {
        free(audio->sound_effects[3].data);
    }
    audio->sound_effects[3].data = (int16_t *)malloc(audio->record_samples * sizeof(int16_t));
    memcpy(audio->sound_effects[3].data, audio->record_buffer, audio->record_samples * sizeof(int16_t));
    audio->sound_effects[3].sample_count = audio->record_samples;
    audio->sound_effects[3].sample_rate = AUDIO_SAMPLE_RATE;
}

void audio_update_recording(AudioContext *audio) {
    if (!audio->is_recording) return;
    
    int16_t buffer[AUDIO_BUFFER_SAMPLES];
    sceAudioInInput(SCE_AUDIO_IN_PORT_TYPE_VOICE, buffer);
    
    int space = audio->record_max_samples - audio->record_samples;
    int to_copy = (AUDIO_BUFFER_SAMPLES < space) ? AUDIO_BUFFER_SAMPLES : space;
    
    if (to_copy > 0) {
        memcpy(&audio->record_buffer[audio->record_samples], buffer, to_copy * sizeof(int16_t));
        audio->record_samples += to_copy;
    }
    
    if (audio->record_samples >= audio->record_max_samples) {
        audio_stop_recording(audio);
    }
}

void audio_play_se(AudioContext *audio, int se_index) {
    if (se_index < 0 || se_index >= MAX_SOUND_EFFECTS) return;
    if (!audio->se_enabled[se_index]) return;
    if (!audio->sound_effects[se_index].data) return;
    if (!audio->audio_initialized) return;
    
    audio->current_playing_se = se_index;
    audio->playback_position = 0;
    audio->is_playing_audio = true;
}

void audio_stop_se(AudioContext *audio) {
    audio->is_playing_audio = false;
}

void audio_set_se_trigger(AudioContext *audio, int frame, int se_index, bool enabled) {
    if (frame < 0 || frame >= 999) return;
    if (se_index < 0 || se_index >= MAX_SOUND_EFFECTS) return;
    audio->se_triggers[frame][se_index] = enabled ? 1 : 0;
}

bool audio_get_se_trigger(AudioContext *audio, int frame, int se_index) {
    if (frame < 0 || frame >= 999) return false;
    if (se_index < 0 || se_index >= MAX_SOUND_EFFECTS) return false;
    return audio->se_triggers[frame][se_index] != 0;
}

void audio_play_frame_sounds(AudioContext *audio, int frame) {
    for (int i = 0; i < MAX_SOUND_EFFECTS; i++) {
        if (audio_get_se_trigger(audio, frame, i)) {
            audio_play_se(audio, i);
        }
    }
    
    if (audio->metronome_enabled && audio->metronome_interval > 0) {
        if (frame % audio->metronome_interval == 0) {
            audio_play_se(audio, 0); // Click come metronomo
        }
    }
}

void audio_update(AudioContext *audio) {
    if (!audio->is_playing_audio || !audio->audio_initialized) return;
    
    SoundClip *clip = &audio->sound_effects[audio->current_playing_se];
    if (!clip->data) {
        audio->is_playing_audio = false;
        return;
    }
    
    int16_t buffer[AUDIO_BUFFER_SAMPLES];
    int remaining = clip->sample_count - audio->playback_position;
    int to_play = (AUDIO_BUFFER_SAMPLES < remaining) ? AUDIO_BUFFER_SAMPLES : remaining;
    
    if (to_play > 0) {
        for (int i = 0; i < to_play; i++) {
            float sample = clip->data[audio->playback_position + i] * audio->se_volume;
            if (sample > 32767) sample = 32767;
            if (sample < -32768) sample = -32768;
            buffer[i] = (int16_t)sample;
        }
        for (int i = to_play; i < AUDIO_BUFFER_SAMPLES; i++) {
            buffer[i] = 0;
        }
        
        sceAudioOutOutput(audio->audio_port, buffer);
        audio->playback_position += to_play;
    }
    
    if (audio->playback_position >= clip->sample_count) {
        audio->is_playing_audio = false;
    }
}

void audio_set_bgm_volume(AudioContext *audio, float vol) {
    if (vol < 0) vol = 0;
    if (vol > 1) vol = 1;
    audio->bgm_volume = vol;
}

void audio_set_se_volume(AudioContext *audio, float vol) {
    if (vol < 0) vol = 0;
    if (vol > 1) vol = 1;
    audio->se_volume = vol;
}
