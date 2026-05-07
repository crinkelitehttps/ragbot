#include "Embedder.h"
#include "../generation/GeneratorFactory.h"
#include "../parsers/ParserJSON.h"
#include "../ConfigKeys.h"
#include "../compat/Logging.h"
#include "../compat/Strings.h"
#include "../compat/Io.h"
#include "../compat/Sha.h"
#ifdef RAGBOT_USE_QT
#include "../parsers/ManPageResolver.h"
#endif


Embedder::Embedder(const rb::Json& config)
    : m_db(config)
    , m_generator(GeneratorFactory::createEmbedding(config.value(ConfigKeys::Generator)))
    , m_parserType(config.stringValue(ConfigKeys::ParserType, ConfigKeys::ParserCddaJson))
    , m_topK(config.intValue(ConfigKeys::TopK, 10))
    , m_similarityThreshold(static_cast<float>(config.doubleValue(ConfigKeys::SimilarityThreshold, 0.0)))
{
    const rb::Json filesVal = config.value(ConfigKeys::Files);
    if (filesVal.isArray()) {
        for (const auto& v : filesVal.items())
            m_files.push_back(v.toString());
    } else {
        m_files.push_back(filesVal.toString());
    }

    if (!m_db.isOpen()) {
        RAGBOT_LOG_WARN("Embedder: database did not open");
        return;
    }
    if (!m_generator || !m_generator->isValid()) {
        RAGBOT_LOG_WARN("Embedder: generator failed to initialise");
        return;
    }

    m_isValid = true;

    if (config.boolValue(ConfigKeys::SkipIndex, false)) {
        RAGBOT_LOG_INFO("Embedder: skipping index pass (-s flag)");
    } else {
        if (m_parserType == ConfigKeys::ParserCddaJson)
            m_parser = std::make_unique<ParserJSON>();
        processAllFiles();
    }
}


void Embedder::processAllFiles()
{
    rb::Vector<rb::String> paths;

#ifdef RAGBOT_USE_QT
    if (m_parserType == ConfigKeys::ParserManPage) {
        QStringList qtFiles;
        for (const auto& f : m_files) qtFiles << f;
        const QStringList discovered = ManPageResolver::discover(qtFiles);
        for (const auto& p : discovered) paths.push_back(p);
        RAGBOT_LOG_INFO("Embedder::processAllFiles(): {} man page files across {} directories",
                        static_cast<int>(paths.size()), static_cast<int>(m_files.size()));
    } else
#endif
    {
        for (const rb::String& dir : m_files) {
            const auto dirPaths = rb::iter_files_recursive(dir, rb::from_std(".json"));
            for (const auto& p : dirPaths) paths.push_back(p);
        }
        RAGBOT_LOG_INFO("Embedder::processAllFiles(): {} JSON files across {} directories",
                        static_cast<int>(paths.size()), static_cast<int>(m_files.size()));
        m_registry = CDDAResolver::buildRegistryFromFiles(paths);
    }

    const int total = static_cast<int>(paths.size());

    if (!m_db.beginBatch()) {
        RAGBOT_LOG_WARN("Embedder::processAllFiles(): failed to begin batch transaction");
        return;
    }

    int indexed = 0, skipped = 0, n = 0;
    for (const rb::String& path : paths) {
        const rb::String fname = rb::path_filename(path);
        RAGBOT_LOG_INFO("[{}/{}] {}", ++n, total, rb::to_std(fname));

#ifdef RAGBOT_USE_QT
        if (m_parserType == ConfigKeys::ParserManPage) {
            if (fileEmbedManPage(path)) ++indexed; else ++skipped;
        } else
#endif
        {
            if (fileEmbed(path)) ++indexed; else ++skipped;
        }
    }

    if (!m_db.commitBatch()) {
        RAGBOT_LOG_WARN("Embedder::processAllFiles(): batch commit failed — re-index required");
        return;
    }

    RAGBOT_LOG_INFO("Embedder::processAllFiles(): done — {} newly indexed, {} already up-to-date",
                    indexed, skipped);
}


