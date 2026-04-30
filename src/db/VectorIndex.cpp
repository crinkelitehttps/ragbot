#include "VectorIndex.h"
#include "../compat/Logging.h"
#include <algorithm>
#include <numeric>

VectorIndex::VectorIndex(int dimensions)
    : m_dimensions(dimensions)
{}

auto VectorIndex::add(const rb::Vector<float>& vec) -> int64_t
{
    if (static_cast<int>(vec.size()) != m_dimensions) {
        RAGBOT_LOG_WARN("VectorIndex::add(): dimension mismatch — {} != {}",
                        static_cast<int>(vec.size()), m_dimensions);
        return -1;
    }
    const int64_t id = static_cast<int64_t>(m_vectors.size());
    m_vectors.push_back(vec);
    return id;
}

auto VectorIndex::load(int64_t id, const rb::Vector<float>& vec) -> void
{
    if (static_cast<int>(vec.size()) != m_dimensions) {
        RAGBOT_LOG_WARN("VectorIndex::load(): dimension mismatch at id {}", id);
        return;
    }
    const auto idx = static_cast<size_t>(id);
    if (idx >= m_vectors.size())
        m_vectors.resize(idx + 1);
    m_vectors[idx] = vec;
}

auto VectorIndex::rollbackTo(int64_t priorSize) -> void
{
    if (priorSize >= 0 && priorSize < static_cast<int64_t>(m_vectors.size()))
        m_vectors.resize(static_cast<size_t>(priorSize));
}

auto VectorIndex::search(const rb::Vector<float>& query, int topK) const -> rb::Vector<Hit>
{
    if (static_cast<int>(query.size()) != m_dimensions || m_vectors.empty()) return {};

    const int k = std::min(topK, static_cast<int>(m_vectors.size()));

    rb::Vector<Hit> scored;
    scored.reserve(m_vectors.size());

    const float* q = query.data();
    for (size_t i = 0; i < m_vectors.size(); ++i) {
        if (m_vectors[i].empty()) continue; // sparse gap from load()

        float dot = 0.0f;
        const float* v = m_vectors[i].data();
        for (int d = 0; d < m_dimensions; ++d)
            dot += q[d] * v[d];
        scored.push_back({static_cast<int64_t>(i), dot});
    }

    std::partial_sort(
        scored.begin(),
        scored.begin() + k,
        scored.end(),
        [](const Hit& a, const Hit& b) { return a.similarity > b.similarity; }
    );
    scored.resize(static_cast<size_t>(k));
    return scored;
}
