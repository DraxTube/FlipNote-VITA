#include "drawing.h"
#include "colors.h"
#include <vita2d.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

void drawing_init(DrawingContext *ctx) {
    memset(ctx, 0, sizeof(DrawingContext));
    ctx->current_tool = TOOL_PEN;
    ctx->brush_size = BRUSH_SIZE_2;
    ctx->current_color = 1; // Nero
    ctx->draw_color = COLOR_BLACK;
    ctx->active_layer = 0;
    ctx->zoom = 1.0f;
    ctx->pan_x = 0;
    ctx->pan_y = 0;
    ctx->onion_skin_frames = 1;
    ctx->stabilizer = false;
    
    for (int i = 0; i < MAX_LAYERS; i++) {
        ctx->layer_visible[i] = true;
        memset(&ctx->layers[i], 0, sizeof(LayerData));
    }
    
    ctx->undo.current = -1;
    ctx->undo.count = 0;
    ctx->undo.oldest = 0;
}

void drawing_reset(DrawingContext *ctx) {
    for (int i = 0; i < MAX_LAYERS; i++) {
        memset(&ctx->layers[i], 0, sizeof(LayerData));
    }
    ctx->has_selection = false;
    ctx->has_stamp = false;
    ctx->undo.current = -1;
    ctx->undo.count = 0;
}

unsigned int drawing_get_rgba_color(uint8_t color_index, int layer) {
    if (color_index == 0) {
        if (layer == 0) return COLOR_WHITE;
        else return COLOR_TRANSPARENT;
    }
    if (layer < 0 || layer >= MAX_LAYERS) layer = 0;
    if (color_index >= LAYER_COLORS_COUNT) color_index = 1;
    return LAYER_PALETTE[layer][color_index];
}

void drawing_set_pixel(DrawingContext *ctx, int x, int y, uint8_t color) {
    if (x < 0 || x >= CANVAS_WIDTH || y < 0 || y >= CANVAS_HEIGHT) return;
    ctx->layers[ctx->active_layer].pixels[y * CANVAS_WIDTH + x] = color;
}

uint8_t drawing_get_pixel(DrawingContext *ctx, int x, int y) {
    if (x < 0 || x >= CANVAS_WIDTH || y < 0 || y >= CANVAS_HEIGHT) return 0;
    return ctx->layers[ctx->active_layer].pixels[y * CANVAS_WIDTH + x];
}

void drawing_draw_brush(DrawingContext *ctx, int x, int y) {
    int size = ctx->brush_size;
    uint8_t color = (ctx->current_tool == TOOL_ERASER) ? 0 : ctx->current_color;
    
    if (size <= 1) {
        drawing_set_pixel(ctx, x, y, color);
        return;
    }
    
    int half = size / 2;
    for (int dy = -half; dy <= half; dy++) {
        for (int dx = -half; dx <= half; dx++) {
            // Brush circolare
            if (dx * dx + dy * dy <= half * half) {
                drawing_set_pixel(ctx, x + dx, y + dy, color);
            }
        }
    }
}

void drawing_pen_stroke(DrawingContext *ctx, int x, int y) {
    if (!ctx->is_drawing) {
        ctx->is_drawing = true;
        ctx->last_x = x;
        ctx->last_y = y;
        drawing_draw_brush(ctx, x, y);
        return;
    }
    
    // Bresenham line per connettere i punti
    drawing_line_internal(ctx, ctx->last_x, ctx->last_y, x, y, true);
    ctx->last_x = x;
    ctx->last_y = y;
}

static void drawing_line_internal(DrawingContext *ctx, int x0, int y0, int x1, int y1, bool use_brush) {
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    
    uint8_t color = (ctx->current_tool == TOOL_ERASER) ? 0 : ctx->current_color;
    
    while (1) {
        if (use_brush) {
            drawing_draw_brush(ctx, x0, y0);
        } else {
            drawing_set_pixel(ctx, x0, y0, color);
        }
        
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx) { err += dx; y0 += sy; }
    }
}

void drawing_eraser_stroke(DrawingContext *ctx, int x, int y) {
    ToolType saved = ctx->current_tool;
    ctx->current_tool = TOOL_ERASER;
    drawing_pen_stroke(ctx, x, y);
    ctx->current_tool = saved;
}

