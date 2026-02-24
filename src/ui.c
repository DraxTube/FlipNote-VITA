#include "ui.h"
#include "colors.h"
#include "filemanager.h"
#include <vita2d.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

static vita2d_pgf *font = NULL;

static const char *tool_names[TOOL_COUNT] = {
    "Penna", "Gomma", "Secchiello", "Linea",
    "Rettangolo", "Cerchio", "Sposta", "Seleziona",
    "Timbro", "Contagocce"
};

static const char *tool_icons[TOOL_COUNT] = {
    "P", "E", "B", "L", "R", "C", "M", "S", "T", "D"
};

static unsigned int get_theme_color(UIContext *ui) {
    switch (ui->theme) {
        case 1:  return RGBA8(200, 0, 0, 255);
        case 2:  return RGBA8(0, 0, 200, 255);
        default: return RGBA8(0, 180, 0, 255);
    }
}

static unsigned int get_theme_dark(UIContext *ui) {
    switch (ui->theme) {
        case 1:  return RGBA8(140, 0, 0, 255);
        case 2:  return RGBA8(0, 0, 140, 255);
        default: return RGBA8(0, 120, 0, 255);
    }
}

static void draw_text(int x, int y, unsigned int color, const char *text) {
    if (font && text)
        vita2d_pgf_draw_text(font, x, y, color, 1.0f, text);
}

static void draw_text_scaled(int x, int y, unsigned int color, float scale, const char *text) {
    if (font && text)
        vita2d_pgf_draw_text(font, x, y, color, scale, text);
}

static int point_in_rect(int px, int py, int rx, int ry, int rw, int rh) {
    return (px >= rx && px < rx + rw && py >= ry && py < ry + rh);
}

static unsigned int color_brighten(unsigned int c, int amount) {
    int r = (c & 0xFF) + amount;
    int g = ((c >> 8) & 0xFF) + amount;
    int b = ((c >> 16) & 0xFF) + amount;
    int a = ((c >> 24) & 0xFF);
    if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255;
    return RGBA8(r, g, b, a);
}

/* ========== INIT ========== */
void ui_init(UIContext *ui) {
    memset(ui, 0, sizeof(UIContext));
    ui->current_screen = SCREEN_TITLE;
    ui->prev_screen = SCREEN_TITLE;
    ui->timeline_visible_frames = 10;
    ui->show_frame_counter = 1;
    ui->theme = 0;
    ui->file_browser_selection = 0;
    ui->dialog_active = 0;
    ui->toast_timer = 0.0f;

    if (!font)
        font = vita2d_load_default_pgf();
}

/* ========== NAVIGATION ========== */
void ui_goto_screen(UIContext *ui, ScreenState screen) {
    ui->prev_screen = ui->current_screen;
    ui->current_screen = screen;
}

void ui_go_back(UIContext *ui) {
    ScreenState tmp = ui->current_screen;
    ui->current_screen = ui->prev_screen;
    ui->prev_screen = tmp;
}

/* ========== TOAST ========== */
void ui_show_toast(UIContext *ui, const char *message, float duration) {
    strncpy(ui->toast_message, message, sizeof(ui->toast_message) - 1);
    ui->toast_message[sizeof(ui->toast_message) - 1] = '\0';
    ui->toast_timer = duration;
}

void ui_render_toast(UIContext *ui, float delta_time) {
    float alpha;
    int a, text_w, x, y;

    if (ui->toast_timer <= 0.0f) return;
    ui->toast_timer -= delta_time;

    alpha = 1.0f;
    if (ui->toast_timer < 0.5f && ui->toast_timer > 0.0f)
        alpha = ui->toast_timer / 0.5f;

    a = (int)(alpha * 220.0f);
    text_w = (int)strlen(ui->toast_message) * 10;
    x = (960 - text_w - 40) / 2;
    y = 480;

    vita2d_draw_rectangle(x, y, text_w + 40, 35, RGBA8(0, 0, 0, a));
    draw_text(x + 20, y + 24, RGBA8(255, 255, 255, (int)(alpha * 255.0f)),
              ui->toast_message);
}

/* ========== DIALOG ========== */
void ui_show_dialog(UIContext *ui, const char *message, void (*callback)(int)) {
    ui->dialog_active = 1;
    strncpy(ui->dialog_message, message, sizeof(ui->dialog_message) - 1);
    ui->dialog_message[sizeof(ui->dialog_message) - 1] = '\0';
    ui->dialog_callback = callback;
    ui->dialog_result = 0;
}

void ui_render_dialog(UIContext *ui, InputState *input) {
    unsigned int theme;
    int wx, wy, ww, wh;

    if (!ui->dialog_active) return;
    theme = get_theme_color(ui);

    vita2d_draw_rectangle(0, 0, 960, 544, RGBA8(0, 0, 0, 180));

    wx = 280; wy = 200; ww = 400; wh = 150;
    vita2d_draw_rectangle(wx, wy, ww, wh, RGBA8(60, 60, 60, 255));
    vita2d_draw_rectangle(wx, wy, ww, 30, theme);
    draw_text(wx + 10, wy + 22, COLOR_WHITE, "Conferma");
    draw_text(wx + 20, wy + 70, COLOR_WHITE, ui->dialog_message);

    if (ui_button(wx + 50, wy + wh - 50, 120, 35, "Si", theme, input)) {
        ui->dialog_active = 0;
        ui->dialog_result = 1;
        if (ui->dialog_callback) ui->dialog_callback(1);
    }
    if (ui_button(wx + 230, wy + wh - 50, 120, 35, "No", COLOR_UI_BUTTON, input)) {
        ui->dialog_active = 0;
        ui->dialog_result = 2;
        if (ui->dialog_callback) ui->dialog_callback(2);
    }
}

/* ========== BUTTONS ========== */
int ui_button(int x, int y, int w, int h,
              const char *label, unsigned int color, InputState *input)
{
    int hovered, clicked, text_w, tx, ty;
    unsigned int dc;

    hovered = input->touch_active &&
              point_in_rect(input->touch_x, input->touch_y, x, y, w, h);
    clicked = input->touch_just_pressed &&
              point_in_rect(input->touch_x, input->touch_y, x, y, w, h);

    dc = hovered ? color_brighten(color, 40) : color;

    vita2d_draw_rectangle(x, y, w, h, dc);
    vita2d_draw_rectangle(x, y, w, 2, RGBA8(255, 255, 255, 80));
    vita2d_draw_rectangle(x, y + h - 2, w, 2, RGBA8(0, 0, 0, 80));

    if (label) {
        text_w = (int)strlen(label) * 8;
        tx = x + (w - text_w) / 2;
        ty = y + h / 2 + 5;
        draw_text(tx, ty, COLOR_WHITE, label);
    }
    return clicked;
}

int ui_icon_button(int x, int y, int size,
                   const char *icon_char, unsigned int color, InputState *input)
{
    return ui_button(x, y, size, size, icon_char, color, input);
}

/* ========== TITLE SCREEN ========== */
void ui_render_title_screen(UIContext *ui, InputState *input) {
    unsigned int theme, theme_dark;
    int bx, by, bw, bh, bg, i;

    theme = get_theme_color(ui);
    theme_dark = get_theme_dark(ui);

    vita2d_draw_rectangle(0, 0, 960, 544, theme);
    for (i = 0; i < 544; i += 8)
        vita2d_draw_rectangle(0, i, 960, 2, theme_dark);

    draw_text_scaled(280, 120, COLOR_WHITE, 2.5f, "FLIPNOTE");
    draw_text_scaled(340, 170, COLOR_WHITE, 1.5f, "STUDIO");
    draw_text_scaled(380, 200, COLOR_WHITE, 1.0f, "for PS Vita");

    draw_text_scaled(430, 260, COLOR_WHITE, 1.5f, "^..^");
    draw_text_scaled(425, 280, COLOR_WHITE, 1.5f, "(o  o)");
    draw_text_scaled(420, 300, COLOR_WHITE, 1.5f, "|    |");

    bx = 330; by = 340; bw = 300; bh = 40; bg = 50;

    if (ui_button(bx, by,          bw, bh, "Nuovo Flipnote",  theme_dark, input))
        ui_goto_screen(ui, SCREEN_EDITOR);
    if (ui_button(bx, by + bg,     bw, bh, "Carica Flipnote", theme_dark, input))
        ui_goto_screen(ui, SCREEN_FILE_BROWSER);
    if (ui_button(bx, by + bg * 2, bw, bh, "Impostazioni",    theme_dark, input))
        ui_goto_screen(ui, SCREEN_SETTINGS);
    if (ui_button(bx, by + bg * 3, bw, bh, "Aiuto",           theme_dark, input))
        ui_goto_screen(ui, SCREEN_HELP);

    draw_text(10, 534, RGBA8(255, 255, 255, 150), "v1.0 - Flipnote Vita");
}

