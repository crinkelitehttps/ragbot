#ifndef EMBEDDINGGENERATOR_H
#define EMBEDDINGGENERATOR_H

#include "../compat/Types.h"

class EmbeddingGenerator
{
public:
    virtual ~EmbeddingGenerator() = default;
    [[nodiscard]] virtual auto generate(const rb::String& data) -> rb::Vector<float> = 0;
    [[nodiscard]] virtual auto isValid() const -> bool = 0;

    [[nodiscard]] virtual auto generateBatch(const rb::Vector<rb::String>& inputs)
        -> rb::Vector<rb::Vector<float>>
    {
        rb::Vector<rb::Vector<float>> out;
        out.reserve(inputs.size());
        for (const auto& input : inputs)
            out.push_back(generate(input));
        return out;
    }
};

#endif // EMBEDDINGGENERATOR_H
