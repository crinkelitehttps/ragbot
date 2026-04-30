// Phase 1 stub. Real Qt and libcurl backends land in Phase 5
// (compat/Http_qt.cpp and compat/Http_curl.cpp).
#include "Http.h"

namespace rb {

struct HttpClient::Impl {};

HttpClient::HttpClient()  : m_impl(std::make_unique<Impl>()) {}
HttpClient::~HttpClient() = default;

auto HttpClient::post(const String& /*url*/, const HeaderList& /*headers*/,
                      const Bytes& /*body*/) -> Response
{
    Response r;
    r.error = String("HttpClient::post not implemented yet (Phase 5)");
    return r;
}

auto HttpClient::postStreaming(const String& /*url*/, const HeaderList& /*headers*/,
                               const Bytes& /*body*/, const ChunkSink& /*sink*/) -> Response
{
    Response r;
    r.error = String("HttpClient::postStreaming not implemented yet (Phase 5)");
    return r;
}

}  // namespace rb
