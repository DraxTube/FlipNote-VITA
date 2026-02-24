#include "input.h"
#include "drawing.h"
#include <string.h>

void input_init(void) {
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
    sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);
    sceTouchSetSamplingState(SCE_TOUCH_PORT_BACK, SCE_TOUCH_SAMPLING_STATE_START);
    sceTouchEnableTouchForce(SCE_TOUCH_PORT_FRONT);
}

void input_update(InputState *input) {
    // Salva stato precedente
    memcpy(&input->pad_prev, &input->pad, sizeof(SceCtrlData));
    bool prev_touch = input->touch_active;
    input->touch_prev_x = input->touch_x;
    input->touch_prev_y = input->touch_y;
    input->prev_pinch_distance = input->pinch_distance;
    
    // Aggiorna pulsanti
    sceCtrlPeekBufferPositive(0, &input->pad, 1);
    
    input->pressed = input->pad.buttons & ~input->pad_prev.buttons;
    input->released = ~input->pad.buttons & input->pad_prev.buttons;
    input->held = input->pad.buttons;
    
    // Analog
    input->lx = (int)input->pad.lx - 128;
    input->ly = (int)input->pad.ly - 128;
    input->rx = (int)input->pad.rx - 128;
    input->ry = (int)input->pad.ry - 128;
    
    // Dead zone
    if (abs(input->lx) < 20) input->lx = 0;
    if (abs(input->ly) < 20) input->ly = 0;
    if (abs(input->rx) < 20) input->rx = 0;
    if (abs(input->ry) < 20) input->ry = 0;
    
    // Touch front
    SceTouchData touch;
    sceTouchPeek(SCE_TOUCH_PORT_FRONT, &touch, 1);
    
    input->touch_points = touch.reportNum;
    
    if (touch.reportNum > 0) {
        input->touch_active = true;
        input->touch_x = (touch.report[0].x * 960) / 1920;
        input->touch_y = (touch.report[0].y * 544) / 1088;
        input->touch_count = touch.reportNum;
        
        if (touch.reportNum > 1) {
            input->touch_x2 = (touch.report[1].x * 960) / 1920;
            input->touch_y2 = (touch.report[1].y * 544) / 1088;
            
            int dx = input->touch_x2 - input->touch_x;
            int dy = input->touch_y2 - input->touch_y;
            input->pinch_distance = sqrtf((float)(dx * dx + dy * dy));
        }
    } else {
        input->touch_active = false;
        input->touch_count = 0;
        input->pinch_distance = 0;
    }
    
    input->touch_just_pressed = input->touch_active && !prev_touch;
    input->touch_just_released = !input->touch_active && prev_touch;
    
    // Touch back
    SceTouchData back_touch;
    sceTouchPeek(SCE_TOUCH_PORT_BACK, &back_touch, 1);
    
    if (back_touch.reportNum > 0) {
        input->back_touch_active = true;
        input->back_touch_x = (back_touch.report[0].x * 960) / 1920;
        input->back_touch_y = (back_touch.report[0].y * 544) / 1088;
    } else {
        input->back_touch_active = false;
    }
}

bool input_button_pressed(InputState *input, uint32_t button) {
    return (input->pressed & button) != 0;
}

bool input_button_released(InputState *input, uint32_t button) {
    return (input->released & button) != 0;
}

bool input_button_held(InputState *input, uint32_t button) {
    return (input->held & button) != 0;
}

bool input_touch_to_canvas(InputState *input, int *canvas_x, int *canvas_y) {
    if (!input->touch_active) return false;
    
    int cx = input->touch_x - CANVAS_X;
    int cy = input->touch_y - CANVAS_Y;
    
    if (cx < 0 || cx >= CANVAS_WIDTH || cy < 0 || cy >= CANVAS_HEIGHT) {
        return false;
    }
    
    *canvas_x = cx;
    *canvas_y = cy;
    return true;
}
