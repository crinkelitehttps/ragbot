QT -= gui
QT += core network
CONFIG += c++17 console
CONFIG -= app_bundle
CONFIG += debug
CONFIG -= release

# Source files
SOURCES += \
    src/main.cpp \
    src/RAGBot.cpp \
    src/asset/Embedder.cpp \
    src/asset/Researcher.cpp \
    src/db/EmbeddingDatabase.cpp \
    src/db/RoleplayDatabase.cpp \
    src/db/VectorIndex.cpp \
    src/generation/GeneratorImmediate.cpp \
    src/generation/GeneratorIP.cpp \
    src/parsers/ParserJSON.cpp

# Header files
HEADERS += \
    src/RAGBot.h \
    src/asset/Embedder.h \
    src/asset/Researcher.h \
    src/asset/Roleplayer.h \
    src/db/EmbeddingDatabase.h \
    src/db/RoleplayDatabase.h \
    src/db/VectorIndex.h \
    src/generation/Generator.h \
    src/generation/GeneratorImmediate.h \
    src/generation/GeneratorIP.h \
    src/parsers/Parser.h \
    src/parsers/ParserJSON.h

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
LIBS += /home/joe/source/build-llama.cpp/ggml/src/ggml-vulkan/libggml-vulkan.a
LIBS += -lopenblas -lgomp -lpthread -ldl -lm -lstdc++ -lvulkan -lsqlite3
