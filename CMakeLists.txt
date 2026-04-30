cmake_minimum_required(VERSION 3.16)
project(ragbot LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(RAGBOT_USE_QT             "Build with Qt 5.15 dependency"                  ON)
option(RAGBOT_LIBRARY            "Build only the static library (skip CLI exe)"  OFF)
option(RAGBOT_EMBEDDED_INFERENCE "Link llama.cpp for in-process inference"       OFF)

message(STATUS "RAGBOT_USE_QT             = ${RAGBOT_USE_QT}")
message(STATUS "RAGBOT_LIBRARY            = ${RAGBOT_LIBRARY}")
message(STATUS "RAGBOT_EMBEDDED_INFERENCE = ${RAGBOT_EMBEDDED_INFERENCE}")

# Console executable depends on Qt today (QCommandLineParser, QCoreApplication).
if(NOT RAGBOT_LIBRARY AND NOT RAGBOT_USE_QT)
    message(FATAL_ERROR
        "Console executable (RAGBOT_LIBRARY=OFF) requires RAGBOT_USE_QT=ON. "
        "For a Qt-free build, set -DRAGBOT_LIBRARY=ON to produce only libragbot.a.")
endif()

# ---------------------------------------------------------------------------
# Sources
# ---------------------------------------------------------------------------
set(RAGBOT_LIB_SOURCES
    src/RAGBotSession.cpp
    src/ragbot_c_api.cpp
    src/asset/Embedder.cpp
    src/asset/Reranker.cpp
    src/asset/Researcher.cpp
    src/asset/Roleplayer.cpp
    src/compat/Io.cpp
    src/compat/Json.cpp
    src/compat/Logging.cpp
    src/compat/Sha.cpp
    src/compat/Strings.cpp
    src/db/EmbeddingDatabase.cpp
    src/db/RoleplayDatabase.cpp
    src/db/VectorIndex.cpp
    src/generation/GeneratorFactory.cpp
    src/generation/GeneratorIP.cpp
    src/generation/RerankGeneratorIP.cpp
    src/parsers/CDDAResolver.cpp
    src/parsers/ParserJSON.cpp
)

# ManPageResolver uses QProcess + QRegularExpression — Qt-only.
if(RAGBOT_USE_QT)
    list(APPEND RAGBOT_LIB_SOURCES src/parsers/ManPageResolver.cpp)
    list(APPEND RAGBOT_LIB_SOURCES src/compat/Http_qt.cpp)
else()
    list(APPEND RAGBOT_LIB_SOURCES src/compat/Http_curl.cpp)
endif()

if(RAGBOT_EMBEDDED_INFERENCE)
    list(APPEND RAGBOT_LIB_SOURCES
        src/generation/EmbeddedEmbeddingGenerator.cpp
        src/generation/EmbeddedRerankGenerator.cpp
        src/generation/EmbeddedTextGenerator.cpp
    )
endif()

set(RAGBOT_CLI_SOURCES
    src/main.cpp
    src/RAGBot.cpp
)

# ---------------------------------------------------------------------------
# Library target — always built.
# ---------------------------------------------------------------------------
add_library(ragbot STATIC ${RAGBOT_LIB_SOURCES})
target_include_directories(ragbot PUBLIC ${CMAKE_SOURCE_DIR}/src)

# ---------------------------------------------------------------------------
# Qt path
# ---------------------------------------------------------------------------
if(RAGBOT_USE_QT)
    set(QT_ROOT "/home/joe/Qt/5.15.2/gcc_64" CACHE PATH "Qt 5.15.2 install root")
    list(PREPEND CMAKE_PREFIX_PATH "${QT_ROOT}")
    find_package(Qt5 5.15 REQUIRED COMPONENTS Core Network)
    target_link_libraries(ragbot PUBLIC Qt5::Core Qt5::Network)
    target_compile_definitions(ragbot PUBLIC RAGBOT_USE_QT)
else()
    find_package(CURL REQUIRED)
    include(FetchContent)
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG        v3.11.3
    )
    FetchContent_Declare(
        fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt.git
        GIT_TAG        10.2.1
    )
    FetchContent_MakeAvailable(nlohmann_json fmt)
    target_link_libraries(ragbot PUBLIC
        CURL::libcurl
        nlohmann_json::nlohmann_json
        fmt::fmt
    )