/* ========== TOOLBAR ========== */
void ui_render_toolbar(UIContext *ui, DrawingContext *draw) {
    unsigned int theme, palette[4];
    int x, y, btn_size, gap, i, half, preview_r, color_size, cx, cy;
    char size_str[16];
    (void)ui;

    theme = get_theme_color(ui);
    x = 5; y = CANVAS_Y; btn_size = 35; gap = 3;

    vita2d_draw_rectangle(0, CANVAS_Y - 5, CANVAS_X - 5, CANVAS_HEIGHT + 10,
                          RGBA8(70, 70, 70, 230));

    for (i = 0; i < TOOL_COUNT; i++) {
        unsigned int color = ((int)draw->current_tool == i) ? theme : COLOR_UI_BUTTON;
        if ((int)draw->current_tool == i)
            vita2d_draw_rectangle(x - 1, y - 1, btn_size + 2, btn_size + 2, COLOR_UI_SELECTED);
        vita2d_draw_rectangle(x, y, btn_size, btn_size, color);
        draw_text(x + btn_size / 2 - 4, y + btn_size / 2 + 5, COLOR_WHITE, tool_icons[i]);
        y += btn_size + gap;
    }

    y += 5;
    vita2d_draw_rectangle(x, y, btn_size, 2, COLOR_UI_LIGHT);
    y += 7;

    snprintf(size_str, sizeof(size_str), "%d", draw->brush_size);
    draw_text(x + 2, y + 12, COLOR_WHITE, "Size:");
    draw_text(x + 40, y + 12, COLOR_UI_SELECTED, size_str);
    y += 20;

    half = btn_size / 2;
    vita2d_draw_rectangle(x, y, half, 20, COLOR_UI_BUTTON);
    draw_text(x + 5, y + 15, COLOR_WHITE, "-");
    vita2d_draw_rectangle(x + half + 2, y, half, 20, COLOR_UI_BUTTON);
    draw_text(x + half + 7, y + 15, COLOR_WHITE, "+");
    y += 25;

    vita2d_draw_rectangle(x, y, btn_size, btn_size, COLOR_WHITE);
    preview_r = draw->brush_size;
    if (preview_r > btn_size / 2 - 2) preview_r = btn_size / 2 - 2;
    if (preview_r < 1) preview_r = 1;
    vita2d_draw_fill_circle(x + btn_size / 2, y + btn_size / 2, preview_r, draw->draw_color);
    y += btn_size + gap + 5;

    palette[0] = COLOR_BLACK; palette[1] = COLOR_RED;
    palette[2] = COLOR_BLUE;  palette[3] = COLOR_WHITE;
    color_size = 25;

    for (i = 0; i < 4; i++) {
        cx = x + (i % 2) * (color_size + 2);
        cy = y + (i / 2) * (color_size + 2);
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

/* ========== TIMELINE ========== */
void ui_render_timeline(UIContext *ui, AnimationContext *anim, DrawingContext *draw) {
    int tl_y, tl_h, tl_x, tl_w;
    int thumb_w, thumb_h, thumb_gap, visible;
    int start, i, frame_idx, fx, fy;
    unsigned int bg;
    LayerData *layers;
    float sx_f, sy_f;
    int px, py, src_x, src_y;
    uint8_t pix;
    char num[8];
    float scroll_ratio;
    int scrollbar_w, scrollbar_x;
    (void)draw;

    tl_y = CANVAS_Y + CANVAS_HEIGHT + 5;
    tl_h = 544 - tl_y - 5;
    tl_x = CANVAS_X;
    tl_w = CANVAS_WIDTH;

    vita2d_draw_rectangle(tl_x, tl_y, tl_w, tl_h, COLOR_TIMELINE_BG);

    thumb_w = 40; thumb_h = 30; thumb_gap = 3;
    visible = tl_w / (thumb_w + thumb_gap);

    start = ui->timeline_scroll;
    if (start < 0) start = 0;
    if (start > anim->frame_count - visible) {
        start = anim->frame_count - visible;
        if (start < 0) start = 0;
    }

    for (i = 0; i < visible && (start + i) < anim->frame_count; i++) {
        frame_idx = start + i;
        fx = tl_x + i * (thumb_w + thumb_gap) + 5;
        fy = tl_y + 5;

        bg = (frame_idx == anim->current_frame) ? COLOR_FRAME_SEL : COLOR_FRAME_THUMB;
        vita2d_draw_rectangle(fx, fy, thumb_w, thumb_h, bg);

        layers = animation_get_frame_layers(anim, frame_idx);
        if (layers) {
            sx_f = (float)thumb_w / (float)CANVAS_WIDTH;
            sy_f = (float)thumb_h / (float)CANVAS_HEIGHT;
            for (py = 0; py < thumb_h; py += 2) {
                for (px = 0; px < thumb_w; px += 2) {
                    src_x = (int)((float)px / sx_f);
                    src_y = (int)((float)py / sy_f);
                    if (src_x < CANVAS_WIDTH && src_y < CANVAS_HEIGHT) {
                        pix = layers[0].pixels[src_y * CANVAS_WIDTH + src_x];
                        if (pix != 0) {
                            unsigned int c = drawing_get_rgba_color(pix, 0);
                            vita2d_draw_pixel(fx + px, fy + py, c);
                        }
                    }
                }
            }
        }

        if (frame_idx == anim->current_frame) {
            vita2d_draw_rectangle(fx - 1, fy - 1, thumb_w + 2, 2, COLOR_FRAME_SEL);
            vita2d_draw_rectangle(fx - 1, fy + thumb_h - 1, thumb_w + 2, 2, COLOR_FRAME_SEL);
        }

        snprintf(num, sizeof(num), "%d", frame_idx + 1);
        draw_text(fx + 2, fy + thumb_h + 12, COLOR_UI_LIGHT, num);
    }

    if (anim->frame_count > visible && visible > 0) {
        scroll_ratio = (float)start / (float)(anim->frame_count - visible);
        scrollbar_w = tl_w * visible / anim->frame_count;
        if (scrollbar_w < 20) scrollbar_w = 20;
        scrollbar_x = tl_x + (int)((float)(tl_w - scrollbar_w) * scroll_ratio);
        vita2d_draw_rectangle(tl_x, tl_y + tl_h - 6, tl_w, 6, RGBA8(40, 40, 40, 255));
        vita2d_draw_rectangle(scrollbar_x, tl_y + tl_h - 6, scrollbar_w, 6, COLOR_UI_LIGHT);
    }
}

/* ========== STATUS BAR ========== */
void ui_render_status_bar(UIContext *ui, DrawingContext *draw, AnimationContext *anim) {
    unsigned int theme, theme_dark;
    char buf[64];

    theme = get_theme_color(ui);
    theme_dark = get_theme_dark(ui);

    vita2d_draw_rectangle(0, 0, 960, CANVAS_Y - 2, theme);
    draw_text(10, 14, COLOR_WHITE, tool_names[draw->current_tool]);

    snprintf(buf, sizeof(buf), "Frame: %d/%d", anim->current_frame + 1, anim->frame_count);
    draw_text(200, 14, COLOR_WHITE, buf);

    snprintf(buf, sizeof(buf), "Layer: %d", draw->active_layer + 1);
    draw_text(400, 14, COLOR_WHITE, buf);

    snprintf(buf, sizeof(buf), "FPS: %.0f", anim->playback_speed);
    draw_text(520, 14, COLOR_WHITE, buf);

    vita2d_draw_rectangle(700, 2, 50, 14, theme_dark);
    draw_text(705, 14, COLOR_WHITE, "Salva");
    vita2d_draw_rectangle(755, 2, 50, 14, theme_dark);
    draw_text(760, 14, COLOR_WHITE, "Menu");
    vita2d_draw_rectangle(810, 2, 40, 14, theme_dark);
    draw_text(815, 14, COLOR_WHITE, "Undo");
    vita2d_draw_rectangle(855, 2, 40, 14, theme_dark);
    draw_text(860, 14, COLOR_WHITE, "Redo");
    vita2d_draw_rectangle(900, 2, 55, 14, theme_dark);
    draw_text(905, 14, COLOR_WHITE, "Export");
}

/* ========== EDITOR ========== */
void ui_render_editor(UIContext *ui, DrawingContext *draw, AnimationContext *anim,
                      AudioContext *audio, InputState *input)
{
    unsigned int theme, theme_dark;
    int rx, ry, bs, g, bw2;
    int nf, d;
    LayerData *prev_l, *next_l;
    (void)audio;

    theme = get_theme_color(ui);
    theme_dark = get_theme_dark(ui);

    vita2d_draw_rectangle(0, 0, 960, 544, RGBA8(220, 220, 220, 255));
    drawing_render_canvas(draw);

    if (draw->onion_skin && !anim->is_playing) {
        prev_l = NULL; next_l = NULL;
        if (anim->current_frame > 0)
            prev_l = animation_get_frame_layers(anim, anim->current_frame - 1);
        if (anim->current_frame < anim->frame_count - 1)
            next_l = animation_get_frame_layers(anim, anim->current_frame + 1);
        drawing_render_onion_skin(draw, prev_l, next_l);
    }

    vita2d_draw_rectangle(CANVAS_X - 2, CANVAS_Y - 2, CANVAS_WIDTH + 4, 2, COLOR_UI_DARK);
    vita2d_draw_rectangle(CANVAS_X - 2, CANVAS_Y + CANVAS_HEIGHT, CANVAS_WIDTH + 4, 2, COLOR_UI_DARK);
    vita2d_draw_rectangle(CANVAS_X - 2, CANVAS_Y, 2, CANVAS_HEIGHT, COLOR_UI_DARK);
    vita2d_draw_rectangle(CANVAS_X + CANVAS_WIDTH, CANVAS_Y, 2, CANVAS_HEIGHT, COLOR_UI_DARK);

    ui_render_toolbar(ui, draw);
    ui_render_timeline(ui, anim, draw);
    ui_render_status_bar(ui, draw, anim);

    rx = CANVAS_X + CANVAS_WIDTH + 10;
    ry = 30; bs = 40; g = 5;
    bw2 = bs * 2 + g;

    if (ui_button(rx, ry, bw2, bs, anim->is_playing ? "Stop" : "Play", theme, input)) {
        if (anim->is_playing) animation_stop(anim);
        else { animation_save_current_to_draw(anim, draw); animation_play(anim); }
    }
    ry += bs + g;

    if (ui_button(rx, ry, bs, bs, "<", theme_dark, input))
        animation_prev_frame(anim, draw);
    if (ui_button(rx + bs + g, ry, bs, bs, ">", theme_dark, input))
        animation_next_frame(anim, draw);
    ry += bs + g;

    if (ui_button(rx, ry, bw2, bs, "+Frame", theme, input)) {
        animation_save_current_to_draw(anim, draw);
        nf = animation_insert_frame(anim, anim->current_frame + 1);
        if (nf >= 0) { animation_goto_frame(anim, draw, nf); ui_show_toast(ui, "Frame aggiunto", 1.0f); }
    }
    ry += bs + g;

    if (ui_button(rx, ry, bw2, bs, "Duplica", theme_dark, input)) {
        animation_save_current_to_draw(anim, draw);
        d = animation_duplicate_frame(anim, anim->current_frame);
        if (d >= 0) { animation_goto_frame(anim, draw, d); ui_show_toast(ui, "Frame duplicato", 1.0f); }
    }
    ry += bs + g;

    if (ui_button(rx, ry, bw2, bs, "Elimina", RGBA8(200, 50, 50, 255), input)) {
        if (anim->frame_count > 1) {
            animation_delete_frame(anim, anim->current_frame);
            if (anim->current_frame >= anim->frame_count)
                anim->current_frame = anim->frame_count - 1;
            animation_load_current_from_draw(anim, draw);
            ui_show_toast(ui, "Frame eliminato", 1.0f);
        }
    }
    ry += bs + g + 10;

    if (ui_button(rx, ry, bw2, bs,
                  draw->onion_skin ? "Onion:ON" : "Onion:OFF",
                  draw->onion_skin ? theme : COLOR_UI_GRAY, input))
        draw->onion_skin = !draw->onion_skin;
    ry += bs + g;

    if (ui_button(rx, ry, bw2, bs,
                  draw->show_grid ? "Grid:ON" : "Grid:OFF",
                  draw->show_grid ? theme : COLOR_UI_GRAY, input))
        draw->show_grid = !draw->show_grid;
    ry += bs + g;

    if (ui_button(rx, ry, bw2, bs, "Layer", theme_dark, input))
        ui_goto_screen(ui, SCREEN_LAYER_MENU);
    ry += bs + g;

    if (ui_button(rx, ry, bw2, bs, "Speed", theme_dark, input))
        ui_goto_screen(ui, SCREEN_SPEED_SETTINGS);
    ry += bs + g;

    if (ui_button(rx, ry, bw2, bs, "Suoni", theme_dark, input))
        ui_goto_screen(ui, SCREEN_SOUND_EDITOR);
    ry += bs + g;

    if (ui_button(rx, ry, bw2, bs, "FrameM", theme_dark, input))
        ui_goto_screen(ui, SCREEN_FRAME_MENU);
}

/* ========== PLAYBACK ========== */
void ui_render_playback(UIContext *ui, DrawingContext *draw, AnimationContext *anim,
                        AudioContext *audio, InputState *input)
{
    unsigned int theme;
    int px_off, py_off, l, yy, xx;
    uint8_t p;
    unsigned int c;
    int cy_off, cx_off;
    char fc[32];
    (void)audio;

    theme = get_theme_color(ui);
    vita2d_draw_rectangle(0, 0, 960, 544, COLOR_BLACK);

    px_off = (960 - CANVAS_WIDTH) / 2;
    py_off = (544 - CANVAS_HEIGHT) / 2 - 20;

    vita2d_draw_rectangle(px_off, py_off, CANVAS_WIDTH, CANVAS_HEIGHT, COLOR_WHITE);

    for (l = MAX_LAYERS - 1; l >= 0; l--) {
        if (!draw->layer_visible[l]) continue;
        for (yy = 0; yy < CANVAS_HEIGHT; yy++) {
            for (xx = 0; xx < CANVAS_WIDTH; xx++) {
                p = draw->layers[l].pixels[yy * CANVAS_WIDTH + xx];
                if (p != 0) {
                    c = drawing_get_rgba_color(p, l);
                    if (c != COLOR_TRANSPARENT)
                        vita2d_draw_pixel(px_off + xx, py_off + yy, c);
                }
            }
        }
    }

    cy_off = py_off + CANVAS_HEIGHT + 15;
    cx_off = (960 - 300) / 2;

    if (ui_button(cx_off + 30, cy_off, 60, 35, "<<", get_theme_dark(ui), input)) {
        animation_stop(anim); animation_first_frame(anim, draw);
    }
    if (ui_button(cx_off + 100, cy_off, 60, 35,
                  anim->is_playing ? "||" : ">", theme, input))
        animation_toggle_play(anim);
    if (ui_button(cx_off + 170, cy_off, 60, 35, ">>", get_theme_dark(ui), input)) {
        animation_stop(anim); animation_last_frame(anim, draw);
    }
    if (ui_button(cx_off + 240, cy_off, 60, 35, "Edit", RGBA8(200, 50, 50, 255), input)) {
        animation_stop(anim); ui_goto_screen(ui, SCREEN_EDITOR);
    }

    snprintf(fc, sizeof(fc), "%d / %d", anim->current_frame + 1, anim->frame_count);
    draw_text(cx_off + 110, cy_off + 55, COLOR_WHITE, fc);
    draw_text(cx_off, cy_off + 55,
              anim->loop ? COLOR_GREEN : COLOR_RED,
              anim->loop ? "Loop: ON" : "Loop: OFF");
}

/* ========== COLOR PICKER ========== */
void ui_render_color_picker(UIContext *ui, DrawingContext *draw, InputState *input) {
    unsigned int theme;
    int wx, wy, ww, wh, cols, csz, cgap, sx_off, sy_off, i, cx, cy;
    unsigned int colors[16];

    theme = get_theme_color(ui);
    vita2d_draw_rectangle(0, 0, 960, 544, RGBA8(0, 0, 0, 180));

    wx = 200; wy = 100; ww = 560; wh = 344;
    vita2d_draw_rectangle(wx, wy, ww, wh, RGBA8(50, 50, 50, 255));
    vita2d_draw_rectangle(wx, wy, ww, 30, theme);
    draw_text(wx + 10, wy + 22, COLOR_WHITE, "Seleziona Colore");

    colors[0]  = COLOR_BLACK;  colors[1]  = COLOR_RED;
    colors[2]  = COLOR_BLUE;   colors[3]  = COLOR_WHITE;
    colors[4]  = RGBA8(64,64,64,255);    colors[5]  = RGBA8(200,0,0,255);
    colors[6]  = RGBA8(0,0,200,255);     colors[7]  = RGBA8(200,200,200,255);
    colors[8]  = RGBA8(128,128,128,255); colors[9]  = RGBA8(255,100,100,255);
    colors[10] = RGBA8(100,100,255,255); colors[11] = COLOR_YELLOW;
    colors[12] = COLOR_GREEN;  colors[13] = COLOR_ORANGE;
    colors[14] = COLOR_PURPLE; colors[15] = COLOR_CYAN;

    cols = 4; csz = 60; cgap = 10;
    sx_off = wx + 50; sy_off = wy + 50;

    for (i = 0; i < 16; i++) {
        cx = sx_off + (i % cols) * (csz + cgap);
        cy = sy_off + (i / cols) * (csz + cgap);
        vita2d_draw_rectangle(cx, cy, csz, csz, colors[i]);
        if (colors[i] == COLOR_WHITE) {
            vita2d_draw_rectangle(cx, cy, csz, 1, COLOR_UI_DARK);
            vita2d_draw_rectangle(cx, cy + csz - 1, csz, 1, COLOR_UI_DARK);
        }
        if (draw->draw_color == colors[i]) {
            vita2d_draw_rectangle(cx-3, cy-3, csz+6, 3, COLOR_UI_SELECTED);
            vita2d_draw_rectangle(cx-3, cy+csz, csz+6, 3, COLOR_UI_SELECTED);
            vita2d_draw_rectangle(cx-3, cy, 3, csz, COLOR_UI_SELECTED);
            vita2d_draw_rectangle(cx+csz, cy, 3, csz, COLOR_UI_SELECTED);
        }
        if (input->touch_just_pressed && point_in_rect(input->touch_x, input->touch_y, cx, cy, csz, csz)) {
            draw->draw_color = colors[i];
            if      (colors[i] == COLOR_BLACK) draw->current_color = 1;
            else if (colors[i] == COLOR_RED)   draw->current_color = 2;
            else if (colors[i] == COLOR_BLUE)  draw->current_color = 3;
            else                               draw->current_color = 0;
        }
    }

    draw_text(sx_off + 300, sy_off + 10, COLOR_WHITE, "Corrente:");
    vita2d_draw_rectangle(sx_off + 300, sy_off + 20, 100, 60, draw->draw_color);

    if (ui_button(wx + ww - 100, wy + wh - 50, 80, 35, "Chiudi", theme, input))
        ui_go_back(ui);
}

/* ========== LAYER MENU ========== */
void ui_render_layer_menu(UIContext *ui, DrawingContext *draw, InputState *input) {
    unsigned int theme, bg;
    int wx, wy, ww, wh, ly, lx, i;
    char label[32];

    theme = get_theme_color(ui);
    vita2d_draw_rectangle(0, 0, 960, 544, RGBA8(0, 0, 0, 180));

    wx = 250; wy = 80; ww = 460; wh = 400;
    vita2d_draw_rectangle(wx, wy, ww, wh, RGBA8(50, 50, 50, 255));
    vita2d_draw_rectangle(wx, wy, ww, 30, theme);
    draw_text(wx + 10, wy + 22, COLOR_WHITE, "Gestione Layer");

    ly = wy + 50;
    for (i = 0; i < MAX_LAYERS; i++) {
        lx = wx + 20;
        bg = (i == draw->active_layer) ? RGBA8(80,120,80,255) : RGBA8(70,70,70,255);
        vita2d_draw_rectangle(lx, ly, ww - 40, 50, bg);
        snprintf(label, sizeof(label), "Layer %d", i + 1);
        draw_text(lx + 10, ly + 30, COLOR_WHITE, label);

        if (ui_button(lx+200, ly+10, 60, 30,
                      draw->layer_visible[i] ? "Vis" : "Hid",
                      draw->layer_visible[i] ? theme : COLOR_UI_GRAY, input))
            drawing_toggle_layer_visibility(draw, i);
        if (ui_button(lx+270, ly+10, 60, 30, "Sel",
                      (i == draw->active_layer) ? theme : COLOR_UI_BUTTON, input))
            drawing_set_layer(draw, i);
        if (ui_button(lx+340, ly+10, 60, 30, "Clear", RGBA8(200,50,50,255), input)) {
            drawing_save_undo(draw); drawing_clear_layer(draw, i);
            ui_show_toast(ui, "Layer cancellato", 1.0f);
        }
        ly += 55;
    }

    ly += 10;
    if (ui_button(wx+20, ly, 200, 35, "Unisci Layer", theme, input)) {
        drawing_save_undo(draw); drawing_merge_layers(draw);
        ui_show_toast(ui, "Layer uniti", 1.0f);
    }
    if (ui_button(wx+230, ly, 200, 35, "Scambia 1<>2", theme, input)) {
        drawing_save_undo(draw); drawing_swap_layers(draw, 0, 1);
        ui_show_toast(ui, "Layer scambiati", 1.0f);
    }
    ly += 45;

    if (ui_button(wx+20,  ly, 130, 35, "Flip H",  COLOR_UI_BUTTON, input)) {
        drawing_save_undo(draw); drawing_flip_horizontal(draw);
    }
    if (ui_button(wx+160, ly, 130, 35, "Flip V",  COLOR_UI_BUTTON, input)) {
        drawing_save_undo(draw); drawing_flip_vertical(draw);
    }
    if (ui_button(wx+300, ly, 130, 35, "Inverti", COLOR_UI_BUTTON, input)) {
        drawing_save_undo(draw); drawing_invert_colors(draw);
    }

    if (ui_button(wx + ww - 100, wy + wh - 50, 80, 35, "Chiudi", RGBA8(200,50,50,255), input))
        ui_go_back(ui);
}

/* ========== FRAME MENU ========== */
void ui_render_frame_menu(UIContext *ui, AnimationContext *anim,
                          DrawingContext *draw, InputState *input)
{
    unsigned int theme;
    int wx, wy, ww, wh, sy, btn_w, btn_h, gap_v, half, n, d;
    char info[64];

    theme = get_theme_color(ui);
    vita2d_draw_rectangle(0, 0, 960, 544, RGBA8(0, 0, 0, 180));

    wx = 250; wy = 60; ww = 460; wh = 430;
    vita2d_draw_rectangle(wx, wy, ww, wh, RGBA8(50, 50, 50, 255));
    vita2d_draw_rectangle(wx, wy, ww, 30, theme);
    draw_text(wx + 10, wy + 22, COLOR_WHITE, "Menu Frame");

    sy = wy + 50; btn_w = ww - 40; btn_h = 35; gap_v = 5;

    snprintf(info, sizeof(info), "Frame corrente: %d / %d",
             anim->current_frame + 1, anim->frame_count);
    draw_text(wx + 20, sy, COLOR_WHITE, info);
    sy += 30;

    if (ui_button(wx+20, sy, btn_w, btn_h, "Inserisci Frame Dopo", theme, input)) {
        animation_save_current_to_draw(anim, draw);
        n = animation_insert_frame(anim, anim->current_frame + 1);
        if (n >= 0) { animation_goto_frame(anim, draw, n); ui_show_toast(ui, "Frame inserito", 1.0f); }
    }
    sy += btn_h + gap_v;

    if (ui_button(wx+20, sy, btn_w, btn_h, "Inserisci Frame Prima", theme, input)) {
        animation_save_current_to_draw(anim, draw);
        n = animation_insert_frame(anim, anim->current_frame);
        if (n >= 0) { animation_goto_frame(anim, draw, n); ui_show_toast(ui, "Frame inserito", 1.0f); }
    }
    sy += btn_h + gap_v;

    if (ui_button(wx+20, sy, btn_w, btn_h, "Duplica Frame Corrente", theme, input)) {
        animation_save_current_to_draw(anim, draw);
        d = animation_duplicate_frame(anim, anim->current_frame);
        if (d >= 0) { animation_goto_frame(anim, draw, d); ui_show_toast(ui, "Frame duplicato", 1.0f); }
    }
    sy += btn_h + gap_v;

    half = btn_w / 2 - 5;
    if (ui_button(wx+20, sy, half, btn_h, "Copia Frame", COLOR_UI_BUTTON, input)) {
        animation_save_current_to_draw(anim, draw);
        animation_copy_frame(anim, anim->current_frame);
        ui_show_toast(ui, "Frame copiato", 1.0f);
    }
    if (ui_button(wx+20+half+10, sy, half, btn_h, "Incolla Frame",
                  anim->frame_clipboard_valid ? theme : COLOR_UI_GRAY, input)) {
        if (anim->frame_clipboard_valid) {
            animation_paste_frame(anim, anim->current_frame + 1);
            ui_show_toast(ui, "Frame incollato", 1.0f);
        }
    }
    sy += btn_h + gap_v;

    if (ui_button(wx+20, sy, btn_w, btn_h, "Cancella Contenuto Frame",
                  RGBA8(200,100,0,255), input)) {
        drawing_save_undo(draw);
        animation_clear_frame(anim, anim->current_frame);
        animation_load_current_from_draw(anim, draw);
        ui_show_toast(ui, "Frame cancellato", 1.0f);
    }
    sy += btn_h + gap_v;

    if (ui_button(wx+20, sy, btn_w, btn_h, "Elimina Frame", RGBA8(200,50,50,255), input)) {
        if (anim->frame_count > 1) {
            animation_delete_frame(anim, anim->current_frame);
            animation_load_current_from_draw(anim, draw);
            ui_show_toast(ui, "Frame eliminato", 1.0f);
        } else {
            ui_show_toast(ui, "Impossibile: ultimo frame", 1.5f);
        }
    }
    sy += btn_h + gap_v;

    if (ui_button(wx+20, sy, half, btn_h, "Sposta <-", COLOR_UI_BUTTON, input)) {
        if (anim->current_frame > 0) {
            animation_save_current_to_draw(anim, draw);
            animation_swap_frames(anim, anim->current_frame, anim->current_frame - 1);
            anim->current_frame--;
            animation_load_current_from_draw(anim, draw);
        }
    }
    if (ui_button(wx+20+half+10, sy, half, btn_h, "Sposta ->", COLOR_UI_BUTTON, input)) {
        if (anim->current_frame < anim->frame_count - 1) {
            animation_save_current_to_draw(anim, draw);
            animation_swap_frames(anim, anim->current_frame, anim->current_frame + 1);
            anim->current_frame++;
            animation_load_current_from_draw(anim, draw);
        }
    }

    if (ui_button(wx + ww - 100, wy + wh - 50, 80, 35, "Chiudi", RGBA8(200,50,50,255), input))
        ui_go_back(ui);
}

/* ========== SPEED SETTINGS ========== */
void ui_render_speed_settings(UIContext *ui, AnimationContext *anim, InputState *input) {
    unsigned int theme, col;
    int wx, wy, ww, wh, sy, sl_x, sl_w, knob_x, i, bx;
    float ratio, nr;
    char buf[32];
    float presets[8];
    const char *plbl[8];

    theme = get_theme_color(ui);
    vita2d_draw_rectangle(0, 0, 960, 544, RGBA8(0, 0, 0, 180));

    wx = 250; wy = 150; ww = 460; wh = 244;
    vita2d_draw_rectangle(wx, wy, ww, wh, RGBA8(50, 50, 50, 255));
    vita2d_draw_rectangle(wx, wy, ww, 30, theme);
    draw_text(wx + 10, wy + 22, COLOR_WHITE, "Velocita' Animazione");

    sy = wy + 60;
    snprintf(buf, sizeof(buf), "FPS: %.0f", anim->playback_speed);
    draw_text(wx + 180, sy, COLOR_WHITE, buf);
    sy += 30;

    sl_x = wx + 30; sl_w = ww - 60;
    vita2d_draw_rectangle(sl_x, sy, sl_w, 10, COLOR_UI_DARK);
    ratio = (anim->playback_speed - MIN_SPEED) / (float)(MAX_SPEED - MIN_SPEED);
    knob_x = sl_x + (int)(ratio * (float)(sl_w - 20));
    vita2d_draw_rectangle(knob_x, sy - 5, 20, 20, theme);

    if (input->touch_active && point_in_rect(input->touch_x, input->touch_y, sl_x, sy-10, sl_w, 30)) {
        nr = (float)(input->touch_x - sl_x) / (float)sl_w;
        if (nr < 0.0f) nr = 0.0f;
        if (nr > 1.0f) nr = 1.0f;
        anim->playback_speed = MIN_SPEED + nr * (MAX_SPEED - MIN_SPEED);
    }
    sy += 40;

    presets[0]=2; presets[1]=4; presets[2]=6; presets[3]=8;
    presets[4]=12; presets[5]=16; presets[6]=20; presets[7]=24;
    plbl[0]="2"; plbl[1]="4"; plbl[2]="6"; plbl[3]="8";
    plbl[4]="12"; plbl[5]="16"; plbl[6]="20"; plbl[7]="24";

    for (i = 0; i < 8; i++) {
        bx = wx + 20 + i * 53;
        col = ((int)anim->playback_speed == (int)presets[i]) ? theme : COLOR_UI_BUTTON;
        if (ui_button(bx, sy, 48, 30, plbl[i], col, input))
            anim->playback_speed = presets[i];
    }
    sy += 50;

    if (ui_button(wx+20, sy, 200, 35,
                  anim->loop ? "Loop: ON" : "Loop: OFF",
                  anim->loop ? theme : COLOR_UI_GRAY, input))
        anim->loop = !anim->loop;

    if (ui_button(wx+ww-100, wy+wh-50, 80, 35, "Chiudi", RGBA8(200,50,50,255), input))
        ui_go_back(ui);
}

/* ========== SOUND EDITOR ========== */
void ui_render_sound_editor(UIContext *ui, AudioContext *audio,
                            AnimationContext *anim, InputState *input)
{
    unsigned int theme;
    int wx, wy, ww, wh, sy, sx, i, trigger;
    int vl_x, vl_w, vk_x;
    float vol;
    char info_buf[32];
    const char *se_names[MAX_SOUND_EFFECTS];

    se_names[0] = "Click"; se_names[1] = "Beep";
    se_names[2] = "Drum";  se_names[3] = "Voce/Custom";

    theme = get_theme_color(ui);
    vita2d_draw_rectangle(0, 0, 960, 544, RGBA8(0, 0, 0, 180));

    wx = 150; wy = 50; ww = 660; wh = 444;
    vita2d_draw_rectangle(wx, wy, ww, wh, RGBA8(50, 50, 50, 255));
    vita2d_draw_rectangle(wx, wy, ww, 30, theme);
    draw_text(wx + 10, wy + 22, COLOR_WHITE, "Editor Suoni");

    sy = wy + 50;
    for (i = 0; i < MAX_SOUND_EFFECTS; i++) {
        sx = wx + 20;
        vita2d_draw_rectangle(sx, sy, ww - 40, 45, RGBA8(70, 70, 70, 255));
        draw_text(sx + 10, sy + 28, COLOR_WHITE, se_names[i]);

        if (ui_button(sx+250, sy+8, 60, 30, "Play", theme, input))
            audio_play_se(audio, i);

        trigger = audio_get_se_trigger(audio, anim->current_frame, i);
        if (ui_button(sx+320, sy+8, 80, 30,
                      trigger ? "Frame:ON" : "Frame:OFF",
                      trigger ? theme : COLOR_UI_GRAY, input))
            audio_set_se_trigger(audio, anim->current_frame, i, !trigger);

        if (ui_button(sx+410, sy+8, 60, 30,
                      audio->se_enabled[i] ? "ON" : "OFF",
                      audio->se_enabled[i] ? theme : COLOR_UI_GRAY, input))
            audio->se_enabled[i] = !audio->se_enabled[i];

        if (audio->sound_effects[i].sample_count > 0) {
            snprintf(info_buf, sizeof(info_buf), "%.1fs",
                     (float)audio->sound_effects[i].sample_count / AUDIO_SAMPLE_RATE);
            draw_text(sx + 490, sy + 28, COLOR_UI_LIGHT, info_buf);
        }
        sy += 50;
    }

    sy += 10;
    if (ui_button(wx+20, sy, 200, 40,
                  audio->is_recording ? "Stop Reg." : "Registra Voce",
                  audio->is_recording ? RGBA8(200,50,50,255) : theme, input)) {
        if (audio->is_recording) {
            audio_stop_recording(audio);
            ui_show_toast(ui, "Registrazione salvata", 1.5f);
        } else {
            audio_start_recording(audio);
            ui_show_toast(ui, "Registrazione...", 1.0f);
        }
    }
    if (ui_button(wx+240, sy, 200, 40,
                  audio->metronome_enabled ? "Metronomo: ON" : "Metronomo: OFF",
                  audio->metronome_enabled ? theme : COLOR_UI_GRAY, input))
        audio->metronome_enabled = !audio->metronome_enabled;

    sy += 55;
    draw_text(wx + 20, sy + 18, COLOR_WHITE, "Volume SE:");
    vl_x = wx + 150; vl_w = 400;
    vita2d_draw_rectangle(vl_x, sy + 5, vl_w, 15, COLOR_UI_DARK);
    vk_x = vl_x + (int)(audio->se_volume * (float)(vl_w - 15));
    vita2d_draw_rectangle(vk_x, sy, 15, 25, theme);

    if (input->touch_active && point_in_rect(input->touch_x, input->touch_y, vl_x, sy-5, vl_w, 35)) {
        vol = (float)(input->touch_x - vl_x) / (float)vl_w;
        if (vol < 0.0f) vol = 0.0f;
        if (vol > 1.0f) vol = 1.0f;
        audio->se_volume = vol;
    }

    if (ui_button(wx+ww-100, wy+wh-50, 80, 35, "Chiudi", RGBA8(200,50,50,255), input))
        ui_go_back(ui);
}

/* ========== FILE BROWSER ========== */
void ui_render_file_browser(UIContext *ui, InputState *input) {
    unsigned int theme;
    int count, list_y, item_h, start, visible, i, idx, iy;
    SaveSlotInfo slots[MAX_SAVE_SLOTS];
    char info_buf[64];

    theme = get_theme_color(ui);
    vita2d_draw_rectangle(0, 0, 960, 544, RGBA8(40, 40, 40, 255));
    vita2d_draw_rectangle(0, 0, 960, 30, theme);
    draw_text(10, 22, COLOR_WHITE, "Flipnote salvati");

    count = filemanager_list_saves(slots, MAX_SAVE_SLOTS);
    ui->file_browser_count = count;

    if (count == 0) {
        draw_text(350, 272, COLOR_UI_LIGHT, "Nessun flipnote salvato");
    } else {
        list_y = 40; item_h = 60;
        start = ui->file_browser_scroll;
        visible = (544 - 80) / item_h;

        for (i = 0; i < visible && (start + i) < count; i++) {
            unsigned int bg_c;
            idx = start + i;
            iy = list_y + i * item_h;
            bg_c = (idx == ui->file_browser_selection)
                   ? RGBA8(80,80,80,255) : RGBA8(60,60,60,255);
            vita2d_draw_rectangle(10, iy, 940, item_h - 5, bg_c);
            draw_text(20, iy + 25, COLOR_WHITE, slots[idx].title);
            snprintf(info_buf, sizeof(info_buf), "di %s - %d frame",
                     slots[idx].author, slots[idx].frame_count);
            draw_text(20, iy + 45, COLOR_UI_LIGHT, info_buf);
            if (input->touch_just_pressed &&
                point_in_rect(input->touch_x, input->touch_y, 10, iy, 940, item_h-5))
                ui->file_browser_selection = idx;
        }
    }

    if (ui_button(10, 500, 120, 35, "Carica", theme, input)) {
        /* caricamento gestito in ui_update */
    }
    if (ui_button(140, 500, 120, 35, "Elimina", RGBA8(200,50,50,255), input)) {
        if (ui->file_browser_selection >= 0 && ui->file_browser_selection < count)
            filemanager_delete(slots[ui->file_browser_selection].filename);
    }
    if (ui_button(830, 500, 120, 35, "Indietro", COLOR_UI_BUTTON, input))
        ui_go_back(ui);
}

/* ========== SETTINGS ========== */
void ui_render_settings(UIContext *ui, InputState *input) {
    unsigned int theme, tc[3], col;
    int sy, i;
    const char *tn[3];

    theme = get_theme_color(ui);
    vita2d_draw_rectangle(0, 0, 960, 544, RGBA8(40, 40, 40, 255));
    vita2d_draw_rectangle(0, 0, 960, 30, theme);
    draw_text(10, 22, COLOR_WHITE, "Impostazioni");

    sy = 50;
    draw_text(30, sy + 18, COLOR_WHITE, "Tema:");
    tc[0] = RGBA8(0,180,0,255); tc[1] = RGBA8(200,0,0,255); tc[2] = RGBA8(0,0,200,255);
    tn[0] = "Verde"; tn[1] = "Rosso"; tn[2] = "Blu";
    for (i = 0; i < 3; i++) {
        col = (ui->theme == i) ? tc[i] : COLOR_UI_BUTTON;
        if (ui_button(150 + i * 120, sy, 110, 30, tn[i], col, input))
            ui->theme = i;
    }
    sy += 50;

    if (ui_button(30, sy, 300, 35,
                  ui->show_frame_counter ? "Contatore Frame: ON" : "Contatore Frame: OFF",
                  ui->show_frame_counter ? theme : COLOR_UI_GRAY, input))
        ui->show_frame_counter = !ui->show_frame_counter;
    sy += 80;

    draw_text(30, sy, COLOR_UI_LIGHT, "Flipnote Studio per PS Vita");   sy += 25;
    draw_text(30, sy, COLOR_UI_LIGHT, "Versione 1.0");                  sy += 25;
    draw_text(30, sy, COLOR_UI_LIGHT, "Ispirato a Flipnote Studio di Nintendo"); sy += 35;
    draw_text(30, sy, COLOR_UI_LIGHT, "Comandi:");                      sy += 25;
    draw_text(30, sy, COLOR_UI_LIGHT, "  Touch: Disegna / UI");         sy += 25;
    draw_text(30, sy, COLOR_UI_LIGHT, "  L/R: Frame prec./succ.");      sy += 25;
    draw_text(30, sy, COLOR_UI_LIGHT, "  Triangolo: Play/Stop");        sy += 25;
    draw_text(30, sy, COLOR_UI_LIGHT, "  Quadrato: Undo");              sy += 25;
    draw_text(30, sy, COLOR_UI_LIGHT, "  Cerchio: Redo");               sy += 25;
    draw_text(30, sy, COLOR_UI_LIGHT, "  Croce: Cambia strumento");     sy += 25;
    draw_text(30, sy, COLOR_UI_LIGHT, "  Start: Menu/Salva");           sy += 25;
    draw_text(30, sy, COLOR_UI_LIGHT, "  Select: Impostazioni");

    if (ui_button(830, 500, 120, 35, "Indietro", theme, input))
        ui_go_back(ui);
}

/* ========== EXPORT MENU ========== */
void ui_render_export_menu(UIContext *ui, AnimationContext *anim,
                           AudioContext *audio, InputState *input)
{
    unsigned int theme;
    int wx, wy, ww, wh, sy, btn_w;
    char fn[256];

    theme = get_theme_color(ui);
    vita2d_draw_rectangle(0, 0, 960, 544, RGBA8(0, 0, 0, 180));

    wx = 250; wy = 100; ww = 460; wh = 300;
    vita2d_draw_rectangle(wx, wy, ww, wh, RGBA8(50, 50, 50, 255));
    vita2d_draw_rectangle(wx, wy, ww, 30, theme);
    draw_text(wx + 10, wy + 22, COLOR_WHITE, "Esporta");

    sy = wy + 50; btn_w = ww - 40;

    if (ui_button(wx+20, sy, btn_w, 40, "Salva Flipnote (.fnv)", theme, input)) {
        filemanager_generate_filename(fn, sizeof(fn));
        if (filemanager_save(anim, audio, fn))
            ui_show_toast(ui, "Salvato con successo!", 2.0f);
        else
            ui_show_toast(ui, "Errore nel salvataggio!", 2.0f);
    }
    sy += 50;

    if (ui_button(wx+20, sy, btn_w, 40, "Esporta GIF Animata", theme, input)) {
        snprintf(fn, sizeof(fn), "%sexport.gif", SAVE_DIR);
        if (filemanager_export_gif(anim, fn))
            ui_show_toast(ui, "GIF esportata!", 2.0f);
        else
            ui_show_toast(ui, "Errore nell'esportazione!", 2.0f);
    }
    sy += 50;

    if (ui_button(wx+20, sy, btn_w, 40, "Esporta Sequenza Immagini", theme, input)) {
        snprintf(fn, sizeof(fn), "%sframes/", SAVE_DIR);
        if (filemanager_export_png_sequence(anim, fn))
            ui_show_toast(ui, "Immagini esportate!", 2.0f);
        else
            ui_show_toast(ui, "Errore nell'esportazione!", 2.0f);
    }
    sy += 50;

    if (ui_button(wx+20, sy, btn_w, 40, "Esporta Frame Corrente", theme, input)) {
        snprintf(fn, sizeof(fn), "%sframe_%d.bmp", SAVE_DIR, anim->current_frame);
        if (filemanager_export_frame_png(anim, anim->current_frame, fn))
            ui_show_toast(ui, "Frame esportato!", 2.0f);
        else
            ui_show_toast(ui, "Errore!", 2.0f);
    }

    if (ui_button(wx+ww-100, wy+wh-50, 80, 35, "Chiudi", RGBA8(200,50,50,255), input))
        ui_go_back(ui);
}

/* ========== HELP ========== */
void ui_render_help(UIContext *ui, InputState *input) {
    unsigned int theme;
    int sy, lh;

    theme = get_theme_color(ui);
    vita2d_draw_rectangle(0, 0, 960, 544, RGBA8(40, 40, 40, 255));
    vita2d_draw_rectangle(0, 0, 960, 30, theme);
    draw_text(10, 22, COLOR_WHITE, "Guida - Flipnote Vita");

    sy = 50; lh = 22;
    draw_text(30, sy, COLOR_UI_SELECTED, "Come usare Flipnote Vita:"); sy += lh + 5;
    draw_text(30, sy, COLOR_WHITE, "1. Disegna sul canvas usando il touch screen");   sy += lh;
    draw_text(30, sy, COLOR_WHITE, "2. Scegli lo strumento dalla barra a sinistra");  sy += lh;
    draw_text(30, sy, COLOR_WHITE, "3. Aggiungi frame per creare l'animazione");      sy += lh;
    draw_text(30, sy, COLOR_WHITE, "4. Premi Play per vedere l'animazione");          sy += lh;
    draw_text(30, sy, COLOR_WHITE, "5. Usa l'Onion Skin per vedere i frame vicini");  sy += lh + 10;
    draw_text(30, sy, COLOR_UI_SELECTED, "Strumenti:"); sy += lh + 5;
    draw_text(30, sy, COLOR_WHITE, "P - Penna: disegno libero");         sy += lh;
    draw_text(30, sy, COLOR_WHITE, "E - Gomma: cancella");               sy += lh;
    draw_text(30, sy, COLOR_WHITE, "B - Secchiello: riempimento area");  sy += lh;
    draw_text(30, sy, COLOR_WHITE, "L - Linea: linea dritta");           sy += lh;
    draw_text(30, sy, COLOR_WHITE, "R - Rettangolo");                    sy += lh;
    draw_text(30, sy, COLOR_WHITE, "C - Cerchio");                       sy += lh;
    draw_text(30, sy, COLOR_WHITE, "M - Sposta: muovi il contenuto");    sy += lh;
    draw_text(30, sy, COLOR_WHITE, "S - Seleziona: seleziona un'area");  sy += lh;
    draw_text(30, sy, COLOR_WHITE, "T - Timbro: incolla selezione");     sy += lh;
    draw_text(30, sy, COLOR_WHITE, "D - Contagocce: preleva colore");    sy += lh + 10;
    draw_text(30, sy, COLOR_UI_SELECTED, "Max 999 frame per animazione"); sy += lh;
    draw_text(30, sy, COLOR_UI_SELECTED, "3 layer per frame");            sy += lh;
    draw_text(30, sy, COLOR_UI_SELECTED, "4 effetti sonori assegnabili");

    if (ui_button(830, 500, 120, 35, "Indietro", theme, input))
        ui_go_back(ui);
}

/* ========== MAIN UPDATE ========== */
void ui_update(UIContext *ui, DrawingContext *draw, AnimationContext *anim,
               AudioContext *audio, InputState *input, float delta_time)
{
    if (ui->current_screen == SCREEN_EDITOR) {
        /* L / R */
        if (input_button_pressed(input, SCE_CTRL_LTRIGGER))
            animation_prev_frame(anim, draw);
        if (input_button_pressed(input, SCE_CTRL_RTRIGGER))
            animation_next_frame(anim, draw);

        /* Triangle */
        if (input_button_pressed(input, SCE_CTRL_TRIANGLE)) {
            if (anim->is_playing) {
                animation_stop(anim);
            } else {
                animation_save_current_to_draw(anim, draw);
                ui_goto_screen(ui, SCREEN_PLAYBACK);
                animation_play(anim);
            }
        }

        /* Square = undo */
        if (input_button_pressed(input, SCE_CTRL_SQUARE)) {
            drawing_undo(draw); ui_show_toast(ui, "Undo", 0.5f);
        }

        /* Circle = redo */
        if (input_button_pressed(input, SCE_CTRL_CIRCLE)) {
            drawing_redo(draw); ui_show_toast(ui, "Redo", 0.5f);
        }

        /* Cross = tool cycle */
        if (input_button_pressed(input, SCE_CTRL_CROSS)) {
            draw->current_tool = (draw->current_tool + 1) % TOOL_COUNT;
            ui_show_toast(ui, tool_names[draw->current_tool], 0.8f);
        }

        /* D-Pad up/down = brush size */
        if (input_button_pressed(input, SCE_CTRL_UP)) {
            draw->brush_size++; if (draw->brush_size > 16) draw->brush_size = 16;
        }
        if (input_button_pressed(input, SCE_CTRL_DOWN)) {
            draw->brush_size--; if (draw->brush_size < 1) draw->brush_size = 1;
        }

        /* D-Pad left/right = color */
        if (input_button_pressed(input, SCE_CTRL_LEFT)) {
            draw->current_color = (draw->current_color + LAYER_COLORS_COUNT - 1) % LAYER_COLORS_COUNT;
            draw->draw_color = drawing_get_rgba_color(draw->current_color, draw->active_layer);
        }
        if (input_button_pressed(input, SCE_CTRL_RIGHT)) {
            draw->current_color = (draw->current_color + 1) % LAYER_COLORS_COUNT;
            draw->draw_color = drawing_get_rgba_color(draw->current_color, draw->active_layer);
        }

        /* Start = export */
        if (input_button_pressed(input, SCE_CTRL_START)) {
            animation_save_current_to_draw(anim, draw);
            ui_goto_screen(ui, SCREEN_EXPORT);
        }

        /* Select = settings */
        if (input_button_pressed(input, SCE_CTRL_SELECT))
            ui_goto_screen(ui, SCREEN_SETTINGS);

        /* Touch drawing */
        if (!anim->is_playing) {
            int cx, cy;

            if (input_touch_to_canvas(input, &cx, &cy)) {
                if (input->touch_just_pressed) {
                    drawing_save_undo(draw);
                    draw->is_drawing = 0;
                    if (draw->current_tool == TOOL_LINE ||
                        draw->current_tool == TOOL_RECT ||
                        draw->current_tool == TOOL_CIRCLE) {
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
                        if (input->touch_just_pressed)
                            drawing_bucket_fill(draw, cx, cy);
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
                        break;
                    default:
                        drawing_pen_stroke(draw, cx, cy);
                        break;
                }
            }

            /* Release => shape tools */
            if (input->touch_just_released) {
                int ex = input->touch_prev_x - CANVAS_X;
                int ey = input->touch_prev_y - CANVAS_Y;

                if (draw->current_tool == TOOL_LINE) {
                    drawing_line(draw, draw->start_x, draw->start_y, ex, ey);
                } else if (draw->current_tool == TOOL_RECT) {
                    drawing_rect(draw, draw->start_x, draw->start_y, ex, ey, 0);
                } else if (draw->current_tool == TOOL_CIRCLE) {
                    int ddx = ex - draw->start_x;
                    int ddy = ey - draw->start_y;
                    int r = (int)sqrtf((float)(ddx*ddx + ddy*ddy));
                    drawing_circle(draw, draw->start_x, draw->start_y, r, 0);
                }
                draw->is_drawing = 0;
            }

            /* Touch on toolbar */
            if (input->touch_just_pressed) {
                int tx = input->touch_x;
                int ty = input->touch_y;

                if (tx < CANVAS_X - 5 && ty >= CANVAS_Y) {
                    int tool_y = CANVAS_Y;
                    int bsz = 35;
                    int tg = 3;
                    int ti;

                    for (ti = 0; ti < TOOL_COUNT; ti++) {
                        if (point_in_rect(tx, ty, 5, tool_y, bsz, bsz)) {
                            draw->current_tool = ti;
                            ui_show_toast(ui, tool_names[ti], 0.8f);
                            break;
                        }
                        tool_y += bsz + tg;
                    }

                    {
                        int size_y = tool_y + 5 + 7 + 20;
                        if (point_in_rect(tx, ty, 5, size_y, 17, 20)) {
                            draw->brush_size--; if (draw->brush_size < 1) draw->brush_size = 1;
                        }
                        if (point_in_rect(tx, ty, 24, size_y, 17, 20)) {
                            draw->brush_size++; if (draw->brush_size > 16) draw->brush_size = 16;
                        }
                    }

                    {
                        unsigned int pal[4] = { COLOR_BLACK, COLOR_RED, COLOR_BLUE, COLOR_WHITE };
                        uint8_t ci[4] = { 1, 2, 3, 0 };
                        int tool_y2 = CANVAS_Y;
                        int csz = 25;
                        int color_y, ccx, ccy, ci2;

                        for (ti = 0; ti < TOOL_COUNT; ti++) tool_y2 += 35 + 3;
                        color_y = tool_y2 + 5 + 7 + 20 + 25 + 35 + 3 + 5;

                        for (ci2 = 0; ci2 < 4; ci2++) {
                            ccx = 5 + (ci2 % 2) * (csz + 2);
                            ccy = color_y + (ci2 / 2) * (csz + 2);
                            if (point_in_rect(tx, ty, ccx, ccy, csz, csz)) {
                                draw->current_color = ci[ci2];
                                draw->draw_color = pal[ci2];
                            }
                        }
                    }
                }

                /* Status bar */
                if (ty < CANVAS_Y) {
                    if (point_in_rect(tx, ty, 700, 2, 50, 14)) {
                        animation_save_current_to_draw(anim, draw);
                        ui_goto_screen(ui, SCREEN_EXPORT);
                    }
                    if (point_in_rect(tx, ty, 810, 2, 40, 14))
                        drawing_undo(draw);
                    if (point_in_rect(tx, ty, 855, 2, 40, 14))
                        drawing_redo(draw);
                    if (point_in_rect(tx, ty, 900, 2, 55, 14)) {
                        animation_save_current_to_draw(anim, draw);
                        ui_goto_screen(ui, SCREEN_EXPORT);
                    }
                }

                /* Timeline click */
                {
                    int tl_y = CANVAS_Y + CANVAS_HEIGHT + 5;
                    if (ty > tl_y && tx >= CANVAS_X && tx < CANVAS_X + CANVAS_WIDTH) {
                        int tw = 40, tgp = 3;
                        int cf = ui->timeline_scroll + (tx - CANVAS_X - 5) / (tw + tgp);
                        if (cf >= 0 && cf < anim->frame_count)
                            animation_goto_frame(anim, draw, cf);
                    }
                }
            }
        }

        animation_update(anim, draw, delta_time);
        if (anim->is_playing)
            audio_play_frame_sounds(audio, anim->current_frame);

        if (anim->current_frame < ui->timeline_scroll)
            ui->timeline_scroll = anim->current_frame;
        if (anim->current_frame >= ui->timeline_scroll + ui->timeline_visible_frames)
            ui->timeline_scroll = anim->current_frame - ui->timeline_visible_frames + 1;
    }
    else if (ui->current_screen == SCREEN_PLAYBACK) {
        animation_update(anim, draw, delta_time);
        if (anim->is_playing)
            audio_play_frame_sounds(audio, anim->current_frame);
        if (input_button_pressed(input, SCE_CTRL_TRIANGLE) ||
            input_button_pressed(input, SCE_CTRL_CIRCLE)) {
            animation_stop(anim);
            ui_goto_screen(ui, SCREEN_EDITOR);
        }
    }
    else if (ui->current_screen == SCREEN_TITLE) {
        if (input_button_pressed(input, SCE_CTRL_START) ||
            input_button_pressed(input, SCE_CTRL_CROSS))
            ui_goto_screen(ui, SCREEN_EDITOR);
    }
    else {
        if (input_button_pressed(input, SCE_CTRL_CIRCLE))
            ui_go_back(ui);
    }

    audio_update(audio);
    if (audio->is_recording)
        audio_update_recording(audio);
}
