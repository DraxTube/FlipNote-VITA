#include "filemanager.h"
#include "colors.h"
#include <psp2/io/fcntl.h>
#include <psp2/io/dirent.h>
#include <psp2/io/stat.h>
#include <psp2/rtc.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void filemanager_init(void) {
    sceIoMkdir(SAVE_DIR, 0777);
}

void filemanager_generate_filename(char *buffer, int buffer_size) {
    SceDateTime time;
    sceRtcGetCurrentClockLocalTime(&time);
    snprintf(buffer, buffer_size, "%s%04d%02d%02d_%02d%02d%02d.fnv",
             SAVE_DIR, time.year, time.month, time.day,
             time.hour, time.minute, time.second);
}

bool filemanager_save(AnimationContext *anim, AudioContext *audio, const char *filename) {
    SceUID fd = sceIoOpen(filename, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
    if (fd < 0) return false;
    
    // Header
    FNVHeader header;
    memset(&header, 0, sizeof(header));
    memcpy(header.magic, "FNVT", 4);
    header.version = 1;
    strncpy(header.title, anim->title, 63);
    strncpy(header.author, anim->author, 63);
    header.frame_count = anim->frame_count;
    header.playback_speed = anim->playback_speed;
    header.loop = anim->loop;
    header.canvas_width = CANVAS_WIDTH;
    header.canvas_height = CANVAS_HEIGHT;
    header.layers_per_frame = MAX_LAYERS;
    
    sceIoWrite(fd, &header, sizeof(header));
    
    // Frame data
    for (int f = 0; f < anim->frame_count; f++) {
        // Frame speed
        sceIoWrite(fd, &anim->frames[f].frame_speed, sizeof(float));
        sceIoWrite(fd, &anim->frames[f].is_keyframe, sizeof(bool));
        
        // Layer data (compressione RLE semplice)
        for (int l = 0; l < MAX_LAYERS; l++) {
            uint8_t *pixels = anim->frames[f].layers[l].pixels;
            int total = CANVAS_WIDTH * CANVAS_HEIGHT;
            
            // RLE encoding
            int i = 0;
            while (i < total) {
                uint8_t val = pixels[i];
                int count = 1;
                while (i + count < total && pixels[i + count] == val && count < 255) {
                    count++;
                }
                uint8_t c = (uint8_t)count;
                sceIoWrite(fd, &c, 1);
                sceIoWrite(fd, &val, 1);
                i += count;
            }
            
            // Marker fine layer
            uint8_t end_marker[2] = {0, 0xFF};
            sceIoWrite(fd, end_marker, 2);
        }
    }
    
    // Audio triggers
    uint32_t audio_marker = 0xAUD10;
    sceIoWrite(fd, &audio_marker, sizeof(uint32_t));
    
    for (int f = 0; f < anim->frame_count && f < 999; f++) {
        for (int s = 0; s < MAX_SOUND_EFFECTS; s++) {
            uint8_t trigger = audio->se_triggers[f][s] ? 1 : 0;
            sceIoWrite(fd, &trigger, 1);
        }
    }
    
    // Audio recordings
    for (int s = 0; s < MAX_SOUND_EFFECTS; s++) {
        int32_t sample_count = audio->sound_effects[s].sample_count;
        sceIoWrite(fd, &sample_count, sizeof(int32_t));
        if (sample_count > 0 && audio->sound_effects[s].data) {
            sceIoWrite(fd, audio->sound_effects[s].data, sample_count * sizeof(int16_t));
        }
    }
    
    sceIoClose(fd);
    return true;
}

bool filemanager_load(AnimationContext *anim, AudioContext *audio, DrawingContext *draw, const char *filename) {
    SceUID fd = sceIoOpen(filename, SCE_O_RDONLY, 0);
    if (fd < 0) return false;
    
    FNVHeader header;
    sceIoRead(fd, &header, sizeof(header));
    
    if (memcmp(header.magic, "FNVT", 4) != 0) {
        sceIoClose(fd);
        return false;
    }
    
    // Reset animation
    animation_free(anim);
    animation_init(anim);
    
    strncpy(anim->title, header.title, 63);
    strncpy(anim->author, header.author, 63);
    anim->playback_speed = header.playback_speed;
    anim->loop = header.loop;
    
    // Carica frame
    anim->frame_count = 0;
    for (int f = 0; f < (int)header.frame_count; f++) {
        if (f > 0) animation_add_frame(anim);
        else anim->frame_count = 1;
        
        sceIoRead(fd, &anim->frames[f].frame_speed, sizeof(float));
        sceIoRead(fd, &anim->frames[f].is_keyframe, sizeof(bool));
        
        for (int l = 0; l < MAX_LAYERS; l++) {
            uint8_t *pixels = anim->frames[f].layers[l].pixels;
            int pos = 0;
            
            while (pos < CANVAS_WIDTH * CANVAS_HEIGHT) {
                uint8_t count, val;
                if (sceIoRead(fd, &count, 1) <= 0) break;
                if (sceIoRead(fd, &val, 1) <= 0) break;
                
                // Check end marker
                if (count == 0 && val == 0xFF) break;
                
                for (int i = 0; i < count && pos < CANVAS_WIDTH * CANVAS_HEIGHT; i++) {
                    pixels[pos++] = val;
                }
            }
        }
    }
    
    // Audio triggers
    uint32_t audio_marker;
    if (sceIoRead(fd, &audio_marker, sizeof(uint32_t)) == sizeof(uint32_t)) {
        if (audio_marker == 0xAUD10) {
            for (int f = 0; f < (int)header.frame_count && f < 999; f++) {
                for (int s = 0; s < MAX_SOUND_EFFECTS; s++) {
                    uint8_t trigger;
                    sceIoRead(fd, &trigger, 1);
                    audio->se_triggers[f][s] = trigger;
                }
            }
            
            // Audio recordings
            for (int s = 0; s < MAX_SOUND_EFFECTS; s++) {
                int32_t sample_count;
                sceIoRead(fd, &sample_count, sizeof(int32_t));
                if (sample_count > 0 && sample_count < MAX_RECORDING_SAMPLES) {
                    if (audio->sound_effects[s].data) {
                        free(audio->sound_effects[s].data);
                    }
                    audio->sound_effects[s].data = (int16_t *)malloc(sample_count * sizeof(int16_t));
                    sceIoRead(fd, audio->sound_effects[s].data, sample_count * sizeof(int16_t));
                    audio->sound_effects[s].sample_count = sample_count;
                    audio->sound_effects[s].sample_rate = AUDIO_SAMPLE_RATE;
                }
            }
        }
    }
    
    sceIoClose(fd);
    
    // Carica primo frame nel drawing context
    anim->current_frame = 0;
    animation_load_current_from_draw(anim, draw);
    
    return true;
}

bool filemanager_delete(const char *filename) {
    return sceIoRemove(filename) >= 0;
}

int filemanager_list_saves(SaveSlotInfo *slots, int max_slots) {
    SceUID dir = sceIoDopen(SAVE_DIR);
    if (dir < 0) return 0;
    
    int count = 0;
    SceIoDirent entry;
    
    while (count < max_slots && sceIoDread(dir, &entry) > 0) {
        int len = strlen(entry.d_name);
        if (len > 4 && strcmp(&entry.d_name[len - 4], ".fnv") == 0) {
            snprintf(slots[count].filename, 256, "%s%s", SAVE_DIR, entry.d_name);
            
            // Prova a leggere header per info
            SceUID fd = sceIoOpen(slots[count].filename, SCE_O_RDONLY, 0);
            if (fd >= 0) {
                FNVHeader header;
                sceIoRead(fd, &header, sizeof(header));
                if (memcmp(header.magic, "FNVT", 4) == 0) {
                    strncpy(slots[count].title, header.title, 63);
                    strncpy(slots[count].author, header.author, 63);
                    slots[count].frame_count = header.frame_count;
                    slots[count].exists = true;
                }
                sceIoClose(fd);
            }
            count++;
        }
    }
    
    sceIoDclose(dir);
    return count;
}

bool filemanager_exists(const char *filename) {
    SceIoStat stat;
    return sceIoGetstat(filename, &stat) >= 0;
}

void filemanager_autosave(AnimationContext *anim, AudioContext *audio) {
    char path[256];
    snprintf(path, sizeof(path), "%s_autosave.fnv", SAVE_DIR);
    filemanager_save(anim, audio, path);
}

bool filemanager_load_autosave(AnimationContext *anim, AudioContext *audio, DrawingContext *draw) {
    char path[256];
    snprintf(path, sizeof(path), "%s_autosave.fnv", SAVE_DIR);
    return filemanager_load(anim, audio, draw, path);
}

bool filemanager_export_gif(AnimationContext *anim, const char *filename) {
    // GIF export semplificato - scrive un GIF animato
    // GIF89a format
    SceUID fd = sceIoOpen(filename, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
    if (fd < 0) return false;
    
    // GIF Header
    uint8_t gif_header[] = {
        'G', 'I', 'F', '8', '9', 'a',
        (CANVAS_WIDTH & 0xFF), (CANVAS_WIDTH >> 8) & 0xFF,
        (CANVAS_HEIGHT & 0xFF), (CANVAS_HEIGHT >> 8) & 0xFF,
        0xF1, // GCT flag, 4 colors
        0x00, // BG color
        0x00  // Aspect ratio
    };
    sceIoWrite(fd, gif_header, sizeof(gif_header));
    
    // Global Color Table (4 colors: white, black, red, blue)
    uint8_t gct[] = {
        255, 255, 255,  // White
        0, 0, 0,        // Black
        255, 0, 0,      // Red
        0, 0, 255,      // Blue
    };
    sceIoWrite(fd, gct, sizeof(gct));
    
    // Netscape extension per loop
    uint8_t ns_ext[] = {
        0x21, 0xFF, 0x0B,
        'N','E','T','S','C','A','P','E','2','.','0',
        0x03, 0x01, 0x00, 0x00, 0x00
    };
    sceIoWrite(fd, ns_ext, sizeof(ns_ext));
    
    int delay = (int)(100.0f / anim->playback_speed);
    
    for (int f = 0; f < anim->frame_count; f++) {
        // Graphic Control Extension
        uint8_t gce[] = {
            0x21, 0xF9, 0x04,
            0x04, // Dispose: restore bg
            (uint8_t)(delay & 0xFF), (uint8_t)((delay >> 8) & 0xFF),
            0x00, // Transparent color
            0x00
        };
        sceIoWrite(fd, gce, sizeof(gce));
        
        // Image Descriptor
        uint8_t img_desc[] = {
            0x2C,
            0x00, 0x00, 0x00, 0x00, // Left, Top
            (CANVAS_WIDTH & 0xFF), (CANVAS_WIDTH >> 8) & 0xFF,
            (CANVAS_HEIGHT & 0xFF), (CANVAS_HEIGHT >> 8) & 0xFF,
            0x00 // No local color table
        };
        sceIoWrite(fd, img_desc, sizeof(img_desc));
        
        // LZW Minimum Code Size
        uint8_t lzw_min = 2;
        sceIoWrite(fd, &lzw_min, 1);
        
        // Dati pixel semplificati (non compressi LZW completi, sub-blocks)
        // Per semplicità, scriviamo dati raw in sub-blocks
        LayerData *layers = anim->frames[f].layers;
        
        int total_pixels = CANVAS_WIDTH * CANVAS_HEIGHT;
        int written = 0;
        
        while (written < total_pixels) {
            int block_size = total_pixels - written;
            if (block_size > 254) block_size = 254;
            
            uint8_t bs = (uint8_t)(block_size + 1);
            sceIoWrite(fd, &bs, 1);
            
            uint8_t clear_code = 4; // 2^lzw_min
            if (written == 0) {
                sceIoWrite(fd, &clear_code, 1);
            }
            
            for (int i = 0; i < block_size; i++) {
                int px = written + i;
                uint8_t color = layers[0].pixels[px];
                if (color >= 4) color = 0;
                sceIoWrite(fd, &color, 1);
            }
            
            written += block_size;
        }
        
        // Block terminator
        uint8_t term = 0x00;
        sceIoWrite(fd, &term, 1);
    }
    
    // GIF Trailer
    uint8_t trailer = 0x3B;
    sceIoWrite(fd, &trailer, 1);
    
    sceIoClose(fd);
    return true;
}

bool filemanager_export_png_sequence(AnimationContext *anim, const char *dirname) {
    sceIoMkdir(dirname, 0777);
    
    for (int f = 0; f < anim->frame_count; f++) {
        char path[256];
        snprintf(path, sizeof(path), "%s/frame_%04d.png", dirname, f);
        filemanager_export_frame_png(anim, f, path);
    }
    return true;
}

bool filemanager_export_frame_png(AnimationContext *anim, int frame, const char *filename) {
    if (frame < 0 || frame >= anim->frame_count) return false;
    
    // Crea texture vita2d temporanea
    vita2d_texture *tex = vita2d_create_empty_texture(CANVAS_WIDTH, CANVAS_HEIGHT);
    if (!tex) return false;
    
    unsigned int *tex_data = (unsigned int *)vita2d_texture_get_datap(tex);
    int stride = vita2d_texture_get_stride(tex) / 4;
    
    // Render composito
    for (int y = 0; y < CANVAS_HEIGHT; y++) {
        for (int x = 0; x < CANVAS_WIDTH; x++) {
            unsigned int final_color = COLOR_WHITE;
            
            for (int l = MAX_LAYERS - 1; l >= 0; l--) {
                uint8_t pix = anim->frames[frame].layers[l].pixels[y * CANVAS_WIDTH + x];
                if (pix != 0) {
                    final_color = drawing_get_rgba_color(pix, l);
                }
            }
            
            tex_data[y * stride + x] = final_color;
        }
    }
    
    // Nota: vita2d non ha export PNG diretto, salviamo come BMP raw
    // In alternativa usare libpng se disponibile
    SceUID fd = sceIoOpen(filename, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
    if (fd >= 0) {
        // BMP header semplice
        uint32_t file_size = 54 + CANVAS_WIDTH * CANVAS_HEIGHT * 3;
        uint8_t bmp_header[54] = {0};
        bmp_header[0] = 'B'; bmp_header[1] = 'M';
        *(uint32_t*)&bmp_header[2] = file_size;
        *(uint32_t*)&bmp_header[10] = 54;
        *(uint32_t*)&bmp_header[14] = 40;
        *(int32_t*)&bmp_header[18] = CANVAS_WIDTH;
        *(int32_t*)&bmp_header[22] = -CANVAS_HEIGHT; // Top-down
        *(uint16_t*)&bmp_header[26] = 1;
        *(uint16_t*)&bmp_header[28] = 24;
        
        sceIoWrite(fd, bmp_header, 54);
        
        for (int y = 0; y < CANVAS_HEIGHT; y++) {
            for (int x = 0; x < CANVAS_WIDTH; x++) {
                unsigned int c = tex_data[y * stride + x];
                uint8_t rgb[3] = {
                    (c >> 16) & 0xFF, // B
                    (c >> 8) & 0xFF,  // G
                    c & 0xFF          // R
                };
                sceIoWrite(fd, rgb, 3);
            }
            // Padding BMP
            int padding = (4 - (CANVAS_WIDTH * 3) % 4) % 4;
            uint8_t pad[3] = {0, 0, 0};
            if (padding > 0) sceIoWrite(fd, pad, padding);
        }
        
        sceIoClose(fd);
    }
    
    vita2d_free_texture(tex);
    return true;
}
