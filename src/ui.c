#include "ui.h"
#include "colors.h"
#include "filemanager.h"
#include <vita2d.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

static vita2d_pgf *font = NULL;

static const char *tool_names[] = {
    "Penna", "Gomma", "Secchiello", "Linea", 
    "Rettangolo", "Cerchio", "Sposta", "Seleziona",
    "Timbro", "Contagocce"
};

static const char *tool_icons[] = {
    "P", "E", "B", "L", "R", "C", "M", "S", "T", "D"
};

void ui_init(UIContext *ui) {
    memset(ui, 0, sizeof(UIContext));
    ui->current_screen = SCREEN_TITLE;
    ui->timeline_visible_frames = 10;
    ui->show_frame_counter = true;
    ui->theme = 0;
    
    if (!font) {
        font = vita2d_load_default_pgf();
    }
}

static unsigned int get_theme_color(UIContext *ui) {
    switch (ui->theme) {
        case 0: return RGBA8(0, 180, 0, 255);    // Verde Flipnote
        case 1: return RGBA8(200, 0, 0, 255);     // Rosso
        case 2: return RGBA8(0, 0, 200, 255);     // Blu
        default: return RGBA8(0, 180, 0, 255);
    }
}

static unsigned int get_theme_dark(UIContext *ui) {
    switch (ui->theme) {
        case 0: return RGBA8(0, 120, 0, 255);
        case 1: return RGBA8(140, 0, 0, 255);
        case 2: return RGBA8(0, 0, 140, 255);
        default: return RGBA8(0, 120, 0, 255);
    }
}

static unsigned int get_theme_light(UIContext *ui) {
    switch (ui->theme) {
        case 0: return RGBA8(100, 220, 100, 255);
        case 1: return RGBA8(255, 100, 100, 255);
        case 2: return RGBA8(100, 100, 255, 255);
        default: return RGBA8(100, 220, 100, 255);
    }
}

static void draw_text(int x, int y, unsigned int color, const char *text) {
    if (font) {
        vita2d_pgf_draw_text(font, x, y, color, 1.0f, text);
    }
}

static void draw_text_scaled(int x, int y, unsigned int color, float scale, const char *text) {
    if (font) {
        vita2d_pgf_draw_text(font, x, y, color, scale, text);
    }
}

static bool point_in_rect(int px, int py, int rx, int ry, int rw, int rh) {
    return px >= rx && px < rx + rw && py >= ry && py < ry + rh;
}

bool ui_button(int x, int y, int w, int h, const char *label, unsigned int color, InputState *input) {
    bool hovered = input->touch_active && point_in_rect(input->touch_x, input->touch_y, x, y, w, h);
    bool clicked = input->touch_just_pressed && point_in_rect(input->touch_x, input->touch_y, x, y, w, h);
    
    unsigned int draw_color = hovered ? RGBA8(
        ((color & 0xFF) * 3 / 4 + 64),
        (((color >> 8) & 0xFF) * 3 / 4 + 64),
        (((color >> 16) & 0xFF) * 3 / 4 + 64),
        255
    ) : color;
    
    vita2d_draw_rectangle(x, y, w, h, draw_color);
    vita2d_draw_rectangle(x, y, w, 2, RGBA8(255, 255, 255, 80));
    vita2d_draw_rectangle(x, y + h - 2, w, 2, RGBA8(0, 0, 0, 80));
    
    if (label) {
        int text_w = strlen(label) * 8;
        int tx = x + (w - text_w) / 2;
        int ty = y + h / 2 + 5;
        draw_text(tx, ty, COLOR_WHITE, label);
    }
    
    return clicked;
}

bool ui_icon_button(int x, int y, int size, const char *icon_char, unsigned int color, InputState *input) {
    return ui_button(x, y, size, size, icon_char, color, input);
}

// ====== TITLE SCREEN ======
void ui_render_title_screen(UIContext *ui, InputState *input) {
    // Sfondo
    unsigned int theme = get_theme_color(ui);
    unsigned int theme_dark = get_theme_dark(ui);
    
    vita2d_draw_rectangle(0, 0, 960, 544, theme);
    
    // Pattern decorativo (righe come Flipnote)
    for (int i = 0; i < 544; i += 8) {
        vita2d_draw_rectangle(0, i, 960, 2, theme_dark);
    }
    
    // Titolo
    draw_text_scaled(280, 120, COLOR_WHITE, 2.5f, "FLIPNOTE");
    draw_text_scaled(340, 170, COLOR_WHITE, 1.5f, "STUDIO");
    draw_text_scaled(380, 200, COLOR_WHITE, 1.0f, "for PS Vita");
    
    // Rana mascotte (ASCII art semplice)
    draw_text_scaled(430, 260, COLOR_WHITE, 1.5f, "^..^");
    draw_text_scaled(425, 280, COLOR_WHITE, 1.5f, "(o  o)");
    draw_text_scaled(420, 300, COLOR_WHITE, 1.5f, "|    |");
    
    // Menu
    int btn_x = 330;
    int btn_y = 340;
    int btn_w = 300;
    int btn_h = 40;
    int btn_gap = 50;
    
    if (ui_button(btn_x, btn_y, btn_w, btn_h, "Nuovo Flipnote", theme_dark, input)) {
        ui_goto_screen(ui, SCREEN_EDITOR);
    }
    
    if (ui_button(btn_x, btn_y + btn_gap, btn_w, btn_h, "Carica Flipnote", theme_dark, input)) {
        ui_goto_screen(ui, SCREEN_FILE_BROWSER);
    }
    
    if (ui_button(btn_x, btn_y + btn_gap * 2, btn_w, btn_h, "Impostazioni", theme_dark, input)) {
        ui_goto_screen(ui, SCREEN_SETTINGS);
    }
    
    if (ui_button(btn_x, btn_y + btn_gap * 3, btn_w, btn_h, "Aiuto", theme_dark, input)) {
        ui_goto_screen(ui, SCREEN_HELP);
    }
    
    // Versione
    draw_text(10, 534, RGBA8(255, 255, 255, 150), "v1.0 - Flipnote Vita");
}

