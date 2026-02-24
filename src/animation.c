#include "animation.h"
#include <stdlib.h>
#include <string.h>

#define INITIAL_FRAMES 16

void animation_init(AnimationContext *anim) {
    memset(anim, 0, sizeof(AnimationContext));
    
    anim->max_frames_allocated = INITIAL_FRAMES;
    anim->frames = (Frame *)calloc(INITIAL_FRAMES, sizeof(Frame));
    anim->frame_count = 1;
    anim->current_frame = 0;
    anim->playback_speed = DEFAULT_SPEED;
    anim->loop = true;
    anim->frame_clipboard_valid = false;
    
    for (int i = 0; i < INITIAL_FRAMES; i++) {
        anim->frames[i].frame_speed = -1;
        anim->frames[i].is_keyframe = false;
    }
    
    strcpy(anim->author, "Player");
    strcpy(anim->title, "Untitled");
}

void animation_free(AnimationContext *anim) {
    if (anim->frames) {
        free(anim->frames);
        anim->frames = NULL;
    }
}

static void animation_ensure_capacity(AnimationContext *anim, int needed) {
    if (needed <= anim->max_frames_allocated) return;
    
    int new_size = anim->max_frames_allocated * 2;
    while (new_size < needed) new_size *= 2;
    if (new_size > MAX_FRAMES) new_size = MAX_FRAMES;
    
    Frame *new_frames = (Frame *)realloc(anim->frames, new_size * sizeof(Frame));
    if (new_frames) {
        anim->frames = new_frames;
        // Inizializza i nuovi frame
        for (int i = anim->max_frames_allocated; i < new_size; i++) {
            memset(&anim->frames[i], 0, sizeof(Frame));
            anim->frames[i].frame_speed = -1;
        }
        anim->max_frames_allocated = new_size;
    }
}

int animation_add_frame(AnimationContext *anim) {
    if (anim->frame_count >= MAX_FRAMES) return -1;
    
    animation_ensure_capacity(anim, anim->frame_count + 1);
    
    int idx = anim->frame_count;
    memset(&anim->frames[idx], 0, sizeof(Frame));
    anim->frames[idx].frame_speed = -1;
    anim->frame_count++;
    return idx;
}

int animation_insert_frame(AnimationContext *anim, int position) {
    if (anim->frame_count >= MAX_FRAMES) return -1;
    if (position < 0) position = 0;
    if (position > anim->frame_count) position = anim->frame_count;
    
    animation_ensure_capacity(anim, anim->frame_count + 1);
    
    // Sposta frame successivi
    for (int i = anim->frame_count; i > position; i--) {
        memcpy(&anim->frames[i], &anim->frames[i - 1], sizeof(Frame));
    }
    
    memset(&anim->frames[position], 0, sizeof(Frame));
    anim->frames[position].frame_speed = -1;
    anim->frame_count++;
    
    if (anim->current_frame >= position) {
        anim->current_frame++;
    }
    
    return position;
}

int animation_duplicate_frame(AnimationContext *anim, int frame_idx) {
    if (anim->frame_count >= MAX_FRAMES) return -1;
    if (frame_idx < 0 || frame_idx >= anim->frame_count) return -1;
    
    int new_pos = frame_idx + 1;
    animation_ensure_capacity(anim, anim->frame_count + 1);
    
    // Sposta frame successivi
    for (int i = anim->frame_count; i > new_pos; i--) {
        memcpy(&anim->frames[i], &anim->frames[i - 1], sizeof(Frame));
    }
    
    memcpy(&anim->frames[new_pos], &anim->frames[frame_idx], sizeof(Frame));
    anim->frame_count++;
    
    return new_pos;
}

void animation_delete_frame(AnimationContext *anim, int frame_idx) {
    if (anim->frame_count <= 1) return; // Almeno 1 frame
    if (frame_idx < 0 || frame_idx >= anim->frame_count) return;
    
    for (int i = frame_idx; i < anim->frame_count - 1; i++) {
        memcpy(&anim->frames[i], &anim->frames[i + 1], sizeof(Frame));
    }
    
    anim->frame_count--;
    
    if (anim->current_frame >= anim->frame_count) {
        anim->current_frame = anim->frame_count - 1;
    }
}

void animation_move_frame(AnimationContext *anim, int from, int to) {
    if (from < 0 || from >= anim->frame_count) return;
    if (to < 0 || to >= anim->frame_count) return;
    if (from == to) return;
    
    Frame temp;
    memcpy(&temp, &anim->frames[from], sizeof(Frame));
    
    if (from < to) {
        for (int i = from; i < to; i++) {
            memcpy(&anim->frames[i], &anim->frames[i + 1], sizeof(Frame));
        }
    } else {
        for (int i = from; i > to; i--) {
            memcpy(&anim->frames[i], &anim->frames[i - 1], sizeof(Frame));
        }
    }
    
    memcpy(&anim->frames[to], &temp, sizeof(Frame));
}

void animation_swap_frames(AnimationContext *anim, int a, int b) {
    if (a < 0 || a >= anim->frame_count) return;
    if (b < 0 || b >= anim->frame_count) return;
    
    Frame temp;
    memcpy(&temp, &anim->frames[a], sizeof(Frame));
    memcpy(&anim->frames[a], &anim->frames[b], sizeof(Frame));
    memcpy(&anim->frames[b], &temp, sizeof(Frame));
}

