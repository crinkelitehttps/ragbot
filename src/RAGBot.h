#ifndef RAGBOT_H
#define RAGBOT_H

class RAGBotSession;

class RAGBot
{
public:
    explicit RAGBot(RAGBotSession& session);
    void start();

private:
    RAGBotSession& m_session;
};

#endif // RAGBOT_H