void drawing_line(DrawingContext *ctx, int x0, int y0, int x1, int y1) {
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    
    while (1) {
        drawing_draw_brush(ctx, x0, y0);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx) { err += dx; y0 += sy; }
    }
}

void drawing_rect(DrawingContext *ctx, int x0, int y0, int x1, int y1, bool filled) {
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    
    uint8_t color = ctx->current_color;
    
    if (filled) {
        for (int y = y0; y <= y1; y++) {
            for (int x = x0; x <= x1; x++) {
                drawing_set_pixel(ctx, x, y, color);
            }
        }
    } else {
        for (int x = x0; x <= x1; x++) {
            drawing_set_pixel(ctx, x, y0, color);
            drawing_set_pixel(ctx, x, y1, color);
        }
        for (int y = y0; y <= y1; y++) {
            drawing_set_pixel(ctx, x0, y, color);
            drawing_set_pixel(ctx, x1, y, color);
        }
    }
}

void drawing_circle(DrawingContext *ctx, int cx, int cy, int radius, bool filled) {
    uint8_t color = ctx->current_color;
    
    if (filled) {
        for (int y = -radius; y <= radius; y++) {
            for (int x = -radius; x <= radius; x++) {
                if (x * x + y * y <= radius * radius) {
                    drawing_set_pixel(ctx, cx + x, cy + y, color);
                }
            }
        }
    } else {
        int x = radius, y = 0;
        int err = 0;
        while (x >= y) {
            drawing_set_pixel(ctx, cx + x, cy + y, color);
            drawing_set_pixel(ctx, cx + y, cy + x, color);
            drawing_set_pixel(ctx, cx - y, cy + x, color);
            drawing_set_pixel(ctx, cx - x, cy + y, color);
            drawing_set_pixel(ctx, cx - x, cy - y, color);
            drawing_set_pixel(ctx, cx - y, cy - x, color);
            drawing_set_pixel(ctx, cx + y, cy - x, color);
            drawing_set_pixel(ctx, cx + x, cy - y, color);
            
            if (err <= 0) {
                y += 1;
                err += 2 * y + 1;
            }
            if (err > 0) {
                x -= 1;
                err -= 2 * x + 1;
            }
        }
    }
}

// Flood fill iterativo (stack-based per evitare stack overflow)
#define FILL_STACK_SIZE (CANVAS_WIDTH * CANVAS_HEIGHT)

typedef struct {
    int x, y;
} FillPoint;

void drawing_bucket_fill(DrawingContext *ctx, int x, int y) {
    if (x < 0 || x >= CANVAS_WIDTH || y < 0 || y >= CANVAS_HEIGHT) return;
    
    uint8_t target_color = drawing_get_pixel(ctx, x, y);
    uint8_t fill_color = ctx->current_color;
    
    if (target_color == fill_color) return;
    
    FillPoint *stack = (FillPoint *)malloc(FILL_STACK_SIZE * sizeof(FillPoint));
    if (!stack) return;
    
    int stack_top = 0;
    stack[stack_top].x = x;
    stack[stack_top].y = y;
    stack_top++;
    
    while (stack_top > 0) {
        stack_top--;
        int cx = stack[stack_top].x;
        int cy = stack[stack_top].y;
        
        if (cx < 0 || cx >= CANVAS_WIDTH || cy < 0 || cy >= CANVAS_HEIGHT) continue;
        if (drawing_get_pixel(ctx, cx, cy) != target_color) continue;
        
        drawing_set_pixel(ctx, cx, cy, fill_color);
        
        if (stack_top + 4 < FILL_STACK_SIZE) {
            stack[stack_top].x = cx + 1; stack[stack_top].y = cy; stack_top++;
            stack[stack_top].x = cx - 1; stack[stack_top].y = cy; stack_top++;
            stack[stack_top].x = cx; stack[stack_top].y = cy + 1; stack_top++;
            stack[stack_top].x = cx; stack[stack_top].y = cy - 1; stack_top++;
        }
    }
    
    free(stack);
}

