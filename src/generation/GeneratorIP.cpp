#include "GeneratorIP.h"
#include "../compat/Logging.h"
#include "../compat/Strings.h"
#include "../ConfigKeys.h"
#include <cstdio>
#include <string_view>


GeneratorIP::GeneratorIP(const rb::Json& config)
    : m_modelName(config.stringValue(ConfigKeys::ModelName))
    , m_timeout(config.intValue(ConfigKeys::Timeout, DefaultTimeout))
    , m_isValid(false)
{
    m_basePath = config.stringValue(ConfigKeys::BasePath);
    if (rb::str_empty(m_basePath)) {
        const rb::String legacy = config.stringValue(ConfigKeys::RemotePath);
        if (!rb::str_empty(legacy)) {
            RAGBOT_LOG_WARN("GeneratorIP: 'remotePath' is deprecated — use 'basePath'");
            m_basePath = legacy;
        }
    }
    m_isValid = !rb::str_empty(m_basePath);
    if (!m_isValid)
        RAGBOT_LOG_WARN("GeneratorIP: no basePath in config — disabled");
}


static auto parseOneEmbedding(const rb::Json& item) -> rb::Vector<float>
{
    const rb::Json embArray = item.value("embedding");
    if (!embArray.isArray()) return {};
    rb::Vector<float> result;
    result.reserve(static_cast<int>(embArray.size()));
    for (const auto& val : embArray.items())
        result.push_back(static_cast<float>(val.toDouble()));
    return result;
}

auto GeneratorIP::parseEmbeddingResponse(const rb::Bytes& data) -> rb::Vector<float>
{
    const rb::Json doc = rb::Json::parse(data);
    if (!doc.isValid()) return {};

    const rb::Json dataArr = doc.value("data");
    if (!dataArr.isArray() || dataArr.size() == 0) {
        RAGBOT_LOG_WARN("GeneratorIP::parseEmbeddingResponse(): empty data array");
        return {};
    }
    return parseOneEmbedding(dataArr.at(0));
}


auto GeneratorIP::generate(const rb::String& data) -> rb::Vector<float>
{
    if (!m_isValid) return {};

    rb::Json body = rb::Json::object();
    body.setString("input", data);
    body.setString("model", m_modelName);

    const rb::HttpClient::HeaderList headers {
        { rb::from_std("Content-Type"), rb::from_std("application/json") }
    };
    const rb::Bytes bodyBytes = body.dump(true);
    const rb::HttpClient::Response resp =
        m_http.post(m_basePath + rb::from_std("v1/embeddings"), headers, bodyBytes);

    if (!rb::str_empty(resp.error)) {
        RAGBOT_LOG_WARN("GeneratorIP::generate() error: {}", rb::to_std(resp.error));
        return {};
    }
    return parseEmbeddingResponse(resp.body);
}

auto GeneratorIP::generateBatch(const rb::Vector<rb::String>& inputs)
    -> rb::Vector<rb::Vector<float>>
{
    if (!m_isValid || inputs.empty()) return {};

    static constexpr size_t MaxBatch { 64 };

    const rb::HttpClient::HeaderList headers {
        { rb::from_std("Content-Type"), rb::from_std("application/json") }
    };
    const rb::String endpoint = m_basePath + rb::from_std("v1/embeddings");

    rb::Vector<rb::Vector<float>> results(inputs.size());
    const size_t total = inputs.size();

    for (size_t start = 0; start < total; start += MaxBatch) {
        const size_t end = (start + MaxBatch < total) ? start + MaxBatch : total;

        rb::Json inputArr = rb::Json::array();
        for (size_t idx = start; idx < end; ++idx)
            inputArr.append(rb::Json::fromString(inputs[static_cast<int>(idx)]));

        rb::Json body = rb::Json::object();
        body.set("input", inputArr);
        body.setString("model", m_modelName);

        const rb::HttpClient::Response resp = m_http.post(endpoint, headers, body.dump(true));
        if (!rb::str_empty(resp.error)) {
            RAGBOT_LOG_WARN("GeneratorIP::generateBatch() error: {}", rb::to_std(resp.error));
            return {};
        }

        const rb::Json doc = rb::Json::parse(resp.body);
        if (!doc.isValid()) {
            RAGBOT_LOG_WARN("GeneratorIP::generateBatch(): invalid response");
            return {};
        }
        const rb::Json dataArr = doc.value("data");
        if (!dataArr.isArray()) {
            RAGBOT_LOG_WARN("GeneratorIP::generateBatch(): missing data array");
            return {};
        }

        const size_t batchSize = end - start;
        if (dataArr.size() < batchSize) {
            RAGBOT_LOG_WARN("GeneratorIP::generateBatch(): got {} embeddings, expected {}",
                            dataArr.size(), batchSize);
            return {};
        }
        for (size_t idx = 0; idx < batchSize; ++idx)
            results[static_cast<int>(start + idx)] = parseOneEmbedding(dataArr.at(idx));
    }

    return results;
}


