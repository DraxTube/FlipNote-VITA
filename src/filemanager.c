#include "filemanager.h"
#include "colors.h"
#include <psp2/io/fcntl.h>
#include <psp2/io/dirent.h>
#include <psp2/io/stat.h>
#include <psp2/rtc.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define AUDIO_MARKER 0xADD100

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

int filemanager_save(AnimationContext *anim, AudioContext *audio, const char *filename) {
    SceUID fd;
    FNVHeader header;
    int f, l, i, total;
    uint8_t c, val;
    uint8_t end_marker[2];
    uint32_t audio_marker;
    uint8_t trigger;
    int32_t sample_count;
    int count;

    fd = sceIoOpen(filename, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
    if (fd < 0) return 0;

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

    for (f = 0; f < anim->frame_count; f++) {
        sceIoWrite(fd, &anim->frames[f].frame_speed, sizeof(float));
        sceIoWrite(fd, &anim->frames[f].is_keyframe, sizeof(int));

        for (l = 0; l < MAX_LAYERS; l++) {
            uint8_t *pixels = anim->frames[f].layers[l].pixels;
            total = CANVAS_WIDTH * CANVAS_HEIGHT;

            i = 0;
            while (i < total) {
                val = pixels[i];
                count = 1;
                while (i + count < total && pixels[i + count] == val && count < 255) {
                    count++;
                }
                c = (uint8_t)count;
                sceIoWrite(fd, &c, 1);
                sceIoWrite(fd, &val, 1);
                i += count;
            }

            end_marker[0] = 0;
            end_marker[1] = 0xFF;
            sceIoWrite(fd, end_marker, 2);
        }
    }

    audio_marker = AUDIO_MARKER;
    sceIoWrite(fd, &audio_marker, sizeof(uint32_t));

    for (f = 0; f < anim->frame_count && f < 999; f++) {
        for (i = 0; i < MAX_SOUND_EFFECTS; i++) {
            trigger = audio->se_triggers[f][i] ? 1 : 0;
            sceIoWrite(fd, &trigger, 1);
        }
    }

    for (i = 0; i < MAX_SOUND_EFFECTS; i++) {
        sample_count = audio->sound_effects[i].sample_count;
        sceIoWrite(fd, &sample_count, sizeof(int32_t));
        if (sample_count > 0 && audio->sound_effects[i].data) {
            sceIoWrite(fd, audio->sound_effects[i].data, sample_count * sizeof(int16_t));
        }
    }

    sceIoClose(fd);
    return 1;
}

int filemanager_load(AnimationContext *anim, AudioContext *audio,
                     DrawingContext *draw, const char *filename)
{
    SceUID fd;
    FNVHeader header;
    int f, l, pos;
    uint8_t count_byte, val;
    uint32_t audio_marker;
    uint8_t trigger;
    int32_t sample_count;
    int i;

    fd = sceIoOpen(filename, SCE_O_RDONLY, 0);
    if (fd < 0) return 0;

    sceIoRead(fd, &header, sizeof(header));

    if (memcmp(header.magic, "FNVT", 4) != 0) {
        sceIoClose(fd);
        return 0;
    }

    animation_free(anim);
    animation_init(anim);

    strncpy(anim->title, header.title, 63);
    strncpy(anim->author, header.author, 63);
    anim->playback_speed = header.playback_speed;
    anim->loop = header.loop;

    anim->frame_count = 0;
    for (f = 0; f < (int)header.frame_count; f++) {
        if (f > 0) animation_add_frame(anim);
        else anim->frame_count = 1;

        sceIoRead(fd, &anim->frames[f].frame_speed, sizeof(float));
        sceIoRead(fd, &anim->frames[f].is_keyframe, sizeof(int));

        for (l = 0; l < MAX_LAYERS; l++) {
            uint8_t *pixels = anim->frames[f].layers[l].pixels;
            pos = 0;

            while (pos < CANVAS_WIDTH * CANVAS_HEIGHT) {
                if (sceIoRead(fd, &count_byte, 1) <= 0) break;
                if (sceIoRead(fd, &val, 1) <= 0) break;

                if (count_byte == 0 && val == 0xFF) break;

                for (i = 0; i < count_byte && pos < CANVAS_WIDTH * CANVAS_HEIGHT; i++) {
                    pixels[pos++] = val;
                }
            }
        }
    }

    if (sceIoRead(fd, &audio_marker, sizeof(uint32_t)) == sizeof(uint32_t)) {
        if (audio_marker == AUDIO_MARKER) {
            for (f = 0; f < (int)header.frame_count && f < 999; f++) {
                for (i = 0; i < MAX_SOUND_EFFECTS; i++) {
                    sceIoRead(fd, &trigger, 1);
                    audio->se_triggers[f][i] = trigger;
                }
            }

            for (i = 0; i < MAX_SOUND_EFFECTS; i++) {
                sceIoRead(fd, &sample_count, sizeof(int32_t));
                if (sample_count > 0 && sample_count < MAX_RECORDING_SAMPLES) {
                    if (audio->sound_effects[i].data) {
                        free(audio->sound_effects[i].data);
                    }
                    audio->sound_effects[i].data =
                        (int16_t *)malloc(sample_count * sizeof(int16_t));
                    sceIoRead(fd, audio->sound_effects[i].data,
                              sample_count * sizeof(int16_t));
                    audio->sound_effects[i].sample_count = sample_count;
                    audio->sound_effects[i].sample_rate = AUDIO_SAMPLE_RATE;
                }
            }
        }
    }

    sceIoClose(fd);

    anim->current_frame = 0;
    animation_load_current_from_draw(anim, draw);

    return 1;
}

int filemanager_delete(const char *filename) {
    return sceIoRemove(filename) >= 0;
}

int filemanager_list_saves(SaveSlotInfo *slots, int max_slots) {
    SceUID dir;
    int count = 0;
    SceIoDirent entry;
    int len;
    SceUID fd;
    FNVHeader header;

    dir = sceIoDopen(SAVE_DIR);
    if (dir < 0) return 0;

    memset(&entry, 0, sizeof(SceIoDirent));

    while (count < max_slots && sceIoDread(dir, &entry) > 0) {
        len = (int)strlen(entry.d_name);
        if (len > 4 && strcmp(&entry.d_name[len - 4], ".fnv") == 0) {
            snprintf(slots[count].filename, 256, "%s%s", SAVE_DIR, entry.d_name);

            fd = sceIoOpen(slots[count].filename, SCE_O_RDONLY, 0);
            if (fd >= 0) {
                sceIoRead(fd, &header, sizeof(header));
                if (memcmp(header.magic, "FNVT", 4) == 0) {
                    strncpy(slots[count].title, header.title, 63);
                    slots[count].title[63] = '\0';
                    strncpy(slots[count].author, header.author, 63);
                    slots[count].author[63] = '\0';
                    slots[count].frame_count = header.frame_count;
                    slots[count].exists = 1;
                }
                sceIoClose(fd);
            }
            count++;
        }
        memset(&entry, 0, sizeof(SceIoDirent));
    }

    sceIoDclose(dir);
    return count;
}

int filemanager_exists(const char *filename) {
    SceIoStat stat;
    return sceIoGetstat(filename, &stat) >= 0;
}

void filemanager_autosave(AnimationContext *anim, AudioContext *audio) {
    char path[256];
    snprintf(path, sizeof(path), "%s_autosave.fnv", SAVE_DIR);
    filemanager_save(anim, audio, path);
}

int filemanager_load_autosave(AnimationContext *anim, AudioContext *audio, DrawingContext *draw) {
    char path[256];
    snprintf(path, sizeof(path), "%s_autosave.fnv", SAVE_DIR);
    return filemanager_load(anim, audio, draw, path);
}

int filemanager_export_gif(AnimationContext *anim, const char *filename) {
    SceUID fd;
    int f, delay;
    uint8_t gif_header[13];
    uint8_t gct[12];
    uint8_t ns_ext[19];
    uint8_t gce[8];
    uint8_t img_desc[10];
    uint8_t lzw_min;
    int total_pixels, written;
    uint8_t trailer;

    fd = sceIoOpen(filename, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
    if (fd < 0) return 0;

    gif_header[0] = 'G'; gif_header[1] = 'I'; gif_header[2] = 'F';
    gif_header[3] = '8'; gif_header[4] = '9'; gif_header[5] = 'a';
    gif_header[6] = (CANVAS_WIDTH & 0xFF);
    gif_header[7] = (CANVAS_WIDTH >> 8) & 0xFF;
    gif_header[8] = (CANVAS_HEIGHT & 0xFF);
    gif_header[9] = (CANVAS_HEIGHT >> 8) & 0xFF;
    gif_header[10] = 0xF1;
    gif_header[11] = 0x00;
    gif_header[12] = 0x00;
    sceIoWrite(fd, gif_header, 13);

    gct[0] = 255; gct[1] = 255; gct[2] = 255;
    gct[3] = 0;   gct[4] = 0;   gct[5] = 0;
    gct[6] = 255; gct[7] = 0;   gct[8] = 0;
    gct[9] = 0;   gct[10] = 0;  gct[11] = 255;
    sceIoWrite(fd, gct, 12);

    ns_ext[0] = 0x21; ns_ext[1] = 0xFF; ns_ext[2] = 0x0B;
    ns_ext[3] = 'N';  ns_ext[4] = 'E';  ns_ext[5] = 'T';
    ns_ext[6] = 'S';  ns_ext[7] = 'C';  ns_ext[8] = 'A';
    ns_ext[9] = 'P';  ns_ext[10] = 'E'; ns_ext[11] = '2';
    ns_ext[12] = '.'; ns_ext[13] = '0';
    ns_ext[14] = 0x03; ns_ext[15] = 0x01;
    ns_ext[16] = 0x00; ns_ext[17] = 0x00; ns_ext[18] = 0x00;
    sceIoWrite(fd, ns_ext, 19);

    delay = (int)(100.0f / anim->playback_speed);

    for (f = 0; f < anim->frame_count; f++) {
        gce[0] = 0x21; gce[1] = 0xF9; gce[2] = 0x04;
        gce[3] = 0x04;
        gce[4] = (uint8_t)(delay & 0xFF);
        gce[5] = (uint8_t)((delay >> 8) & 0xFF);
        gce[6] = 0x00; gce[7] = 0x00;
        sceIoWrite(fd, gce, 8);

        img_desc[0] = 0x2C;
        img_desc[1] = 0x00; img_desc[2] = 0x00;
        img_desc[3] = 0x00; img_desc[4] = 0x00;
        img_desc[5] = (CANVAS_WIDTH & 0xFF);
        img_desc[6] = (CANVAS_WIDTH >> 8) & 0xFF;
        img_desc[7] = (CANVAS_HEIGHT & 0xFF);
        img_desc[8] = (CANVAS_HEIGHT >> 8) & 0xFF;
        img_desc[9] = 0x00;
        sceIoWrite(fd, img_desc, 10);

        lzw_min = 2;
        sceIoWrite(fd, &lzw_min, 1);

        total_pixels = CANVAS_WIDTH * CANVAS_HEIGHT;
        written = 0;

        while (written < total_pixels) {
            int block_size = total_pixels - written;
            uint8_t bs, clear_code, color;
            int pi;

            if (block_size > 254) block_size = 254;

            bs = (uint8_t)(block_size + 1);
            sceIoWrite(fd, &bs, 1);

            if (written == 0) {
                clear_code = 4;
                sceIoWrite(fd, &clear_code, 1);
            }

            for (pi = 0; pi < block_size; pi++) {
                int px = written + pi;
                color = anim->frames[f].layers[0].pixels[px];
                if (color >= 4) color = 0;
                sceIoWrite(fd, &color, 1);
            }

            written += block_size;
        }

        {
            uint8_t term = 0x00;
            sceIoWrite(fd, &term, 1);
        }
    }

    trailer = 0x3B;
    sceIoWrite(fd, &trailer, 1);

    sceIoClose(fd);
    return 1;
}

int filemanager_export_png_sequence(AnimationContext *anim, const char *dirname) {
    int f;
    char path[256];

    sceIoMkdir(dirname, 0777);

    for (f = 0; f < anim->frame_count; f++) {
        snprintf(path, sizeof(path), "%s/frame_%04d.bmp", dirname, f);
        filemanager_export_frame_png(anim, f, path);
    }
    return 1;
}

int filemanager_export_frame_png(AnimationContext *anim, int frame, const char *filename) {
    SceUID fd;
    uint32_t file_size;
    uint8_t bmp_header[54];
    int x, y, l, padding;
    unsigned int final_color;
    uint8_t rgb[3];
    uint8_t pad_bytes[3] = {0, 0, 0};
    uint8_t pix;

    if (frame < 0 || frame >= anim->frame_count) return 0;

    fd = sceIoOpen(filename, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
    if (fd < 0) return 0;

    padding = (4 - (CANVAS_WIDTH * 3) % 4) % 4;
    file_size = 54 + (CANVAS_WIDTH * 3 + padding) * CANVAS_HEIGHT;

    memset(bmp_header, 0, 54);
    bmp_header[0] = 'B'; bmp_header[1] = 'M';
    *(uint32_t *)&bmp_header[2] = file_size;
    *(uint32_t *)&bmp_header[10] = 54;
    *(uint32_t *)&bmp_header[14] = 40;
    *(int32_t *)&bmp_header[18] = CANVAS_WIDTH;
    *(int32_t *)&bmp_header[22] = -(int32_t)CANVAS_HEIGHT;
    *(uint16_t *)&bmp_header[26] = 1;
    *(uint16_t *)&bmp_header[28] = 24;

    sceIoWrite(fd, bmp_header, 54);

    for (y = 0; y < CANVAS_HEIGHT; y++) {
        for (x = 0; x < CANVAS_WIDTH; x++) {
            final_color = COLOR_WHITE;

            for (l = MAX_LAYERS - 1; l >= 0; l--) {
                pix = anim->frames[frame].layers[l].pixels[y * CANVAS_WIDTH + x];
                if (pix != 0) {
                    final_color = drawing_get_rgba_color(pix, l);
                }
            }

            rgb[0] = (final_color >> 16) & 0xFF;
            rgb[1] = (final_color >> 8) & 0xFF;
            rgb[2] = final_color & 0xFF;
            sceIoWrite(fd, rgb, 3);
        }
        if (padding > 0)
            sceIoWrite(fd, pad_bytes, padding);
    }

    sceIoClose(fd);
    return 1;
}