void drawing_eyedropper(DrawingContext *ctx, int x, int y) {
    if (x < 0 || x >= CANVAS_WIDTH || y < 0 || y >= CANVAS_HEIGHT) return;
    ctx->current_color = drawing_get_pixel(ctx, x, y);
    ctx->draw_color = drawing_get_rgba_color(ctx->current_color, ctx->active_layer);
}

// Layer operations
void drawing_set_layer(DrawingContext *ctx, int layer) {
    if (layer >= 0 && layer < MAX_LAYERS) {
        ctx->active_layer = layer;
    }
}

void drawing_toggle_layer_visibility(DrawingContext *ctx, int layer) {
    if (layer >= 0 && layer < MAX_LAYERS) {
        ctx->layer_visible[layer] = !ctx->layer_visible[layer];
    }
}

void drawing_clear_layer(DrawingContext *ctx, int layer) {
    if (layer >= 0 && layer < MAX_LAYERS) {
        memset(&ctx->layers[layer], 0, sizeof(LayerData));
    }
}

void drawing_copy_layer(DrawingContext *ctx, int src, int dst) {
    if (src >= 0 && src < MAX_LAYERS && dst >= 0 && dst < MAX_LAYERS) {
        memcpy(&ctx->layers[dst], &ctx->layers[src], sizeof(LayerData));
    }
}

void drawing_merge_layers(DrawingContext *ctx) {
    for (int y = 0; y < CANVAS_HEIGHT; y++) {
        for (int x = 0; x < CANVAS_WIDTH; x++) {
            int idx = y * CANVAS_WIDTH + x;
            for (int l = MAX_LAYERS - 1; l > 0; l--) {
                if (ctx->layers[l].pixels[idx] != 0) {
                    ctx->layers[0].pixels[idx] = ctx->layers[l].pixels[idx];
                    ctx->layers[l].pixels[idx] = 0;
                }
            }
        }
    }
}

void drawing_swap_layers(DrawingContext *ctx, int a, int b) {
    if (a >= 0 && a < MAX_LAYERS && b >= 0 && b < MAX_LAYERS) {
        LayerData temp;
        memcpy(&temp, &ctx->layers[a], sizeof(LayerData));
        memcpy(&ctx->layers[a], &ctx->layers[b], sizeof(LayerData));
        memcpy(&ctx->layers[b], &temp, sizeof(LayerData));
    }
}

// Selection
void drawing_select_area(DrawingContext *ctx, int x, int y, int w, int h) {
    ctx->has_selection = true;
    ctx->sel_x = x;
    ctx->sel_y = y;
    ctx->sel_w = w;
    ctx->sel_h = h;
}

void drawing_copy_selection(DrawingContext *ctx) {
    if (!ctx->has_selection) return;
    memset(ctx->selection_buffer, 0, sizeof(ctx->selection_buffer));
    for (int y = 0; y < ctx->sel_h; y++) {
        for (int x = 0; x < ctx->sel_w; x++) {
            int sx = ctx->sel_x + x;
            int sy = ctx->sel_y + y;
            if (sx >= 0 && sx < CANVAS_WIDTH && sy >= 0 && sy < CANVAS_HEIGHT) {
                ctx->selection_buffer[y * CANVAS_WIDTH + x] = drawing_get_pixel(ctx, sx, sy);
            }
        }
    }
    ctx->has_stamp = true;
    ctx->stamp_w = ctx->sel_w;
    ctx->stamp_h = ctx->sel_h;
    memcpy(ctx->stamp_buffer, ctx->selection_buffer, sizeof(ctx->selection_buffer));
}

void drawing_cut_selection(DrawingContext *ctx) {
    drawing_copy_selection(ctx);
    if (!ctx->has_selection) return;
    for (int y = 0; y < ctx->sel_h; y++) {
        for (int x = 0; x < ctx->sel_w; x++) {
            drawing_set_pixel(ctx, ctx->sel_x + x, ctx->sel_y + y, 0);
        }
    }
}

void drawing_paste_selection(DrawingContext *ctx, int x, int y) {
    if (!ctx->has_stamp) return;
    for (int py = 0; py < ctx->stamp_h; py++) {
        for (int px = 0; px < ctx->stamp_w; px++) {
            uint8_t color = ctx->stamp_buffer[py * CANVAS_WIDTH + px];
            if (color != 0) {
                drawing_set_pixel(ctx, x + px, y + py, color);
            }
        }
    }
}