// ====== EDITOR ======
void ui_render_editor(UIContext *ui, DrawingContext *draw, AnimationContext *anim,
                      AudioContext *audio, InputState *input) {
    unsigned int theme = get_theme_color(ui);
    unsigned int theme_dark = get_theme_dark(ui);
    
    // Sfondo
    vita2d_draw_rectangle(0, 0, 960, 544, RGBA8(220, 220, 220, 255));
    
    // Canvas
    drawing_render_canvas(draw);
    
    // Onion skin
    if (draw->onion_skin && !anim->is_playing) {
        LayerData *prev = NULL;
        LayerData *next = NULL;
        if (anim->current_frame > 0) {
            prev = animation_get_frame_layers(anim, anim->current_frame - 1);
        }
        if (anim->current_frame < anim->frame_count - 1) {
            next = animation_get_frame_layers(anim, anim->current_frame + 1);
        }
        drawing_render_onion_skin(draw, prev, next);
    }
    
    // Bordo canvas
    vita2d_draw_rectangle(CANVAS_X - 2, CANVAS_Y - 2, CANVAS_WIDTH + 4, 2, COLOR_UI_DARK);
    vita2d_draw_rectangle(CANVAS_X - 2, CANVAS_Y + CANVAS_HEIGHT, CANVAS_WIDTH + 4, 2, COLOR_UI_DARK);
    vita2d_draw_rectangle(CANVAS_X - 2, CANVAS_Y, 2, CANVAS_HEIGHT, COLOR_UI_DARK);
    vita2d_draw_rectangle(CANVAS_X + CANVAS_WIDTH, CANVAS_Y, 2, CANVAS_HEIGHT, COLOR_UI_DARK);
    
    // Toolbar sinistra
    ui_render_toolbar(ui, draw);
    
    // Timeline in basso
    ui_render_timeline(ui, anim, draw);
    
    // Status bar in alto
    ui_render_status_bar(ui, draw, anim);
    
    // Pulsanti a destra
    int right_x = CANVAS_X + CANVAS_WIDTH + 10;
    int right_y = 30;
    int btn_size = 40;
    int gap = 5;
    
    // Play/Stop
    if (ui_button(right_x, right_y, btn_size * 2 + gap, btn_size, 
                  anim->is_playing ? "Stop" : "Play", theme, input)) {
        if (anim->is_playing) {
            animation_stop(anim);
        } else {
            animation_save_current_to_draw(anim, draw);
            animation_play(anim);
        }
    }
    right_y += btn_size + gap;
    
    // Frame precedente
    if (ui_button(right_x, right_y, btn_size, btn_size, "<", theme_dark, input)) {
        animation_prev_frame(anim, draw);
    }
    // Frame successivo
    if (ui_button(right_x + btn_size + gap, right_y, btn_size, btn_size, ">", theme_dark, input)) {
        animation_next_frame(anim, draw);
    }
    right_y += btn_size + gap;
    
    // Aggiungi frame
    if (ui_button(right_x, right_y, btn_size * 2 + gap, btn_size, "+Frame", theme, input)) {
        animation_save_current_to_draw(anim, draw);
        int new_frame = animation_insert_frame(anim, anim->current_frame + 1);
        if (new_frame >= 0) {
            animation_goto_frame(anim, draw, new_frame);
            ui_show_toast(ui, "Frame aggiunto", 1.0f);
        }
    }
    right_y += btn_size + gap;
    
    // Duplica frame
    if (ui_button(right_x, right_y, btn_size * 2 + gap, btn_size, "Duplica", theme_dark, input)) {
        animation_save_current_to_draw(anim, draw);
        int dup = animation_duplicate_frame(anim, anim->current_frame);
        if (dup >= 0) {
            animation_goto_frame(anim, draw, dup);
            ui_show_toast(ui, "Frame duplicato", 1.0f);
        }
    }
    right_y += btn_size + gap;
    
    // Elimina frame
    if (ui_button(right_x, right_y, btn_size * 2 + gap, btn_size, "Elimina", RGBA8(200, 50, 50, 255), input)) {
        if (anim->frame_count > 1) {
            animation_delete_frame(anim, anim->current_frame);
            if (anim->current_frame >= anim->frame_count) {
                anim->current_frame = anim->frame_count - 1;
            }
            animation_load_current_from_draw(anim, draw);
            ui_show_toast(ui, "Frame eliminato", 1.0f);
        }
    }
    right_y += btn_size + gap + 10;
    
    // Onion Skin toggle
    if (ui_button(right_x, right_y, btn_size * 2 + gap, btn_size, 
                  draw->onion_skin ? "Onion:ON" : "Onion:OFF",
                  draw->onion_skin ? theme : COLOR_UI_GRAY, input)) {
        draw->onion_skin = !draw->onion_skin;
    }
    right_y += btn_size + gap;
    
    // Griglia toggle
    if (ui_button(right_x, right_y, btn_size * 2 + gap, btn_size,
                  draw->show_grid ? "Grid:ON" : "Grid:OFF",
                  draw->show_grid ? theme : COLOR_UI_GRAY, input)) {
        draw->show_grid = !draw->show_grid;
    }
    right_y += btn_size + gap;
    
    // Menu Layer
    if (ui_button(right_x, right_y, btn_size * 2 + gap, btn_size, "Layer", theme_dark, input)) {
        ui_goto_screen(ui, SCREEN_LAYER_MENU);
    }
    right_y += btn_size + gap;
    
    // Menu Velocità
    if (ui_button(right_x, right_y, btn_size * 2 + gap, btn_size, "Speed", theme_dark, input)) {
        ui_goto_screen(ui, SCREEN_SPEED_SETTINGS);
    }
    right_y += btn_size + gap;
    
    // Suoni
    if (ui_button(right_x, right_y, btn_size * 2 + gap, btn_size, "Suoni", theme_dark, input)) {
        ui_goto_screen(ui, SCREEN_SOUND_EDITOR);
    }
}

// ====== TOOLBAR ======
void ui_render_toolbar(UIContext *ui, DrawingContext *draw) {
    unsigned int theme = get_theme_color(ui);
    int x = 5;
    int y = CANVAS_Y;
    int btn_size = 35;
    int gap = 3;
    
    // Sfondo toolbar
    vita2d_draw_rectangle(0, CANVAS_Y - 5, CANVAS_X - 5, CANVAS_HEIGHT + 10, RGBA8(70, 70, 70, 230));
    
    // Strumenti
    for (int i = 0; i < TOOL_COUNT; i++) {
        unsigned int color = (draw->current_tool == i) ? theme : COLOR_UI_BUTTON;
        
        vita2d_draw_rectangle(x, y, btn_size, btn_size, color);
        
        if (draw->current_tool == i) {
            vita2d_draw_rectangle(x - 1, y - 1, btn_size + 2, btn_size + 2, COLOR_UI_SELECTED);
            vita2d_draw_rectangle(x, y, btn_size, btn_size, color);
        }
        
        draw_text(x + btn_size / 2 - 4, y + btn_size / 2 + 5, COLOR_WHITE, tool_icons[i]);
        
        y += btn_size + gap;
    }
    
    // Separatore
    y += 5;
    vita2d_draw_rectangle(x, y, btn_size, 2, COLOR_UI_LIGHT);
    y += 7;
    
    // Dimensione pennello
    char size_str[16];
    snprintf(size_str, sizeof(size_str), "%d", draw->brush_size);
    draw_text(x + 2, y + 12, COLOR_WHITE, "Size:");
    draw_text(x + 40, y + 12, COLOR_UI_SELECTED, size_str);
    y += 20;
    
    // Pulsanti size + e -
    // Diminuisci non richiede InputState qui, gestito in update
    vita2d_draw_rectangle(x, y, btn_size / 2, 20, COLOR_UI_BUTTON);
    draw_text(x + 5, y + 15, COLOR_WHITE, "-");
    vita2d_draw_rectangle(x + btn_size / 2 + 2, y, btn_size / 2, 20, COLOR_UI_BUTTON);
    draw_text(x + btn_size / 2 + 7, y + 15, COLOR_WHITE, "+");
    y += 25;
    
    // Preview pennello
    vita2d_draw_rectangle(x, y, btn_size, btn_size, COLOR_WHITE);
    int preview_r = draw->brush_size;
    if (preview_r > btn_size / 2 - 2) preview_r = btn_size / 2 - 2;
    vita2d_draw_fill_circle(x + btn_size / 2, y + btn_size / 2, preview_r, draw->draw_color);
    y += btn_size + gap + 5;
    
    // Colori
    unsigned int palette[] = { COLOR_BLACK, COLOR_RED, COLOR_BLUE, COLOR_WHITE };
    int color_size = 25;
    for (int i = 0; i < 4; i++) {
        int cx = x + (i % 2) * (color_size + 2);
        int cy = y + (i / 2) * (color_size + 2);
        
        vita2d_draw_rectangle(cx, cy, color_size, color_size, palette[i]);
        if (palette[i] == COLOR_WHITE) {
            vita2d_draw_rectangle(cx, cy, color_size, 1, COLOR_UI_DARK);
            vita2d_draw_rectangle(cx, cy + color_size - 1, color_size, 1, COLOR_UI_DARK);
            vita2d_draw_rectangle(cx, cy, 1, color_size, COLOR_UI_DARK);
            vita2d_draw_rectangle(cx + color_size - 1, cy, 1, color_size, COLOR_UI_DARK);
        }
        
        if (draw->draw_color == palette[i]) {
            vita2d_draw_rectangle(cx - 2, cy - 2, color_size + 4, 2, COLOR_UI_SELECTED);
            vita2d_draw_rectangle(cx - 2, cy + color_size, color_size + 4, 2, COLOR_UI_SELECTED);
            vita2d_draw_rectangle(cx - 2, cy, 2, color_size, COLOR_UI_SELECTED);
            vita2d_draw_rectangle(cx + color_size, cy, 2, color_size, COLOR_UI_SELECTED);
        }
    }
}

