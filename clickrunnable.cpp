#include "clickrunnable.h"
#include "click.h"

#include <QDebug>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QThread>
#include <QTimer>

ClickRunnable::ClickRunnable(Click* click) :
    m_click(click)
{

}

ClickRunnable::~ClickRunnable()
{

}

void ClickRunnable::run()
{
    HttpHandle http;
    RetType ret_type;

    http.setUA(m_click->getUa());
    ret_type = http.request(QUrl(m_url));

    if (ret_type == FATAL) {
        m_click->onErrorOffer(m_url);
    }
}

HttpHandle::HttpHandle()
{
    // init error string
    errors << "no redirecting offer given";
}

RetType HttpHandle::request(QUrl url)
{
    RetType ret;
    int retry_counter = 0;

    while (1) {
        qDebug() << "http handle request url:" << url;
        if (url.toString().startsWith("https://itunes.apple.com") ||
                url.toString().startsWith("itms-appss")) {
            return SUCCESS;
        }
        ret = sendClick(url);
        if (ret == FAILED) {
            if (retry_counter == 10) {
                qDebug() << "Max retries";
                return FAILED;
            }
            retry_counter++;
            QThread::sleep(2);
        } else if (ret == REDIRECT) {
            url = QUrl(m_redirect_url);
        } else if (ret == SUCCESS) {
            return SUCCESS;
        } else if (ret == FATAL) {
            return FATAL;
        }
    }
}

RetType HttpHandle::sendClick(QUrl url)
{
    QEventLoop eventLoop;
    QTimer timer;
    timer.setInterval(6000);
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, &eventLoop, &QEventLoop::quit);
    QNetworkAccessManager mgr;
    QByteArray enc = url.toEncoded();
    QUrl encUrl(enc);
    QNetworkRequest qnr(encUrl);
    qnr.setHeader(QNetworkRequest::UserAgentHeader, m_ua);
    QNetworkReply* reply = mgr.get(qnr);
    QObject::connect(reply, &QNetworkReply::finished, &eventLoop, &QEventLoop::quit);
    timer.start();
    eventLoop.exec();

    if (!timer.isActive()) {
        qDebug() << "request timeout";
        reply->close();
        reply->deleteLater();
        return FAILED;
    }

    QString reply_str = reply->readAll();
    // qDebug() << "reply text:" << reply_str;

    foreach (QString err, errors) {
        if (reply_str.contains(err)) {
            // qDebug() << "Error occurred";
            qDebug() << "error reply text:" << reply_str;
            return FATAL;
        }
    }

    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "url:" << reply->url() << "\n" << "reply error: " << reply->errorString();
        reply->close();
        reply->deleteLater();
        return FAILED;
    }

    int http_status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    // qDebug() << "http:" << http_status;

    reply->close();
    reply->deleteLater();

    if (http_status == 302 ||
            http_status == 301) {
        QVariant possibleRedirectUrl =
                reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
        // qDebug() << possibleRedirectUrl.toString();
        // request(QUrl(possibleRedirectUrl.toString())/*, mgr*/);
        m_redirect_url = possibleRedirectUrl.toString();
        return REDIRECT;
    }

    return SUCCESS;
}