void drawing_clear_selection(DrawingContext *ctx) {
    ctx->has_selection = false;
}

// Canvas operations
void drawing_flip_horizontal(DrawingContext *ctx) {
    LayerData *layer = &ctx->layers[ctx->active_layer];
    for (int y = 0; y < CANVAS_HEIGHT; y++) {
        for (int x = 0; x < CANVAS_WIDTH / 2; x++) {
            int idx1 = y * CANVAS_WIDTH + x;
            int idx2 = y * CANVAS_WIDTH + (CANVAS_WIDTH - 1 - x);
            uint8_t temp = layer->pixels[idx1];
            layer->pixels[idx1] = layer->pixels[idx2];
            layer->pixels[idx2] = temp;
        }
    }
}

void drawing_flip_vertical(DrawingContext *ctx) {
    LayerData *layer = &ctx->layers[ctx->active_layer];
    for (int y = 0; y < CANVAS_HEIGHT / 2; y++) {
        for (int x = 0; x < CANVAS_WIDTH; x++) {
            int idx1 = y * CANVAS_WIDTH + x;
            int idx2 = (CANVAS_HEIGHT - 1 - y) * CANVAS_WIDTH + x;
            uint8_t temp = layer->pixels[idx1];
            layer->pixels[idx1] = layer->pixels[idx2];
            layer->pixels[idx2] = temp;
        }
    }
}

void drawing_rotate_90(DrawingContext *ctx) {
    LayerData temp;
    LayerData *layer = &ctx->layers[ctx->active_layer];
    memcpy(&temp, layer, sizeof(LayerData));
    memset(layer, 0, sizeof(LayerData));
    
    int new_w = CANVAS_HEIGHT;
    int new_h = CANVAS_WIDTH;
    (void)new_w; (void)new_h;
    
    for (int y = 0; y < CANVAS_HEIGHT; y++) {
        for (int x = 0; x < CANVAS_WIDTH; x++) {
            int new_x = CANVAS_HEIGHT - 1 - y;
            int new_y = x;
            if (new_x >= 0 && new_x < CANVAS_WIDTH && new_y >= 0 && new_y < CANVAS_HEIGHT) {
                layer->pixels[new_y * CANVAS_WIDTH + new_x] = temp.pixels[y * CANVAS_WIDTH + x];
            }
        }
    }
}

void drawing_invert_colors(DrawingContext *ctx) {
    LayerData *layer = &ctx->layers[ctx->active_layer];
    for (int i = 0; i < CANVAS_WIDTH * CANVAS_HEIGHT; i++) {
        if (layer->pixels[i] == 0) layer->pixels[i] = 1;
        else if (layer->pixels[i] == 1) layer->pixels[i] = 0;
    }
}

// Undo/Redo
void drawing_save_undo(DrawingContext *ctx) {
    ctx->undo.current++;
    if (ctx->undo.current >= MAX_UNDO) {
        ctx->undo.current = 0;
    }
    
    CanvasState *state = &ctx->undo.states[ctx->undo.current];
    for (int i = 0; i < MAX_LAYERS; i++) {
        memcpy(&state->layers[i], &ctx->layers[i], sizeof(LayerData));
        state->layer_visible[i] = ctx->layer_visible[i];
    }
    state->active_layer = ctx->active_layer;
    
    ctx->undo.count = ctx->undo.current + 1;
    if (ctx->undo.count > MAX_UNDO) ctx->undo.count = MAX_UNDO;
}

void drawing_undo(DrawingContext *ctx) {
    if (ctx->undo.current <= 0) return;
    ctx->undo.current--;
    
    CanvasState *state = &ctx->undo.states[ctx->undo.current];
    for (int i = 0; i < MAX_LAYERS; i++) {
        memcpy(&ctx->layers[i], &state->layers[i], sizeof(LayerData));
        ctx->layer_visible[i] = state->layer_visible[i];
    }
    ctx->active_layer = state->active_layer;
}

