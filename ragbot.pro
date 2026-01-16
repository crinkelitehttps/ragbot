QT -= gui
QT += core network sql

CONFIG += c++17 console
CONFIG -= app_bundle
CONFIG += debug
CONFIG -= release

SOURCES += ragbot.cpp

# Include paths
INCLUDEPATH += /home/joe/source/llama.cpp/include
INCLUDEPATH += /home/joe/source/llama.cpp/common
INCLUDEPATH += /home/joe/source/llama.cpp/ggml/include

# Static libraries
LIBS += /home/joe/source/build-llama.cpp/common/libcommon.a
LIBS += /home/joe/source/build-llama.cpp/src/libllama.a
LIBS += /home/joe/source/build-llama.cpp/ggml/src/libggml.a
LIBS += /home/joe/source/build-llama.cpp/ggml/src/libggml-base.a
LIBS += /home/joe/source/build-llama.cpp/ggml/src/libggml-cpu.a

# System libraries
LIBS += -lgomp -lpthread -ldl -lm -lstdc++
