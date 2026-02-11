#ifndef EMBEDDER_H
#define EMBEDDER_H

#include "../config/ConfigEmbed.h"
#include "../generation/Generator.h"
#include "../db/EmbeddingDatabase.h"
#include "llama.h"

class Embedder
{
public:
    Embedder(ConfigEmbed embedderConfig);
    
    ~Embedder()
    {
    }

    void processAllFiles();

    bool embedAndSave(const QString &text, const QString &sourcePath, const QString &itemId);
    bool embedFile(const QString &inputPath);
    QString extractTextFromJson(const QJsonValue &value, const QStringList &keys);
    void extractTextRecursive(const QJsonValue &value, const QStringList &keys, QStringList &texts);
    QVector<float> generateEmbedding(QString text);
    Generator* initGenerator();

private:

    ConfigEmbed m_config;
    EmbeddingDatabase m_embed_db;
    Generator* m_generator;
};

#endif // EMBEDDER_H
