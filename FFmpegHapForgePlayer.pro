# Setup
TEMPLATE = app
CONFIG += c++14
CONFIG -= app_bundle
CONFIG -= qt

# Turn this off to skip runtime info log
DEFINES += LOG_RUNTIME_INFO

include(ShaderCompiler.pri)

THE_FORGE_ROOT = $$PWD/The-Forge
include($$THE_FORGE_ROOT/The-Cute-Forge.pri)

# Sources
HEADERS += \
    src/HAPAvFormatForgeRenderer.h \
    src/hap/hap.h \
    src/shadercompilerhelper.h

SOURCES += \
    src/HAPAvFormatForgeRenderer.cpp \
    src/main.cpp \
    src/hap/hap.c \
    src/shadercompilerhelper.cpp

EXECUTABLE_PATH = $${OUT_PWD}/$${TARGET}

mac {
    # Dependencies pathes
    FFMPEGPATH =   $$_PRO_FILE_PWD_/dependencies/Mac/x86_64/ffmpeg
    INCLUDEPATH += $${FFMPEGPATH}/include

    SNAPPY_PATH =  $$_PRO_FILE_PWD_/dependencies/Mac/x86_64/snappy
    INCLUDEPATH += $${SNAPPY_PATH}/include

    # FFMPEG
    FFMPEGLIBPATH = $${FFMPEGPATH}/lib
    LIBS += $${FFMPEGLIBPATH}/libavcodec.a
    LIBS += $${FFMPEGLIBPATH}/libavdevice.a
    LIBS += $${FFMPEGLIBPATH}/libavformat.a
    LIBS += $${FFMPEGLIBPATH}/libavutil.a
    LIBS += $${FFMPEGLIBPATH}/libswresample.a
    LIBS += $${FFMPEGLIBPATH}/libswscale.a
    LIBS += -framework VideoDecodeAcceleration
    LIBS += -framework AudioToolbox
    LIBS += -framework CoreFoundation
    LIBS += -framework CoreVideo
    LIBS += -framework CoreMedia
    LIBS += -framework VideoToolbox
    LIBS += -framework Security
    LIBS += -lz
    LIBS += -lbz2
    LIBS += /usr/lib/libiconv.dylib
    LIBS += /usr/lib/liblzma.dylib

    # Snappy
    SNAPPY_LIB_PATH = $${SNAPPY_PATH}/lib
    LIBS += $${SNAPPY_LIB_PATH}/libsnappy.dylib
    QMAKE_POST_LINK += install_name_tool -change @rpath/libsnappy.dylib $${SNAPPY_LIB_PATH}/libsnappy.dylib $${EXECUTABLE_PATH};

    # The Forge
    QMAKE_CXXFLAGS += -ObjC++ -fobjc-arc
    LIBS += -framework QuartzCore
    LIBS += -framework Metal
    LIBS += -framework MetalKit
    LIBS += -framework MetalPerformanceShaders
    LIBS += -framework IOKit
    LIBS += -framework Cocoa
    LIBS += -framework AppKit

    DEFINES += METAL
}

