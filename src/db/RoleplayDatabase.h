#ifndef ROLEPLAYDATABASE_H
#define ROLEPLAYDATABASE_H

#include <QString>
#include <QVector>
#include <QSqlDatabase>

class RoleplayDatabase
{
public:
    RoleplayDatabase(const QString &dbName = "conversations.db");
    
    bool logConversation(const QVector<float> &queryEmbedding,
                        const QString &query,
                        const QString &researchResponse,
                        const QString &roleplayResponse = "");
    
    bool isInitialized() const;

private:
    void initializeSchema();

    QSqlDatabase m_db;
};

#endif // ROLEPLAYDATABASE_H
