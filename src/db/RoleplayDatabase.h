#ifndef ROLEPLAYDATABASE_H
#define ROLEPLAYDATABASE_H

#include <QString>
#include <QVector>
#include <sqlite3.h>

class RoleplayDatabase
{
public:
    explicit RoleplayDatabase(const QString& dbName = "conversations.db");
    ~RoleplayDatabase();

    RoleplayDatabase(const RoleplayDatabase&)            = delete;
    RoleplayDatabase& operator=(const RoleplayDatabase&) = delete;

    auto logConversation(
        const QVector<float>& queryEmbedding,
        const QString& query,
        const QString& researchResponse,
        const QString& roleplayResponse = {}
    ) -> bool;

    [[nodiscard]] auto isOpen() const -> bool { return m_db != nullptr; }

private:
    void initializeSchema();
    auto exec(const char* sql) -> bool;

    sqlite3* m_db {};
};

#endif // ROLEPLAYDATABASE_H
