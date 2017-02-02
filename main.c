#include <stdio.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <caca.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include <dirent.h>
#include <stdlib.h>
#include <time.h>

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 900

caca_canvas_t *cv = NULL;
SDL_Event event;
SDL_Surface *caca_view = NULL;
SDL_Window *window = NULL;
SDL_Surface *screen = NULL;
SDL_Surface *text = NULL;
SDL_Texture *texture = NULL;
SDL_Renderer *renderer = NULL;
caca_dither_t *dither = NULL;
int font_size = -1, font_width, font_height;
uint16_t bg, fg;
SDL_Color text_color;
SDL_Color background_color;
SDL_Rect pos;
char ch[2];
uint32_t *chars, *attrs;
float zoom = 1;
AVFormatContext *pFormatCtx = NULL;
AVCodecContext *pCodecCtxOrig = NULL;
AVCodecContext *pCodecCtx = NULL;
AVCodecParameters *codecparams = NULL;
AVCodec *pCodec = NULL;
AVFrame *pFrame = NULL;
AVFrame *pFrameRGBA = NULL;
uint8_t *imageBuffer = NULL;
struct SwsContext *sws_ctx = NULL;
AVPacket packet;
int videoStream = -1, frameFinished, numBytes, stream_index, start_time, full_screen = -1, sound = -1;

DIR *dir;
int num_pics = 0;
struct dirent *ent;
char* filename;
char** pictures;
char *image_name;

char *base_path = NULL;
char *font_name = NULL;

void exit_msg(char *msg) {
    printf(msg);
    exit(-1);
}

void cleanup() {
    free(base_path);
    free(font_name);
    av_free(imageBuffer);
    av_free(pFrame);
    av_free(pFrameRGBA);
    av_free_packet(&packet);
    avcodec_close(pCodecCtx);
    avcodec_close(pCodecCtxOrig);
    avformat_close_input(&pFormatCtx);
    caca_free_canvas(cv);
    caca_free_dither(dither);
    SDL_Quit();
}

void read_config() {
    char ch, *line, *split;
    int i = 0, line_length = 255, line_end_flag = 0, num_lines = 0;

    FILE *fp = NULL;

    // check if the file exists
    if(access("config.txt", F_OK) != -1) {
        // file does exist to read in configuration settings
        fp = fopen("config.txt", "r");
        // make sure we opened the file
        if(fp == NULL)
            exit_msg("Failed to open config.txt");

        // allocate memory
        line = malloc(sizeof(char) * line_length);

        // read the whole file
        while(fgets(line, line_length + 1, fp)) {
            // check if the line is a new line character
            if(*line != '\n') {
                // split on : character
                split = strtok(line, "=");
                // check the config setting name to set the proper variable
                if(strcmp(split, "font_size") == 0) {
                    split = strtok(NULL, "=");
                    font_size = atoi(split);
                }
                if(strcmp(split, "base_path") == 0) {
                    split = strtok(NULL, "=");
                    split = strtok(split, "\n");
                    base_path = malloc(sizeof(char) * 256);
                    strcpy(base_path, split);
                }
                if(strcmp(split, "font_name") == 0) {
                    split = strtok(NULL, "=");
                    split = strtok(split, "\n");
                    font_name = malloc(sizeof(char) * 256);
                    strcpy(font_name, split);
                }
                if(strcmp(split, "full_screen") == 0) {
                    split = strtok(NULL, " ");
                    full_screen = atoi(split);
                }
                if(strcmp(split, "sound") == 0) {
                    split = strtok(NULL, " ");
                    sound = atoi(split);
                }

                i++;
            }
        }

        // check to make sure that everything got set to something
        if(font_size == -1 || font_size == 0)
            font_size = 14;

        if(base_path == NULL)
            base_path = "pics\\";

        if(font_name == NULL)
            font_name = "FreeMonoBold.ttf";

        if(full_screen == -1)
            full_screen = 1;

        if(sound == -1)
            sound = 0;
    } else {
        // file doesn't exist so write default configuration
        // and use default values
        fp = fopen("config.txt", "w");
         // make sure we opened the file
        if(fp == NULL)
            exit_msg("Failed to open config.txt");
        // write default settings to file
        fprintf(fp, "font_size=14\n");
        fprintf(fp, "base_path=pics\\\n");
        fprintf(fp, "font_name=FreeMonoBold.ttf\n");
        fprintf(fp, "full_screen=1\n");
        fprintf(fp, "sound=0\n");
        // set default settings for variables
        font_size = 14;
        base_path = "pics\\";
        font_name = "FreeMonoBold.ttf";
        full_screen = 1;
        sound = 0;
    }

    // cleanup
    fclose(fp);
    free(line);
}

