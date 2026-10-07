#include "video.h"
#include "cga.h"
#include "palette.h"
#include <SDL2/SDL.h>
#include <stdio.h>

static SDL_Window *g_window = NULL;
static SDL_Renderer *g_renderer = NULL;
static SDL_Texture *g_texture = NULL;
static uint32_t g_pixels[CGA_WIDTH * CGA_HEIGHT];

bool video_init(int window_scale) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }
    g_window = SDL_CreateWindow("Alley Cat (SDL port, WIP)",
                                 SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                 CGA_WIDTH * window_scale, CGA_HEIGHT * window_scale,
                                 SDL_WINDOW_SHOWN);
    if (!g_window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }
    g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED);
    if (!g_renderer) {
        g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!g_renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        return false;
    }
    SDL_RenderSetLogicalSize(g_renderer, CGA_WIDTH, CGA_HEIGHT);
    g_texture = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_ARGB8888,
                                   SDL_TEXTUREACCESS_STREAMING, CGA_WIDTH, CGA_HEIGHT);
    if (!g_texture) {
        fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        return false;
    }
    return true;
}

/* Unpack cga_mem (2bpp, bank-interleaved) into g_pixels (32bpp, linear) */
static void unpack_cga(void) {
    /* Colores segun el estado de paleta del juego (palette.c: puerto 0x3D9 / registros PCjr), una vez por frame. */
    uint32_t palette[4];
    for (unsigned i = 0; i < 4; i++) palette[i] = palette_rgb(i);
    for (int row = 0; row < CGA_HEIGHT; row++) {
        size_t row_base = (size_t)(row >> 1) * CGA_BYTES_PER_ROW;
        if (row & 1) row_base += CGA_BANK_SIZE;
        uint32_t *out = &g_pixels[row * CGA_WIDTH];
        for (int col = 0; col < CGA_WIDTH; col++) {
            uint8_t byte = cga_mem[row_base + (col >> 2)];
            uint8_t shift = (uint8_t)(6 - ((col & 0x3) << 1));
            uint8_t idx = (uint8_t)((byte >> shift) & 0x3);
            out[col] = palette[idx];
        }
    }
}

void video_present(void) {
    unpack_cga();
    SDL_UpdateTexture(g_texture, NULL, g_pixels, CGA_WIDTH * (int)sizeof(uint32_t));
    {   /* borde: las franjas fuera de la imagen logica toman el color de fondo/borde de CGA (T78) */
        uint32_t b = palette_border_rgb();
        SDL_SetRenderDrawColor(g_renderer, (uint8_t)(b >> 16), (uint8_t)(b >> 8), (uint8_t)b, 0xff);
    }
    SDL_RenderClear(g_renderer);
    SDL_RenderCopy(g_renderer, g_texture, NULL, NULL);
    SDL_RenderPresent(g_renderer);
}

void video_shutdown(void) {
    if (g_texture) SDL_DestroyTexture(g_texture);
    if (g_renderer) SDL_DestroyRenderer(g_renderer);
    if (g_window) SDL_DestroyWindow(g_window);
    SDL_Quit();
}
