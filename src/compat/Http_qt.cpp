// Qt implementation of rb::HttpClient — wraps QNetworkAccessManager.
// Compiled only when RAGBOT_USE_QT is defined (CMakeLists.txt gates this).
#include "Http.h"
#include "Strings.h"

#ifdef RAGBOT_USE_QT
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

namespace rb {

struct HttpClient::Impl
{
    QNetworkAccessManager mgr;
};

HttpClient::HttpClient()  : m_impl(std::make_unique<Impl>()) {}
HttpClient::~HttpClient() = default;

static auto runLoop(QNetworkReply* reply, int timeoutMs) -> bool
{
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(reply,  &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout,         &loop, &QEventLoop::quit);
    timer.start(timeoutMs);
    loop.exec();
    if (timer.isActive()) { timer.stop(); return true; }
    reply->abort();
    return false;
}

static auto makeHeaders(const HttpClient::HeaderList& headers) -> QNetworkRequest
{
    QNetworkRequest req;
    for (const auto& hdr : headers)
        req.setRawHeader(QByteArray::fromStdString(hdr.first.toStdString()),
                         QByteArray::fromStdString(hdr.second.toStdString()));
    return req;
}

auto HttpClient::post(const String& url,
                      const HeaderList& headers,
                      const Bytes& body) -> Response
{
    QNetworkRequest req = makeHeaders(headers);
    req.setUrl(QUrl(url));

    QNetworkReply* reply = m_impl->mgr.post(req, body);
    Response r;
    if (runLoop(reply, 240000)) {
        if (reply->error() == QNetworkReply::NoError) {
            r.statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            r.body = reply->readAll();
        } else {
            r.error = reply->errorString();
        }
    } else {
        r.error = from_std("request timed out");
    }
    reply->deleteLater();
    return r;
}

auto HttpClient::postStreaming(const String& url,
                               const HeaderList& headers,
                               const Bytes& body,
                               const ChunkSink& sink) -> Response
{
    QNetworkRequest req = makeHeaders(headers);
    req.setUrl(QUrl(url));

    QNetworkReply* reply = m_impl->mgr.post(req, body);
    QObject::connect(reply, &QNetworkReply::readyRead, [&]() {
        const QByteArray chunk = reply->readAll();
        if (sink) sink(std::string_view(chunk.constData(), static_cast<size_t>(chunk.size())));
    });

    Response r;
    if (runLoop(reply, 240000)) {
        r.statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError)
            r.error = reply->errorString();
    } else {
        r.error = from_std("streaming request timed out");
    }
    QObject::disconnect(reply, &QNetworkReply::readyRead, nullptr, nullptr);
    reply->deleteLater();
    return r;
}

}  // namespace rb

#endif // RAGBOT_USE_QT
