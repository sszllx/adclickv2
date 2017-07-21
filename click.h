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

class TimerThread : public QThread
{
    Q_OBJECT
public:
    TimerThread();

    void run() Q_DECL_OVERRIDE;

};

class OfferItem
{
public:
    OfferItem(QString name);

    QString name();
    void addOffer(QString offer);
    QStringList getOffers();
    QString offer();
    void advancedIndex();
    void logClick();
    QString clickInfo();

private:
    QString offer_name;
    int curOffer;
    QStringList offers;
    qint64 click_counter;
    QMap<int, qint64> offer_map;
};

class Click : public QObject
{
    Q_OBJECT
public:
    static Click *getInstance();

    QString getUa();
    void onErrorOffer(QString offer_url);

    void setWangmeng(QString wangmeng) {
        m_wangmeng = wangmeng;
    }

    typedef enum {
        NAME = 0,
        CONTENT,
        COMMENTS,
        CHUNK_COMM,
    } PARSESTAT;
    bool should_quit;

signals:
    void reloadID();

public slots:
    void startRequest();
    void onTimeout();
    void onReloadID();

private:
    explicit Click(QObject *parent = NULL);
    void writeLog();
    void getIdFromServer(bool no_id);

    QThreadPool* m_thread_pool;

    QStringList uas;
    qint64 total_click;
    int pool_size;
    qint64 idfa_counter;
    QList<OfferItem *> offer_items;
    static Click* m_self;

    QTimer *m_timer;
    TimerThread *m_thread;
    QString server_index;
    QString server_ip;
    QString md5_str;
    uint check_interval;
    QString m_wangmeng;
};

#endif // CLICK_H
