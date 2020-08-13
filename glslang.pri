macos {
    GLSLANG_BASE_FOLDER = $$PWD/glslang/osx
}
windows {
    GLSLANG_BASE_FOLDER = $$PWD/glslang/windows
}

#Only release build is provided for windows

windows {
    CONFIG(debug, debug|release) {
        LIBS += \
            -L$$GLSLANG_BASE_FOLDER/lib \
            -lMachineIndependentd \
            -lHLSLd \
            -lOGLCompilerd \
            -lOSDependentd \
            -lGenericCodeGend \
            -lSPIRV-Tools-optd \
            -lSPIRV-Toolsd \
            -lSPIRVd \
            -lSPVRemapperd \
            -lglslangd \

    } else {
        LIBS += \
            -L$$GLSLANG_BASE_FOLDER/lib \
            -lMachineIndependent \
            -lHLSL \
            -lOGLCompiler \
            -lOSDependent \
            -lGenericCodeGen \
            -lSPIRV-Tools-opt \
            -lSPIRV-Tools \
            -lSPIRV \
            -lSPVRemapper \
            -lglslang \

    }
} else {
    LIBS += \
        -L$$GLSLANG_BASE_FOLDER/lib \
        -lMachineIndependent \
        -lHLSL \
        -lOGLCompiler \
        -lOSDependent \
        -lGenericCodeGen \
        -lSPIRV-Tools-opt \
        -lSPIRV-Tools \
        -lSPIRV \
        -lSPVRemapper \
        -lglslang \

}

INCLUDEPATH += \
    $$GLSLANG_BASE_FOLDER/include