// ====== TIMELINE ======
void ui_render_timeline(UIContext *ui, AnimationContext *anim, DrawingContext *draw) {
    int tl_y = CANVAS_Y + CANVAS_HEIGHT + 5;
    int tl_h = 544 - tl_y - 5;
    int tl_x = CANVAS_X;
    int tl_w = CANVAS_WIDTH;
    
    // Sfondo timeline
    vita2d_draw_rectangle(tl_x, tl_y, tl_w, tl_h, COLOR_TIMELINE_BG);
    
    // Thumbnails dei frame
    int thumb_w = 40;
    int thumb_h = 30;
    int thumb_gap = 3;
    int visible = tl_w / (thumb_w + thumb_gap);
    
    int start = ui->timeline_scroll;
    if (start < 0) start = 0;
    if (start > anim->frame_count - visible) {
        start = anim->frame_count - visible;
        if (start < 0) start = 0;
    }
    
    for (int i = 0; i < visible && (start + i) < anim->frame_count; i++) {
        int frame_idx = start + i;
        int fx = tl_x + i * (thumb_w + thumb_gap) + 5;
        int fy = tl_y + 5;
        
        // Sfondo thumbnail
        unsigned int bg = (frame_idx == anim->current_frame) ? COLOR_FRAME_SEL : COLOR_FRAME_THUMB;
        vita2d_draw_rectangle(fx, fy, thumb_w, thumb_h, bg);
        
        // Mini-preview del contenuto
        LayerData *layers = animation_get_frame_layers(anim, frame_idx);
        if (layers) {
            float scale_x = (float)thumb_w / CANVAS_WIDTH;
            float scale_y = (float)thumb_h / CANVAS_HEIGHT;
            
            for (int py = 0; py < thumb_h; py++) {
                for (int px = 0; px < thumb_w; px++) {
                    int src_x = (int)(px / scale_x);
                    int src_y = (int)(py / scale_y);
                    if (src_x < CANVAS_WIDTH && src_y < CANVAS_HEIGHT) {
                        uint8_t pix = layers[0].pixels[src_y * CANVAS_WIDTH + src_x];
                        if (pix != 0) {
                            unsigned int c = drawing_get_rgba_color(pix, 0);
                            vita2d_draw_pixel(fx + px, fy + py, c);
                        }
                    }
                }
            }
        }
        
        // Bordo se selezionato
        if (frame_idx == anim->current_frame) {
            vita2d_draw_rectangle(fx - 1, fy - 1, thumb_w + 2, 2, COLOR_FRAME_SEL);
            vita2d_draw_rectangle(fx - 1, fy + thumb_h - 1, thumb_w + 2, 2, COLOR_FRAME_SEL);
        }
        
        // Numero frame
        char num[8];
        snprintf(num, sizeof(num), "%d", frame_idx + 1);
        draw_text(fx + 2, fy + thumb_h + 12, COLOR_UI_LIGHT, num);
    }
    
    // Scrollbar
    if (anim->frame_count > visible) {
        float scroll_ratio = (float)start / (anim->frame_count - visible);
        int scrollbar_w = tl_w * visible / anim->frame_count;
        if (scrollbar_w < 20) scrollbar_w = 20;
        int scrollbar_x = tl_x + (int)((tl_w - scrollbar_w) * scroll_ratio);
        
        vita2d_draw_rectangle(tl_x, tl_y + tl_h - 6, tl_w, 6, RGBA8(40, 40, 40, 255));
        vita2d_draw_rectangle(scrollbar_x, tl_y + tl_h - 6, scrollbar_w, 6, COLOR_UI_LIGHT);
    }
}

// ====== STATUS BAR ======
void ui_render_status_bar(UIContext *ui, DrawingContext *draw, AnimationContext *anim) {
    unsigned int theme = get_theme_color(ui);
    
    // Sfondo status bar
    vita2d_draw_rectangle(0, 0, 960, CANVAS_Y - 2, theme);
    
    // Tool name
    draw_text(10, 14, COLOR_WHITE, tool_names[draw->current_tool]);
    
    // Frame info
    char frame_info[64];
    snprintf(frame_info, sizeof(frame_info), "Frame: %d/%d", 
             anim->current_frame + 1, anim->frame_count);
    draw_text(200, 14, COLOR_WHITE, frame_info);
    
    // Layer info
    char layer_info[32];
    snprintf(layer_info, sizeof(layer_info), "Layer: %d", draw->active_layer + 1);
    draw_text(400, 14, COLOR_WHITE, layer_info);
    
    // Speed info
    char speed_info[32];
    snprintf(speed_info, sizeof(speed_info), "FPS: %.0f", anim->playback_speed);
    draw_text(520, 14, COLOR_WHITE, speed_info);
    
    // Menu buttons
    unsigned int theme_dark = get_theme_dark(ui);
    
    // Salva
    vita2d_draw_rectangle(700, 2, 50, 14, theme_dark);
    draw_text(705, 14, COLOR_WHITE, "Salva");
    
    // Menu
    vita2d_draw_rectangle(755, 2, 50, 14, theme_dark);
    draw_text(760, 14, COLOR_WHITE, "Menu");
    
    // Undo
    vita2d_draw_rectangle(810, 2, 40, 14, theme_dark);
    draw_text(815, 14, COLOR_WHITE, "Undo");
    
    // Redo
    vita2d_draw_rectangle(855, 2, 40, 14, theme_dark);
    draw_text(860, 14, COLOR_WHITE, "Redo");
    
    // Export
    vita2d_draw_rectangle(900, 2, 55, 14, theme_dark);
    draw_text(905, 14, COLOR_WHITE, "Export");
}

// ====== PLAYBACK ======
void ui_render_playback(UIContext *ui, DrawingContext *draw, AnimationContext *anim,
                        AudioContext *audio, InputState *input) {
    // Sfondo nero
    vita2d_draw_rectangle(0, 0, 960, 544, COLOR_BLACK);
    
    // Canvas centrato
    int play_x = (960 - CANVAS_WIDTH) / 2;
    int play_y = (544 - CANVAS_HEIGHT) / 2 - 20;
    
    // Sfondo canvas
    vita2d_draw_rectangle(play_x, play_y, CANVAS_WIDTH, CANVAS_HEIGHT, COLOR_WHITE);
    
    // Render frame
    for (int l = MAX_LAYERS - 1; l >= 0; l--) {
        if (!draw->layer_visible[l]) continue;
        for (int y = 0; y < CANVAS_HEIGHT; y++) {
            for (int x = 0; x < CANVAS_WIDTH; x++) {
                uint8_t pix = draw->layers[l].pixels[y * CANVAS_WIDTH + x];
                if (pix != 0) {
                    unsigned int color = drawing_get_rgba_color(pix, l);
                    if (color != COLOR_TRANSPARENT) {
                        vita2d_draw_pixel(play_x + x, play_y + y, color);
                    }
                }
            }
        }
    }
    
    // Controlli playback
    int ctrl_y = play_y + CANVAS_HEIGHT + 15;
    int ctrl_x = (960 - 300) / 2;
    
    // Play/Pause
    if (ui_button(ctrl_x + 100, ctrl_y, 60, 35, 
                  anim->is_playing ? "||" : ">", 
                  get_theme_color(ui), input)) {
        animation_toggle_play(anim);
    }
    
    // Stop/Indietro
    if (ui_button(ctrl_x + 30, ctrl_y, 60, 35, "<<", get_theme_dark(ui), input)) {
        animation_stop(anim);
        animation_first_frame(anim, draw);
    }
    
    // Avanti
    if (ui_button(ctrl_x + 170, ctrl_y, 60, 35, ">>", get_theme_dark(ui), input)) {
        animation_stop(anim);
        animation_last_frame(anim, draw);
    }
    
    // Torna all'editor
    if (ui_button(ctrl_x + 240, ctrl_y, 60, 35, "Edit", RGBA8(200, 50, 50, 255), input)) {
        animation_stop(anim);
        ui_goto_screen(ui, SCREEN_EDITOR);
    }
    
    // Frame counter
    char fc[32];
    snprintf(fc, sizeof(fc), "%d / %d", anim->current_frame + 1, anim->frame_count);
    draw_text(ctrl_x + 110, ctrl_y + 55, COLOR_WHITE, fc);
    
    // Loop indicator
    draw_text(ctrl_x, ctrl_y + 55, anim->loop ? COLOR_GREEN : COLOR_RED, 
              anim->loop ? "Loop: ON" : "Loop: OFF");
}

