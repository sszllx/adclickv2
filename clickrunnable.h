#ifndef CLICKRUNNABLE_H
#define CLICKRUNNABLE_H

#include <QNetworkProxy>
#include <QObject>
#include <QRunnable>
#include <QUrl>

class Click;
class QNetworkAccessManager;
class QNetworkReply;
class QNetworkRequest;

typedef enum {
    SUCCESS = 0,
    REDIRECT,
    FAILED,
    FATAL,
} RetType;

class HttpHandle : public QObject
{
    Q_OBJECT
public:
    HttpHandle();

    void setUA(QString ua) {m_ua = ua;}

public slots:
    RetType request(QUrl url);

private:
    RetType sendClick(QUrl url);

    QString m_ua;
    QString m_redirect_url;
};

class ClickRunnable : public QRunnable
{
public:
    ClickRunnable(Click* click);
    ~ClickRunnable();

    void setUrl(QString url) { m_url = url; }

    void run() Q_DECL_OVERRIDE;

private:
    Click* m_click;
    QString m_url;
};

#endif // CLICKRUNNABLE_H
