#include "GeneratorIP.h"
#include "../compat/Io.h"
#include "../compat/Logging.h"
#include "../compat/Strings.h"
#include "../ConfigKeys.h"
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <string_view>


static auto resolveVastFile(std::string_view filePrefix,
                             std::string_view excludePrefix = {}) -> rb::String
{
    const char* home = std::getenv("HOME");
    if (!home) {
        RAGBOT_LOG_WARN("GeneratorIP: $HOME not set — cannot locate .network file");
        return {};
    }

    const std::filesystem::path dir = std::filesystem::path(home) / ".vast";
    if (!std::filesystem::is_directory(dir)) {
        RAGBOT_LOG_WARN("GeneratorIP: ~/.vast/ not found");
        return {};
    }

    const std::string prefix(filePrefix);
    const std::string exclude(excludePrefix);

    rb::Vector<rb::String> matches;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        const std::string name = entry.path().filename().string();
        if (name.rfind(prefix, 0) != 0) continue;
        if (name.size() < filePrefix.size() + 9) continue;
        if (name.substr(name.size() - 8) != ".network") continue;
        if (!exclude.empty() && name.rfind(exclude, 0) == 0) continue;
        matches.push_back(entry.path().string());
    }

    if (matches.empty()) {
        RAGBOT_LOG_WARN("GeneratorIP: no ~/.vast/{}*.network file found", prefix);
        return {};
    }
    if (matches.size() > 1) {
        RAGBOT_LOG_WARN("GeneratorIP: multiple ~/.vast/{}*.network files — using first alphabetically", prefix);
        std::sort(matches.begin(), matches.end());
    }

    bool ok = false;
    rb::String url = rb::read_file_text(matches[0], &ok);
    if (!ok || url.empty()) {
        RAGBOT_LOG_WARN("GeneratorIP: failed to read {}", matches[0]);
        return {};
    }
    while (!url.empty() && (url.back() == '\n' || url.back() == '\r' || url.back() == ' '))
        url.pop_back();

    RAGBOT_LOG_INFO("GeneratorIP: resolved endpoint {} from {}", url, matches[0]);
    return url;
}

static auto resolveVastNetworkEndpoint() -> rb::String
{
    return resolveVastFile("ragbot-", "ragbot-text-");
}

