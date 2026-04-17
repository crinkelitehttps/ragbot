#ifndef PARSER_H
#define PARSER_H

#include <QString>
#include <QJsonObject>
#include <QHash>

class Parser
{
public:
    struct Chunk {
        QString embedText; // flattened natural-language text — used to generate the embedding vector
        QString content;   // verbatim content stored in the DB and retrieved for the LLM context
    };

    virtual ~Parser() = default;
    virtual auto toChunks(const QVariant& dataVariant) -> QVector<Chunk> = 0;

    // Optional: provide a global id→object registry so the parser can resolve
    // inheritance (e.g. CDDA's copy-from). Default is a no-op.
    virtual void setObjectRegistry(const QHash<QString, QJsonObject>&) {}

protected:
    Parser() {}
};

#endif // PARSER_H
