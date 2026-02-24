#ifndef DRAWING_H
#define DRAWING_H

#include <stdint.h>

#define CANVAS_WIDTH  512
#define CANVAS_HEIGHT 384
#define CANVAS_X      112
#define CANVAS_Y      20
#define MAX_LAYERS    3
#define MAX_UNDO      50

typedef enum {
    TOOL_PEN,
    TOOL_ERASER,
    TOOL_BUCKET,
    TOOL_LINE,
    TOOL_RECT,
    TOOL_CIRCLE,
    TOOL_MOVE,
    TOOL_SELECT,
    TOOL_STAMP,
    TOOL_EYEDROPPER,
    TOOL_COUNT
} ToolType;

typedef struct {
    uint8_t pixels[CANVAS_WIDTH * CANVAS_HEIGHT];
} LayerData;

typedef struct {
    LayerData layers[MAX_LAYERS];
    int layer_visible[MAX_LAYERS];
    int active_layer;
} CanvasState;

typedef struct {
    CanvasState states[MAX_UNDO];
    int current;
    int count;
    int oldest;
} UndoHistory;

typedef struct {
    ToolType current_tool;
    int brush_size;
    uint8_t current_color;
    unsigned int draw_color;
    int active_layer;
    int layer_visible[MAX_LAYERS];
    int is_drawing;
    int last_x, last_y;
    int start_x, start_y;
    int show_grid;
    int onion_skin;
    int onion_skin_frames;
    float zoom;
    int pan_x, pan_y;

    int has_selection;
    int sel_x, sel_y, sel_w, sel_h;
    uint8_t selection_buffer[CANVAS_WIDTH * CANVAS_HEIGHT];

    int has_stamp;
    uint8_t stamp_buffer[CANVAS_WIDTH * CANVAS_HEIGHT];
    int stamp_w, stamp_h;

    UndoHistory undo;
    LayerData layers[MAX_LAYERS];

    int stabilizer;
    int stab_points_x[8];
    int stab_points_y[8];
    int stab_count;
} DrawingContext;

void drawing_init(DrawingContext *ctx);
void drawing_reset(DrawingContext *ctx);

void drawing_pen_stroke(DrawingContext *ctx, int x, int y);
void drawing_eraser_stroke(DrawingContext *ctx, int x, int y);
void drawing_line(DrawingContext *ctx, int x0, int y0, int x1, int y1);
void drawing_rect(DrawingContext *ctx, int x0, int y0, int x1, int y1, int filled);
void drawing_circle(DrawingContext *ctx, int cx, int cy, int radius, int filled);
void drawing_bucket_fill(DrawingContext *ctx, int x, int y);
void drawing_eyedropper(DrawingContext *ctx, int x, int y);

void drawing_set_pixel(DrawingContext *ctx, int x, int y, uint8_t color);
uint8_t drawing_get_pixel(DrawingContext *ctx, int x, int y);
void drawing_draw_brush(DrawingContext *ctx, int x, int y);

void drawing_set_layer(DrawingContext *ctx, int layer);
void drawing_toggle_layer_visibility(DrawingContext *ctx, int layer);
void drawing_clear_layer(DrawingContext *ctx, int layer);
void drawing_copy_layer(DrawingContext *ctx, int src, int dst);
void drawing_merge_layers(DrawingContext *ctx);
void drawing_swap_layers(DrawingContext *ctx, int a, int b);

void drawing_select_area(DrawingContext *ctx, int x, int y, int w, int h);
void drawing_copy_selection(DrawingContext *ctx);
void drawing_cut_selection(DrawingContext *ctx);
void drawing_paste_selection(DrawingContext *ctx, int x, int y);
void drawing_clear_selection(DrawingContext *ctx);

void drawing_flip_horizontal(DrawingContext *ctx);
void drawing_flip_vertical(DrawingContext *ctx);
void drawing_rotate_90(DrawingContext *ctx);
void drawing_invert_colors(DrawingContext *ctx);

void drawing_save_undo(DrawingContext *ctx);
void drawing_undo(DrawingContext *ctx);
void drawing_redo(DrawingContext *ctx);

void drawing_render_canvas(DrawingContext *ctx);
void drawing_render_onion_skin(DrawingContext *ctx, LayerData *prev_layers, LayerData *next_layers);
unsigned int drawing_get_rgba_color(uint8_t color_index, int layer);

#endif
