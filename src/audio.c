#include "audio.h"
#include <psp2/audioout.h>
#include <psp2/audioin.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void audio_init(AudioContext *audio) {
    int i;
    int vol[2];

    memset(audio, 0, sizeof(AudioContext));

    audio->bgm_volume = 1.0f;
    audio->se_volume = 1.0f;
    audio->metronome_interval = 4;
    audio->is_recording = 0;
    audio->is_playing_audio = 0;
    audio->bgm_enabled = 0;
    audio->metronome_enabled = 0;
    audio->audio_initialized = 0;
    audio->audio_in_port = -1;

    audio->audio_out_port = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_MAIN,
                                                 AUDIO_BUFFER_SAMPLES,
                                                 AUDIO_SAMPLE_RATE,
                                                 SCE_AUDIO_OUT_MODE_MONO);

    if (audio->audio_out_port >= 0) {
        audio->audio_initialized = 1;
        vol[0] = SCE_AUDIO_OUT_MAX_VOL;
        vol[1] = SCE_AUDIO_OUT_MAX_VOL;
        sceAudioOutSetVolume(audio->audio_out_port,
                             SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH,
                             vol);
    }

    audio->record_buffer = (int16_t *)calloc(MAX_RECORDING_SAMPLES, sizeof(int16_t));
    audio->record_max_samples = MAX_RECORDING_SAMPLES;

    audio_generate_click(&audio->sound_effects[0]);
    audio_generate_beep(&audio->sound_effects[1], 880.0f, 0.1f);
    audio_generate_drum(&audio->sound_effects[2]);
    audio_generate_snap(&audio->sound_effects[3]);

    for (i = 0; i < MAX_SOUND_EFFECTS; i++) {
        audio->se_enabled[i] = 1;
    }
}

void audio_free(AudioContext *audio) {
    int i;

    if (audio->audio_out_port >= 0) {
        sceAudioOutReleasePort(audio->audio_out_port);
    }

    if (audio->audio_in_port >= 0) {
        sceAudioInReleasePort(audio->audio_in_port);
    }

    if (audio->record_buffer) {
        free(audio->record_buffer);
    }

    for (i = 0; i < MAX_SOUND_EFFECTS; i++) {
        if (audio->sound_effects[i].data) {
            free(audio->sound_effects[i].data);
        }
    }

    if (audio->bgm.data) {
        free(audio->bgm.data);
    }
}

void audio_generate_click(SoundClip *clip) {
    int i, samples;
    float t, envelope, wave;

    samples = AUDIO_SAMPLE_RATE / 20;
    clip->data = (int16_t *)malloc(samples * sizeof(int16_t));
    clip->sample_count = samples;
    clip->sample_rate = AUDIO_SAMPLE_RATE;

    for (i = 0; i < samples; i++) {
        t = (float)i / (float)AUDIO_SAMPLE_RATE;
        envelope = 1.0f - (float)i / (float)samples;
        wave = sinf(2.0f * (float)M_PI * 1000.0f * t) * 0.5f +
               sinf(2.0f * (float)M_PI * 2000.0f * t) * 0.3f;
        clip->data[i] = (int16_t)(wave * envelope * 16000.0f);
    }
}

void audio_generate_beep(SoundClip *clip, float frequency, float duration) {
    int i, samples;
    float t, envelope;

    samples = (int)(AUDIO_SAMPLE_RATE * duration);
    clip->data = (int16_t *)malloc(samples * sizeof(int16_t));
    clip->sample_count = samples;
    clip->sample_rate = AUDIO_SAMPLE_RATE;

    for (i = 0; i < samples; i++) {
        t = (float)i / (float)AUDIO_SAMPLE_RATE;
        envelope = 1.0f;
        if (i > samples - samples / 10) {
            envelope = (float)(samples - i) / (float)(samples / 10);
        }
        clip->data[i] = (int16_t)(sinf(2.0f * (float)M_PI * frequency * t) * envelope * 12000.0f);
    }
}

void audio_generate_drum(SoundClip *clip) {
    int i, samples;
    float t, envelope, freq, noise, wave;

    samples = AUDIO_SAMPLE_RATE / 10;
    clip->data = (int16_t *)malloc(samples * sizeof(int16_t));
    clip->sample_count = samples;
    clip->sample_rate = AUDIO_SAMPLE_RATE;

    for (i = 0; i < samples; i++) {
        t = (float)i / (float)AUDIO_SAMPLE_RATE;
        envelope = expf(-t * 30.0f);
        freq = 150.0f * expf(-t * 20.0f);
        noise = ((float)(rand() % 2000 - 1000)) / 1000.0f;
        wave = sinf(2.0f * (float)M_PI * freq * t) * 0.7f + noise * 0.3f;
        clip->data[i] = (int16_t)(wave * envelope * 16000.0f);
    }
}

void audio_generate_snap(SoundClip *clip) {
    int i, samples;
    float t, envelope, noise;

    samples = AUDIO_SAMPLE_RATE / 25;
    clip->data = (int16_t *)malloc(samples * sizeof(int16_t));
    clip->sample_count = samples;
    clip->sample_rate = AUDIO_SAMPLE_RATE;

    for (i = 0; i < samples; i++) {
        t = (float)i / (float)AUDIO_SAMPLE_RATE;
        envelope = expf(-t * 50.0f);
        noise = ((float)(rand() % 2000 - 1000)) / 1000.0f;
        clip->data[i] = (int16_t)(noise * envelope * 20000.0f);
    }
}

