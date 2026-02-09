// Modified to use local llama-swap embedding server
#ifndef EMBEDDER_H
#define EMBEDDER_H

#include "config/ConfigEmbed.h"
#include "Generator.h"
#include "db/EmbeddingDatabase.h"
#include "llama.h"
#include "Generator.h"

class Embedder
{
public:
    Embedder(ConfigEmbed embedderConfig);
    
    ~Embedder()
    {
    }

    void processAllFiles();

private:
    bool embedAndSave(const QString &text, const QString &sourcePath, const QString &itemId);
    bool embedFile(const QString &inputPath);
    QString extractTextFromJson(const QJsonValue &value, const QStringList &keys);
    void extractTextRecursive(const QJsonValue &value, const QStringList &keys, QStringList &texts);
    QVector<float> generateEmbedding(const QString &text);

private:
    ConfigEmbed m_config;
    EmbeddingDatabase m_embed_db;
};

#endif // EMBEDDER_H