// ====== COLOR PICKER ======
void ui_render_color_picker(UIContext *ui, DrawingContext *draw, InputState *input) {
    unsigned int theme = get_theme_color(ui);
    
    // Overlay scuro
    vita2d_draw_rectangle(0, 0, 960, 544, RGBA8(0, 0, 0, 180));
    
    // Finestra color picker
    int win_x = 200, win_y = 100, win_w = 560, win_h = 344;
    vita2d_draw_rectangle(win_x, win_y, win_w, win_h, RGBA8(50, 50, 50, 255));
    vita2d_draw_rectangle(win_x, win_y, win_w, 30, theme);
    draw_text(win_x + 10, win_y + 22, COLOR_WHITE, "Seleziona Colore");
    
    // Palette colori
    unsigned int colors[] = {
        COLOR_BLACK, COLOR_RED, COLOR_BLUE, COLOR_WHITE,
        RGBA8(64, 64, 64, 255), RGBA8(200, 0, 0, 255), RGBA8(0, 0, 200, 255), RGBA8(200, 200, 200, 255),
        RGBA8(128, 128, 128, 255), RGBA8(255, 100, 100, 255), RGBA8(100, 100, 255, 255), RGBA8(255, 255, 0, 255),
        COLOR_GREEN, COLOR_ORANGE, COLOR_PURPLE, COLOR_CYAN,
    };
    
    int cols = 4;
    int color_size = 60;
    int color_gap = 10;
    int start_x = win_x + 50;
    int start_y = win_y + 50;
    
    for (int i = 0; i < 16; i++) {
        int cx = start_x + (i % cols) * (color_size + color_gap);
        int cy = start_y + (i / cols) * (color_size + color_gap);
        
        vita2d_draw_rectangle(cx, cy, color_size, color_size, colors[i]);
        
        // Bordo
        if (colors[i] == COLOR_WHITE) {
            vita2d_draw_rectangle(cx, cy, color_size, 1, COLOR_UI_DARK);
            vita2d_draw_rectangle(cx, cy + color_size - 1, color_size, 1, COLOR_UI_DARK);
        }
        
        if (draw->draw_color == colors[i]) {
            vita2d_draw_rectangle(cx - 3, cy - 3, color_size + 6, 3, COLOR_UI_SELECTED);
            vita2d_draw_rectangle(cx - 3, cy + color_size, color_size + 6, 3, COLOR_UI_SELECTED);
            vita2d_draw_rectangle(cx - 3, cy, 3, color_size, COLOR_UI_SELECTED);
            vita2d_draw_rectangle(cx + color_size, cy, 3, color_size, COLOR_UI_SELECTED);
        }
        
        // Click detection
        if (input->touch_just_pressed && point_in_rect(input->touch_x, input->touch_y, cx, cy, color_size, color_size)) {
            draw->draw_color = colors[i];
            if (colors[i] == COLOR_BLACK) draw->current_color = 1;
            else if (colors[i] == COLOR_RED) draw->current_color = 2;
            else if (colors[i] == COLOR_BLUE) draw->current_color = 3;
            else draw->current_color = 0;
        }
    }
    
    // Colore corrente
    draw_text(start_x + 300, start_y + 10, COLOR_WHITE, "Corrente:");
    vita2d_draw_rectangle(start_x + 300, start_y + 20, 100, 60, draw->draw_color);
    
    // Chiudi
    if (ui_button(win_x + win_w - 100, win_y + win_h - 50, 80, 35, "Chiudi", theme, input)) {
        ui_go_back(ui);
    }
}

// ====== LAYER MENU ======
void ui_render_layer_menu(UIContext *ui, DrawingContext *draw, InputState *input) {
    unsigned int theme = get_theme_color(ui);
    
    vita2d_draw_rectangle(0, 0, 960, 544, RGBA8(0, 0, 0, 180));
    
    int win_x = 250, win_y = 100, win_w = 460, win_h = 344;
    vita2d_draw_rectangle(win_x, win_y, win_w, win_h, RGBA8(50, 50, 50, 255));
    vita2d_draw_rectangle(win_x, win_y, win_w, 30, theme);
    draw_text(win_x + 10, win_y + 22, COLOR_WHITE, "Gestione Layer");
    
    int ly = win_y + 50;
    
    for (int i = 0; i < MAX_LAYERS; i++) {
        int lx = win_x + 20;
        unsigned int bg = (i == draw->active_layer) ? RGBA8(80, 120, 80, 255) : RGBA8(70, 70, 70, 255);
        
        vita2d_draw_rectangle(lx, ly, win_w - 40, 50, bg);
        
        char label[32];
        snprintf(label, sizeof(label), "Layer %d", i + 1);
        draw_text(lx + 10, ly + 30, COLOR_WHITE, label);
        
        // Visibilità
        if (ui_button(lx + 200, ly + 10, 60, 30,
                      draw->layer_visible[i] ? "Vis" : "Hid",
                      draw->layer_visible[i] ? theme : COLOR_UI_GRAY, input)) {
            drawing_toggle_layer_visibility(draw, i);
        }
        
        // Seleziona
        if (ui_button(lx + 270, ly + 10, 60, 30, "Sel", 
                      (i == draw->active_layer) ? theme : COLOR_UI_BUTTON, input)) {
            drawing_set_layer(draw, i);
        }
        
        // Cancella
        if (ui_button(lx + 340, ly + 10, 60, 30, "Clear", RGBA8(200, 50, 50, 255), input)) {
            drawing_save_undo(draw);
            drawing_clear_layer(draw, i);
            ui_show_toast(ui, "Layer cancellato", 1.0f);
        }
        
        ly += 55;
    }
    
    ly += 10;
    
    // Merge layers
    if (ui_button(win_x + 20, ly, 200, 35, "Unisci Layer", theme, input)) {
        drawing_save_undo(draw);
        drawing_merge_layers(draw);
        ui_show_toast(ui, "Layer uniti", 1.0f);
    }
    
    // Scambia layer
    if (ui_button(win_x + 230, ly, 200, 35, "Scambia 1<>2", theme, input)) {
        drawing_save_undo(draw);
        drawing_swap_layers(draw, 0, 1);
        ui_show_toast(ui, "Layer scambiati", 1.0f);
    }
    
    ly += 45;
    
    // Canvas operations
    if (ui_button(win_x + 20, ly, 130, 35, "Flip H", COLOR_UI_BUTTON, input)) {
        drawing_save_undo(draw);
        drawing_flip_horizontal(draw);
    }
    if (ui_button(win_x + 160, ly, 130, 35, "Flip V", COLOR_UI_BUTTON, input)) {
        drawing_save_undo(draw);
        drawing_flip_vertical(draw);
    }
    if (ui_button(win_x + 300, ly, 130, 35, "Inverti", COLOR_UI_BUTTON, input)) {
        drawing_save_undo(draw);
        drawing_invert_colors(draw);
    }
    
    // Chiudi
    if (ui_button(win_x + win_w - 100, win_y + win_h - 50, 80, 35, "Chiudi", RGBA8(200, 50, 50, 255), input)) {
        ui_go_back(ui);
    }
}

