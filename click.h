#ifndef CLICK_H
#define CLICK_H

#include <QMap>
#include <QObject>
#include <QRunnable>
#include <QSharedPointer>
#include <QtCore>

class QThreadPool;
class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

class Click : public QObject
{
    Q_OBJECT
public:
    explicit Click(QObject *parent = NULL);

    QString getUa();
    void removeOffer(QString offer_url);

signals:

public slots:
    void startRequest();

private:
    QThreadPool* m_thread_pool;

    QStringList offers;
    QStringList err_offers;
    QStringList uas;
    qint64 total_click;
    int pool_size;
    qint64 idfa_counter;
};

#endif // CLICK_H
