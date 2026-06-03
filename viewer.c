/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Michael Novotny
 * See LICENSE for full terms.
 *
 * ppm-viewer — a minimal PPM image viewer in C using SDL2.
 */

#include <stdio.h>    /* printf, fprintf, fscanf, fopen, fclose, fread, fgetc */
#include <stdlib.h>   /* malloc, free, exit */
#include <string.h>   /* strcmp, strcpy, strncpy */
#include <stdbool.h>  /* bool, true, false */
#include <SDL2/SDL.h> /* everything SDL2: window, renderer, texture, events */

/* Stores the width and height of the image in pixels */
typedef struct {
    int width;
    int height;
} Dimensions;

/* Stores the metadata read from the PPM file header */
typedef struct {
    char magicIdent[3]; /* PPM format identifier, e.g. "P6" — needs 3 bytes for null terminator */
    Dimensions dims;    /* width and height of the image */
    int maxVal;         /* maximum color value, must be 255 for standard PPM */
} ImgInfo;

/* Represents a single pixel with red, green, and blue color channels */
typedef struct {
    int r, g, b;
} Pixel;

/* Shown to the user when they pass the wrong number of arguments */
const char* g_usage_str = "Usage: ./viewer <image.ppm>\n";

/* Forward declarations — tell the compiler these functions exist before main sees them */
void checkArgs(int argc, char* argv[], char* filename, int length);
Pixel* loadImage(const char* filename, ImgInfo* img);
int initSDL(const char* title, int width, int height, SDL_Window** window, SDL_Renderer** renderer);
SDL_Texture* createTexture(SDL_Renderer* renderer, Pixel* p, ImgInfo* img);
void runEventLoop(SDL_Renderer* renderer, SDL_Texture* texture);

