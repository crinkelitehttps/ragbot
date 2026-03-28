#include "GeneratorImmediate.h"
#include <QJsonObject>
#include <QDir>
#include <QDebug>
#include "llama.h"
#include "common.h" // Required for common_tokenize, common_batch_add, etc.

//--------------------------------------------------------------------------------
GeneratorImmediate::GeneratorImmediate(const QJsonObject& config) 
    : Generator(config)
{
    // Initialize backend once
    static bool backend_initialized = false;
    if (!backend_initialized) {
        llama_backend_init();
        backend_initialized = true;
    }

    llama_log_set([](ggml_log_level level, const char * text, void * user_data) -> void {
        Q_UNUSED(user_data)
        if (level == GGML_LOG_LEVEL_ERROR) {
            qCritical() << "llama.cpp error:" << text;
        }
    }, nullptr);

    // Model Params
    llama_model_params model_params = llama_model_default_params();
    // For many local setups, setting n_gpu_layers is desired:
    // model_params.n_gpu_layers = 99; 

    m_embedModel = llama_model_load_from_file(
        "/home/joe/.local/share/models/nomic-embed-text-v1.5.f32.gguf", 
        model_params
    );

    if (m_embedModel == nullptr) {
        qCritical() << "GeneratorImmediate: Failed to load model";
        return;
    }

    // Context Params
    llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_ctx     = DefaultCtx;
    ctx_params.n_batch   = DefaultBatch;
    ctx_params.embeddings = true;
    ctx_params.pooling_type = LLAMA_POOLING_TYPE_MEAN; // Required for Nomic

    m_embedCtx = llama_init_from_model(m_embedModel, ctx_params);

    if (m_embedCtx == nullptr) {
        qCritical() << "GeneratorImmediate: Failed to create context";
    }
}

//--------------------------------------------------------------------------------
GeneratorImmediate::~GeneratorImmediate()
{
    if (m_embedCtx != nullptr) {
        llama_free(m_embedCtx);
        m_embedCtx = nullptr;
    }
    if (m_embedModel != nullptr) {
        llama_model_free(m_embedModel);
        m_embedModel = nullptr;
    }
}


//--------------------------------------------------------------------------------
 auto GeneratorImmediate::generate(const QString& data) -> QVector<float>
{
    if (m_embedCtx == nullptr) return {};

    std::string preparedData = "search_document: " + data.toStdString();

    auto vtok = common_tokenize(m_embedCtx, preparedData, true, true);

    const auto safe_limit = llama_n_ubatch(m_embedCtx); 
    
    if (vtok.size() > safe_limit) {
        qWarning() << "Truncating tokens from" << vtok.size() << "to" << safe_limit;
        vtok.resize(safe_limit);
    }

    llama_memory_t kv_cache = llama_get_memory(m_embedCtx);
    llama_memory_seq_rm(kv_cache, -1, 0, -1);

    llama_batch batch = llama_batch_init(static_cast<int>(vtok.size()), 0, 1);
    for (int32_t i = 0; i < static_cast<int32_t>(vtok.size()); i++) {
        common_batch_add(batch, vtok[i], i, {0}, (i == static_cast<int32_t>(vtok.size()) - 1));
    }

    if (llama_decode(m_embedCtx, batch) != 0) {
        qWarning() << "llama_decode failed";
        llama_batch_free(batch);
        return {};
    }

    int n_embd = llama_model_n_embd(m_embedModel);
    const float *emb = llama_get_embeddings_seq(m_embedCtx, 0);
    if (emb == nullptr) emb = llama_get_embeddings(m_embedCtx);

    QVector<float> result;
    if (emb != nullptr) {
        result.reserve(n_embd);
        for (int i = 0; i < n_embd; i++) result.append(emb[i]);
    }

    llama_batch_free(batch);
    return result;
}


//--------------------------------------------------------------------------------
auto GeneratorImmediate::generateText(
        QString& systemPrompt,
        bool isStream,
        QString& prompt
    ) -> QString
{
    if (!isValid()) return "Generator not initialized.";

    QString fullPrompt = systemPrompt + "\n\n" + prompt;
    
    auto vtok = common_tokenize(m_embedCtx, fullPrompt.toStdString(), true, true);
    if (vtok.empty()) return "";

    llama_memory_t kv_cache = llama_get_memory(m_embedCtx);
    llama_memory_seq_rm(kv_cache, -1, 0, -1);
    
    llama_batch batch = llama_batch_get_one(vtok.data(), static_cast<int>(vtok.size()));
    if (llama_decode(m_embedCtx, batch) != 0) return "Error: Decode failed.";

    // Initialize the Sampler (Greedy)
    // In 2026 llama.cpp, we use the unified sampler API
    struct llama_sampler * smpl = llama_sampler_init_greedy();
    
    QString response;
    int n_cur = batch.n_tokens;
   
    while (n_cur < DefaultMaxTokenGen) {
        llama_token tokId = llama_sampler_sample(smpl, m_embedCtx, -1);
        
        // Check for End of Generation
        if (llama_vocab_is_eog(llama_model_get_vocab(m_embedModel), tokId)) {
            break;
        }

        // Convert token to text
        std::array<char, DefaultBufferLength> buf;
        //char buf[128];
        int num = llama_token_to_piece(
            llama_model_get_vocab(m_embedModel),
            tokId,
            buf.data(),
            sizeof(buf),
            0,
            true
        );
        if (num > 0) {
            QString piece = QString::fromUtf8(buf.data(), num);
            response += piece;
            if (isStream) {
                // If you have a signal/callback for streaming, emit it here
                qDebug().noquote() << piece; 
            }
        }

        // Prepare the next token for the batch
        batch = llama_batch_get_one(&tokId, 1);
        
        if (llama_decode(m_embedCtx, batch) != 0) {
            break;
        }
        
        n_cur++;
    }

    llama_sampler_free(smpl);
    return response.trimmed();
}


