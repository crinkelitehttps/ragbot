QT -= gui
QT += core network
CONFIG += c++17 console
CONFIG -= app_bundle
CONFIG += debug
CONFIG -= release

SOURCES += \
    src/main.cpp \
    src/RAGBot.cpp \
    src/asset/Embedder.cpp \
    src/asset/Researcher.cpp \
    src/asset/Roleplayer.cpp \
    src/db/EmbeddingDatabase.cpp \
    src/db/RoleplayDatabase.cpp \
    src/db/VectorIndex.cpp \
    src/generation/GeneratorFactory.cpp \
    src/generation/GeneratorIP.cpp \
    src/parsers/CDDAResolver.cpp \
    src/parsers/ParserJSON.cpp

HEADERS += \
    src/ConversationTurn.h \
    src/RAGBot.h \
    src/asset/Embedder.h \
    src/asset/Researcher.h \
    src/asset/Roleplayer.h \
    src/db/EmbeddingDatabase.h \
    src/db/RoleplayDatabase.h \
    src/db/VectorIndex.h \
    src/generation/EmbeddingGenerator.h \
    src/generation/GeneratorFactory.h \
    src/generation/GeneratorIP.h \
    src/generation/TextGenerator.h \
    src/parsers/CDDAResolver.h \
    src/parsers/Parser.h \
    src/parsers/ParserJSON.h

# ---------------------------------------------------------------------------
# Embedded inference — build with: qmake CONFIG+=embedded_inference
# Without this flag only network-backed generators (GeneratorIP) are available.
# ---------------------------------------------------------------------------
embedded_inference {
    DEFINES += RAGBOT_EMBEDDED_INFERENCE

    SOURCES += \
        src/generation/EmbeddedEmbeddingGenerator.cpp \
        src/generation/EmbeddedTextGenerator.cpp

    HEADERS += \
        src/generation/EmbeddedEmbeddingGenerator.h \
        src/generation/EmbeddedTextGenerator.h

    INCLUDEPATH += /home/joe/source/llama.cpp/include
    INCLUDEPATH += /home/joe/source/llama.cpp/common
    INCLUDEPATH += /home/joe/source/llama.cpp/ggml/include

    LIBS += /home/joe/source/build-llama.cpp/common/libcommon.a
    LIBS += /home/joe/source/build-llama.cpp/src/libllama.a
    LIBS += /home/joe/source/build-llama.cpp/ggml/src/libggml.a
    LIBS += /home/joe/source/build-llama.cpp/ggml/src/libggml-base.a
    LIBS += /home/joe/source/build-llama.cpp/ggml/src/libggml-cpu.a
    LIBS += /home/joe/source/build-llama.cpp/ggml/src/ggml-vulkan/libggml-vulkan.a
    LIBS += -lopenblas -lgomp -lvulkan
}

QT_ROOT = /home/joe/Qt/5.15.2/gcc_64
message("Using Qt from: $$QT_ROOT")

QMAKE_INCDIR = $${QT_ROOT}/include
QMAKE_LIBDIR = $${QT_ROOT}/lib

INCLUDEPATH  = $${QT_ROOT}/include $$INCLUDEPATH
INCLUDEPATH += $${QT_ROOT}/include/QtCore
INCLUDEPATH += $${QT_ROOT}/include/QtNetwork

LIBS += -lpthread -ldl -lm -lstdc++ -lsqlite3
