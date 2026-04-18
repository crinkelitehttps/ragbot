#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDebug>
#include "Embedder.h"
#include "../generation/GeneratorFactory.h"
#include "../parsers/ParserJSON.h"


//--------------------------------------------------------------------------------
Embedder::Embedder(const QJsonObject& config)
    : m_db(config)
    , m_files(config.value("files").toString())
    , m_generator(GeneratorFactory::createEmbedding(config.value("generator").toObject()))
    , m_parser(std::make_unique<ParserJSON>())
    , m_topK(config.value("topK").toInt(10))
    , m_similarityThreshold(static_cast<float>(config.value("similarityThreshold").toDouble(0.0)))
{
    if (config.isEmpty()) {
        qWarning() << "Embedder: empty config";
        return;
    }
    if (!m_db.isOpen()) {
        qWarning() << "Embedder: database did not open";
        return;
    }
    if (!m_generator || !m_generator->isValid()) {
        qWarning() << "Embedder: generator failed to initialise";
        return;
    }

    m_isValid = true;

    if (config.value("skipIndex").toBool(false)) {
        qDebug() << "Embedder: skipping index pass (-s flag)";
    } else {
        m_registry = CDDAResolver::buildRegistry(m_files);
        processAllFiles();
    }
}


//--------------------------------------------------------------------------------
void Embedder::processAllFiles()
{
    QStringList paths;
    QDirIterator it(m_files, {"*.json"}, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext())
        paths << it.next();

    const int total = paths.size();
    qDebug() << "Embedder::processAllFiles():" << total << "JSON files in" << m_files;

    int indexed = 0, skipped = 0, n = 0;
    for (const QString& path : paths) {
        QFile file(path);
        qDebug() << QString("[%1/%2] %3").arg(++n).arg(total)
                                         .arg(QFileInfo(path).fileName());
        if (fileEmbed(file)) ++indexed; else ++skipped;
    }

    qDebug() << "Embedder::processAllFiles(): done —"
             << indexed << "newly indexed," << skipped << "already up-to-date";
}


//--------------------------------------------------------------------------------
auto Embedder::fileEmbed(QFile& file) -> bool
{
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Embedder::fileEmbed(): cannot open" << file.fileName();
        return false;
    }
    const QByteArray fileData = file.readAll();
    file.close();

    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(fileData);

    m_db.beginFileTransaction();

    const int sourceId = m_db.newSourceFileId(hash.result().toHex(), file.fileName());
    if (sourceId < 0) {
        m_db.rollbackFileTransaction(); // nothing was written; clean up the transaction
        return false; // unchanged since last index
    }

    const QJsonDocument doc = QJsonDocument::fromJson(fileData);
    QVector<QJsonObject> objects;
    if (doc.isArray()) {
        for (const QJsonValue& v : doc.array())
            if (v.isObject()) objects << v.toObject();
    } else if (doc.isObject()) {
        objects << doc.object();
    }

    int added = 0;
    for (const QJsonObject& raw : objects) {
        if (raw.value("abstract").toBool(false)) continue; // base templates — not indexed

        const QJsonObject resolved = CDDAResolver::resolve(raw, m_registry);
        const Parser::Chunk chunk  = m_parser->objectToChunk(resolved);

        // Nomic embed task prefix for indexed content.
        const QVector<float> embedding = m_generator->generate("search_document: " + chunk.embedText);
        if (embedding.isEmpty()) {
            qWarning() << "Embedder::fileEmbed(): empty embedding for chunk — aborting file";
            m_db.rollbackFileTransaction();
            return false;
        }
        if (m_db.embeddingSave(sourceId, embedding, chunk.content)) ++added;
    }

    if (!m_db.commitFileTransaction()) {
        qWarning() << "Embedder::fileEmbed(): commit failed —"
                   << QFileInfo(file.fileName()).fileName();
        m_db.rollbackFileTransaction();
        return false;
    }

    qDebug() << "Embedder::fileEmbed(): indexed" << added
             << "chunks from" << QFileInfo(file.fileName()).fileName();
    return true;
}


//--------------------------------------------------------------------------------
auto Embedder::search(const QString& query)
    -> QVector<EmbeddingDatabase::SearchResult>
{
    if (!m_generator || !m_generator->isValid()) {
        qWarning() << "Embedder::search(): generator not available";
        return {};
    }

    m_lastQueryEmbedding = m_generator->generate("search_query: " + query);
    if (m_lastQueryEmbedding.isEmpty()) {
        qWarning() << "Embedder::search(): failed to embed query";
        return {};
    }

    return m_db.textResults(m_lastQueryEmbedding, m_topK, m_similarityThreshold);
}
