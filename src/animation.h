#ifndef ANIMATION_H
#define ANIMATION_H

#include "drawing.h"
#include <stdbool.h>

#define MAX_FRAMES      999
#define MIN_SPEED       1
#define MAX_SPEED        24
#define DEFAULT_SPEED   8

typedef struct {
    LayerData layers[MAX_LAYERS];
    float frame_speed;    // Velocità specifica per frame (-1 = usa globale)
    bool is_keyframe;
} Frame;

typedef struct {
    Frame *frames;
    int frame_count;
    int current_frame;
    int max_frames_allocated;
    
    // Playback
    bool is_playing;
    bool loop;
    float playback_speed;   // FPS
    float frame_timer;
    int play_start_frame;
    int play_end_frame;
    bool play_range_set;
    
    // Proprietà animazione
    bool locked;
    char author[64];
    char title[64];
    
    // Copia frame
    bool frame_clipboard_valid;
    Frame frame_clipboard;
} AnimationContext;

void animation_init(AnimationContext *anim);
void animation_free(AnimationContext *anim);

// Frame management
int animation_add_frame(AnimationContext *anim);
int animation_insert_frame(AnimationContext *anim, int position);
int animation_duplicate_frame(AnimationContext *anim, int frame_idx);
void animation_delete_frame(AnimationContext *anim, int frame_idx);
void animation_move_frame(AnimationContext *anim, int from, int to);
void animation_swap_frames(AnimationContext *anim, int a, int b);
void animation_clear_frame(AnimationContext *anim, int frame_idx);

// Frame navigation
void animation_goto_frame(AnimationContext *anim, DrawingContext *draw, int frame);
void animation_next_frame(AnimationContext *anim, DrawingContext *draw);
void animation_prev_frame(AnimationContext *anim, DrawingContext *draw);
void animation_first_frame(AnimationContext *anim, DrawingContext *draw);
void animation_last_frame(AnimationContext *anim, DrawingContext *draw);

// Salva/carica frame corrente nel/dal contesto disegno
void animation_save_current_to_draw(AnimationContext *anim, DrawingContext *draw);
void animation_load_current_from_draw(AnimationContext *anim, DrawingContext *draw);

// Playback
void animation_play(AnimationContext *anim);
void animation_stop(AnimationContext *anim);
void animation_toggle_play(AnimationContext *anim);
void animation_update(AnimationContext *anim, DrawingContext *draw, float delta_time);
void animation_set_speed(AnimationContext *anim, float fps);
void animation_set_play_range(AnimationContext *anim, int start, int end);
void animation_clear_play_range(AnimationContext *anim);

// Frame clipboard
void animation_copy_frame(AnimationContext *anim, int frame_idx);
void animation_paste_frame(AnimationContext *anim, int position);

// Onion skin helpers
LayerData* animation_get_frame_layers(AnimationContext *anim, int frame_idx);

// Proprietà
void animation_set_loop(AnimationContext *anim, bool loop);
int animation_get_frame_count(AnimationContext *anim);

#endif
