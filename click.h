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
    explicit Click(QObject *parent = NULL);

    QString getUa();
    void onErrorOffer(QString offer_url);

    typedef enum {
        NAME = 0,
        CONTENT,
        COMMENTS,
        CHUNK_COMM,
    } PARSESTAT;

signals:

public slots:
    void startRequest();

private:
    void writeLog();
    QThreadPool* m_thread_pool;

    QStringList uas;
    qint64 total_click;
    int pool_size;
    qint64 idfa_counter;
    QList<OfferItem *> offer_items;
};

#endif // CLICK_H
