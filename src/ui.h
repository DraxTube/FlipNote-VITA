#ifndef UI_H
#define UI_H

#include "drawing.h"
#include "animation.h"
#include "audio.h"
#include "input.h"
#include <stdbool.h>

typedef enum {
    SCREEN_TITLE,
    SCREEN_EDITOR,
    SCREEN_PLAYBACK,
    SCREEN_FILE_BROWSER,
    SCREEN_SETTINGS,
    SCREEN_TOOL_OPTIONS,
    SCREEN_LAYER_MENU,
    SCREEN_FRAME_MENU,
    SCREEN_COLOR_PICKER,
    SCREEN_SPEED_SETTINGS,
    SCREEN_SOUND_EDITOR,
    SCREEN_HELP,
    SCREEN_EXPORT,
    SCREEN_CONFIRM_DIALOG,
} ScreenState;

typedef enum {
    MENU_NONE,
    MENU_FILE,
    MENU_EDIT,
    MENU_VIEW,
    MENU_TOOLS,
    MENU_ANIMATION,
} MenuType;

typedef struct {
    int x, y, w, h;
    const char *label;
    bool highlighted;
    bool active;
    unsigned int color;
    unsigned int text_color;
} UIButton;

typedef struct {
    ScreenState current_screen;
    ScreenState prev_screen;
    MenuType active_menu;
    
    // Toolbar
    int toolbar_selection;
    bool toolbar_expanded;
    
    // Timeline
    int timeline_scroll;
    int timeline_visible_frames;
    bool timeline_dragging;
    
    // Dialoghi
    bool dialog_active;
    char dialog_message[256];
    int dialog_result;  // 0=pending, 1=yes, 2=no
    void (*dialog_callback)(int result);
    
    // Color picker
    int color_selection;
    
    // Speed slider
    float speed_slider_value;
    
    // File browser
    int file_browser_selection;
    int file_browser_scroll;
    int file_browser_count;
    
    // Layer menu
    int layer_menu_selection;
    
    // Toast/notifiche
    char toast_message[128];
    float toast_timer;
    
    // Frame counter
    bool show_frame_counter;
    
    // Frog character (mascotte Flipnote)
    int frog_state;
    float frog_animation;
    
    // Tema
    int theme; // 0=verde (originale), 1=rosso, 2=blu
    
} UIContext;

void ui_init(UIContext *ui);

// Rendering screens
void ui_render_title_screen(UIContext *ui, InputState *input);
void ui_render_editor(UIContext *ui, DrawingContext *draw, AnimationContext *anim, 
                      AudioContext *audio, InputState *input);
void ui_render_playback(UIContext *ui, DrawingContext *draw, AnimationContext *anim,
                        AudioContext *audio, InputState *input);
void ui_render_file_browser(UIContext *ui, InputState *input);
void ui_render_settings(UIContext *ui, InputState *input);
void ui_render_color_picker(UIContext *ui, DrawingContext *draw, InputState *input);
void ui_render_speed_settings(UIContext *ui, AnimationContext *anim, InputState *input);
void ui_render_sound_editor(UIContext *ui, AudioContext *audio, AnimationContext *anim, InputState *input);
void ui_render_layer_menu(UIContext *ui, DrawingContext *draw, InputState *input);
void ui_render_frame_menu(UIContext *ui, AnimationContext *anim, DrawingContext *draw, InputState *input);
void ui_render_export_menu(UIContext *ui, AnimationContext *anim, AudioContext *audio, InputState *input);
void ui_render_help(UIContext *ui, InputState *input);

// UI elementi
void ui_render_toolbar(UIContext *ui, DrawingContext *draw);
void ui_render_timeline(UIContext *ui, AnimationContext *anim, DrawingContext *draw);
void ui_render_status_bar(UIContext *ui, DrawingContext *draw, AnimationContext *anim);
void ui_render_toast(UIContext *ui, float delta_time);
void ui_render_dialog(UIContext *ui, InputState *input);

// Button helper
bool ui_button(int x, int y, int w, int h, const char *label, unsigned int color, InputState *input);
bool ui_icon_button(int x, int y, int size, const char *icon_char, unsigned int color, InputState *input);

// Toast
void ui_show_toast(UIContext *ui, const char *message, float duration);

// Dialog
void ui_show_dialog(UIContext *ui, const char *message, void (*callback)(int));

// Navigation
void ui_goto_screen(UIContext *ui, ScreenState screen);
void ui_go_back(UIContext *ui);

// Update
void ui_update(UIContext *ui, DrawingContext *draw, AnimationContext *anim,
               AudioContext *audio, InputState *input, float delta_time);

#endif
