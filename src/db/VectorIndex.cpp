#include "VectorIndex.h"
#include <QDebug>
#include <algorithm>
#include <numeric>

//--------------------------------------------------------------------------------
VectorIndex::VectorIndex(int dimensions)
    : m_dimensions(dimensions)
{}


//--------------------------------------------------------------------------------
auto VectorIndex::add(const QVector<float>& vec) -> int64_t
{
    if (vec.size() != m_dimensions) {
        qWarning() << "VectorIndex::add(): dimension mismatch —"
                   << vec.size() << "!=" << m_dimensions;
        return -1;
    }
    const int64_t id = static_cast<int64_t>(m_vectors.size());
    m_vectors.append(vec);
    return id;
}


//--------------------------------------------------------------------------------
auto VectorIndex::load(int64_t id, const QVector<float>& vec) -> void
{
    if (vec.size() != m_dimensions) {
        qWarning() << "VectorIndex::load(): dimension mismatch at id" << id;
        return;
    }
    const int idx = static_cast<int>(id);
    if (idx >= m_vectors.size()) {
        m_vectors.resize(idx + 1); // gaps remain as empty QVector<float>
    }
    m_vectors[idx] = vec;
}


//--------------------------------------------------------------------------------
auto VectorIndex::rollbackTo(int64_t priorSize) -> void
{
    if (priorSize >= 0 && priorSize < static_cast<int64_t>(m_vectors.size()))
        m_vectors.resize(static_cast<int>(priorSize));
}


//--------------------------------------------------------------------------------
auto VectorIndex::search(const QVector<float>& query, int topK) const -> QVector<Hit>
{
    if (query.size() != m_dimensions || m_vectors.isEmpty()) return {};

    const int k = qMin(topK, m_vectors.size());

    // Score every non-empty slot.  Dot product == cosine similarity for
    // L2-normalized vectors, so no division needed.
    QVector<Hit> scored;
    scored.reserve(m_vectors.size());

    const float* q = query.constData();
    for (int i = 0; i < m_vectors.size(); ++i) {
        if (m_vectors[i].isEmpty()) continue; // sparse gap from load()

        float dot = 0.0f;
        const float* v = m_vectors[i].constData();
        for (int d = 0; d < m_dimensions; ++d) {
            dot += q[d] * v[d];
        }
        scored.append({static_cast<int64_t>(i), dot});
    }

    // Move the top-K highest-similarity hits to the front, leave rest unsorted.
    std::partial_sort(
        scored.begin(),
        scored.begin() + k,
        scored.end(),
        [](const Hit& a, const Hit& b) { return a.similarity > b.similarity; }
    );
    scored.resize(k);
    return scored;
}