static auto resolveVastTextEndpoint() -> rb::String
{
    return resolveVastFile("ragbot-text-");
}


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
    if (rb::str_empty(m_basePath)) {
        const rb::String platform = config.stringValue(ConfigKeys::Platform);
        if (platform == ConfigKeys::PlatformVastAi)
            m_basePath = resolveVastNetworkEndpoint();
        else if (platform == ConfigKeys::PlatformVastAiText)
            m_basePath = resolveVastTextEndpoint();
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


// llama-server fails an entire request if any single input exceeds --ubatch-size
// tokens. Dense CDDA JSON (short IDs, punctuation) has been observed at ~1.05
// chars/token, so 2800 chars produced ~2660 tokens — well over UBATCH=2048.
// 1700 chars yields ~1620 tokens at the observed ratio, comfortably under 2048
// even if density reaches 1.0 chars/token (ASCII worst case).
static constexpr size_t MaxInputChars { 1700 };

static auto truncateForEmbedding(const rb::String& input) -> rb::String
{
    if (input.size() <= MaxInputChars) return input;
    rb::String truncated = input.substr(0, MaxInputChars);
    // If the cut fell inside a multi-byte UTF-8 sequence, strip the partial
    // bytes — nlohmann/json rejects incomplete sequences on encode.
    if ((static_cast<unsigned char>(input[MaxInputChars]) & 0xC0) == 0x80) {
        while (!truncated.empty() &&
               (static_cast<unsigned char>(truncated.back()) & 0xC0) == 0x80)
            truncated.pop_back();
        if (!truncated.empty()) truncated.pop_back();  // drop leader byte
    }
    RAGBOT_LOG_WARN("GeneratorIP: truncating input from {} to {} chars",
                    static_cast<int>(input.size()), static_cast<int>(truncated.size()));
    return truncated;
}


static auto buildBatchBody(const rb::Vector<rb::String>& inputs,
                           size_t start, size_t end,
                           const rb::String& modelName) -> rb::Bytes
{
    rb::Json inputArr = rb::Json::array();
    for (size_t idx = start; idx < end; ++idx)
        inputArr.append(rb::Json::fromString(
            truncateForEmbedding(inputs[static_cast<int>(idx)])));

    rb::Json body = rb::Json::object();
    body.set("input", inputArr);
    body.setString("model", modelName);
    return body.dump(true);
}


static auto bodySnippet(const rb::Bytes& body, size_t maxLen = 200) -> std::string
{
    const size_t len = body.size() < maxLen ? body.size() : maxLen;
    return std::string(reinterpret_cast<const char*>(body.data()), len);
}


static auto decodeBatchResponse(const rb::HttpClient::Response& resp, size_t expectedSize)
    -> rb::Vector<rb::Vector<float>>
{
    if (!rb::str_empty(resp.error)) {
        RAGBOT_LOG_WARN("GeneratorIP::generateBatch() curl error: {}", rb::to_std(resp.error));
        return {};
    }
    const rb::Json doc = rb::Json::parse(resp.body);
    if (!doc.isValid()) {
        RAGBOT_LOG_WARN("GeneratorIP::generateBatch(): invalid response (status {}, {} bytes): {}",
                        resp.statusCode, static_cast<int>(resp.body.size()),
                        bodySnippet(resp.body));
        return {};
    }
    const rb::Json dataArr = doc.value("data");
    if (!dataArr.isArray()) {
        RAGBOT_LOG_WARN("GeneratorIP::generateBatch(): missing data array (status {}): {}",
                        resp.statusCode, bodySnippet(resp.body));
        return {};
    }
    if (static_cast<size_t>(dataArr.size()) < expectedSize) {
        RAGBOT_LOG_WARN("GeneratorIP::generateBatch(): got {} embeddings, expected {} (status {})",
                        dataArr.size(), expectedSize, resp.statusCode);
        return {};
    }
    rb::Vector<rb::Vector<float>> out;
    out.reserve(static_cast<int>(expectedSize));
    for (size_t idx = 0; idx < expectedSize; ++idx)
        out.push_back(parseOneEmbedding(dataArr.at(idx)));
    return out;
}


auto GeneratorIP::generate(const rb::String& data) -> rb::Vector<float>
{
    if (!m_isValid) return {};

    rb::Json body = rb::Json::object();
    body.setString("input", truncateForEmbedding(data));
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

auto GeneratorIP::generateBatch(
    const rb::Vector<rb::String>& inputs,
    const ProgressCallback& onProgress
) -> rb::Vector<rb::Vector<float>>
{
    if (!m_isValid || inputs.empty()) return {};

    // Larger batches amortise HTTP overhead; bounded concurrency keeps the
    // server's slot queue from backing up past the request timeout.
    static constexpr size_t MaxBatch      { 256 };
    static constexpr size_t MaxConcurrent { 8 };

    const rb::HttpClient::HeaderList headers {
        { rb::from_std("Content-Type"), rb::from_std("application/json") }
    };
    const rb::String endpoint = m_basePath + rb::from_std("v1/embeddings");

    const size_t total = static_cast<size_t>(inputs.size());

    // Build one request body per sub-batch of MaxBatch inputs.
    rb::Vector<rb::Bytes> bodies;
    rb::Vector<size_t> batchStarts;
    for (size_t start = 0; start < total; start += MaxBatch) {
        const size_t end = (start + MaxBatch < total) ? start + MaxBatch : total;
        bodies.push_back(buildBatchBody(inputs, start, end, m_modelName));
        batchStarts.push_back(start);
    }

    rb::Vector<rb::Vector<float>> results(static_cast<int>(total));
    const int totalBatches = static_cast<int>(bodies.size());
    int failedBatches = 0;

    RAGBOT_LOG_INFO("GeneratorIP::generateBatch(): {} inputs, {} sub-batches × {} = {} waves",
                    static_cast<int>(total), totalBatches, static_cast<int>(MaxConcurrent),
                    (totalBatches + static_cast<int>(MaxConcurrent) - 1)
                        / static_cast<int>(MaxConcurrent));

    // Dispatch in waves of MaxConcurrent sub-batches.
    for (int waveStart = 0; waveStart < totalBatches;
         waveStart += static_cast<int>(MaxConcurrent)) {
        const int waveEnd = (waveStart + static_cast<int>(MaxConcurrent) < totalBatches)
            ? waveStart + static_cast<int>(MaxConcurrent)
            : totalBatches;

        rb::Vector<rb::Bytes> waveBodies;
        for (int batchIdx = waveStart; batchIdx < waveEnd; ++batchIdx)
            waveBodies.push_back(bodies[batchIdx]);

        const int waveNum = (waveStart / static_cast<int>(MaxConcurrent)) + 1;
        RAGBOT_LOG_INFO("GeneratorIP::generateBatch(): wave {} dispatching sub-batches {}–{}",
                        waveNum, waveStart, waveEnd - 1);

        const rb::Vector<rb::HttpClient::Response> responses =
            m_http.postMany(endpoint, headers, waveBodies);

        RAGBOT_LOG_INFO("GeneratorIP::generateBatch(): wave {} responses received",
                        waveNum);

        for (int waveIdx = 0; waveIdx < responses.size(); ++waveIdx) {
            const int batchIdx = waveStart + waveIdx;
            const size_t start = batchStarts[batchIdx];
            const size_t end = (start + MaxBatch < total) ? start + MaxBatch : total;
            const size_t batchSize = end - start;

            rb::HttpClient::Response resp = responses[waveIdx];
            rb::Vector<rb::Vector<float>> decoded = decodeBatchResponse(resp, batchSize);

            if (decoded.empty()) {
                RAGBOT_LOG_WARN("GeneratorIP::generateBatch(): retrying sub-batch {}", batchIdx);
                resp = m_http.post(endpoint, headers, bodies[batchIdx]);
                decoded = decodeBatchResponse(resp, batchSize);
            }

            if (decoded.empty()) {
                RAGBOT_LOG_WARN("GeneratorIP::generateBatch(): sub-batch {} permanently failed; "
                                "{} chunks will be skipped", batchIdx, static_cast<int>(batchSize));
                ++failedBatches;
                continue;  // leave results[start..end) as default-empty
            }

            for (size_t idx = 0; idx < batchSize; ++idx)
                results[static_cast<int>(start + idx)] = decoded[static_cast<int>(idx)];
        }

        if (onProgress) {
            const size_t done = static_cast<size_t>(waveEnd) * MaxBatch < total
                ? static_cast<size_t>(waveEnd) * MaxBatch
                : total;
            onProgress(done, results);
        }
    }

    if (failedBatches > 0)
        RAGBOT_LOG_WARN("GeneratorIP::generateBatch(): {} of {} sub-batches failed",
                        failedBatches, totalBatches);

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
