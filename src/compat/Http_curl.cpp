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
        handle = curl_easy_init();
    }
    ~Impl() { if (handle) curl_easy_cleanup(handle); }
    CURL* handle { nullptr };
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

static void prepareHandle(CURL* curl,
                           const String& url,
                           const HttpClient::HeaderList& headers,
                           const Bytes& body,
                           curl_slist** outHeaders)
{
    curl_easy_reset(curl);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS,
                     reinterpret_cast<const char*>(body.data()));
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L);

    *outHeaders = buildHeaders(headers);
    if (*outHeaders)
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, *outHeaders);
}

// ─── public API ─────────────────────────────────────────────────────────────

auto HttpClient::post(const String& url,
                      const HeaderList& headers,
                      const Bytes& body) -> Response
{
    CURL* curl = m_impl->handle;
    Response r;
    if (!curl) { r.error = "curl_easy_init failed"; return r; }

    curl_slist* hdrs = nullptr;
    prepareHandle(curl, url, headers, body, &hdrs);
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
    return r;
}

auto HttpClient::postStreaming(const String& url,
                               const HeaderList& headers,
                               const Bytes& body,
                               const ChunkSink& sink) -> Response
{
    CURL* curl = m_impl->handle;
    Response r;
    if (!curl) { r.error = "curl_easy_init failed"; return r; }

    curl_slist* hdrs = nullptr;
    prepareHandle(curl, url, headers, body, &hdrs);
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
    return r;
}

auto HttpClient::postMany(const String& url,
                           const HeaderList& headers,
                           const Vector<Bytes>& bodies,
                           int timeoutMs) -> Vector<Response>
{
    const size_t n = static_cast<size_t>(bodies.size());
    Vector<Response> results(static_cast<int>(n));
    if (n == 0) return results;

    CURLM* multi = curl_multi_init();
    if (!multi) {
        for (auto& r : results) r.error = "curl_multi_init failed";
        return results;
    }

    Vector<CURL*> easies(static_cast<int>(n), nullptr);
    Vector<curl_slist*> headerLists(static_cast<int>(n), nullptr);

    for (size_t i = 0; i < n; ++i) {
        CURL* curl = curl_easy_init();
        if (!curl) {
            results[static_cast<int>(i)].error = "curl_easy_init failed";
            continue;
        }
        easies[static_cast<int>(i)] = curl;
        prepareHandle(curl, url, headers, bodies[static_cast<int>(i)],
                      &headerLists[static_cast<int>(i)]);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToBytes);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &results[static_cast<int>(i)].body);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, static_cast<long>(timeoutMs));
        curl_easy_setopt(curl, CURLOPT_PRIVATE, reinterpret_cast<void*>(static_cast<intptr_t>(i)));
        curl_multi_add_handle(multi, curl);
    }

    int stillRunning = 0;
    do {
        const CURLMcode mc = curl_multi_perform(multi, &stillRunning);
        if (mc != CURLM_OK) break;
        if (stillRunning) {
            const int pollMs = (timeoutMs > 0 && timeoutMs < 1000) ? timeoutMs : 1000;
            curl_multi_poll(multi, nullptr, 0, pollMs, nullptr);
        }
    } while (stillRunning > 0);

    CURLMsg* msg = nullptr;
    int msgsLeft = 0;
    while ((msg = curl_multi_info_read(multi, &msgsLeft)) != nullptr) {
        if (msg->msg != CURLMSG_DONE) continue;
        CURL* easy = msg->easy_handle;
        char* priv = nullptr;
        curl_easy_getinfo(easy, CURLINFO_PRIVATE, &priv);
        const auto idx = static_cast<int>(reinterpret_cast<intptr_t>(priv));
        Response& r = results[idx];
        if (msg->data.result == CURLE_OK) {
            long code = 0;
            curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, &code);
            r.statusCode = static_cast<int>(code);
        } else {
            r.error = curl_easy_strerror(msg->data.result);
        }
    }

    for (size_t i = 0; i < n; ++i) {
        if (easies[static_cast<int>(i)]) {
            curl_multi_remove_handle(multi, easies[static_cast<int>(i)]);
            curl_easy_cleanup(easies[static_cast<int>(i)]);
        }
        if (headerLists[static_cast<int>(i)])
            curl_slist_free_all(headerLists[static_cast<int>(i)]);
    }
    curl_multi_cleanup(multi);

    return results;
}

}  // namespace rb

#endif // !RAGBOT_USE_QT
