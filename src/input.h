#ifndef INPUT_H
#define INPUT_H

#include <psp2/ctrl.h>
#include <psp2/touch.h>
#include <stdbool.h>

#define TOUCH_FRONT  0
#define TOUCH_BACK   1

typedef struct {
    // Buttons
    SceCtrlData pad;
    SceCtrlData pad_prev;
    uint32_t pressed;
    uint32_t released;
    uint32_t held;
    
    // Analog
    int lx, ly;
    int rx, ry;
    
    // Touch front
    bool touch_active;
    int touch_x, touch_y;
    int touch_prev_x, touch_prev_y;
    bool touch_just_pressed;
    bool touch_just_released;
    int touch_count;
    
    // Touch back
    bool back_touch_active;
    int back_touch_x, back_touch_y;
    
    // Multi-touch
    int touch_points;
    int touch_x2, touch_y2; // Secondo punto touch
    float pinch_distance;
    float prev_pinch_distance;
    
} InputState;

void input_init(void);
void input_update(InputState *input);

bool input_button_pressed(InputState *input, uint32_t button);
bool input_button_released(InputState *input, uint32_t button);
bool input_button_held(InputState *input, uint32_t button);

// Converti coordinate touch in coordinate canvas
bool input_touch_to_canvas(InputState *input, int *canvas_x, int *canvas_y);

#endif
