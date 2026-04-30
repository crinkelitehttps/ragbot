#ifndef RERANKGENERATORIP_H
#define RERANKGENERATORIP_H

#include "../compat/Http.h"
#include "../compat/Json.h"
#include "RerankGenerator.h"

class RerankGeneratorIP : public RerankGenerator
{
public:
    explicit RerankGeneratorIP(const rb::Json& config);

    auto score(const rb::String& query,
               const rb::Vector<rb::String>& documents) -> rb::Vector<float> override;
    [[nodiscard]] auto isValid() const -> bool override { return m_isValid; }

private:
    static constexpr int DefaultTimeout { 60000 };

    rb::HttpClient m_http;
    rb::String     m_basePath;
    rb::String     m_modelName;
    int            m_timeout;
    bool           m_isValid;
};

#endif // RERANKGENERATORIP_H
