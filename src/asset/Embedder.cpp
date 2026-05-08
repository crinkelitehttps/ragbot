#include "Embedder.h"
#include "../generation/GeneratorFactory.h"
#include "../parsers/ParserJSON.h"
#include "../ConfigKeys.h"
#include "../compat/Logging.h"
#include "../compat/Strings.h"
#include "../compat/Io.h"
#include "../compat/Sha.h"

namespace {

struct FlatChunk {
    int        slotIdx;
    rb::String embedText;
    rb::String content;
};
struct FileSlot {
    rb::String      path;
    rb::String      hexHash;
    rb::Vector<int> flatIndices;
    bool            skipped { false };
};

} // namespace


Embedder::Embedder(const rb::Json& config)
    : m_db(config)
    , m_generator(GeneratorFactory::createEmbedding(config.value(ConfigKeys::Generator)))
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
        m_parser = std::make_unique<ParserJSON>();
        processAllFiles();
    }
}


void Embedder::processAllFiles()
{
    rb::Vector<rb::String> paths;

    for (const rb::String& dir : m_files) {
        const auto dirPaths = rb::iter_files_recursive(dir, rb::from_std(".json"));
        for (const auto& p : dirPaths) paths.push_back(p);
    }
    RAGBOT_LOG_INFO("Embedder::processAllFiles(): {} JSON files across {} directories",
                    static_cast<int>(paths.size()), static_cast<int>(m_files.size()));
    m_registry = CDDAResolver::buildRegistryFromFiles(paths);

    const int total = static_cast<int>(paths.size());

    if (!m_db.beginBatch()) {
        RAGBOT_LOG_WARN("Embedder::processAllFiles(): failed to begin batch transaction");
        return;
    }

    int indexed = 0;
    int skipped = 0;

    const auto counts = processJsonFiles(paths);
    indexed = counts.first;
    skipped = counts.second;

    if (!m_db.commitBatch()) {
        RAGBOT_LOG_WARN("Embedder::processAllFiles(): batch commit failed — re-index required");
        return;
    }

    RAGBOT_LOG_INFO("Embedder::processAllFiles(): done — {} newly indexed, {} already up-to-date",
                    indexed, skipped);
}


auto Embedder::processJsonFiles(const rb::Vector<rb::String>& paths) -> std::pair<int, int>
{
    rb::Vector<FileSlot>  fileSlots;
    rb::Vector<FlatChunk> flat;
    fileSlots.reserve(paths.size());

    // Phase 1 — pre-parse: read, sha256, JSON parse, resolve, build chunks.
    // Skip files whose hash is already in the DB before doing any work.
    int progress = 0;
    const int total = static_cast<int>(paths.size());
    for (const rb::String& path : paths) {
        const rb::String fname = rb::path_filename(path);
        RAGBOT_LOG_INFO("[{}/{}] {}", ++progress, total, rb::to_std(fname));

        bool readOk = false;
        const rb::Bytes fileData = rb::read_file_bytes(path, &readOk);
        if (!readOk) {
            RAGBOT_LOG_WARN("Embedder::processJsonFiles(): cannot open {}", rb::to_std(path));
            FileSlot bad;
            bad.path    = path;
            bad.skipped = true;
            fileSlots.push_back(bad);
            continue;
        }

        FileSlot slot;
        slot.path    = path;
        slot.hexHash = rb::sha256_hex(fileData);

        if (m_db.sourceFileExists(slot.hexHash)) {
            RAGBOT_LOG_INFO("Embedder::processJsonFiles(): already indexed — {}", rb::to_std(path));
            slot.skipped = true;
            fileSlots.push_back(slot);
            continue;
        }

        const rb::Json doc = rb::Json::parse(fileData);
        rb::Vector<rb::Json> objects;
        if (doc.isArray()) {
            for (const auto& item : doc.items())
                if (item.isObject()) objects.push_back(item);
        } else if (doc.isObject()) {
            objects.push_back(doc);
        }

        const int slotIdx = static_cast<int>(fileSlots.size());
        for (const rb::Json& raw : objects) {
            if (raw.boolValue("abstract", false)) continue;
            const rb::Json resolved = CDDAResolver::resolve(raw, m_registry);
            const Parser::Chunk chunk = m_parser->objectToChunk(resolved);
            slot.flatIndices.push_back(static_cast<int>(flat.size()));
            FlatChunk fc;
            fc.slotIdx   = slotIdx;
            fc.embedText = rb::from_std("search_document: ") + chunk.embedText;
            fc.content   = chunk.content;
            flat.push_back(fc);
        }

        fileSlots.push_back(slot);
    }

    // Phase 2 — single concurrent dispatch across all files.
    rb::Vector<rb::String> inputs;
    inputs.reserve(flat.size());
    for (const auto& fc : flat) inputs.push_back(fc.embedText);

    RAGBOT_LOG_INFO("Embedder::processJsonFiles(): dispatching {} chunks across {} files",
                    static_cast<int>(flat.size()), static_cast<int>(fileSlots.size()));

    const rb::Vector<rb::Vector<float>> embeddings = m_generator->generateBatch(inputs);
    if (static_cast<size_t>(embeddings.size()) != static_cast<size_t>(flat.size())) {
        RAGBOT_LOG_WARN("Embedder::processJsonFiles(): batch embedding size mismatch ({} vs {})",
                        static_cast<int>(embeddings.size()), static_cast<int>(flat.size()));
        return { 0, static_cast<int>(fileSlots.size()) };
    }

    // Phase 3 — per-file commit in original order.
    int indexed = 0;
    int skipped = 0;
    for (FileSlot& slot : fileSlots) {
        if (slot.skipped) { ++skipped; continue; }

        if (!m_db.beginFileTransaction()) {
            RAGBOT_LOG_WARN("Embedder::processJsonFiles(): failed to open savepoint for {}",
                            rb::to_std(slot.path));
            ++skipped;
            continue;
        }

        const int sourceId = m_db.newSourceFileId(slot.hexHash, slot.path);
        if (sourceId < 0) {
            m_db.rollbackFileTransaction();
            ++skipped;
            continue;
        }

        bool ok = true;
        int added = 0;
        for (const int flatIdx : slot.flatIndices) {
            if (embeddings[flatIdx].empty()) {
                RAGBOT_LOG_WARN("Embedder::processJsonFiles(): empty embedding for chunk in {}",
                                rb::to_std(slot.path));
                ok = false;
                break;
            }
            if (m_db.embeddingSave(sourceId, embeddings[flatIdx], flat[flatIdx].content)) ++added;
        }

        if (!ok || !m_db.commitFileTransaction()) {
            m_db.rollbackFileTransaction();
            ++skipped;
            continue;
        }

        RAGBOT_LOG_INFO("Embedder::processJsonFiles(): indexed {} chunks from {}",
                        added, rb::to_std(rb::path_filename(slot.path)));
        ++indexed;
    }

    return { indexed, skipped };
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
