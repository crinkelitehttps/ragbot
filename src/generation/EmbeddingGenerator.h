#ifndef EMBEDDINGGENERATOR_H
#define EMBEDDINGGENERATOR_H

#include "../compat/Types.h"

class EmbeddingGenerator
{
public:
    virtual ~EmbeddingGenerator() = default;
    [[nodiscard]] virtual auto generate(const rb::String& data) -> rb::Vector<float> = 0;
    [[nodiscard]] virtual auto isValid() const -> bool = 0;
};

#endif // EMBEDDINGGENERATOR_H