endif()

# ---------------------------------------------------------------------------
# Always-on dependencies
# ---------------------------------------------------------------------------
find_package(SQLite3 REQUIRED)
find_package(Threads REQUIRED)
target_link_libraries(ragbot PUBLIC
    SQLite3::SQLite3
    Threads::Threads
    ${CMAKE_DL_LIBS}
)

# ---------------------------------------------------------------------------
# Embedded inference (llama.cpp)
# ---------------------------------------------------------------------------
if(RAGBOT_EMBEDDED_INFERENCE)
    target_compile_definitions(ragbot PUBLIC RAGBOT_EMBEDDED_INFERENCE)
    set(LLAMA_ROOT  "/home/joe/source/llama.cpp"        CACHE PATH "llama.cpp source root")
    set(LLAMA_BUILD "/home/joe/source/build-llama.cpp"  CACHE PATH "llama.cpp build root")
    target_include_directories(ragbot PUBLIC
        ${LLAMA_ROOT}/include
        ${LLAMA_ROOT}/common
        ${LLAMA_ROOT}/ggml/include
    )
    target_link_libraries(ragbot PUBLIC
        ${LLAMA_BUILD}/common/libcommon.a
        ${LLAMA_BUILD}/src/libllama.a
        ${LLAMA_BUILD}/ggml/src/libggml.a
        ${LLAMA_BUILD}/ggml/src/libggml-base.a
        ${LLAMA_BUILD}/ggml/src/libggml-cpu.a
        ${LLAMA_BUILD}/ggml/src/ggml-vulkan/libggml-vulkan.a
        openblas
        gomp
        vulkan
    )
endif()

# ---------------------------------------------------------------------------
# CLI executable — Qt-only, suppressed when RAGBOT_LIBRARY=ON.
# Output binary is named "ragbot" (CMake target is "ragbot_cli" to avoid
# a name clash with the static-lib target).
# ---------------------------------------------------------------------------
if(NOT RAGBOT_LIBRARY)
    add_executable(ragbot_cli ${RAGBOT_CLI_SOURCES})
    target_link_libraries(ragbot_cli PRIVATE ragbot)
    set_target_properties(ragbot_cli PROPERTIES OUTPUT_NAME ragbot)
endif()

# ---------------------------------------------------------------------------
# compat_smoke — Phase 1 compat layer self-test. Links only the compat sources
# and their dependencies, so it builds in both Qt and non-Qt modes regardless
# of the migration state of the rest of the codebase.
# ---------------------------------------------------------------------------
set(RAGBOT_COMPAT_SOURCES
    src/compat/Io.cpp
    src/compat/Json.cpp
    src/compat/Logging.cpp
    src/compat/Sha.cpp
    src/compat/Strings.cpp
)
if(RAGBOT_USE_QT)
    list(APPEND RAGBOT_COMPAT_SOURCES src/compat/Http_qt.cpp)
else()
    list(APPEND RAGBOT_COMPAT_SOURCES src/compat/Http_curl.cpp)
endif()

add_executable(compat_smoke tests/compat_smoke.cpp ${RAGBOT_COMPAT_SOURCES})
target_include_directories(compat_smoke PRIVATE ${CMAKE_SOURCE_DIR}/src)
if(RAGBOT_USE_QT)
    target_link_libraries(compat_smoke PRIVATE Qt5::Core Qt5::Network)
    target_compile_definitions(compat_smoke PRIVATE RAGBOT_USE_QT)
else()
    target_link_libraries(compat_smoke PRIVATE nlohmann_json::nlohmann_json CURL::libcurl)
endif()
