#ifndef FILEMANAGER_H
#define FILEMANAGER_H

#include "animation.h"
#include "audio.h"
#include <stdbool.h>

#define SAVE_DIR "ux0:data/FlipnoteVita/"
#define MAX_SAVE_SLOTS 100

typedef struct {
    char filename[256];
    char title[64];
    char author[64];
    int frame_count;
    bool exists;
} SaveSlotInfo;

// Formato file .fnv (Flipnote Vita)
typedef struct {
    char magic[4];          // "FNVT"
    uint32_t version;       // 1
    char title[64];
    char author[64];
    uint32_t frame_count;
    float playback_speed;
    bool loop;
    uint32_t canvas_width;
    uint32_t canvas_height;
    uint32_t layers_per_frame;
    // Seguito da frame data
} FNVHeader;

void filemanager_init(void);

// Salvataggio/Caricamento
bool filemanager_save(AnimationContext *anim, AudioContext *audio, const char *filename);
bool filemanager_load(AnimationContext *anim, AudioContext *audio, DrawingContext *draw, const char *filename);
bool filemanager_delete(const char *filename);

// Lista file
int filemanager_list_saves(SaveSlotInfo *slots, int max_slots);
bool filemanager_exists(const char *filename);

// Auto-save
void filemanager_autosave(AnimationContext *anim, AudioContext *audio);
bool filemanager_load_autosave(AnimationContext *anim, AudioContext *audio, DrawingContext *draw);

// Export
bool filemanager_export_gif(AnimationContext *anim, const char *filename);
bool filemanager_export_png_sequence(AnimationContext *anim, const char *dirname);
bool filemanager_export_frame_png(AnimationContext *anim, int frame, const char *filename);

// Genera nome file
void filemanager_generate_filename(char *buffer, int buffer_size);

#endif