// ====== SPEED SETTINGS ======
void ui_render_speed_settings(UIContext *ui, AnimationContext *anim, InputState *input) {
    unsigned int theme = get_theme_color(ui);
    
    vita2d_draw_rectangle(0, 0, 960, 544, RGBA8(0, 0, 0, 180));
    
    int win_x = 250, win_y = 150, win_w = 460, win_h = 244;
    vita2d_draw_rectangle(win_x, win_y, win_w, win_h, RGBA8(50, 50, 50, 255));
    vita2d_draw_rectangle(win_x, win_y, win_w, 30, theme);
    draw_text(win_x + 10, win_y + 22, COLOR_WHITE, "Velocita' Animazione");
    
    int sy = win_y + 60;
    
    char speed_text[32];
    snprintf(speed_text, sizeof(speed_text), "FPS: %.0f", anim->playback_speed);
    draw_text(win_x + 180, sy, COLOR_WHITE, speed_text);
    sy += 30;
    
    // Slider
    int slider_x = win_x + 30;
    int slider_w = win_w - 60;
    int slider_y = sy;
    
    vita2d_draw_rectangle(slider_x, slider_y, slider_w, 10, COLOR_UI_DARK);
    
    float ratio = (anim->playback_speed - MIN_SPEED) / (MAX_SPEED - MIN_SPEED);
    int knob_x = slider_x + (int)(ratio * (slider_w - 20));
    vita2d_draw_rectangle(knob_x, slider_y - 5, 20, 20, theme);
    
    // Touch per slider
    if (input->touch_active && point_in_rect(input->touch_x, input->touch_y, slider_x, slider_y - 10, slider_w, 30)) {
        float new_ratio = (float)(input->touch_x - slider_x) / slider_w;
        if (new_ratio < 0) new_ratio = 0;
        if (new_ratio > 1) new_ratio = 1;
        anim->playback_speed = MIN_SPEED + new_ratio * (MAX_SPEED - MIN_SPEED);
    }
    
    sy += 40;
    
    // Preset velocità
    float presets[] = {2, 4, 6, 8, 12, 16, 20, 24};
    const char *preset_labels[] = {"2", "4", "6", "8", "12", "16", "20", "24"};
    
    for (int i = 0; i < 8; i++) {
        int bx = win_x + 20 + i * 53;
        unsigned int col = (anim->playback_speed == presets[i]) ? theme : COLOR_UI_BUTTON;
        if (ui_button(bx, sy, 48, 30, preset_labels[i], col, input)) {
            anim->playback_speed = presets[i];
        }
    }
    
    sy += 50;
    
    // Loop toggle
    if (ui_button(win_x + 20, sy, 200, 35, 
                  anim->loop ? "Loop: ON" : "Loop: OFF",
                  anim->loop ? theme : COLOR_UI_GRAY, input)) {
        anim->loop = !anim->loop;
    }
    
    // Chiudi
    if (ui_button(win_x + win_w - 100, win_y + win_h - 50, 80, 35, "Chiudi", RGBA8(200, 50, 50, 255), input)) {
        ui_go_back(ui);
    }
}

// ====== SOUND EDITOR ======
void ui_render_sound_editor(UIContext *ui, AudioContext *audio, AnimationContext *anim, InputState *input) {
    unsigned int theme = get_theme_color(ui);
    
    vita2d_draw_rectangle(0, 0, 960, 544, RGBA8(0, 0, 0, 180));
    
    int win_x = 150, win_y = 50, win_w = 660, win_h = 444;
    vita2d_draw_rectangle(win_x, win_y, win_w, win_h, RGBA8(50, 50, 50, 255));
    vita2d_draw_rectangle(win_x, win_y, win_w, 30, theme);
    draw_text(win_x + 10, win_y + 22, COLOR_WHITE, "Editor Suoni");
    
    int sy = win_y + 50;
    
    const char *se_names[] = {"Click", "Beep", "Drum", "Voce/Custom"};
    
    for (int i = 0; i < MAX_SOUND_EFFECTS; i++) {
        int sx = win_x + 20;
        
        vita2d_draw_rectangle(sx, sy, win_w - 40, 45, RGBA8(70, 70, 70, 255));
        draw_text(sx + 10, sy + 28, COLOR_WHITE, se_names[i]);
        
        // Preview suono
        if (ui_button(sx + 250, sy + 8, 60, 30, "Play", theme, input)) {
            audio_play_se(audio, i);
        }
        
        // On/Off per frame corrente
        bool trigger = audio_get_se_trigger(audio, anim->current_frame, i);
        if (ui_button(sx + 320, sy + 8, 80, 30, 
                      trigger ? "Frame:ON" : "Frame:OFF",
                      trigger ? theme : COLOR_UI_GRAY, input)) {
            audio_set_se_trigger(audio, anim->current_frame, i, !trigger);
        }
        
        // Enable/Disable
        if (ui_button(sx + 410, sy + 8, 60, 30,
                      audio->se_enabled[i] ? "ON" : "OFF",
                      audio->se_enabled[i] ? theme : COLOR_UI_GRAY, input)) {
            audio->se_enabled[i] = !audio->se_enabled[i];
        }
        
        // Volume info
        if (audio->sound_effects[i].sample_count > 0) {
            char info[32];
            snprintf(info, sizeof(info), "%.1fs", 
                     (float)audio->sound_effects[i].sample_count / AUDIO_SAMPLE_RATE);
            draw_text(sx + 490, sy + 28, COLOR_UI_LIGHT, info);
        }
        
        sy += 50;
    }
    
    sy += 10;
    
    // Registra
    if (ui_button(win_x + 20, sy, 200, 40,
                  audio->is_recording ? "Stop Reg." : "Registra Voce",
                  audio->is_recording ? RGBA8(200, 50, 50, 255) : theme, input)) {
        if (audio->is_recording) {
            audio_stop_recording(audio);
            ui_show_toast(ui, "Registrazione salvata in Voce", 1.5f);
        } else {
            audio_start_recording(audio);
            ui_show_toast(ui, "Registrazione...", 1.0f);
        }
    }
    
    // Metronomo
    if (ui_button(win_x + 240, sy, 200, 40,
                  audio->metronome_enabled ? "Metronomo: ON" : "Metronomo: OFF",
                  audio->metronome_enabled ? theme : COLOR_UI_GRAY, input)) {
        audio->metronome_enabled = !audio->metronome_enabled;
    }
    
    sy += 55;
    
    // Volume SE
    draw_text(win_x + 20, sy + 18, COLOR_WHITE, "Volume SE:");
    int vol_slider_x = win_x + 150;
    int vol_slider_w = 400;
    vita2d_draw_rectangle(vol_slider_x, sy + 5, vol_slider_w, 15, COLOR_UI_DARK);
    int vol_knob_x = vol_slider_x + (int)(audio->se_volume * (vol_slider_w - 15));
    vita2d_draw_rectangle(vol_knob_x, sy, 15, 25, theme);
    
    if (input->touch_active && point_in_rect(input->touch_x, input->touch_y, vol_slider_x, sy - 5, vol_slider_w, 35)) {
        float vol = (float)(input->touch_x - vol_slider_x) / vol_slider_w;
        if (vol < 0) vol = 0;
        if (vol > 1) vol = 1;
        audio->se_volume = vol;
    }
    
    // Chiudi
    if (ui_button(win_x + win_w - 100, win_y + win_h - 50, 80, 35, "Chiudi", RGBA8(200, 50, 50, 255), input)) {
        ui_go_back(ui);
    }
}

