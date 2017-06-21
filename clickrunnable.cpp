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

    QString ua = m_click->getUa();
    http.setUA(ua);
    http.request(QUrl(m_url));
}

HttpHandle::HttpHandle()
{

}

void HttpHandle::request(QUrl url)
{
    RetType ret;
    int retry_counter = 0;

    while (1) {
        qDebug() << "url::" << url;
        if (url.toString().startsWith("https://itunes.apple.com")) {
            break;
        }
        ret = sendClick(url);
        if (ret == FAILED) {
            if (retry_counter == 10) {
                qDebug() << "Max retries";
                break;
            }
            retry_counter++;
            QThread::sleep(2);
        } else if (ret == REDIRECT) {
            url = QUrl(m_redirect_url);
        } else if (ret == SUCCESS) {
            break;
        }
    }
}

HttpHandle::RetType HttpHandle::sendClick(QUrl url)
{
    QEventLoop eventLoop;
    QTimer timer;
    timer.setInterval(6000);
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, &eventLoop, &QEventLoop::quit);
    QNetworkAccessManager mgr;
    QNetworkRequest qnr(url);
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

    qDebug() << "reply text:" << reply->readAll();

    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "error:" << reply->error() << "reply error: " << reply->errorString();
        reply->close();
        reply->deleteLater();
        return FAILED;
    }

    int http_status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    qDebug() << "http:" << http_status;

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
