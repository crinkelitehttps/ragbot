#ifndef CONVERSATIONDATABASE_H
#define CONVERSATIONDATABASE_H

#include <QString>
#include <QVector>
#include <QSqlDatabase>

class ConversationDatabase
{
public:
    ConversationDatabase(const QString &dbName = "conversations.db");
    
    bool logConversation(const QVector<float> &queryEmbedding,
                        const QString &query,
                        const QString &researchResponse,
                        const QString &roleplayResponse = "");
    
    bool isInitialized() const;

private:
    void initializeSchema();

    QSqlDatabase m_db;
};

#endif // CONVERSATIONDATABASE_H