// ====== FILE BROWSER ======
void ui_render_file_browser(UIContext *ui, InputState *input) {
    unsigned int theme = get_theme_color(ui);
    
    vita2d_draw_rectangle(0, 0, 960, 544, RGBA8(40, 40, 40, 255));
    vita2d_draw_rectangle(0, 0, 960, 30, theme);
    draw_text(10, 22, COLOR_WHITE, "Flipnote salvati");
    
    // Lista file
    SaveSlotInfo slots[MAX_SAVE_SLOTS];
    int count = filemanager_list_saves(slots, MAX_SAVE_SLOTS);
    ui->file_browser_count = count;
    
    if (count == 0) {
        draw_text(350, 272, COLOR_UI_LIGHT, "Nessun flipnote salvato");
    } else {
        int list_y = 40;
        int item_h = 60;
        int start = ui->file_browser_scroll;
        int visible = (544 - 80) / item_h;
        
        for (int i = 0; i < visible && (start + i) < count; i++) {
            int idx = start + i;
            int iy = list_y + i * item_h;
            
            unsigned int bg = (idx == ui->file_browser_selection) ? 
                RGBA8(80, 80, 80, 255) : RGBA8(60, 60, 60, 255);
            
            vita2d_draw_rectangle(10, iy, 940, item_h - 5, bg);
            
            draw_text(20, iy + 25, COLOR_WHITE, slots[idx].title);
            
            char info[64];
            snprintf(info, sizeof(info), "di %s - %d frame", slots[idx].author, slots[idx].frame_count);
            draw_text(20, iy + 45, COLOR_UI_LIGHT, info);
            
            // Seleziona con touch
            if (input->touch_just_pressed && point_in_rect(input->touch_x, input->touch_y, 10, iy, 940, item_h - 5)) {
                ui->file_browser_selection = idx;
            }
        }
    }
    
    // Pulsanti in basso
    int btn_y = 500;
    if (ui_button(10, btn_y, 120, 35, "Carica", theme, input)) {
        if (ui->file_browser_selection >= 0 && ui->file_browser_selection < count) {
            // Il caricamento verrà gestito in update
        }
    }
    
    if (ui_button(140, btn_y, 120, 35, "Elimina", RGBA8(200, 50, 50, 255), input)) {
        if (ui->file_browser_selection >= 0 && ui->file_browser_selection < count) {
            filemanager_delete(slots[ui->file_browser_selection].filename);
        }
    }
    
    if (ui_button(830, btn_y, 120, 35, "Indietro", COLOR_UI_BUTTON, input)) {
        ui_go_back(ui);
    }
}

// ====== SETTINGS ======
void ui_render_settings(UIContext *ui, InputState *input) {
    unsigned int theme = get_theme_color(ui);
    
    vita2d_draw_rectangle(0, 0, 960, 544, RGBA8(40, 40, 40, 255));
    vita2d_draw_rectangle(0, 0, 960, 30, theme);
    draw_text(10, 22, COLOR_WHITE, "Impostazioni");
    
    int sy = 50;
    
    // Tema
    draw_text(30, sy + 18, COLOR_WHITE, "Tema:");
    
    unsigned int theme_colors[] = { RGBA8(0, 180, 0, 255), RGBA8(200, 0, 0, 255), RGBA8(0, 0, 200, 255) };
    const char *theme_names[] = { "Verde", "Rosso", "Blu" };
    
    for (int i = 0; i < 3; i++) {
        unsigned int col = (ui->theme == i) ? theme_colors[i] : COLOR_UI_BUTTON;
        if (ui_button(150 + i * 120, sy, 110, 30, theme_names[i], col, input)) {
            ui->theme = i;
        }
    }
    sy += 50;
    
    // Mostra contatore frame
    if (ui_button(30, sy, 300, 35,
                  ui->show_frame_counter ? "Contatore Frame: ON" : "Contatore Frame: OFF",
                  ui->show_frame_counter ? theme : COLOR_UI_GRAY, input)) {
        ui->show_frame_counter = !ui->show_frame_counter;
    }
    sy += 50;
    
    // Info
    sy += 30;
    draw_text(30, sy, COLOR_UI_LIGHT, "Flipnote Studio per PS Vita");
    sy += 25;
    draw_text(30, sy, COLOR_UI_LIGHT, "Versione 1.0");
    sy += 25;
    draw_text(30, sy, COLOR_UI_LIGHT, "Ispirato a Flipnote Studio di Nintendo");
    sy += 25;
    draw_text(30, sy, COLOR_UI_LIGHT, "Comandi:");
    sy += 25;
    draw_text(30, sy, COLOR_UI_LIGHT, "  Touch: Disegna / Interagisci UI");
    sy += 25;
    draw_text(30, sy, COLOR_UI_LIGHT, "  L/R: Frame prec./succ.");
    sy += 25;
    draw_text(30, sy, COLOR_UI_LIGHT, "  Triangolo: Play/Stop");
    sy += 25;
    draw_text(30, sy, COLOR_UI_LIGHT, "  Quadrato: Undo");
    sy += 25;
    draw_text(30, sy, COLOR_UI_LIGHT, "  Cerchio: Redo");
    sy += 25;
    draw_text(30, sy, COLOR_UI_LIGHT, "  Croce: Cambia strumento");
    sy += 25;
    draw_text(30, sy, COLOR_UI_LIGHT, "  Start: Menu/Salva");
    sy += 25;
    draw_text(30, sy, COLOR_UI_LIGHT, "  Select: Impostazioni");
    
    // Indietro
    if (ui_button(830, 500, 120, 35, "Indietro", theme, input)) {
        ui_go_back(ui);
    }
}

// ====== EXPORT MENU ======
void ui_render_export_menu(UIContext *ui, AnimationContext *anim, AudioContext *audio, InputState *input) {
    unsigned int theme = get_theme_color(ui);
    
    vita2d_draw_rectangle(0, 0, 960, 544, RGBA8(0, 0, 0, 180));
    
    int win_x = 250, win_y = 100, win_w = 460, win_h = 300;
    vita2d_draw_rectangle(win_x, win_y, win_w, win_h, RGBA8(50, 50, 50, 255));
    vita2d_draw_rectangle(win_x, win_y, win_w, 30, theme);
    draw_text(win_x + 10, win_y + 22, COLOR_WHITE, "Esporta");
    
    int sy = win_y + 50;
    
    // Salva come FNV
    if (ui_button(win_x + 20, sy, win_w - 40, 40, "Salva Flipnote (.fnv)", theme, input)) {
        char filename[256];
        filemanager_generate_filename(filename, sizeof(filename));
        if (filemanager_save(anim, audio, filename)) {
            ui_show_toast(ui, "Salvato con successo!", 2.0f);
        } else {
            ui_show_toast(ui, "Errore nel salvataggio!", 2.0f);
        }
    }
    sy += 50;
    
    // Esporta GIF
    if (ui_button(win_x + 20, sy, win_w - 40, 40, "Esporta GIF Animata", theme, input)) {
        char filename[256];
        snprintf(filename, sizeof(filename), "%sexport.gif", SAVE_DIR);
        if (filemanager_export_gif(anim, filename)) {
            ui_show_toast(ui, "GIF esportata!", 2.0f);
        } else {
            ui_show_toast(ui, "Errore nell'esportazione!", 2.0f);
        }
    }
    sy += 50;
    
    // Esporta sequenza BMP
    if (ui_button(win_x + 20, sy, win_w - 40, 40, "Esporta Sequenza Immagini", theme, input)) {
        char dirname[256];
        snprintf(dirname, sizeof(dirname), "%sframes/", SAVE_DIR);
        if (filemanager_export_png_sequence(anim, dirname)) {
            ui_show_toast(ui, "Immagini esportate!", 2.0f);
        } else {
            ui_show_toast(ui, "Errore nell'esportazione!", 2.0f);
        }
    }
    sy += 50;
    
    // Esporta frame corrente
    if (ui_button(win_x + 20, sy, win_w - 40, 40, "Esporta Frame Corrente", theme, input)) {
        char filename[256];
        snprintf(filename, sizeof(filename), "%sframe_%d.bmp", SAVE_DIR, anim->current_frame);
        if (filemanager_export_frame_png(anim, anim->current_frame, filename)) {
            ui_show_toast(ui, "Frame esportato!", 2.0f);
        } else {
            ui_show_toast(ui, "Errore!", 2.0f);
        }
    }
    
    // Chiudi
    if (ui_button(win_x + win_w - 100, win_y + win_h - 50, 80, 35, "Chiudi", RGBA8(200, 50, 50, 255), input)) {
        ui_go_back(ui);
    }
}

