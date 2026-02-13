#ifndef EMBEDDER_H
#define EMBEDDER_H

#include "../config/ConfigEmbed.h"
#include "../generation/Generator.h"
#include "../db/EmbeddingDatabase.h"
#include "llama.h"
#include "../parsers/Parser.h"

class Embedder
{
public:
    Embedder(ConfigEmbed embedderConfig);
    
    ~Embedder()
    {
    }

    void processAllFiles();
    bool updateOrCreate(const QString &text, const QString &sourcePath, const QString &itemId);
    bool embedFile(const QString &inputPath);

    Generator* initGenerator();

private:

    ConfigEmbed m_config;
    EmbeddingDatabase m_embed_db;
    Generator* m_generator;
    Parser* m_parser;

};

#endif // EMBEDDER_H
