#ifndef EMBEDDINGGENERATOR_H
#define EMBEDDINGGENERATOR_H

#include "../compat/Types.h"
#include <cstddef>
#include <functional>

class EmbeddingGenerator
{
public:
    // Fired periodically from generateBatch. `doneInputs` is the cumulative
    // count of inputs whose embeddings are populated in `partial`. Slots
    // [0, doneInputs) may contain empty vectors for sub-batches that failed.
    using ProgressCallback = std::function<void(
        std::size_t doneInputs,
        const rb::Vector<rb::Vector<float>>& partial)>;

    virtual ~EmbeddingGenerator() = default;
    [[nodiscard]] virtual auto generate(const rb::String& data) -> rb::Vector<float> = 0;
    [[nodiscard]] virtual auto isValid() const -> bool = 0;

    [[nodiscard]] virtual auto generateBatch(
        const rb::Vector<rb::String>& inputs,
        const ProgressCallback& onProgress = {}
    ) -> rb::Vector<rb::Vector<float>>
    {
        rb::Vector<rb::Vector<float>> out;
        out.reserve(inputs.size());
        for (const auto& input : inputs)
            out.push_back(generate(input));
        if (onProgress)
            onProgress(static_cast<std::size_t>(out.size()), out);
        return out;
    }
};

#endif // EMBEDDINGGENERATOR_H
