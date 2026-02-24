#include <psp2/kernel/processmgr.h>
#include <psp2/power.h>
#include <psp2/display.h>
#include <psp2/ctrl.h>
#include <psp2/touch.h>
#include <psp2/rtc.h>
#include <vita2d.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "drawing.h"
#include "animation.h"
#include "audio.h"
#include "input.h"
#include "ui.h"
#include "filemanager.h"
#include "colors.h"

// Contesto globale
static DrawingContext g_draw;
static AnimationContext g_anim;
static AudioContext g_audio;
static InputState g_input;
static UIContext g_ui;

// Timing
static SceUInt64 prev_time;
static float delta_time;

static void app_init(void) {
    // Abilita max CPU/GPU
    scePowerSetArmClockFrequency(444);
    scePowerSetBusClockFrequency(222);
    scePowerSetGpuClockFrequency(222);
    scePowerSetGpuXbarClockFrequency(166);
    
    // Inizializza vita2d
    vita2d_init();
    vita2d_set_clear_color(RGBA8(0, 0, 0, 255));
    
    // Inizializza input
    input_init();
    
    // Inizializza moduli
    drawing_init(&g_draw);
    animation_init(&g_anim);
    audio_init(&g_audio);
    ui_init(&g_ui);
    filemanager_init();
    
    // Salva stato iniziale per undo
    drawing_save_undo(&g_draw);
    
    // Timing
    SceRtcTick tick;
    sceRtcGetCurrentTick(&tick);
    prev_time = tick.tick;
}

static float get_delta_time(void) {
    SceRtcTick tick;
    sceRtcGetCurrentTick(&tick);
    SceUInt64 current_time = tick.tick;
    
    float dt = (float)(current_time - prev_time) / 1000000.0f;
    prev_time = current_time;
    
    // Clamp delta time
    if (dt > 0.1f) dt = 0.1f;
    if (dt < 0.0001f) dt = 0.0001f;
    
    return dt;
}

static void app_render(void) {
    vita2d_start_drawing();
    vita2d_clear_screen();
    
    switch (g_ui.current_screen) {
        case SCREEN_TITLE:
            ui_render_title_screen(&g_ui, &g_input);
            break;
            
        case SCREEN_EDITOR:
            ui_render_editor(&g_ui, &g_draw, &g_anim, &g_audio, &g_input);
            break;
            
        case SCREEN_PLAYBACK:
            ui_render_playback(&g_ui, &g_draw, &g_anim, &g_audio, &g_input);
            break;
            
        case SCREEN_FILE_BROWSER:
            ui_render_file_browser(&g_ui, &g_input);
            break;
            
        case SCREEN_SETTINGS:
            ui_render_settings(&g_ui, &g_input);
            break;
            
        case SCREEN_COLOR_PICKER:
            // Render editor dietro
            ui_render_editor(&g_ui, &g_draw, &g_anim, &g_audio, &g_input);
            ui_render_color_picker(&g_ui, &g_draw, &g_input);
            break;
            
        case SCREEN_LAYER_MENU:
            ui_render_editor(&g_ui, &g_draw, &g_anim, &g_audio, &g_input);
            ui_render_layer_menu(&g_ui, &g_draw, &g_input);
            break;
            
        case SCREEN_SPEED_SETTINGS:
            ui_render_editor(&g_ui, &g_draw, &g_anim, &g_audio, &g_input);
            ui_render_speed_settings(&g_ui, &g_anim, &g_input);
            break;
            
        case SCREEN_SOUND_EDITOR:
            ui_render_editor(&g_ui, &g_draw, &g_anim, &g_audio, &g_input);
            ui_render_sound_editor(&g_ui, &g_audio, &g_anim, &g_input);
            break;
            
        case SCREEN_EXPORT:
            ui_render_editor(&g_ui, &g_draw, &g_anim, &g_audio, &g_input);
            ui_render_export_menu(&g_ui, &g_anim, &g_audio, &g_input);
            break;
            
        case SCREEN_HELP:
            ui_render_help(&g_ui, &g_input);
            break;
            
        case SCREEN_FRAME_MENU:
            ui_render_editor(&g_ui, &g_draw, &g_anim, &g_audio, &g_input);
            ui_render_frame_menu(&g_ui, &g_anim, &g_draw, &g_input);
            break;
            
        default:
            ui_render_title_screen(&g_ui, &g_input);
            break;
    }
    
    // Dialog sopra a tutto
    if (g_ui.dialog_active) {
        ui_render_dialog(&g_ui, &g_input);
    }
    
    // Toast sopra a tutto
    ui_render_toast(&g_ui, delta_time);
    
    vita2d_end_drawing();
    vita2d_swap_buffers();
    sceDisplayWaitVblankStart();
}

static void app_update(void) {
    delta_time = get_delta_time();
    
    // Aggiorna input
    input_update(&g_input);
    
    // Aggiorna logica UI
    ui_update(&g_ui, &g_draw, &g_anim, &g_audio, &g_input, delta_time);
    
    // Autosave periodico (ogni 5 minuti circa)
    static float autosave_timer = 0;
    autosave_timer += delta_time;
    if (autosave_timer > 300.0f) { // 5 minuti
        autosave_timer = 0;
        if (g_ui.current_screen == SCREEN_EDITOR) {
            animation_save_current_to_draw(&g_anim, &g_draw);
            filemanager_autosave(&g_anim, &g_audio);
        }
    }
}

static void app_cleanup(void) {
    // Autosave finale
    animation_save_current_to_draw(&g_anim, &g_draw);
    filemanager_autosave(&g_anim, &g_audio);
    
    audio_free(&g_audio);
    animation_free(&g_anim);
    vita2d_fini();
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    app_init();
    
    while (1) {
        app_update();
        app_render();
    }
    
    app_cleanup();
    sceKernelExitProcess(0);
    return 0;
}