auto GeneratorIP::parseStaticResponse(const rb::Bytes& data) -> rb::String
{
    const rb::Json doc = rb::Json::parse(data);
    if (!doc.isValid()) return {};

    const rb::Json choices = doc.value("choices");
    if (!choices.isArray() || choices.size() == 0) {
        RAGBOT_LOG_WARN("GeneratorIP::parseStaticResponse(): empty choices array");
        return {};
    }
    return choices.at(0).value("message").stringValue("content");
}


auto GeneratorIP::parseStreamChunk(std::string_view data) -> rb::String
{
    rb::String result;
    size_t pos = 0;
    while (pos < data.size()) {
        const size_t nl = data.find('\n', pos);
        const std::string_view line = (nl == std::string_view::npos)
            ? data.substr(pos)
            : data.substr(pos, nl - pos);
        pos = (nl == std::string_view::npos) ? data.size() : nl + 1;

        const std::string_view prefix = "data: ";
        if (line.substr(0, prefix.size()) != prefix) continue;
        const std::string_view json = line.substr(prefix.size());
        if (json == "[DONE]" || json.empty()) continue;

        const rb::Json doc = rb::Json::parse(json);
        if (!doc.isValid()) continue;

        const rb::Json choices = doc.value("choices");
        if (!choices.isArray() || choices.size() == 0) continue;
        const rb::Json delta = choices.at(0).value("delta");
        if (delta.contains("content"))
            result += delta.stringValue("content");
    }
    return result;
}


auto GeneratorIP::generateText(
        const rb::String& systemPrompt,
        bool isStream,
        const rb::String& prompt,
        const TokenSink& tokenSink
) -> rb::String
{
    if (!m_isValid) return {};

    rb::Json messages = rb::Json::array();
    if (!rb::str_empty(systemPrompt)) {
        rb::Json sys = rb::Json::object();
        sys.setString("role", "system");
        sys.setString("content", systemPrompt);
        messages.append(sys);
    }
    rb::Json user = rb::Json::object();
    user.setString("role", "user");
    user.setString("content", prompt);
    messages.append(user);

    rb::Json body = rb::Json::object();
    body.setString("model",   m_modelName);
    body.setBool("stream",    isStream);
    body.set("messages",      messages);
    body.setDouble("temperature", TextGenerator::DefaultTemp);
    body.setInt("max_tokens",     TextGenerator::DefaultMaxTokens);

    const rb::HttpClient::HeaderList headers {
        { rb::from_std("Content-Type"), rb::from_std("application/json") }
    };
    const rb::Bytes bodyBytes = body.dump(true);
    const rb::String endpoint = m_basePath + rb::from_std("v1/chat/completions");

    if (!isStream) {
        const rb::HttpClient::Response resp = m_http.post(endpoint, headers, bodyBytes);
        if (!rb::str_empty(resp.error)) {
            RAGBOT_LOG_WARN("GeneratorIP::generateText() error: {}", rb::to_std(resp.error));
            return {};
        }
        return parseStaticResponse(resp.body);
    }

    // Streaming path
    rb::String accumulated;
    const rb::HttpClient::Response resp = m_http.postStreaming(
        endpoint, headers, bodyBytes,
        [&](std::string_view chunk) {
            const rb::String parsed = parseStreamChunk(chunk);
            accumulated += parsed;
            if (tokenSink)
                tokenSink(rb::StringView(parsed));
            else {
                std::fputs(rb::to_std(parsed).c_str(), stdout);
                std::fflush(stdout);
            }
        });

    if (!rb::str_empty(resp.error))
        RAGBOT_LOG_WARN("GeneratorIP::generateText() stream error: {}", rb::to_std(resp.error));

    return accumulated;
}
