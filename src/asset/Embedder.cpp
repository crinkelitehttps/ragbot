// Modified to use local llama-swap embedding server
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QJsonObject>
#include "Embedder.h"
#include "../parsers/ParserJSON.h"
#ifdef RAGBOT_EMBEDDED_INFERENCE
#include "../generation/GeneratorEmbedded.h"
#endif
#include "../generation/GeneratorIP.h"


//--------------------------------------------------------------------------------
Embedder::Embedder(const QJsonObject& config)
    : m_db(config)
    , m_files(config.value("files").toString())
    , m_isValid(false)
    , m_parser(new ParserJSON())
{
    if (config.isEmpty()) {
        qWarning() << "Embedder::Embedder(): empty config";
        return;
    }

    if (!m_db.isOpen()) {
        qWarning() << "Embedder::Embedder(): database did not open";
        return;
    }

    const QJsonObject generatorConfig = config.value("generator").toObject();
    if (generatorConfig.value("isImmediate").toBool(false)) {
#ifdef RAGBOT_EMBEDDED_INFERENCE
        m_generator = new GeneratorEmbedded(generatorConfig);
#else
        qCritical() << "Embedder: config requests embedded inference but binary was built without it";
        return;
#endif
    } else {
        m_generator = new GeneratorIP(generatorConfig);
    }

    if (m_generator == nullptr) {
        qWarning() << "Embedder::Embedder(): failed to create generator";
        return;
    }

    m_isValid = true;

    if (config.value("skipIndex").toBool(false)) {
        qDebug() << "Embedder: skipping index pass (-s flag set)";
    } else {
        processAllFiles();
    }
}


//--------------------------------------------------------------------------------
void Embedder::processAllFiles()
{
    // Count files first so we can log progress as N/total.
    int total = 0;
    QDirIterator countIt(m_files, {"*.json"}, QDir::Files, QDirIterator::Subdirectories);
    while (countIt.hasNext()) { countIt.next(); ++total; }

    qDebug() << "Embedder::processAllFiles(): found" << total << "JSON files in" << m_files;

    int indexed = 0;
    int skipped = 0;
    int processed = 0;

    QDirIterator it(m_files, {"*.json"}, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString filePath = it.next();
        ++processed;

        qDebug() << QString("[%1/%2] %3")
            .arg(processed).arg(total)
            .arg(QFileInfo(filePath).fileName());

        QFile file(filePath);
        const bool wasNew = fileEmbed(file);
        if (wasNew) ++indexed; else ++skipped;
    }

    qDebug() << "Embedder::processAllFiles(): done —"
             << indexed << "newly indexed,"
             << skipped << "already up-to-date";
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

    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(fileData);
    const QByteArray fileHash = hash.result().toHex();

    const int sourceId = m_db.newSourceFileId(fileHash, file.fileName());
    if (sourceId < 0) {
        // -1 means the checksum already exists in the database — skip re-embedding.
        return false;
    }

    int chunksAdded = 0;
    for (const Parser::Chunk& chunk : m_parser->toChunks(QVariant(QString::fromUtf8(fileData)))) {
        // "search_document: " is the Nomic embedding task prefix for indexed content.
        const QVector<float> embedding = m_generator->generate("search_document: " + chunk.embedText);

        if (embedding.isEmpty()) {
            qWarning() << "Embedder::fileEmbed(): empty embedding returned for chunk";
            continue;
        }

        if (m_db.embeddingSave(sourceId, embedding, chunk.content)) {
            ++chunksAdded;
        }
    }

    qDebug() << "Embedder::fileEmbed(): indexed" << chunksAdded
             << "chunks from" << QFileInfo(file.fileName()).fileName();
    return true;
}


//--------------------------------------------------------------------------------
auto Embedder::search(const QString& query, int topK)
    -> QVector<EmbeddingDatabase::SearchResult>
{
    if (!m_generator || !m_generator->isValid()) {
        qWarning() << "Embedder::search(): generator not available";
        return {};
    }

    // "search_query: " is the Nomic task prefix for query embeddings.
    const QVector<float> embedding = m_generator->generate("search_query: " + query);
    if (embedding.isEmpty()) {
        qWarning() << "Embedder::search(): failed to embed query";
        return {};
    }

    return m_db.textResults(embedding, topK);
}


//--------------------------------------------------------------------------------
auto Embedder::generationEmbed(const QString& generation) -> void
{
    Q_UNUSED(generation)
    qDebug() << "Embedder::generationEmbed() [ not implemented ]";
}
