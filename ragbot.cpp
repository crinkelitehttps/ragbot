#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QTextStream>
#include <QDebug>
#include <QEventLoop>
#include <QTimer>
#include <QDir>
#include <QFileInfo>
#include <cmath>

#include "llama.h"
#include "common.h"

// Configuration for remote LLM
struct RemoteLLMConfig {
    bool enabled = false;
    QString baseUrl = "http://192.168.1.100:1234/v1";  // Change to your Windows PC IP
    QString model = "qwen2.5-v1-7b";
    int timeout = 60000;  // 60 seconds
};

class RemoteLLMClient
{
public:
    RemoteLLMClient(const RemoteLLMConfig &config)
        : m_config(config), m_manager(new QNetworkAccessManager())
    {
    }
    
    ~RemoteLLMClient()
    {
        delete m_manager;
    }

    QString chat(const QString &systemPrompt, const QString &userMessage, bool stream = false)
    {
        QJsonObject request;
        request["model"] = m_config.model;
        request["stream"] = stream;
        
        QJsonArray messages;
        
        if (!systemPrompt.isEmpty()) {
            QJsonObject sysMsg;
            sysMsg["role"] = "system";
            sysMsg["content"] = systemPrompt;
            messages.append(sysMsg);
        }
        
        QJsonObject userMsg;
        userMsg["role"] = "user";
        userMsg["content"] = userMessage;
        messages.append(userMsg);
        
        request["messages"] = messages;
        request["temperature"] = 0.7;
        request["max_tokens"] = 2000;
        
        QJsonDocument doc(request);
        QByteArray jsonData = doc.toJson();
        
        QNetworkRequest netRequest;
        netRequest.setUrl(QUrl(m_config.baseUrl + "/chat/completions"));
        netRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        netRequest.setTransferTimeout(m_config.timeout);
        
        QNetworkReply *reply = m_manager->post(netRequest, jsonData);
        
        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        
        QTimer timer;
        timer.setSingleShot(true);
        QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        timer.start(m_config.timeout);
        
        if (stream) {
            QString fullResponse;
            QObject::connect(reply, &QNetworkReply::readyRead, [&]() {
                QByteArray data = reply->readAll();
                QString text(data);
                
                // Parse SSE format
                QStringList lines = text.split("\n");
                for (const QString &line : lines) {
                    if (line.startsWith("data: ")) {
                        QString jsonStr = line.mid(6).trimmed();
                        if (jsonStr == "[DONE]") continue;
                        
                        QJsonDocument streamDoc = QJsonDocument::fromJson(jsonStr.toUtf8());
                        if (!streamDoc.isNull()) {
                            QJsonObject obj = streamDoc.object();
                            if (obj.contains("choices")) {
                                QJsonArray choices = obj["choices"].toArray();
                                if (!choices.isEmpty()) {
                                    QJsonObject choice = choices[0].toObject();
                                    if (choice.contains("delta")) {
                                        QJsonObject delta = choice["delta"].toObject();
                                        if (delta.contains("content")) {
                                            QString content = delta["content"].toString();
                                            fullResponse += content;
                                            QTextStream(stdout) << content << Qt::flush;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            });
            
            loop.exec();
            
            if (timer.isActive()) {
                timer.stop();
                reply->deleteLater();
                return fullResponse;
            } else {
                reply->abort();
                reply->deleteLater();
                qWarning() << "Request timed out";
                return QString();
            }
        }
        
        loop.exec();
        
        QString response;
        
        if (timer.isActive()) {
            timer.stop();
            
            if (reply->error() == QNetworkReply::NoError) {
                QByteArray responseData = reply->readAll();
                QJsonDocument responseDoc = QJsonDocument::fromJson(responseData);
                
                if (!responseDoc.isNull()) {
                    QJsonObject obj = responseDoc.object();
                    if (obj.contains("choices")) {
                        QJsonArray choices = obj["choices"].toArray();
                        if (!choices.isEmpty()) {
                            QJsonObject choice = choices[0].toObject();
                            if (choice.contains("message")) {
                                QJsonObject message = choice["message"].toObject();
                                response = message["content"].toString();
                            }
                        }
                    }
                }
            } else {
                qWarning() << "Network error:" << reply->errorString();
            }
        } else {
            reply->abort();
            qWarning() << "Request timed out";
        }
        
        reply->deleteLater();
        return response;
    }

private:
    RemoteLLMConfig m_config;
    QNetworkAccessManager *m_manager;
};

class EmbeddingDatabase
{
public:
    EmbeddingDatabase(const QString &dbName = "embeddings.db")
    {
        m_db = QSqlDatabase::addDatabase("QSQLITE");
        m_db.setDatabaseName(dbName);
        
        if (!m_db.open()) {
            qCritical() << "Failed to open database:" << m_db.lastError().text();
        }
    }
    
    struct SearchResult {
        QString content;
        QString sourceFile;
        QString itemId;
        float similarity;
    };
    
    QVector<SearchResult> search(const QVector<float> &queryEmbedding, int topK = 10)
    {
        QSqlQuery query(m_db);
        query.prepare("SELECT content, source_file, item_id, embedding FROM embeddings");
        
        if (!query.exec()) {
            qCritical() << "Query failed:" << query.lastError().text();
            return {};
        }
        
        QVector<SearchResult> results;
        
        while (query.next()) {
            QString content = query.value(0).toString();
            QString sourceFile = query.value(1).toString();
            QString itemId = query.value(2).toString();
            QByteArray embBlob = query.value(3).toByteArray();
            
            const float *embData = reinterpret_cast<const float*>(embBlob.constData());
            int embSize = embBlob.size() / sizeof(float);
            
            if (embSize != queryEmbedding.size()) continue;
            
            float similarity = cosineSimilarity(queryEmbedding, embData, embSize);
            
            SearchResult result;
            result.content = content;
            result.sourceFile = sourceFile;
            result.itemId = itemId;
            result.similarity = similarity;
            results.append(result);
        }
        
        std::sort(results.begin(), results.end(), 
                 [](const SearchResult &a, const SearchResult &b) {
                     return a.similarity > b.similarity;
                 });
        
        if (results.size() > topK) {
            results.resize(topK);
        }
        
        return results;
    }

private:
    float cosineSimilarity(const QVector<float> &a, const float *b, int size)
    {
        float dotProduct = 0.0f;
        float normA = 0.0f;
        float normB = 0.0f;
        
        for (int i = 0; i < size; i++) {
            dotProduct += a[i] * b[i];
            normA += a[i] * a[i];
            normB += b[i] * b[i];
        }
        
        if (normA == 0.0f || normB == 0.0f) return 0.0f;
        
        return dotProduct / (std::sqrt(normA) * std::sqrt(normB));
    }

    QSqlDatabase m_db;
};

class RAGBot
{
public:
    RAGBot(const QString &embedModelPath, EmbeddingDatabase *db, 
           const RemoteLLMConfig &llmConfig)
        : m_embedModelPath(embedModelPath), m_db(db), m_llmConfig(llmConfig),
          m_embedModel(nullptr), m_embedCtx(nullptr), m_remoteLLM(nullptr)
    {
        if (m_llmConfig.enabled) {
            m_remoteLLM = new RemoteLLMClient(m_llmConfig);
            qDebug() << "Remote LLM enabled:" << m_llmConfig.baseUrl;
        } else {
            qDebug() << "Remote LLM disabled - would use local models";
        }
    }
    
    ~RAGBot()
    {
        cleanup();
        delete m_remoteLLM;
    }
    
    bool initialize()
    {
        qDebug() << "Initializing embedding model...";
        
        llama_log_set([](ggml_log_level level, const char * text, void * user_data) {
            if (level == GGML_LOG_LEVEL_ERROR) {
                fprintf(stderr, "%s", text);
            }
        }, nullptr);
        
        llama_backend_init();
        
        llama_model_params model_params = llama_model_default_params();
        m_embedModel = llama_model_load_from_file(m_embedModelPath.toUtf8().constData(), model_params);
        
        if (!m_embedModel) {
            qCritical() << "Failed to load embedding model";
            return false;
        }
        
        llama_context_params ctx_params = llama_context_default_params();
        ctx_params.n_ctx = 2048;
        ctx_params.n_batch = 2048;
        ctx_params.n_ubatch = 2048;
        ctx_params.embeddings = true;
        ctx_params.pooling_type = LLAMA_POOLING_TYPE_MEAN;
        
        m_embedCtx = llama_init_from_model(m_embedModel, ctx_params);
        
        if (!m_embedCtx) {
            qCritical() << "Failed to create embedding context";
            return false;
        }
        
        qDebug() << "Embedding model ready";
        return true;
    }
    
    void startChatLoop()
    {
        if (!initialize()) {
            QCoreApplication::exit(1);
            return;
        }
        
        qDebug() << "\n=== RAG Bot Ready ===";
        qDebug() << "Type your questions (or 'quit' to exit)\n";
        
        QTextStream in(stdin);
        
        while (true) {
            QTextStream(stdout) << "\nYou: " << Qt::flush;
            QString question = in.readLine().trimmed();
            
            if (question.isEmpty()) continue;
            if (question.toLower() == "quit" || question.toLower() == "exit") {
                qDebug() << "Goodbye!";
                break;
            }
            
            processQuestion(question);
        }
        
        cleanup();
        QCoreApplication::quit();
    }

private:
    QVector<float> generateEmbedding(const QString &text)
    {
        std::vector<llama_token> tokens = common_tokenize(m_embedCtx, text.toStdString(), true);
        if (tokens.empty()) return {};
        
        int max_tokens = llama_n_ctx(m_embedCtx) - 10;
        if (tokens.size() > max_tokens) {
            tokens.resize(max_tokens);
        }
        
        llama_batch batch = llama_batch_init(tokens.size(), 0, 1);
        for (size_t i = 0; i < tokens.size(); i++) {
            common_batch_add(batch, tokens[i], i, {0}, true);
        }
        
        if (llama_encode(m_embedCtx, batch) != 0) {
            llama_batch_free(batch);
            return {};
        }
        
        llama_synchronize(m_embedCtx);
        
        int n_embd = llama_model_n_embd(m_embedModel);
        const float *embeddings = llama_get_embeddings_seq(m_embedCtx, 0);
        
        if (!embeddings) {
            embeddings = llama_get_embeddings(m_embedCtx);
        }
        
        QVector<float> result;
        if (embeddings) {
            result.resize(n_embd);
            for (int i = 0; i < n_embd; i++) {
                result[i] = embeddings[i];
            }
        }
        
        llama_batch_free(batch);
        return result;
    }
    
    void processQuestion(const QString &question)
    {
        qDebug() << "\n[Searching database...]";
        
        QVector<float> queryEmb = generateEmbedding(question);
        if (queryEmb.isEmpty()) {
            qWarning() << "Failed to generate query embedding";
            return;
        }
        
        auto results = m_db->search(queryEmb, 10);
        
        if (results.isEmpty()) {
            qDebug() << "No relevant documents found";
            return;
        }
        
        qDebug() << QString("Found %1 relevant documents").arg(results.size());
        
        // Build context from top results
        QString context;
        for (int i = 0; i < results.size(); i++) {
            context += QString("Document %1 (similarity: %2):\n%3\n\n")
                .arg(i + 1)
                .arg(results[i].similarity, 0, 'f', 3)
                .arg(results[i].content);
        }
        
        // Stage 1: Research with remote LLM
        qDebug() << "\n[Stage 1: Research Query]";
        
        QString researchPrompt = QString(
            "You are a helpful assistant analyzing Cataclysm: Dark Days Ahead game data. "
            "Answer the question based on the provided context documents.\n\n"
            "Context:\n%1\n\n"
            "Question: %2"
        ).arg(context, question);
        
        QString researchAnswer;
        
        if (m_llmConfig.enabled) {
            QTextStream(stdout) << "\nBot (Research): " << Qt::flush;
            researchAnswer = m_remoteLLM->chat("", researchPrompt, true);
            QTextStream(stdout) << "\n" << Qt::flush;
        } else {
            qDebug() << "Would call local Qwen 7B model here";
            researchAnswer = "[Research answer would appear here with local model]";
        }
        
        // Stage 2: Roleplay response (optional - can enable later)
        // For now, just show research answer
        
        if (researchAnswer.isEmpty()) {
            qWarning() << "No response from LLM";
        }
    }
    
    void cleanup()
    {
        if (m_embedCtx) {
            llama_free(m_embedCtx);
            m_embedCtx = nullptr;
        }
        if (m_embedModel) {
            llama_model_free(m_embedModel);
            m_embedModel = nullptr;
        }
        llama_backend_free();
    }

private:
    QString m_embedModelPath;
    EmbeddingDatabase *m_db;
    RemoteLLMConfig m_llmConfig;
    RemoteLLMClient *m_remoteLLM;
    
    llama_model *m_embedModel;
    llama_context *m_embedCtx;
};

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    QString embedModelPath = QDir::homePath() + "/.ollama/models/blobs/nomic-embed-text-v1.5.f32.gguf";
    QString dbPath = "embeddings.db";
    
    if (!QFile::exists(embedModelPath)) {
        qCritical() << "Embedding model not found:" << embedModelPath;
        return 1;
    }
    
    if (!QFile::exists(dbPath)) {
        qCritical() << "Database not found:" << dbPath;
        return 1;
    }
    
    // Configure remote LLM
    RemoteLLMConfig llmConfig;
    llmConfig.enabled = true;  // Set to false to use local models
    llmConfig.baseUrl = "http://192.168.0.97:1234/v1";  // Change to your Windows PC IP
    llmConfig.model = "qwen2.5-v1-7b";
    llmConfig.timeout = 60000;
    
    EmbeddingDatabase db(dbPath);
    RAGBot bot(embedModelPath, &db, llmConfig);
    
    QTimer::singleShot(0, [&bot]() {
        bot.startChatLoop();
    });
    
    return app.exec();
}