// ====== HELP ======
void ui_render_help(UIContext *ui, InputState *input) {
    unsigned int theme = get_theme_color(ui);
    
    vita2d_draw_rectangle(0, 0, 960, 544, RGBA8(40, 40, 40, 255));
    vita2d_draw_rectangle(0, 0, 960, 30, theme);
    draw_text(10, 22, COLOR_WHITE, "Guida - Flipnote Vita");
    
    int sy = 50;
    int lh = 22;
    
    draw_text(30, sy, COLOR_UI_SELECTED, "Come usare Flipnote Vita:"); sy += lh + 5;
    
    draw_text(30, sy, COLOR_WHITE, "1. Disegna sul canvas usando il touch screen"); sy += lh;
    draw_text(30, sy, COLOR_WHITE, "2. Scegli lo strumento dalla barra a sinistra"); sy += lh;
    draw_text(30, sy, COLOR_WHITE, "3. Aggiungi frame per creare l'animazione"); sy += lh;
    draw_text(30, sy, COLOR_WHITE, "4. Premi Play per vedere l'animazione"); sy += lh;
    draw_text(30, sy, COLOR_WHITE, "5. Usa l'Onion Skin per vedere i frame vicini"); sy += lh + 10;
    
    draw_text(30, sy, COLOR_UI_SELECTED, "Strumenti:"); sy += lh + 5;
    draw_text(30, sy, COLOR_WHITE, "P - Penna: disegno libero"); sy += lh;
    draw_text(30, sy, COLOR_WHITE, "E - Gomma: cancella"); sy += lh;
    draw_text(30, sy, COLOR_WHITE, "B - Secchiello: riempimento area"); sy += lh;
    draw_text(30, sy, COLOR_WHITE, "L - Linea: linea dritta"); sy += lh;
    draw_text(30, sy, COLOR_WHITE, "R - Rettangolo"); sy += lh;
    draw_text(30, sy, COLOR_WHITE, "C - Cerchio"); sy += lh;
    draw_text(30, sy, COLOR_WHITE, "M - Sposta: muovi il contenuto"); sy += lh;
    draw_text(30, sy, COLOR_WHITE, "S - Seleziona: seleziona un'area"); sy += lh;
    draw_text(30, sy, COLOR_WHITE, "T - Timbro: incolla selezione copiata"); sy += lh;
    draw_text(30, sy, COLOR_WHITE, "D - Contagocce: preleva colore"); sy += lh + 10;
    
    draw_text(30, sy, COLOR_UI_SELECTED, "Max 999 frame per animazione"); sy += lh;
    draw_text(30, sy, COLOR_UI_SELECTED, "3 layer per frame"); sy += lh;
    draw_text(30, sy, COLOR_UI_SELECTED, "4 effetti sonori assegnabili"); sy += lh;
    
    if (ui_button(830, 500, 120, 35, "Indietro", theme, input)) {
        ui_go_back(ui);
    }
}

// ====== TOAST ======
void ui_render_toast(UIContext *ui, float delta_time) {
    if (ui->toast_timer <= 0) return;
    
    ui->toast_timer -= delta_time;
    
    float alpha = 1.0f;
    if (ui->toast_timer < 0.5f) alpha = ui->toast_timer / 0.5f;
    
    int a = (int)(alpha * 220);
    int text_w = strlen(ui->toast_message) * 10;
    int x = (960 - text_w - 40) / 2;
    int y = 480;
    
    vita2d_draw_rectangle(x, y, text_w + 40, 35, RGBA8(0, 0, 0, a));
    
    unsigned int text_color = RGBA8(255, 255, 255, (int)(alpha * 255));
    draw_text(x + 20, y + 24, text_color, ui->toast_message);
}

// ====== DIALOG ======
void ui_render_dialog(UIContext *ui, InputState *input) {
    if (!ui->dialog_active) return;
    
    unsigned int theme = get_theme_color(ui);
    
    vita2d_draw_rectangle(0, 0, 960, 544, RGBA8(0, 0, 0, 180));
    
    int win_x = 280, win_y = 200, win_w = 400, win_h = 150;
    vita2d_draw_rectangle(win_x, win_y, win_w, win_h, RGBA8(60, 60, 60, 255));
    vita2d_draw_rectangle(win_x, win_y, win_w, 30, theme);
    draw_text(win_x + 10, win_y + 22, COLOR_WHITE, "Conferma");
    
    draw_text(win_x + 20, win_y + 70, COLOR_WHITE, ui->dialog_message);
    
    if (ui_button(win_x + 50, win_y + win_h - 50, 120, 35, "Si", theme, input)) {
        ui->dialog_active = false;
        ui->dialog_result = 1;
        if (ui->dialog_callback) ui->dialog_callback(1);
    }
    
    if (ui_button(win_x + 230, win_y + win_h - 50, 120, 35, "No", COLOR_UI_BUTTON, input)) {
        ui->dialog_active = false;
        ui->dialog_result = 2;
        if (ui->dialog_callback) ui->dialog_callback(2);
    }
}

void ui_show_toast(UIContext *ui, const char *message, float duration) {
    strncpy(ui->toast_message, message, sizeof(ui->toast_message) - 1);
    ui->toast_timer = duration;
}

void ui_show_dialog(UIContext *ui, const char *message, void (*callback)(int)) {
    ui->dialog_active = true;
    strncpy(ui->dialog_message, message, sizeof(ui->dialog_message) - 1);
    ui->dialog_callback = callback;
    ui->dialog_result = 0;
}

void ui_goto_screen(UIContext *ui, ScreenState screen) {
    ui->prev_screen = ui->current_screen;
    ui->current_screen = screen;
}

void ui_go_back(UIContext *ui) {
    ScreenState temp = ui->current_screen;
    ui->current_screen = ui->prev_screen;
    ui->prev_screen = temp;
}

