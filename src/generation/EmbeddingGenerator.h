#ifndef EMBEDDINGGENERATOR_H
#define EMBEDDINGGENERATOR_H

#include <QString>
#include <QVector>

class EmbeddingGenerator
{
public:
    virtual ~EmbeddingGenerator() = default;
    [[nodiscard]] virtual auto generate(const QString& data) -> QVector<float> = 0;
    [[nodiscard]] virtual auto isValid() const -> bool = 0;

};

#endif // EMBEDDINGGENERATOR_H