auto Embedder::fileEmbed(const rb::String& path) -> bool
{
    bool readOk = false;
    const rb::Bytes fileData = rb::read_file_bytes(path, &readOk);
    if (!readOk) {
        RAGBOT_LOG_WARN("Embedder::fileEmbed(): cannot open {}", rb::to_std(path));
        return false;
    }

    const rb::String hexHash = rb::sha256_hex(fileData);

    m_db.beginFileTransaction();

    const int sourceId = m_db.newSourceFileId(hexHash, path);
    if (sourceId < 0) {
        m_db.rollbackFileTransaction();
        return false; // unchanged since last index
    }

    const rb::Json doc = rb::Json::parse(fileData);
    rb::Vector<rb::Json> objects;
    if (doc.isArray()) {
        for (const auto& v : doc.items())
            if (v.isObject()) objects.push_back(v);
    } else if (doc.isObject()) {
        objects.push_back(doc);
    }

    struct PendingChunk { rb::String embedText; rb::String content; };
    rb::Vector<PendingChunk> pending;
    for (const rb::Json& raw : objects) {
        if (raw.boolValue("abstract", false)) continue;
        const rb::Json resolved = CDDAResolver::resolve(raw, m_registry);
        const Parser::Chunk chunk = m_parser->objectToChunk(resolved);
        pending.push_back({ rb::from_std("search_document: ") + chunk.embedText, chunk.content });
    }

    rb::Vector<rb::String> inputs;
    inputs.reserve(pending.size());
    for (const auto& pchunk : pending)
        inputs.push_back(pchunk.embedText);

    const rb::Vector<rb::Vector<float>> embeddings = m_generator->generateBatch(inputs);
    if (embeddings.size() != pending.size()) {
        RAGBOT_LOG_WARN("Embedder::fileEmbed(): batch embedding failed — aborting file");
        m_db.rollbackFileTransaction();
        return false;
    }

    int added = 0;
    const auto chunkCount = static_cast<int>(pending.size());
    for (int idx = 0; idx < chunkCount; ++idx) {
        if (embeddings[idx].empty()) {
            RAGBOT_LOG_WARN("Embedder::fileEmbed(): empty embedding for chunk — aborting file");
            m_db.rollbackFileTransaction();
            return false;
        }
        if (m_db.embeddingSave(sourceId, embeddings[idx], pending[idx].content)) ++added;
    }

    if (!m_db.commitFileTransaction()) {
        RAGBOT_LOG_WARN("Embedder::fileEmbed(): commit failed — {}", rb::to_std(rb::path_filename(path)));
        m_db.rollbackFileTransaction();
        return false;
    }

    RAGBOT_LOG_INFO("Embedder::fileEmbed(): indexed {} chunks from {}",
                    added, rb::to_std(rb::path_filename(path)));
    return true;
}


auto Embedder::fileEmbedManPage(const rb::String& path) -> bool
{
#ifdef RAGBOT_USE_QT
    bool readOk = false;
    const rb::Bytes fileData = rb::read_file_bytes(path, &readOk);
    if (!readOk) {
        RAGBOT_LOG_WARN("Embedder::fileEmbedManPage(): cannot open {}", rb::to_std(path));
        return false;
    }

    const rb::String hexHash = rb::sha256_hex(fileData);
    m_db.beginFileTransaction();

    const int sourceId = m_db.newSourceFileId(hexHash, path);
    if (sourceId < 0) {
        m_db.rollbackFileTransaction();
        return false;
    }

    const QVector<Parser::Chunk> chunks = ManPageResolver::fileToChunks(path);
    if (chunks.isEmpty()) {
        RAGBOT_LOG_WARN("Embedder::fileEmbedManPage(): no chunks produced for {}", rb::to_std(path));
        m_db.rollbackFileTransaction();
        return false;
    }

    int added = 0;
    for (const Parser::Chunk& chunk : chunks) {
        const rb::Vector<float> embedding =
            m_generator->generate(rb::from_std("search_document: ") + chunk.embedText);
        if (embedding.empty()) {
            RAGBOT_LOG_WARN("Embedder::fileEmbedManPage(): empty embedding — aborting file");
            m_db.rollbackFileTransaction();
            return false;
        }
        if (m_db.embeddingSave(sourceId, embedding, chunk.content)) ++added;
    }

    if (!m_db.commitFileTransaction()) {
        RAGBOT_LOG_WARN("Embedder::fileEmbedManPage(): commit failed — {}",
                        rb::to_std(rb::path_filename(path)));
        m_db.rollbackFileTransaction();
        return false;
    }

    RAGBOT_LOG_INFO("Embedder::fileEmbedManPage(): indexed {} chunks from {}",
                    added, rb::to_std(rb::path_filename(path)));
    return true;
#else
    (void)path;
    return false;
#endif
}


auto Embedder::search(const rb::String& query) -> rb::Vector<EmbeddingDatabase::SearchResult>
{
    if (!m_generator || !m_generator->isValid()) {
        RAGBOT_LOG_WARN("Embedder::search(): generator not available");
        return {};
    }

    m_lastQueryEmbedding = m_generator->generate(rb::from_std("search_query: ") + query);
    if (m_lastQueryEmbedding.empty()) {
        RAGBOT_LOG_WARN("Embedder::search(): failed to embed query");
        return {};
    }

    return m_db.textResults(m_lastQueryEmbedding, m_topK, m_similarityThreshold);
}