void drawing_redo(DrawingContext *ctx) {
    if (ctx->undo.current >= ctx->undo.count - 1) return;
    ctx->undo.current++;
    
    CanvasState *state = &ctx->undo.states[ctx->undo.current];
    for (int i = 0; i < MAX_LAYERS; i++) {
        memcpy(&ctx->layers[i], &state->layers[i], sizeof(LayerData));
        ctx->layer_visible[i] = state->layer_visible[i];
    }
    ctx->active_layer = state->active_layer;
}

// Rendering
void drawing_render_canvas(DrawingContext *ctx) {
    // Sfondo canvas
    vita2d_draw_rectangle(CANVAS_X, CANVAS_Y, CANVAS_WIDTH, CANVAS_HEIGHT, COLOR_CANVAS_BG);
    
    // Griglia
    if (ctx->show_grid) {
        int grid_size = 16;
        for (int x = 0; x < CANVAS_WIDTH; x += grid_size) {
            vita2d_draw_line(CANVAS_X + x, CANVAS_Y, CANVAS_X + x, CANVAS_Y + CANVAS_HEIGHT, COLOR_GRID);
        }
        for (int y = 0; y < CANVAS_HEIGHT; y += grid_size) {
            vita2d_draw_line(CANVAS_X, CANVAS_Y + y, CANVAS_X + CANVAS_WIDTH, CANVAS_Y + y, COLOR_GRID);
        }
    }
    
    // Render layers dal basso verso l'alto
    for (int l = MAX_LAYERS - 1; l >= 0; l--) {
        if (!ctx->layer_visible[l]) continue;
        
        LayerData *layer = &ctx->layers[l];
        for (int y = 0; y < CANVAS_HEIGHT; y++) {
            for (int x = 0; x < CANVAS_WIDTH; x++) {
                uint8_t pix = layer->pixels[y * CANVAS_WIDTH + x];
                if (pix == 0 && l > 0) continue; // Trasparente per layer sopra
                if (pix == 0 && l == 0) continue; // Bianco è lo sfondo, già disegnato
                
                unsigned int color = drawing_get_rgba_color(pix, l);
                if (color != COLOR_TRANSPARENT && color != COLOR_WHITE) {
                    vita2d_draw_pixel(CANVAS_X + x, CANVAS_Y + y, color);
                }
            }
        }
    }
    
    // Selezione
    if (ctx->has_selection) {
        // Rettangolo tratteggiato di selezione
        unsigned int sel_color = RGBA8(0, 0, 0, 200);
        for (int x = ctx->sel_x; x < ctx->sel_x + ctx->sel_w; x += 4) {
            vita2d_draw_pixel(CANVAS_X + x, CANVAS_Y + ctx->sel_y, sel_color);
            vita2d_draw_pixel(CANVAS_X + x, CANVAS_Y + ctx->sel_y + ctx->sel_h, sel_color);
        }
        for (int y = ctx->sel_y; y < ctx->sel_y + ctx->sel_h; y += 4) {
            vita2d_draw_pixel(CANVAS_X + ctx->sel_x, CANVAS_Y + y, sel_color);
            vita2d_draw_pixel(CANVAS_X + ctx->sel_x + ctx->sel_w, CANVAS_Y + y, sel_color);
        }
    }
}

void drawing_render_onion_skin(DrawingContext *ctx, LayerData *prev_layers, LayerData *next_layers) {
    if (!ctx->onion_skin) return;
    
    // Render frame precedente in rosso semi-trasparente
    if (prev_layers) {
        for (int y = 0; y < CANVAS_HEIGHT; y++) {
            for (int x = 0; x < CANVAS_WIDTH; x++) {
                uint8_t pix = prev_layers[0].pixels[y * CANVAS_WIDTH + x];
                if (pix != 0) {
                    vita2d_draw_pixel(CANVAS_X + x, CANVAS_Y + y, COLOR_ONION_PREV);
                }
            }
        }
    }
    
    // Render frame successivo in blu semi-trasparente
    if (next_layers) {
        for (int y = 0; y < CANVAS_HEIGHT; y++) {
            for (int x = 0; x < CANVAS_WIDTH; x++) {
                uint8_t pix = next_layers[0].pixels[y * CANVAS_WIDTH + x];
                if (pix != 0) {
                    vita2d_draw_pixel(CANVAS_X + x, CANVAS_Y + y, COLOR_ONION_NEXT);
                }
            }
        }
    }
}
