#include "video.h"
#include "cga.h"
#include <SDL2/SDL.h>
#include <stdio.h>

static SDL_Window *g_window = NULL;
static SDL_Renderer *g_renderer = NULL;
static SDL_Texture *g_texture = NULL;
static uint32_t g_pixels[CGA_WIDTH * CGA_HEIGHT];

/* CGA palette 1, high intensity (background black is index 0) */
static const uint32_t palette[4] = {
    0xFF000000u, /* 0: black   */
    0xFF55FFFFu, /* 1: cyan    */
    0xFFFF55FFu, /* 2: magenta */
    0xFFFFFFFFu, /* 3: white   */
};

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
