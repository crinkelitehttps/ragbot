#include "GeneratorImmediate.h"
#include "../config/ConfigGenerator.h"
#include <QJsonObject>
#include <QDir>
#include <QDebug>
#include "llama.h"
#include "common.h" // Required for common_tokenize, common_batch_add, etc.

//--------------------------------------------------------------------------------
GeneratorImmediate::GeneratorImmediate(ConfigGenerator& generatorConfig) 
    : Generator(generatorConfig)
    , m_config(generatorConfig)
{
    // Initialize backend once
    static bool backend_initialized = false;
    if (!backend_initialized) {
        llama_backend_init();
        backend_initialized = true;
    }

    llama_log_set([](ggml_log_level level, const char * text, void * user_data) {
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

    if (!m_embedModel) {
        qCritical() << "GeneratorImmediate: Failed to load model";
        return;
    }

    // Context Params
    llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_ctx     = 2048;
    ctx_params.n_batch   = 2048;
    ctx_params.embeddings = true;
    ctx_params.pooling_type = LLAMA_POOLING_TYPE_MEAN; // Required for Nomic

    m_embedCtx = llama_init_from_model(m_embedModel, ctx_params);

    if (!m_embedCtx) {
        qCritical() << "GeneratorImmediate: Failed to create context";
    }
}

//--------------------------------------------------------------------------------
GeneratorImmediate::~GeneratorImmediate()
{
    if (m_embedCtx) {
        llama_free(m_embedCtx);
        m_embedCtx = nullptr;
    }
    if (m_embedModel) {
        llama_model_free(m_embedModel);
        m_embedModel = nullptr;
    }
}


//--------------------------------------------------------------------------------
QVector<float> GeneratorImmediate::generate(const QString& data) 
{
    if (!m_embedCtx) return {};

    std::string preparedData = "search_document: " + data.toStdString();

    auto vt = common_tokenize(m_embedCtx, preparedData, true, true);

    const auto safe_limit = llama_n_ubatch(m_embedCtx); 
    
    if (vt.size() > safe_limit) {
        qWarning() << "Truncating tokens from" << vt.size() << "to" << safe_limit;
        vt.resize(safe_limit);
    }

    llama_memory_t kv_cache = llama_get_memory(m_embedCtx);
    llama_memory_seq_rm(kv_cache, -1, 0, -1);

    llama_batch batch = llama_batch_init(vt.size(), 0, 1);
    for (size_t i = 0; i < vt.size(); i++) {
        common_batch_add(batch, vt[i], i, {0}, (i == vt.size() - 1));
    }

    if (llama_decode(m_embedCtx, batch) != 0) {
        qWarning() << "llama_decode failed";
        llama_batch_free(batch);
        return {};
    }

    int n_embd = llama_model_n_embd(m_embedModel);
    const float *emb = llama_get_embeddings_seq(m_embedCtx, 0);
    if (!emb) emb = llama_get_embeddings(m_embedCtx);

    QVector<float> result;
    if (emb) {
        result.reserve(n_embd);
        for (int i = 0; i < n_embd; i++) result.append(emb[i]);
    }

    llama_batch_free(batch);
    return result;
}


//--------------------------------------------------------------------------------
QString GeneratorImmediate::generateText(
        QString& systemMessage,
        QString& prompt,
        bool isStream
    ) 
{
    if (!isValid()) return "Generator not initialized.";

    QString fullPrompt = systemMessage + "\n\n" + prompt;
    
    auto vt = common_tokenize(m_embedCtx, fullPrompt.toStdString(), true, true);
    if (vt.empty()) return "";

    llama_memory_t kv_cache = llama_get_memory(m_embedCtx);
    llama_memory_seq_rm(kv_cache, -1, 0, -1);
    
    llama_batch batch = llama_batch_get_one(vt.data(), vt.size());
    if (llama_decode(m_embedCtx, batch) != 0) return "Error: Decode failed.";

    // 4. Initialize the Sampler (Greedy)
    // In 2026 llama.cpp, we use the unified sampler API
    struct llama_sampler * smpl = llama_sampler_init_greedy();
    
    QString response;
    int n_cur = batch.n_tokens;
    int n_predict = 512; // Max tokens to generate
    
    // 5. Generation Loop
    while (n_cur < n_predict) {
        llama_token id = llama_sampler_sample(smpl, m_embedCtx, -1);
        
        // Check for End of Generation
        if (llama_vocab_is_eog(llama_model_get_vocab(m_embedModel), id)) {
            break;
        }

        // Convert token to text
        char buf[128];
        int n = llama_token_to_piece(llama_model_get_vocab(m_embedModel), id, buf, sizeof(buf), 0, true);
        if (n > 0) {
            QString piece = QString::fromUtf8(buf, n);
            response += piece;
            if (isStream) {
                // If you have a signal/callback for streaming, emit it here
                qDebug().noquote() << piece; 
            }
        }

        // Prepare the next token for the batch
        batch = llama_batch_get_one(&id, 1);
        
        if (llama_decode(m_embedCtx, batch) != 0) {
            break;
        }
        
        n_cur++;
    }

    llama_sampler_free(smpl);
    return response.trimmed();
}


//--------------------------------------------------------------------------------
bool GeneratorImmediate::isValid() 
{
    return m_embedModel != nullptr && m_embedCtx != nullptr;
}
