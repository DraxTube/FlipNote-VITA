#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>

#define AUDIO_SAMPLE_RATE    44100
#define AUDIO_CHANNELS       1
#define AUDIO_BUFFER_SAMPLES 1024
#define MAX_SOUND_EFFECTS    4
#define MAX_RECORDING_TIME   10
#define MAX_RECORDING_SAMPLES (AUDIO_SAMPLE_RATE * MAX_RECORDING_TIME)

typedef struct {
    int16_t *data;
    int sample_count;
    int sample_rate;
} SoundClip;

typedef struct {
    SoundClip sound_effects[MAX_SOUND_EFFECTS];
    int se_enabled[MAX_SOUND_EFFECTS];

    int is_recording;
    int16_t *record_buffer;
    int record_samples;
    int record_max_samples;

    int is_playing_audio;
    int playback_position;
    int current_playing_se;

    SoundClip bgm;
    int bgm_enabled;
    float bgm_volume;
    float se_volume;

    int se_triggers[999][MAX_SOUND_EFFECTS];

    int metronome_enabled;
    int metronome_interval;

    int audio_out_port;
    int audio_in_port;
    int audio_initialized;
} AudioContext;

void audio_init(AudioContext *audio);
void audio_free(AudioContext *audio);

void audio_start_recording(AudioContext *audio);
void audio_stop_recording(AudioContext *audio);
void audio_update_recording(AudioContext *audio);

void audio_play_se(AudioContext *audio, int se_index);
void audio_stop_se(AudioContext *audio);
void audio_set_se_trigger(AudioContext *audio, int frame, int se_index, int enabled);
int audio_get_se_trigger(AudioContext *audio, int frame, int se_index);

void audio_generate_click(SoundClip *clip);
void audio_generate_beep(SoundClip *clip, float frequency, float duration);
void audio_generate_drum(SoundClip *clip);
void audio_generate_snap(SoundClip *clip);

void audio_play_frame_sounds(AudioContext *audio, int frame);
void audio_update(AudioContext *audio);

void audio_set_bgm_volume(AudioContext *audio, float vol);
void audio_set_se_volume(AudioContext *audio, float vol);

#endif
