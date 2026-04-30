#include "RerankGeneratorIP.h"
#include "../compat/Logging.h"
#include "../compat/Strings.h"
#include "../ConfigKeys.h"


RerankGeneratorIP::RerankGeneratorIP(const rb::Json& config)
    : m_modelName(config.stringValue(ConfigKeys::ModelName))
    , m_timeout(config.intValue(ConfigKeys::Timeout, DefaultTimeout))
    , m_isValid(false)
{
    m_basePath = config.stringValue(ConfigKeys::BasePath);
    if (rb::str_empty(m_basePath)) {
        const rb::String legacy = config.stringValue(ConfigKeys::RemotePath);
        if (!rb::str_empty(legacy)) {
            RAGBOT_LOG_WARN("RerankGeneratorIP: 'remotePath' is deprecated — use 'basePath'");
            m_basePath = legacy;
        }
    }
    m_isValid = !rb::str_empty(m_basePath);
    if (!m_isValid)
        RAGBOT_LOG_WARN("RerankGeneratorIP: no basePath in config — disabled");
}


auto RerankGeneratorIP::score(const rb::String& query,
                              const rb::Vector<rb::String>& documents) -> rb::Vector<float>
{
    if (!m_isValid) return {};

    rb::Json docs = rb::Json::array();
    for (const auto& doc : documents) docs.append(rb::Json::fromString(doc));

    rb::Json body = rb::Json::object();
    body.setString("model",   m_modelName);
    body.setString("query",   query);
    body.set("documents",     docs);
    body.setBool("return_documents", false);

    const rb::HttpClient::HeaderList headers {
        { rb::from_std("Content-Type"), rb::from_std("application/json") }
    };
    const rb::Bytes bodyBytes = body.dump(true);
    const rb::HttpClient::Response resp =
        m_http.post(m_basePath + rb::from_std("v1/rerank"), headers, bodyBytes);

    rb::Vector<float> scores(documents.size(), 0.0f);

    if (!rb::str_empty(resp.error)) {
        RAGBOT_LOG_WARN("RerankGeneratorIP::score() error: {}", rb::to_std(resp.error));
        return scores;
    }

    const rb::Json doc = rb::Json::parse(resp.body);
    const rb::Json results = doc.value("results");
    if (!results.isArray() || results.size() == 0) {
        RAGBOT_LOG_WARN("RerankGeneratorIP::score(): empty results in response — returning zero scores");
    } else if (results.size() < documents.size()) {
        RAGBOT_LOG_WARN("RerankGeneratorIP::score(): got {} results for {} documents",
                        static_cast<int>(results.size()), static_cast<int>(documents.size()));
    }

    for (const auto& r : results.items()) {
        const int idx = r.intValue("index", -1);
        if (idx >= 0 && static_cast<size_t>(idx) < scores.size())
            scores[static_cast<size_t>(idx)] =
                static_cast<float>(r.doubleValue("relevance_score"));
    }

    return scores;
}
