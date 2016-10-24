#include <stdio.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <caca.h>

#define SCREEN_WIDTH 1280	// initial width  of the program window.
#define SCREEN_HEIGHT 900	// initial height of the program window.
#define CACA_CHAR_SIZE 100

caca_canvas_t *cv = NULL;
SDL_Event event;
SDL_Surface *caca_view = NULL;
SDL_Window *window = NULL;
SDL_Surface *screen = NULL;
char caca_screen[CACA_CHAR_SIZE][CACA_CHAR_SIZE];
char caca_screen_attr[CACA_CHAR_SIZE][CACA_CHAR_SIZE];
SDL_Surface *text = NULL;
SDL_Surface *img[16];
caca_dither_t *dither = NULL;
int font_size = 10;
uint16_t bg, fg;
SDL_Color text_color;
SDL_Color background_color;
SDL_Rect pos;
char ch[2];
uint32_t *chars, *attrs;

int main(int argc, char* args[]) {
    int i, j, k;

    if(SDL_Init(SDL_INIT_EVERYTHING) == -1) {
        printf("Couldn't init SDL");
        exit(-1);
    }

    window = SDL_CreateWindow("CACA", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);

    if(window == NULL) {
        printf("Couldn't init SDL Window");
        exit(-1);
    }

    if(TTF_Init() == -1) {
        printf("Couldn't init SDL TTF");
        exit(-1);
    }

    if(IMG_Init(IMG_INIT_PNG) == -1) {
        printf("Couldn't init SDL Image");
        exit(-1);
    }

    TTF_Font *font = TTF_OpenFont("FreeMonoBold.ttf", font_size);

    cv = caca_create_canvas(CACA_CHAR_SIZE, CACA_CHAR_SIZE);
    caca_set_color_ansi(cv, CACA_WHITE, CACA_BLACK);
    caca_put_str(cv, 0, 0, "caca failed");

    char buffer[100];
    for(i = 0; i < 16; i++) {
        snprintf(buffer, sizeof(buffer), "pics/twi-%d.png", i);
        img[i] = IMG_Load(buffer);
        img[i] = SDL_ConvertSurfaceFormat(img[i], SDL_PIXELFORMAT_RGBA8888, 0);
    }

    while(1) {
        while(SDL_PollEvent(&event)) {
            if(event.type == SDL_QUIT) {
                SDL_Quit();
                return 0;
            } else if(event.type == SDL_MOUSEWHEEL) {
                if(event.wheel.y > 0)
                    font_size += 2;
                if(event.wheel.y < 0)
                    font_size -= 2;
            }
        }

        for(k = 0; k < 16; k++) {
            //Get window surface
            screen = SDL_GetWindowSurface(window);

            //Fill background with black
            SDL_FillRect(screen, NULL, 0x000000);

            caca_clear_canvas(cv);

            dither = cucul_create_dither(32, CACA_CHAR_SIZE, CACA_CHAR_SIZE, img[k]->pitch, img[k]->format->Rmask, img[k]->format->Gmask, img[k]->format->Bmask, img[k]->format->Amask);
            caca_dither_bitmap(cv, 0, 0, CACA_CHAR_SIZE, CACA_CHAR_SIZE, dither, img[k]->pixels);

            chars = caca_get_canvas_chars(cv);
            attrs = caca_get_canvas_attrs(cv);

            pos.x = 0;
            pos.y = 0;
            for(i = 0; i < CACA_CHAR_SIZE; i++) {
                for(j = 0; j < CACA_CHAR_SIZE; j++) {
                    ch[0] = *chars++;
                    ch[1] = '\0';
                    bg = caca_attr_to_rgb12_bg(*attrs);
                    fg = caca_attr_to_rgb12_fg(*attrs);
                    text_color.r = ((fg & 0xf00) >> 8) * 8;
                    text_color.g = ((fg & 0x0f0) >> 4) * 8;
                    text_color.b = (fg & 0x00f) * 8;
                    text_color.a = 0xff;
                    background_color.r = ((bg & 0xf00) >> 8) * 8;
                    background_color.g = ((bg & 0x0f0) >> 4) * 8;
                    background_color.b = (bg & 0x00f) * 8;
                    background_color.a = 0xff;
                    text = TTF_RenderText_Shaded(font, &ch, text_color, background_color);
                    SDL_BlitSurface(text, NULL, screen, &pos);
                    SDL_FreeSurface(text);
                    pos.x += font_size;
                    attrs++;
                }
                pos.x = 0;
                pos.y += font_size;
            }

            //Update the window
            SDL_UpdateWindowSurface(window);
        }
    }

    caca_free_canvas(cv);
    SDL_FreeSurface(img);
    SDL_Quit();
    return 0;
}
