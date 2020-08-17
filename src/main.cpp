#include <iostream>
#include <thread>

#include "HAPAvFormatForgeRenderer.h"

#ifdef __APPLE__
#import <Cocoa/cocoa.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#endif


using namespace std;
using namespace std::chrono;

static double currentMS()
{
    duration<double, milli> time_span = duration_cast<duration<double, milli>>(system_clock::now().time_since_epoch());
    return time_span.count();
}

#ifdef __APPLE__

static inline bool handlePlatformEvents()
{
    NSEvent* event;
    bool quit = false;
    do {
        event = [NSApp nextEventMatchingMask: NSEventMaskAny
                                   untilDate: nil
                                      inMode: NSDefaultRunLoopMode
                                     dequeue: YES];
        //TODO: handle quit events
        switch ([event type]) {
            default:
                [NSApp sendEvent: event];
        }
    } while (event != nil);
    return quit;
}
#elif WIN32

#include <windows.h>

static inline bool handlePlatformEvents()
{
    MSG msg;
    msg.message = NULL;
    bool quit = false;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);

        if (WM_CLOSE == msg.message || WM_QUIT == msg.message)
            quit = true;
    }
    return quit;
}
#else
static inline bool handlePlatformEvents()
{
    return false;
}
#endif


int main(int argc, char** argv)
{
    // Get file path to open
    if (argc != 2) {
        cout << "Requires exactly one parameter: the file path of the movie to playback";
        return -1;
    }
    char* filepath = argv[1];

    // Initialize AV Codec / Format
    #if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(58, 9, 100)
        av_register_all();
    #endif
    avformat_network_init();
    AVFormatContext* pFormatCtx = avformat_alloc_context();

    // Open file
    if(avformat_open_input(&pFormatCtx,filepath,NULL,NULL)!=0){
        fprintf(stderr, "Couldn't open input stream: %s.\n", filepath);
        return -1;
    }
    if(avformat_find_stream_info(pFormatCtx,NULL)<0){
        fprintf(stderr, "Couldn't find stream information.\n");
        return -1;
    }
    int videoindex=-1;
    for(unsigned int i=0; i<pFormatCtx->nb_streams; i++)
    {
        if(pFormatCtx->streams[i]->codecpar->codec_type==AVMEDIA_TYPE_VIDEO){
            videoindex=i;
            break;
        }
    }
    if(videoindex==-1){
        fprintf(stderr, "Didn't find a video stream.\n");
        return -1;
    }

    AVCodecParameters* pCodecParams=pFormatCtx->streams[videoindex]->codecpar;
    if (pCodecParams->codec_id != AV_CODEC_ID_HAP) {
        fprintf(stderr, "This app only playbacks HAP movies.\n");
        return -1;
    }

    // Output Info-----------------------------
    fprintf(stderr, "--------------- File Information ----------------\n");
    av_dump_format(pFormatCtx,0,filepath,0);

    HAPAvFormatForgeRenderer hapAvFormatRenderer;

    // Initialize The forge renderer
    std::cout << "step 1" << std::endl;
    if (hapAvFormatRenderer.initRenderer())
    {
        fprintf(stderr, "Could not initialize The forge - %s\n", hapAvFormatRenderer.get_error());
        return -1;
    }

    // Open window as needed
    std::cout << "step 2" << std::endl;
    if (hapAvFormatRenderer.openWindow("Simple ffmpeg player",
                                       pCodecParams->width, pCodecParams->height))
    {
        fprintf(stderr, "Could not open window - %s\n", hapAvFormatRenderer.get_error());
        return -1;
    }

    // Load rendering resources
    std::cout << "step 4" << std::endl;
    hapAvFormatRenderer.readCodecParams(pCodecParams);

    std::cout << "step 3" << std::endl;
    if (hapAvFormatRenderer.createContext())
    {
        fprintf(stderr, "Could not create context - %s\n", hapAvFormatRenderer.get_error());
        return -1;
    }


    // Loop playing back frames until user ask to close the window
    AVPacket packet;
    bool shouldQuit = false;
    double lastFrameTimeMs = currentMS();
    while (!shouldQuit) {
        while (!shouldQuit && av_read_frame(pFormatCtx, &packet)>=0){
            if(packet.stream_index==videoindex){
                int64_t den = pFormatCtx->streams[packet.stream_index]->time_base.den;
                int64_t num = pFormatCtx->streams[packet.stream_index]->time_base.num;
                int64_t frameDuration = av_rescale(packet.duration, AV_TIME_BASE * num, den);

                // Display new frame in openGL backbuffer
                double preRender = currentMS();
                hapAvFormatRenderer.renderFrame(&packet,lastFrameTimeMs);
                double postRender = currentMS();
                std::cout << "render took " << postRender - preRender << "ms\n";

                // Keep showing previous frame depending on movie FPS (see frameDurationMs below)
                double frameDurationMs = frameDuration / 1000.0;
                double frameEndTimeMs = lastFrameTimeMs + frameDurationMs;

                // Sleep until we reach frame end time
                shouldQuit = handlePlatformEvents();
                while (currentMS() < frameEndTimeMs) {
                    std::this_thread::sleep_for(std::chrono::nanoseconds(300));
                }
                lastFrameTimeMs = frameEndTimeMs;

                // Now swap OpenGL Backbuffer to front
//                SDL_GL_SwapWindow(window);
            }
            av_packet_unref(&packet);
        }

        // Loop - seek back to first frame
        av_seek_frame(pFormatCtx,-1,0,AVSEEK_FLAG_BACKWARD);
    }

    // Free resources - remark: should free OpenGL resources allocated in HAPAvFormatOpenGLRenderer
//    SDL_Quit();
    avformat_close_input(&pFormatCtx);

    return 0;
}
