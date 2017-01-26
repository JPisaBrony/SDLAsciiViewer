#include <stdio.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <caca.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>

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
int font_size = 14;
uint16_t bg, fg;
SDL_Color text_color;
SDL_Color background_color;
SDL_Rect pos;
char ch[2];
uint32_t *chars, *attrs;
float zoom = 1;

char *imageName = "twi.gif";

int main(int argc, char* args[]) {
    int i, j, k, videoStream = -1, frameFinished, numBytes;
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

    // register all file formats
    av_register_all();

    // open up file
    if(avformat_open_input(&pFormatCtx, imageName, NULL, NULL) != 0) {
        printf("Failed to open file");
        exit(-1);
    }

    // get stream info
    if(avformat_find_stream_info(pFormatCtx, NULL) < 0) {
        printf("Failed to get stream info");
        exit(-1);
    }

    // iterate through stream to find video
    for(i = 0; i < pFormatCtx->nb_streams; i++) {
        if(pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStream = i;
            break;
        }
    }

    // check if we found the video stream
    if(videoStream == -1) {
        printf("Couldn't initialize stream");
        exit(-1);
    }

    // get codec context from the video stream
    pCodecCtxOrig = pFormatCtx->streams[videoStream]->codec;

    // find decoder
    pCodec = avcodec_find_decoder(pCodecCtxOrig->codec_id);
    if(pCodec == NULL) {
        // failed to find video codec
        printf("Couldn't find codec");
        exit(-1);
    }

    // copy context
    pCodecCtx = avcodec_alloc_context3(pCodec);
    if(avcodec_copy_context(pCodecCtx, pCodecCtxOrig) != 0) {
        printf("Couldn't copy Codec Context");
        exit(-1);
    }

    // open codec
    if(avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
        printf("Couldn't open Codec");
        exit(-1);
    }

    // allocate video frame
    pFrame = av_frame_alloc();
    if(pFrame == NULL) {
        printf("Couldn't allocate pFrame");
        exit(-1);
    }

    // allocate RGBA video frame
    pFrameRGBA = av_frame_alloc();
    if(pFrameRGBA == NULL) {
        printf("Couldn't allocate pFrameRGBA");
        exit(-1);
    }

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

    if(SDL_Init(SDL_INIT_EVERYTHING) == -1) {
        printf("Couldn't init SDL");
        exit(-1);
    }

    window = SDL_CreateWindow("SDL Ascii Viewer", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);

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

    // get screen surface
    screen = SDL_GetWindowSurface(window);
    cv = caca_create_canvas(screen->w / font_size, screen->h / font_size);
    caca_set_color_ansi(cv, CACA_WHITE, CACA_BLACK);
    caca_put_str(cv, 0, 0, "caca failed");

    // create dither
    dither = caca_create_dither(32, pCodecCtx->width, pCodecCtx->height, pCodecCtx->width * 4, 0x0000ff, 0x00ff00, 0xff0000, 0);

    while(1) {
        while(SDL_PollEvent(&event)) {
            if(event.type == SDL_QUIT) {
                caca_free_canvas(cv);
                //SDL_FreeSurface(img);
                SDL_Quit();
                return 0;
            } else if(event.type == SDL_MOUSEWHEEL) {
                if(event.wheel.y > 0)
                    zoom += 0.025;
                if(event.wheel.y < 0)
                    zoom -= 0.025;
            } else if (event.type == SDL_KEYDOWN) {
                switch(event.key.keysym.sym) {
                    case 'q':
                        caca_free_canvas(cv);
                        //SDL_FreeSurface(img);
                        SDL_Quit();
                        return 0;
                    break;
                }
            }
        }

        while(av_read_frame(pFormatCtx, &packet) >= 0) {
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
                    caca_dither_bitmap(cv, 0, 0, (screen->w / font_size) * zoom, (screen->h / font_size) * zoom, dither, pFrameRGBA->data[0]);
                    // get libcaca internal state
                    chars = caca_get_canvas_chars(cv);
                    attrs = caca_get_canvas_attrs(cv);

                    pos.x = 0;
                    pos.y = 0;
                    for(i = 0; i < screen->h / font_size; i++) {
                        for(j = 0; j < screen->w / font_size; j++) {
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
                            text->w = font_size;
                            text->h = font_size;
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
        }

        // reset to beginning of file
        av_seek_frame(pFormatCtx, 0, NULL, 0);
    }

    // cleanup
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
    return 0;
}
