#ifndef RAGBOT_COMPAT_HTTP_H
#define RAGBOT_COMPAT_HTTP_H

#include "Types.h"

#include <functional>
#include <memory>
#include <string_view>
#include <utility>

namespace rb {

// Backend-agnostic HTTP client.
// Http_qt.cpp implements using QNetworkAccessManager (RAGBOT_USE_QT builds).
// Http_curl.cpp implements using libcurl (non-Qt builds).
class HttpClient
{
public:
    using Header     = std::pair<String, String>;
    using HeaderList = Vector<Header>;
    // ChunkSink always receives raw UTF-8 bytes (independent of Qt/non-Qt).
    using ChunkSink  = std::function<void(std::string_view)>;

    struct Response {
        int    statusCode { 0 };
        Bytes  body;
        String error;
        [[nodiscard]] auto ok() const -> bool { return statusCode >= 200 && statusCode < 300; }
    };

    HttpClient();
    ~HttpClient();

    HttpClient(const HttpClient&)            = delete;
    HttpClient& operator=(const HttpClient&) = delete;

    // Blocking POST with full-body response.
    auto post(const String& url, const HeaderList& headers, const Bytes& body) -> Response;

    // Blocking POST with chunked body callback (for SSE streaming).
    // The sink is called as bytes arrive on the calling thread; the framer
    // (e.g. SSE "data: ... \n\n" boundary detection) lives in the caller.
    auto postStreaming(const String& url, const HeaderList& headers, const Bytes& body,
                       const ChunkSink& sink) -> Response;

    // Fire multiple POSTs to the same URL concurrently, return responses in input order.
    // Qt build: uses QNetworkAccessManager's native async dispatch (single thread).
    // Curl build: falls back to sequential.
    auto postMany(const String& url, const HeaderList& headers,
                  const Vector<Bytes>& bodies, int timeoutMs = 240000) -> Vector<Response>;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

}  // namespace rb

#endif