void audio_start_recording(AudioContext *audio) {
    int port;
    port = sceAudioInOpenPort(SCE_AUDIO_IN_PORT_TYPE_VOICE,
                               AUDIO_BUFFER_SAMPLES,
                               AUDIO_SAMPLE_RATE,
                               SCE_AUDIO_IN_PARAM_FORMAT_S16_MONO);
    if (port >= 0) {
        audio->audio_in_port = port;
        audio->is_recording = 1;
        audio->record_samples = 0;
    }
}

void audio_stop_recording(AudioContext *audio) {
    audio->is_recording = 0;

    if (audio->audio_in_port >= 0) {
        sceAudioInReleasePort(audio->audio_in_port);
        audio->audio_in_port = -1;
    }

    if (audio->record_samples > 0) {
        if (audio->sound_effects[3].data) {
            free(audio->sound_effects[3].data);
        }
        audio->sound_effects[3].data = (int16_t *)malloc(audio->record_samples * sizeof(int16_t));
        memcpy(audio->sound_effects[3].data, audio->record_buffer,
               audio->record_samples * sizeof(int16_t));
        audio->sound_effects[3].sample_count = audio->record_samples;
        audio->sound_effects[3].sample_rate = AUDIO_SAMPLE_RATE;
    }
}

void audio_update_recording(AudioContext *audio) {
    int16_t buffer[AUDIO_BUFFER_SAMPLES];
    int space, to_copy;

    if (!audio->is_recording) return;
    if (audio->audio_in_port < 0) return;

    sceAudioInInput(audio->audio_in_port, buffer);

    space = audio->record_max_samples - audio->record_samples;
    to_copy = (AUDIO_BUFFER_SAMPLES < space) ? AUDIO_BUFFER_SAMPLES : space;

    if (to_copy > 0) {
        memcpy(&audio->record_buffer[audio->record_samples], buffer,
               to_copy * sizeof(int16_t));
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
    audio->is_playing_audio = 1;
}

void audio_stop_se(AudioContext *audio) {
    audio->is_playing_audio = 0;
}

void audio_set_se_trigger(AudioContext *audio, int frame, int se_index, int enabled) {
    if (frame < 0 || frame >= 999) return;
    if (se_index < 0 || se_index >= MAX_SOUND_EFFECTS) return;
    audio->se_triggers[frame][se_index] = enabled ? 1 : 0;
}

int audio_get_se_trigger(AudioContext *audio, int frame, int se_index) {
    if (frame < 0 || frame >= 999) return 0;
    if (se_index < 0 || se_index >= MAX_SOUND_EFFECTS) return 0;
    return audio->se_triggers[frame][se_index];
}

void audio_play_frame_sounds(AudioContext *audio, int frame) {
    int i;
    for (i = 0; i < MAX_SOUND_EFFECTS; i++) {
        if (audio_get_se_trigger(audio, frame, i)) {
            audio_play_se(audio, i);
        }
    }

    if (audio->metronome_enabled && audio->metronome_interval > 0) {
        if (frame % audio->metronome_interval == 0) {
            audio_play_se(audio, 0);
        }
    }
}

void audio_update(AudioContext *audio) {
    int16_t buffer[AUDIO_BUFFER_SAMPLES];
    int remaining, to_play, i;
    float sample;
    SoundClip *clip;

    if (!audio->is_playing_audio || !audio->audio_initialized) return;

    clip = &audio->sound_effects[audio->current_playing_se];
    if (!clip->data) {
        audio->is_playing_audio = 0;
        return;
    }

    remaining = clip->sample_count - audio->playback_position;
    to_play = (AUDIO_BUFFER_SAMPLES < remaining) ? AUDIO_BUFFER_SAMPLES : remaining;

    if (to_play > 0) {
        for (i = 0; i < to_play; i++) {
            sample = (float)clip->data[audio->playback_position + i] * audio->se_volume;
            if (sample > 32767.0f) sample = 32767.0f;
            if (sample < -32768.0f) sample = -32768.0f;
            buffer[i] = (int16_t)sample;
        }
        for (i = to_play; i < AUDIO_BUFFER_SAMPLES; i++) {
            buffer[i] = 0;
        }

        sceAudioOutOutput(audio->audio_out_port, buffer);
        audio->playback_position += to_play;
    }

    if (audio->playback_position >= clip->sample_count) {
        audio->is_playing_audio = 0;
    }
}

void audio_set_bgm_volume(AudioContext *audio, float vol) {
    if (vol < 0.0f) vol = 0.0f;
    if (vol > 1.0f) vol = 1.0f;
    audio->bgm_volume = vol;
}

void audio_set_se_volume(AudioContext *audio, float vol) {
    if (vol < 0.0f) vol = 0.0f;
    if (vol > 1.0f) vol = 1.0f;
    audio->se_volume = vol;
}