windows {
    # Dependencies pathes
    FFMPEGPATH =   $$_PRO_FILE_PWD_/dependencies/Windows/x86_64/ffmpeg
    INCLUDEPATH += $${FFMPEGPATH}/include

    SNAPPY_PATH =  $$_PRO_FILE_PWD_/dependencies/Windows/x86_64/snappy
    INCLUDEPATH += $${SNAPPY_PATH}/include

    # FFMPEG
    FFMPEGLIBPATH = $${FFMPEGPATH}/lib
    LIBS += $${FFMPEGLIBPATH}/avcodec-lav.lib
    LIBS += $${FFMPEGLIBPATH}/avformat-lav.lib
    LIBS += $${FFMPEGLIBPATH}/avutil-lav.lib

    # Snappy
    SNAPPY_LIB_PATH = $${SNAPPY_PATH}/lib
    CONFIG(release, debug|release) {
        LIBS += $${SNAPPY_LIB_PATH}/snappy.lib
    } else {
        LIBS += $${SNAPPY_LIB_PATH}/snappyd.lib
    }

    # The Forge
    defineTest(copyToDestDir) {
        files = $$1
        dir = $$2
        # replace slashes in destination path for Windows
        win32:dir ~= s,/,\\,g

        !exists($$dir){
            win32:QMAKE_POST_LINK += if not exist $$shell_quote($$dir) $$QMAKE_MKDIR $$shell_quote($$dir) $$escape_expand(\\n\\t)
            !win32:QMAKE_POST_LINK += $$QMAKE_MKDIR -p $$shell_quote($$dir) $$escape_expand(\\n\\t)
        }

        for(file, files) {
            # replace slashes in source path for Windows
            win32:file ~= s,/,\\,g

            QMAKE_POST_LINK += $$QMAKE_COPY_DIR $$shell_quote($$file) $$shell_quote($$dir) $$escape_expand(\\n\\t)
        }

        export(QMAKE_POST_LINK)
    }


    LIBS += -L$$THE_FORGE_ROOT/Common_3/ThirdParty/OpenSource/winpixeventruntime/bin -lWinPixEventRuntime
    LIBS += -L$$THE_FORGE_ROOT/Common_3/ThirdParty/OpenSource/ags/ags_lib/lib/ -lamd_ags_x64
    LIBS += -L$$THE_FORGE_ROOT/Common_3/ThirdParty/OpenSource/nvapi/amd64 -lnvapi64
    LIBS += -L$$THE_FORGE_ROOT/Common_3/ThirdParty/OpenSource/DirectXShaderCompiler/lib/x64 -ldxcompiler
    LIBS += -lXinput -lgdi32 -lComdlg32 -lOle32 -lUser32

    copyToDestDir($$PWD/The-Forge/Common_3/ThirdParty/OpenSource/winpixeventruntime/bin/WinPixEventRuntime.dll \
                  $$PWD/The-Forge/Common_3/ThirdParty/OpenSource/ags/ags_lib/lib/amd_ags_x64.dll \
                  $$PWD/The-Forge/Common_3/ThirdParty/OpenSource/DirectXShaderCompiler/bin/x64/dxcompiler.dll \
                  $$PWD/The-Forge/Common_3/ThirdParty/OpenSource/DirectXShaderCompiler/bin/x64/dxil.dll, $$OUT_PWD)

}

linux {
    # seems to be broken
    CC = clang
    CXX = clang
    CFLAGS += -fblocks
    CXXFLAGS += -fblocks

    # important flag ;-)
    DEFINES += Linux

    # Dependencies pathes
    # no change needed (works for me ...)

    # Linux build is broken.
    # Since the changes above don't work, you'll need to manually tweak the Makefile at the end
    #
    # clang IS mandatory to build HAPAvFormatOpenGLRenderer.cpp

    # replace CC = gcc with the line : CC = clang
    # replace CXX = g++ with the line : CXX = clang
    # replace the line CFLAGS = -m64 -pipe     with the line : CFLAGS = -m64 -fblock -pipe
    # replace the line CXXFLAGS = -m64 -pipe   with the line : CFLAGS = -m64 -fblock -pipe

    # do not modify the LIBRARY_PATH. Should work without change anything

    # OpenGL
    LIBS += -lGL

    # libpthread :
    LIBS +=  -lpthread

    # libdl :
    LIBS +=  -ldl

    # libsnappy
    SNAPPY_LIB_PATH = $${SNAPPY_PATH}/lib
    LIBS += -lsnappy

    # libdispatch
    LIBS += -ldispatch

    # BlocksRuntime
    LIBS += -lBlocksRuntime

    # ffmpeg :
    LIBS +=  `pkg-config --cflags --libs libavformat libavcodec libavutil`

    # SDL2 :
    LIBS += `sdl2-config --cflags --libs`
}

# Copy shaders folder next to executable (if the executable is not at project root)
!equals(_PRO_FILE_PWD_,OUT_PWD) {
    windows {
        QMAKE_POST_LINK += xcopy /S /I /E \"$$_PRO_FILE_PWD_/shaders\" \"$${OUT_PWD}\\\";
    }
    !windows {
        QMAKE_POST_LINK += cp -R $$_PRO_FILE_PWD_/shaders $${OUT_PWD};
    }
}

DISTFILES += \
    shaders/Default.frag \
    shaders/ScaledCoCgYPlusAToRGBA.frag \
    shaders/ScaledCoCgYToRGBA.frag \
    shaders/Default.vert

