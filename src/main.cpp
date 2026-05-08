#include <filesystem>
#include <fstream>
#include <iostream>

#include "RAGBot.h"
#include "RAGBotSession.h"
#include "ConfigKeys.h"
#include "compat/Json.h"
#include "compat/Logging.h"
#include "compat/Strings.h"
#include "compat/Types.h"

static void print_help(const char* prog)
{
    std::cout <<
        "Usage: " << prog << " [options]\n"
        "  -c, --config <file>   Config JSON (default: config.json)\n"
        "  -d, --data   <dir>    Data directory — overrides embedder.files\n"
        "  -b, --db     <file>   Embeddings database path\n"
        "  -l, --load            Index only, then exit\n"
        "  -s, --skip-index      Skip indexing, go straight to chat\n"
        "  -h, --help            Show this help\n";
}

auto main(int argc, char* argv[]) -> int
{
    std::string configPath;
    std::string dataOverride;
    std::string dbOverride;
    bool        loadOnly  = false;
    bool        skipIndex = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto require_next = [&]() -> std::string {
            if (i + 1 >= argc) {
                std::cerr << "error: " << arg << " requires an argument\n";
                std::exit(1);
            }
            return argv[++i];
        };
        if      (arg == "-c" || arg == "--config")     configPath   = require_next();
        else if (arg == "-d" || arg == "--data")        dataOverride = require_next();
        else if (arg == "-b" || arg == "--db")          dbOverride   = require_next();
        else if (arg == "-l" || arg == "--load")        loadOnly     = true;
        else if (arg == "-s" || arg == "--skip-index")  skipIndex    = true;
        else if (arg == "-h" || arg == "--help")        { print_help(argv[0]); return 0; }
        else { std::cerr << "error: unknown option: " << arg << "\n"; print_help(argv[0]); return 1; }
    }

    if (configPath.empty()) configPath = "config.json";

    RAGBOT_LOG_INFO("main: working dir {}", std::filesystem::current_path().string());

    std::ifstream configFile(configPath, std::ios::binary);
    if (!configFile) {
        RAGBOT_LOG_WARN("main: cannot open config file: {}", configPath);
        return 1;
    }
    rb::Bytes configBytes((std::istreambuf_iterator<char>(configFile)),
                          std::istreambuf_iterator<char>());
    configFile.close();

    rb::Json root = rb::Json::parse(configBytes);
    if (!root.isObject()) {
        RAGBOT_LOG_WARN("main: config is not a JSON object: {}", configPath);
        return 1;
    }

    rb::Json embedderConfig = root.value(ConfigKeys::Embedder);
    if (!embedderConfig.isObject()) embedderConfig = rb::Json::object();

    if (!dataOverride.empty()) {
        embedderConfig.setString(ConfigKeys::Files, dataOverride);
        RAGBOT_LOG_INFO("main: data directory overridden to {}", dataOverride);
    }
    if (!dbOverride.empty()) {
        embedderConfig.setString(ConfigKeys::DbName, dbOverride);
        RAGBOT_LOG_INFO("main: database path overridden to {}", dbOverride);
    }
    if (skipIndex)
        embedderConfig.setBool(ConfigKeys::SkipIndex, true);

    root.set(ConfigKeys::Embedder, embedderConfig);

    RAGBotSession session(root, loadOnly);
    if (!session.isValid()) return 1;
    if (loadOnly) return 0;

    RAGBot ragbot(session);
    ragbot.start();
    return 0;
}
