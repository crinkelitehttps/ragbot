#ifndef VECTORINDEX_H
#define VECTORINDEX_H

#include "../compat/Types.h"
#include <cstdint>

// Brute-force cosine similarity index for dense float vectors.
//
// All vectors — both stored and query — must be L2-normalized before use.
// For unit vectors, cosine_similarity(a, b) == dot_product(a, b), so search
// reduces to finding the K highest dot products, which is computed here via a
// linear scan + std::partial_sort.
//
// Insertion order determines each vector's ID (0-based). load() is provided
// for warm-starting from a persisted store where IDs may arrive out-of-order.
class VectorIndex
{
public:
    struct Hit {
        int64_t id;
        float   similarity; // cosine similarity in [-1, 1]; 1.0 == identical
    };

    explicit VectorIndex(int dimensions);

    auto add(const rb::Vector<float>& vec) -> int64_t;
    auto load(int64_t id, const rb::Vector<float>& vec) -> void;
    auto rollbackTo(int64_t priorSize) -> void;

    [[nodiscard]] auto search(const rb::Vector<float>& query, int topK) const
        -> rb::Vector<Hit>;

    [[nodiscard]] auto ntotal() const -> int64_t
        { return static_cast<int64_t>(m_vectors.size()); }
    [[nodiscard]] auto dimensions() const -> int { return m_dimensions; }

private:
    int m_dimensions;
    rb::Vector<rb::Vector<float>> m_vectors;
};

#endif // VECTORINDEX_H
