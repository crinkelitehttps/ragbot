#ifndef EMBEDDER_H
#define EMBEDDER_H

#include <QFileInfo>
#include "../generation/Generator.h"
#include "../db/EmbeddingDatabase.h"
#include "llama.h"
#include "../parsers/Parser.h"

class Embedder
{
public:
    Embedder(const QJsonObject& embedderConfig);

    ~Embedder()
    {
    }

    void processAllFiles();
    void fileEmbed(const QString& sourcePath);
    QVector<float>& textVectors();
    bool isValid() { return m_isValid; }

private:
    EmbeddingDatabase* m_db;
    Generator* m_generator;
    Parser* m_parser;
    QString m_files;
    bool m_isValid {};

};

#endif // EMBEDDER_H
