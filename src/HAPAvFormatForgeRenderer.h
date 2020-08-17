#ifndef HAPAVFORMATFORGERENDERER_H
#define HAPAVFORMATFORGERENDERER_H

#include <assert.h>

extern "C"
{
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
}

#include <memory>

class HAPAvFormatForgeRenderer
{
public:
    HAPAvFormatForgeRenderer();
    ~HAPAvFormatForgeRenderer();

    int initRenderer();
    int openWindow(const char* title, int width, int height);
    int createContext();

    void readCodecParams(AVCodecParameters* codecParams);

    void renderFrame(AVPacket* packet, double msTime);

    const char* get_error();
    uint32_t get_error_code();

private:
    uint32_t error_code;

    void* m_winHandle;
    int m_winWidth, m_winHeight;

    int m_textureCount;
    int m_textureWidth, m_textureHeight;
    int m_codedWidth, m_codedHeight;

    int  m_frameIndex = 0;

    // Frame buffers in RAM
    void* m_outputBuffers[2];
    size_t m_outputBufferSize[2];

    // Shader will be stored in Pimpl
    void createShaderProgram(unsigned int codecTag);

    bool addSwapChain();
    bool addDepthBuffer();

    struct Pimpl;
    std::unique_ptr<Pimpl> m_pImpl;

    #ifdef LOG_RUNTIME_INFO
        class RuntimeInfoLogger {
        public:
            void onNewFrame(double msTime, int packetLength) {
                if (m_startTime==0) {
                    // Init
                    m_startTime=msTime;
                    m_lastLogTime=msTime;
                }
                // Log info each 1 second
                if (msTime > m_lastLogTime+1000) {
                    m_lastLogTime = msTime;
                    double elapsedTime = msTime - m_startTime;
                    printf("Decompressed Frames: %lu, Average Input Bitrate: %lf, Average Output Birate: %lf, Average framerate: %lf\n",static_cast<unsigned long>(m_frameCount),m_totalBytesRead*8/elapsedTime,m_totalBytesDecompressed*8/elapsedTime,m_frameCount/double((msTime-m_startTime)/1000));
                }
                m_frameCount++;
                m_totalBytesRead += packetLength;
            }
            void onHapDataDecoded(size_t outputBufferDecodedSize) {
                m_totalBytesDecompressed += outputBufferDecodedSize;
            }
        private:
            double m_startTime=0;
            double m_lastLogTime=0;
            size_t m_frameCount=0;
            size_t m_totalBytesRead=0;
            size_t m_totalBytesDecompressed=0;
        };
        RuntimeInfoLogger m_infoLogger;
    #endif
};

#endif // HAPAVFORMATFORGERENDERER_H
