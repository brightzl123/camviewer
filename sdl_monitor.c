
#include <stdbool.h>
#include <SDL2/SDL.h>

#include "camviewer.h"


SDL_monitor * SDL_CreateContext(SDL_monitorEventsFunc events, int width, int height)
{
    SDL_monitor *monitor = NULL;

    monitor = malloc(sizeof(SDL_monitor));
    if (monitor == NULL) {
        return NULL;
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        goto err_cleanup;
    }

    monitor->window = SDL_CreateWindow("SDL_monitor",
            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
			width, height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (monitor->window == NULL) {
        goto err_cleanup;
    }

    monitor->renderer = SDL_CreateRenderer(monitor->window,
            -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (monitor->renderer == NULL) {
        goto err_cleanup;
    }

    monitor->texture = SDL_CreateTexture(monitor->renderer, SDL_PIXELFORMAT_YUY2,
			SDL_TEXTUREACCESS_STREAMING, width, height);
    if (monitor->texture == NULL) {
        goto err_cleanup;
    }

    // frame pointer
    // memset(monitor->pixels, 0xFF, PICTURE_SIZE_HEIGHT * PICTURE_SIZE_WIDTH * 2);

    if (events == NULL) {
        goto err_cleanup;
    }

    monitor->events = events;

    monitor->rect.w = width;
    monitor->rect.h = height;

    monitor->width  = width;
    monitor->height = height;
    monitor->r_width  = width;
    monitor->r_height = height;

    return monitor;

err_cleanup:
    SDL_DeleteContext(monitor);
    return NULL;
}

void SDL_MainLoop(SDL_monitor *monitor)
{
	bool quit = false;
	while (quit != true) {
        quit = monitor->events(monitor);
    }
}

void SDL_DeleteContext(SDL_monitor *monitor)
{
    if (monitor != NULL) {
        SDL_DestroyTexture(monitor->texture);
        SDL_DestroyRenderer(monitor->renderer);
        SDL_DestroyWindow(monitor->window);
    }
    free(monitor);
}

void SDL_FrameRenderer(SDL_monitor *monitor)
{
	SDL_UpdateTexture(monitor->texture, NULL, monitor->pixels, monitor->r_width * 2);

	//
	monitor->rect.x = 0;
	monitor->rect.y = 0;
	monitor->rect.w = monitor->width;
	monitor->rect.h = monitor->height;

	SDL_RenderClear(monitor->renderer);
	SDL_RenderCopy(monitor->renderer, monitor->texture, NULL, &(monitor->rect));
	SDL_RenderPresent(monitor->renderer);
}
