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
    bool embedFile(const QString &sourcePath);

    Generator* initGenerator();

private:

    ConfigEmbed m_config;
    EmbeddingDatabase m_embedDB;
    Generator* m_generator;
    Parser* m_parser;

};

#endif // EMBEDDER_H
