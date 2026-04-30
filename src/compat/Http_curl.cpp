// libcurl implementation of rb::HttpClient.
// Compiled only when RAGBOT_USE_QT is NOT defined.
#include "Http.h"

#ifndef RAGBOT_USE_QT
#include <curl/curl.h>
#include <cstdlib>
#include <cstring>

namespace rb {

struct HttpClient::Impl
{
    Impl()
    {
        static bool once = false;
        if (!once) { curl_global_init(CURL_GLOBAL_DEFAULT); once = true; }
    }
};

HttpClient::HttpClient()  : m_impl(std::make_unique<Impl>()) {}
HttpClient::~HttpClient() = default;

// ─── write callbacks ────────────────────────────────────────────────────────

static size_t writeToBytes(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    auto* buf = static_cast<Bytes*>(userdata);
    const size_t n = size * nmemb;
    buf->insert(buf->end(),
                reinterpret_cast<const unsigned char*>(ptr),
                reinterpret_cast<const unsigned char*>(ptr) + n);
    return n;
}

struct StreamCtx {
    const HttpClient::ChunkSink& sink;
};

static size_t writeToSink(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    auto* ctx = static_cast<StreamCtx*>(userdata);
    const size_t n = size * nmemb;
    if (ctx->sink)
        ctx->sink(std::string_view(ptr, n));
    return n;
}

// ─── helpers ────────────────────────────────────────────────────────────────

static curl_slist* buildHeaders(const HttpClient::HeaderList& headers)
{
    curl_slist* list = nullptr;
    for (const auto& hdr : headers) {
        const std::string line = hdr.first + ": " + hdr.second;
        list = curl_slist_append(list, line.c_str());
    }
    return list;
}

static CURL* makeEasy(const String& url,
                      const HttpClient::HeaderList& headers,
                      const Bytes& body,
                      curl_slist** outHeaders)
{
    CURL* curl = curl_easy_init();
    if (!curl) return nullptr;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS,
                     reinterpret_cast<const char*>(body.data()));
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L);

    *outHeaders = buildHeaders(headers);
    if (*outHeaders)
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, *outHeaders);

    return curl;
}

// ─── public API ─────────────────────────────────────────────────────────────

auto HttpClient::post(const String& url,
                      const HeaderList& headers,
                      const Bytes& body) -> Response
{
    curl_slist* hdrs = nullptr;
    CURL* curl = makeEasy(url, headers, body, &hdrs);
    Response r;
    if (!curl) { r.error = "curl_easy_init failed"; return r; }

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToBytes);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &r.body);

    const CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK) {
        long code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
        r.statusCode = static_cast<int>(code);
    } else {
        r.error = curl_easy_strerror(res);
    }

    curl_slist_free_all(hdrs);
    curl_easy_cleanup(curl);
    return r;
}

auto HttpClient::postStreaming(const String& url,
                               const HeaderList& headers,
                               const Bytes& body,
                               const ChunkSink& sink) -> Response
{
    curl_slist* hdrs = nullptr;
    CURL* curl = makeEasy(url, headers, body, &hdrs);
    Response r;
    if (!curl) { r.error = "curl_easy_init failed"; return r; }

    StreamCtx ctx { sink };
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToSink);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);

    const CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK) {
        long code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
        r.statusCode = static_cast<int>(code);
    } else {
        r.error = curl_easy_strerror(res);
    }

    curl_slist_free_all(hdrs);
    curl_easy_cleanup(curl);
    return r;
}

}  // namespace rb

#endif // !RAGBOT_USE_QT
