QT -= gui
QT += core network sql
CONFIG += c++17 console
CONFIG -= app_bundle
CONFIG += debug
CONFIG -= release

# Source files
SOURCES += \
    src/main.cpp \
    src/RAGBot.cpp \
    src/RemoteLLMClient.cpp \
    src/EmbeddingDatabase.cpp \
    src/ConversationDatabase.cpp \
    src/Embedder.cpp

# Header files
HEADERS += \
    src/RAGBot.h \
    src/RemoteLLMClient.h \
    src/RemoteLLMConfig.h \
    src/RoleplayConfig.h \
    src/EmbeddingDatabase.h \
    src/ConversationDatabase.h \
    src/Embedder.h

# Include paths
# Force Qt from home directory, NOT system repos
QT_ROOT = /home/joe/Qt/5.15.2/gcc_64
message("Using Qt from: $QT_ROOT")

# Override system Qt paths
QMAKE_INCDIR = $${QT_ROOT}/include
QMAKE_LIBDIR = $${QT_ROOT}/lib

# Explicitly prepend custom Qt to include and lib paths
INCLUDEPATH = $${QT_ROOT}/include $$INCLUDEPATH
INCLUDEPATH += $${QT_ROOT}/include/QtCore
INCLUDEPATH += $${QT_ROOT}/include/QtSql
INCLUDEPATH += $${QT_ROOT}/include/QtNetwork
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