int main(int argc, char* args[]) {
    int i, j;

    read_config();

    pictures = (char**)malloc(sizeof(char*) * 256);
    if ((dir = opendir(base_path)) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            filename = (char*)ent->d_name;
            if (strcmp(filename,".") != 0 && strcmp(filename,"..") != 0) {
                pictures[num_pics] = (char*)malloc(sizeof(char) * 256);
                strcpy(pictures[num_pics], filename);
                num_pics++;
            }
        }
    } else {
        exit_msg("Failed to open folder");
    }

    srand(time(NULL));
    int random = rand() % num_pics;
    char *img = pictures[random];

    if ((image_name = malloc(strlen(base_path) + strlen(img) + 1)) != NULL) {
        image_name[0] = '\0';
        strcat(image_name, base_path);
        strcat(image_name, img);
    } else {
        exit_msg("Failed to append strs - could not malloc.");
    }

    // register all file formats
    av_register_all();

    // open up file
    if(avformat_open_input(&pFormatCtx, image_name, NULL, NULL) != 0)
        exit_msg("Failed to open file");

    // get stream info
    if(avformat_find_stream_info(pFormatCtx, NULL) < 0)
        exit_msg("Failed to get stream info");

    // iterate through stream to find video
    for(i = 0; i < pFormatCtx->nb_streams; i++) {
        if(pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStream = i;
            break;
        }
    }

    // check if we found the video stream
    if(videoStream == -1)
        exit_msg("Couldn't initialize stream");

    // get codec context from the video stream
    pCodecCtxOrig = pFormatCtx->streams[videoStream]->codec;

    // find decoder
    pCodec = avcodec_find_decoder(pCodecCtxOrig->codec_id);
    if(pCodec == NULL)
        exit_msg("Couldn't find codec");

    // copy context
    pCodecCtx = avcodec_alloc_context3(pCodec);
    if(avcodec_copy_context(pCodecCtx, pCodecCtxOrig) != 0)
        exit_msg("Couldn't copy Codec Context");

    // open codec
    if(avcodec_open2(pCodecCtx, pCodec, NULL) < 0)
        exit_msg("Couldn't open Codec");

    // allocate video frame
    pFrame = av_frame_alloc();
    if(pFrame == NULL)
        exit_msg("Couldn't allocate pFrame");

    // allocate RGBA video frame
    pFrameRGBA = av_frame_alloc();
    if(pFrameRGBA == NULL)
        exit_msg("Couldn't allocate pFrameRGBA");

    // determine required buffer size and allocate buffer
    numBytes = avpicture_get_size(AV_PIX_FMT_RGBA, pCodecCtx->width, pCodecCtx->height);
    imageBuffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));
    avpicture_fill((AVPicture *)pFrameRGBA, imageBuffer, AV_PIX_FMT_RGBA, pCodecCtx->width, pCodecCtx->height);

    // setup context
    sws_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height,
            pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height,
            AV_PIX_FMT_RGBA,
            SWS_BILINEAR,
            NULL,
            NULL,
            NULL);

    if(SDL_Init(SDL_INIT_EVERYTHING) == -1)
        exit_msg("Couldn't init SDL");

    // setup SDL window
    window = SDL_CreateWindow("SDL Ascii Viewer", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    // set window as full screen
    if(full_screen)
        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);

    if(window == NULL)
        exit_msg("Couldn't init SDL Window");

    if(TTF_Init() == -1)
        exit_msg("Couldn't init SDL TTF");

    if(IMG_Init(IMG_INIT_PNG) == -1)
        exit_msg("Couldn't init SDL Image");

    // open font
    TTF_Font *font = TTF_OpenFont(font_name, font_size);
    if(font == NULL)
        exit_msg("Failed to open font");
    // get text size
    TTF_SizeText(font, "a", &font_width, &font_height);

    // get screen surface
    screen = SDL_GetWindowSurface(window);
    cv = caca_create_canvas(screen->w / font_width, screen->h / font_height);
    caca_set_color_ansi(cv, CACA_WHITE, CACA_BLACK);
    caca_put_str(cv, 0, 0, "caca failed");

    // create dither
    dither = caca_create_dither(32, pCodecCtx->width, pCodecCtx->height, pCodecCtx->width * 4, 0x0000ff, 0x00ff00, 0xff0000, 0);

    while(1) {
        while(av_read_frame(pFormatCtx, &packet) >= 0) {
            while(SDL_PollEvent(&event)) {
                if(event.type == SDL_QUIT) {
                    cleanup();
                    return 0;
                } else if(event.type == SDL_MOUSEWHEEL) {
                    if(event.wheel.y > 0)
                        zoom += 0.025;
                    if(event.wheel.y < 0)
                        zoom -= 0.025;
                } else if (event.type == SDL_KEYDOWN) {
                    switch(event.key.keysym.sym) {
                        case 'q':
                            cleanup();
                            return 0;
                        break;
                    }
                }
            }

            if(packet.stream_index == videoStream) {
                avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);

                if(frameFinished) {
                    // convert current frame into a ARGB frame
                    sws_scale(sws_ctx, (uint8_t const * const *)pFrame->data,
                        pFrame->linesize, 0, pCodecCtx->height,
                        pFrameRGBA->data, pFrameRGBA->linesize);

                    //Fill background with black
                    SDL_FillRect(screen, NULL, 0x000000);

                    // clear canvas
                    caca_clear_canvas(cv);
                    // dither the frame
                    caca_dither_bitmap(cv, 0, 0, (screen->w / font_width) * zoom, (screen->h / font_height) * zoom, dither, pFrameRGBA->data[0]);
                    // get libcaca internal state
                    chars = caca_get_canvas_chars(cv);
                    attrs = caca_get_canvas_attrs(cv);

                    pos.x = 0;
                    pos.y = 0;
                    for(i = 0; i < screen->h / font_height; i++) {
                        for(j = 0; j < screen->w / font_width; j++) {
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
                            pos.x += font_width;
                            attrs++;
                        }
                        pos.x = 0;
                        pos.y += font_height;
                    }

                    //Update the window
                    SDL_UpdateWindowSurface(window);
                }
            }
            // free packet
            av_free_packet(&packet);
        }

        // reset to beginning of file
        stream_index = av_find_default_stream_index(pFormatCtx);
        start_time = pFormatCtx->streams[stream_index]->time_base.num;
        av_seek_frame(pFormatCtx, stream_index, start_time, AVSEEK_FLAG_BACKWARD);
    }

    // cleanup
    cleanup();
    return 0;
}
