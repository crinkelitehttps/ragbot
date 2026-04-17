#ifndef VECTORINDEX_H
#define VECTORINDEX_H

#include <QVector>
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

    // Appends a normalized vector and returns its assigned ID.
    // Returns -1 and warns if the dimension does not match.
    auto add(const QVector<float>& vec) -> int64_t;

    // Inserts a normalized vector at a specific ID (for warm-start from DB).
    // Grows the internal store as needed; gaps are left as empty slots.
    auto load(int64_t id, const QVector<float>& vec) -> void;

    // Returns up to topK hits ordered by descending similarity.
    // Returns an empty vector if the index is empty or dimensions mismatch.
    [[nodiscard]] auto search(const QVector<float>& query, int topK) const -> QVector<Hit>;

    [[nodiscard]] auto ntotal() const -> int64_t { return static_cast<int64_t>(m_vectors.size()); }
    [[nodiscard]] auto dimensions() const -> int  { return m_dimensions; }

private:
    int m_dimensions;
    QVector<QVector<float>> m_vectors; // m_vectors[id] == embedding for that id
};

#endif // VECTORINDEX_H