// ====== MAIN UPDATE ======
void ui_update(UIContext *ui, DrawingContext *draw, AnimationContext *anim,
               AudioContext *audio, InputState *input, float delta_time) {
    
    // Gestione input globale per bottoni hardware
    if (ui->current_screen == SCREEN_EDITOR) {
        // L/R per navigare frame
        if (input_button_pressed(input, SCE_CTRL_LTRIGGER)) {
            animation_prev_frame(anim, draw);
        }
        if (input_button_pressed(input, SCE_CTRL_RTRIGGER)) {
            animation_next_frame(anim, draw);
        }
        
        // Triangolo per play/stop
        if (input_button_pressed(input, SCE_CTRL_TRIANGLE)) {
            if (anim->is_playing) {
                animation_stop(anim);
            } else {
                animation_save_current_to_draw(anim, draw);
                ui_goto_screen(ui, SCREEN_PLAYBACK);
                animation_play(anim);
            }
        }
        
        // Quadrato per undo
        if (input_button_pressed(input, SCE_CTRL_SQUARE)) {
            drawing_undo(draw);
            ui_show_toast(ui, "Undo", 0.5f);
        }
        
        // Cerchio per redo
        if (input_button_pressed(input, SCE_CTRL_CIRCLE)) {
            drawing_redo(draw);
            ui_show_toast(ui, "Redo", 0.5f);
        }
        
        // Croce per cambiare strumento
        if (input_button_pressed(input, SCE_CTRL_CROSS)) {
            draw->current_tool = (draw->current_tool + 1) % TOOL_COUNT;
            ui_show_toast(ui, tool_names[draw->current_tool], 0.8f);
        }
        
        // D-Pad per dimensione pennello
        if (input_button_pressed(input, SCE_CTRL_UP)) {
            draw->brush_size++;
            if (draw->brush_size > 16) draw->brush_size = 16;
        }
        if (input_button_pressed(input, SCE_CTRL_DOWN)) {
            draw->brush_size--;
            if (draw->brush_size < 1) draw->brush_size = 1;
        }
        
        // D-Pad sinistra/destra per colore
        if (input_button_pressed(input, SCE_CTRL_LEFT)) {
            draw->current_color = (draw->current_color + LAYER_COLORS_COUNT - 1) % LAYER_COLORS_COUNT;
            draw->draw_color = drawing_get_rgba_color(draw->current_color, draw->active_layer);
        }
        if (input_button_pressed(input, SCE_CTRL_RIGHT)) {
            draw->current_color = (draw->current_color + 1) % LAYER_COLORS_COUNT;
            draw->draw_color = drawing_get_rgba_color(draw->current_color, draw->active_layer);
        }
        
        // Start per menu salvataggio/export
        if (input_button_pressed(input, SCE_CTRL_START)) {
            animation_save_current_to_draw(anim, draw);
            ui_goto_screen(ui, SCREEN_EXPORT);
        }
        
        // Select per impostazioni
        if (input_button_pressed(input, SCE_CTRL_SELECT)) {
            ui_goto_screen(ui, SCREEN_SETTINGS);
        }
        
        // Touch - disegno sul canvas
        if (!anim->is_playing) {
            int cx, cy;
            if (input_touch_to_canvas(input, &cx, &cy)) {
                if (input->touch_just_pressed) {
                    drawing_save_undo(draw);
                    draw->is_drawing = false;
                    
                    if (draw->current_tool == TOOL_LINE || draw->current_tool == TOOL_RECT || draw->current_tool == TOOL_CIRCLE) {
                        draw->start_x = cx;
                        draw->start_y = cy;
                    }
                }
                
                switch (draw->current_tool) {
                    case TOOL_PEN:
                        drawing_pen_stroke(draw, cx, cy);
                        break;
                    case TOOL_ERASER:
                        drawing_eraser_stroke(draw, cx, cy);
                        break;
                    case TOOL_BUCKET:
                        if (input->touch_just_pressed) {
                            drawing_bucket_fill(draw, cx, cy);
                        }
                        break;
                    case TOOL_EYEDROPPER:
                        if (input->touch_just_pressed) {
                            drawing_eyedropper(draw, cx, cy);
                            ui_show_toast(ui, "Colore prelevato", 0.5f);
                        }
                        break;
                    case TOOL_LINE:
                    case TOOL_RECT:
                    case TOOL_CIRCLE:
                        // Preview gestita separatamente
                        break;
                    default:
                        drawing_pen_stroke(draw, cx, cy);
                        break;
                }
            }
            
            // Fine stroke
            if (input->touch_just_released) {
                int cx_end, cy_end;
                cx_end = input->touch_prev_x - CANVAS_X;
                cy_end = input->touch_prev_y - CANVAS_Y;
                
                // Applica strumenti shape
                if (draw->current_tool == TOOL_LINE) {
                    drawing_line(draw, draw->start_x, draw->start_y, cx_end, cy_end);
                } else if (draw->current_tool == TOOL_RECT) {
                    drawing_rect(draw, draw->start_x, draw->start_y, cx_end, cy_end, false);
                } else if (draw->current_tool == TOOL_CIRCLE) {
                    int dx = cx_end - draw->start_x;
                    int dy = cy_end - draw->start_y;
                    int radius = (int)sqrtf((float)(dx * dx + dy * dy));
                    drawing_circle(draw, draw->start_x, draw->start_y, radius, false);
                }
                
                draw->is_drawing = false;
            }
            
            // Touch su toolbar (selezione strumento)
            if (input->touch_just_pressed) {
                int tx = input->touch_x;
                int ty = input->touch_y;
                
                // Area toolbar
                if (tx < CANVAS_X - 5 && ty >= CANVAS_Y) {
                    int tool_y = CANVAS_Y;
                    int btn_size = 35;
                    int gap = 3;
                    
                    for (int i = 0; i < TOOL_COUNT; i++) {
                        if (point_in_rect(tx, ty, 5, tool_y, btn_size, btn_size)) {
                            draw->current_tool = i;
                            ui_show_toast(ui, tool_names[i], 0.8f);
                            break;
                        }
                        tool_y += btn_size + gap;
                    }
                    
                    // Brush size buttons
                    int size_btn_y = tool_y + 5 + 7 + 20;
                    if (point_in_rect(tx, ty, 5, size_btn_y, 17, 20)) {
                        draw->brush_size--;
                        if (draw->brush_size < 1) draw->brush_size = 1;
                    }
                    if (point_in_rect(tx, ty, 24, size_btn_y, 17, 20)) {
                        draw->brush_size++;
                        if (draw->brush_size > 16) draw->brush_size = 16;
                    }
                    
                    // Colori
                    unsigned int palette[] = { COLOR_BLACK, COLOR_RED, COLOR_BLUE, COLOR_WHITE };
                    uint8_t color_indices[] = { 1, 2, 3, 0 };
                    int color_y = size_btn_y + 25 + 35 + 3 + 5;
                    int color_size = 25;
                    
                    for (int i = 0; i < 4; i++) {
                        int ccx = 5 + (i % 2) * (color_size + 2);
                        int ccy = color_y + (i / 2) * (color_size + 2);
                        if (point_in_rect(tx, ty, ccx, ccy, color_size, color_size)) {
                            draw->current_color = color_indices[i];
                            draw->draw_color = palette[i];
                        }
                    }
                }
                
                // Status bar touches
                if (ty < CANVAS_Y) {
                    // Salva
                    if (point_in_rect(tx, ty, 700, 2, 50, 14)) {
                        animation_save_current_to_draw(anim, draw);
                        ui_goto_screen(ui, SCREEN_EXPORT);
                    }
                    // Undo
                    if (point_in_rect(tx, ty, 810, 2, 40, 14)) {
                        drawing_undo(draw);
                    }
                    // Redo
                    if (point_in_rect(tx, ty, 855, 2, 40, 14)) {
                        drawing_redo(draw);
                    }
                }
                
                // Timeline touch (navigazione frame)
                int tl_y = CANVAS_Y + CANVAS_HEIGHT + 5;
                if (ty > tl_y && tx >= CANVAS_X && tx < CANVAS_X + CANVAS_WIDTH) {
                    int thumb_w = 40;
                    int thumb_gap = 3;
                    int clicked_frame = ui->timeline_scroll + (tx - CANVAS_X - 5) / (thumb_w + thumb_gap);
                    if (clicked_frame >= 0 && clicked_frame < anim->frame_count) {
                        animation_goto_frame(anim, draw, clicked_frame);
                    }
                }
            }
        }
        
        // Aggiorna animazione durante playback
        animation_update(anim, draw, delta_time);
        
        if (anim->is_playing) {
            audio_play_frame_sounds(audio, anim->current_frame);
        }
        
        // Timeline scroll per seguire frame corrente
        if (anim->current_frame < ui->timeline_scroll) {
            ui->timeline_scroll = anim->current_frame;
        }
        if (anim->current_frame >= ui->timeline_scroll + ui->timeline_visible_frames) {
            ui->timeline_scroll = anim->current_frame - ui->timeline_visible_frames + 1;
        }
    }
    else if (ui->current_screen == SCREEN_PLAYBACK) {
        // Aggiorna animazione
        animation_update(anim, draw, delta_time);
        
        if (anim->is_playing) {
            audio_play_frame_sounds(audio, anim->current_frame);
        }
        
        // Triangolo o cerchio per tornare all'editor
        if (input_button_pressed(input, SCE_CTRL_TRIANGLE) || input_button_pressed(input, SCE_CTRL_CIRCLE)) {
            animation_stop(anim);
            ui_goto_screen(ui, SCREEN_EDITOR);
        }
    }
    else if (ui->current_screen == SCREEN_TITLE) {
        // Start per accesso rapido
        if (input_button_pressed(input, SCE_CTRL_START) || input_button_pressed(input, SCE_CTRL_CROSS)) {
            ui_goto_screen(ui, SCREEN_EDITOR);
        }
    }
    else {
        // Cerchio per tornare indietro in tutti i menu
        if (input_button_pressed(input, SCE_CTRL_CIRCLE)) {
            ui_go_back(ui);
        }
    }
    
    // Aggiorna audio
    audio_update(audio);
    if (audio->is_recording) {
        audio_update_recording(audio);
    }
}
