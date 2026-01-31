#ifndef RAGBOT_H
#define RAGBOT_H

#include <QCoreApplication>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include "common.h"
#include "db.h"
#include "llama.h"

struct RemoteLLMConfig {
    bool enabled = false;
    QString baseUrl = "http://192.168.0.97:8080/upstream/llama-3.2-8b-instruct";
    QString model = "llama-3.2-8b-instruct";
    int timeout = 60000;
};

// Configuration for roleplay
struct RoleplayConfig {
    bool enabled = true;
    QString characterName = "Survivor";
    QString characterBackground = "";
    QString baseUrl = "http://192.168.0.97:8080/upstream/llama-3.2-8b-instruct";
    QString model = "llama-3.2-8b-instruct";
    
    bool loadFromFile(const QString &filePath) {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qWarning() << "Failed to open character background file:" << filePath;
            return false;
        }
        characterBackground = QString::fromUtf8(file.readAll());
        file.close();
        return !characterBackground.isEmpty();
    }
};

class RemoteLLMClient
{
public:
    RemoteLLMClient(const RemoteLLMConfig &config)
        : m_config(config), m_manager(new QNetworkAccessManager())
    {
    }
    
    RemoteLLMClient(const QString &baseUrl, const QString &model, int timeout = 60000)
        : m_manager(new QNetworkAccessManager())
    {
        m_config.enabled = true;
        m_config.baseUrl = baseUrl;
        m_config.model = model;
        m_config.timeout = timeout;
    }
    
    ~RemoteLLMClient()
    {
        delete m_manager;
    }

    QString chat(const QString &systemPrompt, const QString &userMessage, bool stream)
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
        request["max_tokens"] = 20000;
        
        QJsonDocument doc(request);
        QByteArray jsonData = doc.toJson();
        
        QNetworkRequest netRequest;
        netRequest.setUrl(QUrl(m_config.baseUrl + "/v1/chat/completions"));
        netRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        netRequest.setTransferTimeout(m_config.timeout);
        
        QNetworkReply *reply = m_manager->post(netRequest, jsonData);
        
        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        
        QTimer timer;
        timer.setSingleShot(true);
        QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        timer.start(m_config.timeout);
        
        qDebug() << "stream"<< stream;
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

class RAGBot
{
public:
    RAGBot(const QString &embedModelPath, EmbeddingDatabase *db, 
           ConversationDatabase *convDb,
           const RemoteLLMConfig &llmConfig, const RoleplayConfig &rpConfig)
        : m_embedModelPath(embedModelPath), m_db(db), m_convDb(convDb),
          m_llmConfig(llmConfig), m_rpConfig(rpConfig),
          m_embedModel(nullptr), m_embedCtx(nullptr), 
          m_remoteLLM(nullptr), m_roleplayLLM(nullptr)
    {
        if (m_llmConfig.enabled) {
            m_remoteLLM = new RemoteLLMClient(m_llmConfig);
            qDebug() << "Research LLM enabled:" << m_llmConfig.baseUrl;
        } else {
            qDebug() << "Research LLM disabled - would use local models";
        }
        
        if (m_rpConfig.enabled) {
            m_roleplayLLM = new RemoteLLMClient(m_rpConfig.baseUrl, m_rpConfig.model, 60000);
            qDebug() << "Roleplay LLM enabled:" << m_rpConfig.baseUrl;
            qDebug() << "Character:" << m_rpConfig.characterName;
        } else {
            qDebug() << "Roleplay mode disabled";
        }
    }
    
    ~RAGBot()
    {
        cleanup();
        delete m_remoteLLM;
        delete m_roleplayLLM;
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
        if (m_rpConfig.enabled) {
            qDebug() << "Mode: Two-stage (Research + Roleplay)";
            qDebug() << "Character:" << m_rpConfig.characterName;
        } else {
            qDebug() << "Mode: Research only";
        }
        qDebug() << "Type your questions (or 'quit' to exit)";
        qDebug() << "Commands: 'toggle roleplay' to enable/disable stage 2\n";
        
        QTextStream in(stdin);
        
        while (true) {
            QTextStream(stdout) << "\nYou: " << Qt::flush;
            QString question = in.readLine().trimmed();
            
            if (question.isEmpty()) continue;
            if (question.toLower() == "quit" || question.toLower() == "exit") {
                qDebug() << "Goodbye!";
                break;
            }
            if (question.toLower() == "toggle roleplay") {
                m_rpConfig.enabled = !m_rpConfig.enabled;
                qDebug() << "Roleplay mode:" << (m_rpConfig.enabled ? "ENABLED" : "DISABLED");
                continue;
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
            "Answer the question based on the provided context documents. "
            "Be factual, concise, and cite specific game mechanics, items, or data when relevant.\n\n"
            "Context:\n%1\n\n"
            "Question: %2"
        ).arg(context, question);
        
        QString researchAnswer;
        qDebug() << context;
        qDebug() << question;
        qDebug() << researchAnswer;
        
        if (m_llmConfig.enabled) {
            qInfo() << "m_llmConfig.enabled";
            QTextStream(stdout) << "\nBot (Research): " << Qt::flush;
            researchAnswer = m_remoteLLM->chat("", researchPrompt, false);
            QTextStream(stdout) << "\n" << Qt::flush;
        } else {
            qDebug() << "Would call local research model here";
            researchAnswer = "[Research answer would appear here with local model]";
        }
        
        if (researchAnswer.isEmpty()) {
            qWarning() << "No response from research LLM";
            return;
        }
        
        QString roleplayAnswer;
        
        // Stage 2: Roleplay response
        if (m_rpConfig.enabled && m_roleplayLLM) {
            qInfo() << "m_rpConfig.enabled";
            qDebug() << "\n[Stage 2: Roleplay Response]";

            QFile roleplayPromptFile("roleplayPrompt.txt");
            QString rp;
            
            if (roleplayPromptFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                rp = QString::fromUtf8(roleplayPromptFile.readAll());
                roleplayPromptFile.close();
            } else {
                qWarning() << "Failed to open roleplayPrompt.txt";
            }
            
            QString roleplayPrompt = rp.arg("Survivor", researchAnswer, question);
            qDebug() << "thinking";
            
            QTextStream(stdout) << "\n" << m_rpConfig.characterName << ": " << Qt::flush;
            roleplayAnswer = m_roleplayLLM->chat(
                m_rpConfig.characterBackground,
                roleplayPrompt,
                true
            );
            QTextStream(stdout) << "\n" << Qt::flush;
            
            if (roleplayAnswer.isEmpty()) {
                qWarning() << "No response from roleplay LLM";
            }
        }
        
        // Log conversation to database
        if (!m_convDb->logConversation(queryEmb, question, researchAnswer, roleplayAnswer)) {
            qWarning() << "Failed to log conversation to database";
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
    ConversationDatabase *m_convDb;
    RemoteLLMConfig m_llmConfig;
    llama_model *m_embedModel;
    llama_context *m_embedCtx;
    RemoteLLMClient *m_remoteLLM;
    RemoteLLMClient *m_roleplayLLM;
    RoleplayConfig m_rpConfig;
    
};
#endif // RAGBOT_H
