#ifndef ROLEPLAYDATABASE_H
#define ROLEPLAYDATABASE_H

#include "../compat/Types.h"
#include <sqlite3.h>

class RoleplayDatabase
{
public:
    explicit RoleplayDatabase(const rb::String& dbName = "conversations.db");
    ~RoleplayDatabase();

    RoleplayDatabase(const RoleplayDatabase&)            = delete;
    RoleplayDatabase& operator=(const RoleplayDatabase&) = delete;

    auto logConversation(
        const rb::Vector<float>& queryEmbedding,
        const rb::String& query,
        const rb::String& researchResponse,
        const rb::String& roleplayResponse = {}
    ) -> bool;

    [[nodiscard]] auto isOpen() const -> bool { return m_db != nullptr; }

private:
    void initializeSchema();
    auto exec(const char* sql) -> bool;

    sqlite3* m_db {};
};

#endif // ROLEPLAYDATABASE_H