void animation_clear_frame(AnimationContext *anim, int frame_idx) {
    if (frame_idx < 0 || frame_idx >= anim->frame_count) return;
    for (int l = 0; l < MAX_LAYERS; l++) {
        memset(&anim->frames[frame_idx].layers[l], 0, sizeof(LayerData));
    }
}

void animation_save_current_to_draw(AnimationContext *anim, DrawingContext *draw) {
    int idx = anim->current_frame;
    if (idx < 0 || idx >= anim->frame_count) return;
    
    for (int l = 0; l < MAX_LAYERS; l++) {
        memcpy(&anim->frames[idx].layers[l], &draw->layers[l], sizeof(LayerData));
    }
}

void animation_load_current_from_draw(AnimationContext *anim, DrawingContext *draw) {
    int idx = anim->current_frame;
    if (idx < 0 || idx >= anim->frame_count) return;
    
    for (int l = 0; l < MAX_LAYERS; l++) {
        memcpy(&draw->layers[l], &anim->frames[idx].layers[l], sizeof(LayerData));
    }
}

void animation_goto_frame(AnimationContext *anim, DrawingContext *draw, int frame) {
    // Salva frame corrente
    animation_save_current_to_draw(anim, draw);
    
    if (frame < 0) frame = 0;
    if (frame >= anim->frame_count) frame = anim->frame_count - 1;
    
    anim->current_frame = frame;
    
    // Carica nuovo frame
    animation_load_current_from_draw(anim, draw);
}

void animation_next_frame(AnimationContext *anim, DrawingContext *draw) {
    int next = anim->current_frame + 1;
    if (next >= anim->frame_count) {
        if (anim->loop) next = 0;
        else return;
    }
    animation_goto_frame(anim, draw, next);
}

void animation_prev_frame(AnimationContext *anim, DrawingContext *draw) {
    int prev = anim->current_frame - 1;
    if (prev < 0) {
        if (anim->loop) prev = anim->frame_count - 1;
        else return;
    }
    animation_goto_frame(anim, draw, prev);
}

void animation_first_frame(AnimationContext *anim, DrawingContext *draw) {
    animation_goto_frame(anim, draw, 0);
}

void animation_last_frame(AnimationContext *anim, DrawingContext *draw) {
    animation_goto_frame(anim, draw, anim->frame_count - 1);
}

// Playback
void animation_play(AnimationContext *anim) {
    anim->is_playing = true;
    anim->frame_timer = 0;
}

void animation_stop(AnimationContext *anim) {
    anim->is_playing = false;
}

void animation_toggle_play(AnimationContext *anim) {
    if (anim->is_playing) animation_stop(anim);
    else animation_play(anim);
}

void animation_update(AnimationContext *anim, DrawingContext *draw, float delta_time) {
    if (!anim->is_playing) return;
    
    float speed = anim->playback_speed;
    Frame *cur = &anim->frames[anim->current_frame];
    if (cur->frame_speed > 0) {
        speed = cur->frame_speed;
    }
    
    float frame_duration = 1.0f / speed;
    anim->frame_timer += delta_time;
    
    if (anim->frame_timer >= frame_duration) {
        anim->frame_timer -= frame_duration;
        
        int start = anim->play_range_set ? anim->play_start_frame : 0;
        int end = anim->play_range_set ? anim->play_end_frame : anim->frame_count - 1;
        
        // Salva frame corrente
        animation_save_current_to_draw(anim, draw);
        
        int next = anim->current_frame + 1;
        if (next > end) {
            if (anim->loop) next = start;
            else {
                anim->is_playing = false;
                return;
            }
        }
        
        anim->current_frame = next;
        animation_load_current_from_draw(anim, draw);
    }
}

void animation_set_speed(AnimationContext *anim, float fps) {
    if (fps < MIN_SPEED) fps = MIN_SPEED;
    if (fps > MAX_SPEED) fps = MAX_SPEED;
    anim->playback_speed = fps;
}

void animation_set_play_range(AnimationContext *anim, int start, int end) {
    anim->play_range_set = true;
    anim->play_start_frame = start;
    anim->play_end_frame = end;
}

void animation_clear_play_range(AnimationContext *anim) {
    anim->play_range_set = false;
}

void animation_copy_frame(AnimationContext *anim, int frame_idx) {
    if (frame_idx < 0 || frame_idx >= anim->frame_count) return;
    memcpy(&anim->frame_clipboard, &anim->frames[frame_idx], sizeof(Frame));
    anim->frame_clipboard_valid = true;
}

void animation_paste_frame(AnimationContext *anim, int position) {
    if (!anim->frame_clipboard_valid) return;
    int idx = animation_insert_frame(anim, position);
    if (idx >= 0) {
        memcpy(&anim->frames[idx], &anim->frame_clipboard, sizeof(Frame));
    }
}

LayerData* animation_get_frame_layers(AnimationContext *anim, int frame_idx) {
    if (frame_idx < 0 || frame_idx >= anim->frame_count) return NULL;
    return anim->frames[frame_idx].layers;
}

void animation_set_loop(AnimationContext *anim, bool loop) {
    anim->loop = loop;
}

int animation_get_frame_count(AnimationContext *anim) {
    return anim->frame_count;
}
