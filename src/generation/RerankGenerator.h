#ifndef RERANKGENERATOR_H
#define RERANKGENERATOR_H

#include <QStringList>
#include <QVector>

class RerankGenerator
{
public:
    virtual ~RerankGenerator() = default;
    // Returns a relevance score per document in input order.
    virtual auto score(const QString& query, const QStringList& documents) -> QVector<float> = 0;
    virtual auto isValid() const -> bool = 0;
};

#endif // RERANKGENERATOR_H