int main(int argc, char* argv[]) {
    /* Validate arguments and save the filename*/
    char filename[50];
    checkArgs(argc, argv, filename, sizeof(filename));

    /* img will be filled in by loadImage with the PPM header data */
    ImgInfo img = {0};

    /* Load the image from disk — returns a heap-allocated array of Pixels, or NULL on failure */
    Pixel* p = loadImage(filename, &img);
    if (!p) return 1; /* loadImage already printed the error */

    /* SDL needs a pointer-to-pointer so initSDL can write the address back to us */
    SDL_Window* window = NULL;
    SDL_Renderer* renderer = NULL;
    if (initSDL(filename, img.dims.width, img.dims.height, &window, &renderer) != 0) {
        free(p); /* clean up the pixel data before exiting */
        return 1;
    }

    /* Upload the pixel array to the GPU as a texture — after this we don't need p anymore */
    SDL_Texture* texture = createTexture(renderer, p, &img);
    free(p); /* always free heap memory when we're done with it */
    if (!texture) {
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    /* Hand off to the event loop — this blocks until the user closes the window */
    runEventLoop(renderer, texture);

    /* Clean up all SDL resources in reverse order of creation */
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit(); /* shuts down all SDL subsystems */
    return 0;
}

/*
 * loadImage — opens a PPM file, parses its header, reads all pixels into a
 * heap-allocated array, and returns it. Returns NULL on any error.
 * The caller is responsible for freeing the returned pointer.
 */
Pixel* loadImage(const char* filename, ImgInfo* img) {
    /* Open the file in binary mode — PPM pixel data is raw bytes, not text */
    FILE* pImg = fopen(filename, "rb");
    if (!pImg) {
        fprintf(stderr, "Error: File could not be opened.\n");
        return 0;
    }

    /*
     * Read the PPM header. A P6 header looks like:
     *   P6
     *   <width> <height>
     *   <maxval>
     * fscanf returns the number of items successfully read — we expect exactly 4.
     */
    if (fscanf(pImg, "%2s\n%d %d\n%d", img->magicIdent, &img->dims.width, &img->dims.height, &img->maxVal) != 4) {
        fprintf(stderr, "Error: Malformed PPM header.\n");
        fclose(pImg);
        return 0;
    }

    /* After the header there's a single whitespace byte before the pixel data starts */
    fgetc(pImg);

    /* P6 is binary PPM — P3 (ASCII) is not supported */
    if (strcmp(img->magicIdent, "P6")) {
        fprintf(stderr, "Error: Magic Identifier is invalid.\n");
        fclose(pImg);
        return 0;
    }
    else if (img->maxVal != 255) {
        /* We only handle 8-bit color — 16-bit PPM (maxVal 65535) would need different handling */
        fprintf(stderr, "Error: Color Max Value is invalid.\n");
        fclose(pImg);
        return 0;
    }

    /* Total number of pixels = width * height */
    int pixelAmount = img->dims.height * img->dims.width;

    /* Allocate enough memory to hold every pixel */
    Pixel* p = malloc(pixelAmount * sizeof(Pixel));
    if (!p) {
        fprintf(stderr, "Error: Memory allocation failed.\n");
        fclose(pImg);
        return 0;
    }

    for (int i = 0; i < pixelAmount; i++) {
        unsigned char rgb[3]; /* temporary buffer: 1 byte each for R, G, B */

        /* fread returns the number of elements read — if it's not 3, the file is truncated */
        if (fread(rgb, 1, 3, pImg) != 3) {
            fprintf(stderr, "Error: File is truncated at pixel %d.\n", i);
            free(p);
            fclose(pImg);
            return 0;
        }

        /* Copy the raw bytes into our Pixel struct */
        p[i].r = rgb[0];
        p[i].g = rgb[1];
        p[i].b = rgb[2];
    }

    fclose(pImg);
    return p; /* caller must free this */
}

/*
 * initSDL — initializes SDL2, creates a window and hardware-accelerated renderer.
 * Writes the created window and renderer into the provided pointers.
 * Returns 0 on success, 1 on failure (SDL resources are cleaned up internally).
 */
int initSDL(const char* title, int width, int height, SDL_Window** window, SDL_Renderer** renderer) {
    /* SDL_INIT_VIDEO starts the video subsystem — required for windows and rendering */
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "Failed to initialize SDL2\n");
        fprintf(stderr, "SDL2 Error: %s\n", SDL_GetError());
        return 1;
    }

    /* Create the OS window — SDL_WINDOWPOS_CENTERED means center it on screen */
    *window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, 0);
    if (!*window) {
        fprintf(stderr, "Failed to create window\n");
        fprintf(stderr, "SDL2 Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    /* Create a renderer tied to the window — SDL_RENDERER_ACCELERATED uses the GPU */
    *renderer = SDL_CreateRenderer(*window, -1, SDL_RENDERER_ACCELERATED);
    if (!*renderer) {
        fprintf(stderr, "Failed to create renderer\n");
        fprintf(stderr, "SDL2 Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(*window);
        SDL_Quit();
        return 1;
    }

    return 0;
}

/*
 * createTexture — converts the pixel array into an SDL_Texture stored on the GPU.
 * Once uploaded, the CPU-side pixel array is no longer needed.
 * Returns NULL on failure.
 */
SDL_Texture* createTexture(SDL_Renderer* renderer, Pixel* p, ImgInfo* img) {
    int pixelAmount = img->dims.height * img->dims.width;

    /*
     * SDL_PIXELFORMAT_RGB24 = 3 bytes per pixel (R, G, B) with no padding.
     * SDL_TEXTUREACCESS_STATIC = uploaded once and not changed.
     */
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STATIC, img->dims.width, img->dims.height);
    if (!texture) {
        fprintf(stderr, "Failed to create texture\n");
        fprintf(stderr, "SDL2 Error: %s\n", SDL_GetError());
        return 0;
    }

    /* SDL_UpdateTexture expects a flat byte array in RGB order, not our Pixel structs */
    unsigned char* pixels = malloc(pixelAmount * 3); /* 3 bytes per pixel */
    if (!pixels) {
        fprintf(stderr, "Error: Memory allocation failed.\n");
        SDL_DestroyTexture(texture);
        return 0;
    }

    /* Pack our Pixel structs into the flat byte array SDL expects */
    for (int i = 0; i < pixelAmount; i++) {
        pixels[i * 3 + 0] = p[i].r;
        pixels[i * 3 + 1] = p[i].g;
        pixels[i * 3 + 2] = p[i].b;
    }

    /* Upload pixel data to the GPU — pitch is the number of bytes per row */
    SDL_UpdateTexture(texture, NULL, pixels, img->dims.width * 3);
    free(pixels); /* GPU has a copy now, we don't need the CPU copy anymore */
    return texture;
}

/*
 * runEventLoop — draws the texture and processes window events until the user closes the window.
 * The image is already on the GPU, so each frame is just a blit — very efficient.
 */
void runEventLoop(SDL_Renderer* renderer, SDL_Texture* texture) {
    bool keep_window_open = true;
    while (keep_window_open) {
        /* Copy the texture to the renderer's back buffer (NULL = entire texture, entire window) */
        SDL_RenderCopy(renderer, texture, NULL, NULL);

        /* Swap back buffer to screen — this is when the image actually appears */
        SDL_RenderPresent(renderer);

        /* Process all pending OS events (keyboard, mouse, window close, etc.) */
        SDL_Event e;
        while (SDL_PollEvent(&e) > 0) {
            switch (e.type) {
                case SDL_QUIT: /* user clicked the X or pressed Alt+F4 */
                    keep_window_open = false;
                    break;
            }
        }
    }
}

/*
 * checkArgs — validates the number of command-line arguments.
 * Defaults to "image.ppm" if no argument is given.
 */
void checkArgs(int argc, char* argv[], char* filename, int size) {
    if (argc > 2) {
        fprintf(stderr, "Error: Incorrect amount of arguments given.\n");
        printf("%s", g_usage_str);
        exit(1);
    }
    else if (argc == 1) strcpy(filename, "image.ppm"); /* default filename */
    else {
        int length = strlen(argv[1]);
        /* size - 1 to leave room for the null terminator */
        if (length > size - 1) {
            fprintf(stderr, "Error: Filename too long.\n");
            exit(1);
        }
        /* copy length + 1 to include the null terminator from argv[1] */
        strncpy(filename, argv[1], length + 1);
        filename[length] = '\0'; /* strncpy won't null-terminate if src is too long */
    }
}
