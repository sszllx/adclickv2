#include "click.h"
#include "clickrunnable.h"

#include <QDateTime>
#include <QTimer>

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>

#define MAX_THREAD_NUM 16

Click *Click::m_self = NULL;

Click::Click(QObject *parent) : QObject(parent),
    should_quit(false),
    m_thread_pool(QThreadPool::globalInstance()),
    total_click(0),
    pool_size(MAX_THREAD_NUM),
    idfa_counter(0),
    check_interval(60000*30)
{
    qDebug() << QCoreApplication::applicationDirPath() + "/config.txt";
    QFile config(QCoreApplication::applicationDirPath() + "/config.txt");
    if (config.open(QIODevice::ReadOnly)) {
        while(!config.atEnd()) {
            QString ps = config.readLine();
            QStringList ps_pars = ps.split("=");
            if (ps_pars[0] == "pool_size") {
                bool ok =false;
                uint size = ps_pars[1].toUInt(&ok);
                if (ok) {
                    pool_size = size;
                    qDebug() << "pool size:" << size;
                }
            } else if (ps_pars[0] == "index") {
                server_index = ps_pars[1].trimmed();
            } else if (ps_pars[0] == "server") {
                server_ip = ps_pars[1].trimmed();
            } else if (ps_pars[0] == "check_interval") {
                bool ok =false;
                uint interval = ps_pars[1].toUInt(&ok);
                if (ok) {
                    check_interval = interval;
                    qDebug() << "interval:" << check_interval;
                }
            }
        }
    }

    m_thread_pool->setMaxThreadCount(pool_size);

    QFile offer_file(QCoreApplication::applicationDirPath() + "/offers.txt");
    if (!offer_file.open(QIODevice::ReadOnly)) {
        qDebug() << "no offer list";
    } else {
        PARSESTAT stat = NAME;
        OfferItem* item = nullptr;
        while (!offer_file.atEnd()) {
            QString f = offer_file.readLine().trimmed();
            if (f.startsWith("[")) {
                stat = NAME;
            } else if (f.startsWith("#")) {
                stat = COMMENTS;
            } else if (f.startsWith("/*")) {
                stat = CHUNK_COMM;
            } else {
                stat = CONTENT;
            }

            switch (stat) {
            case NAME:
            {
                int index = f.indexOf("]");
                item = new OfferItem(f.mid(1, index-1));
                offer_items << item;
                stat = CONTENT;
            }
                break;
            case CONTENT:
                item->addOffer(f);
                break;
            case COMMENTS:
                break;
            case CHUNK_COMM:
                while (f != "" && !f.endsWith("*/")) {
                    f = offer_file.readLine().trimmed();
                }
                break;
            }
        }
    }

    QFile ua_file(QCoreApplication::applicationDirPath() + "ua.txt");
    if (!ua_file.open(QIODevice::ReadOnly)) {
        uas << "Mozilla/5.0 (iPhone; CPU iPhone OS 9_2 like Mac OS X) AppleWebKit/601.1.46 (KHTML, like Gecko) Version/9.0 Mobile/13C75 Safari/601.1";
        uas << "Mozilla/5.0 (iPhone; CPU iPhone OS 10_0 like Mac OS X) AppleWebKit/602.1.38 (KHTML, like Gecko) Version/10.0 Mobile/14A5297c Safari/602.1";
        uas << "Mozilla/5.0 (iPhone; CPU iPhone OS 10_1 like Mac OS X) AppleWebKit/602.2.14 (KHTML, like Gecko) Version/10.0 Mobile/14B72 Safari/602.1 ";
        uas << "Mozilla/5.0 (iPhone; CPU iPhone OS 10_1 like Mac OS X) AppleWebKit/602.2.14 (KHTML, like Gecko) Version/10.0 Mobile/14B72c Safari/602.1 ";
    } else {
        while (!ua_file.atEnd()) {
            QString f = ua_file.readLine().trimmed();
            uas << f;
        }
    }

    m_thread = new TimerThread;
    m_thread->start();

    m_timer = new QTimer;
    connect(m_timer, SIGNAL(timeout()), this, SLOT(onTimeout()), Qt::DirectConnection);
//    m_timer->setSingleShot(true);
    m_timer->start(60000 * 30);
    m_timer->moveToThread(m_thread);

    connect(this, SIGNAL(reloadID()), this, SLOT(onReloadID()));
}

Click *Click::getInstance()
{
    if (!m_self) {
        m_self = new Click;
    }

    return m_self;
}

QString Click::getUa()
{
    QTime time = QTime::currentTime();
    qsrand(time.msec() + time.second() * 1000);
    int rand = qrand();
    return uas.at(rand % uas.size());
}

void Click::onErrorOffer(QString offer_url)
{
    int index = offer_url.indexOf("&idfa");
    QString offer = offer_url.mid(0, index);
    foreach(OfferItem* item, offer_items) {
        QStringList list = item->getOffers();
        if (list.contains(offer)) {
            item->advancedIndex();
            break;
        }
    }
}

void Click::startRequest()
{
    // qDebug() << "dir: " << QCoreApplication::applicationDirPath();
    QDir dir(QCoreApplication::applicationDirPath() + "/ioslogs/");
    QStringList files_type;
    files_type << "*.id";
    QFileInfoList file_list = dir.entryInfoList(files_type, QDir::Files);
    if (file_list.size() == 0) {
        qDebug() << "no id files";
        getIdFromServer(true);
        return;
    }

    foreach (QFileInfo fi, file_list) {
        QFile id_file(fi.absoluteFilePath());
        if (!id_file.open(QIODevice::ReadOnly)) {
            continue;
        }

        while (!id_file.atEnd()) {
            if (should_quit) {
                break;
            }

            QStringList info = QString(id_file.readLine().trimmed()).split("\t");
            qDebug() << "infosize:" << info.size();
            qDebug() << "idfa counter:" << ++idfa_counter;
            foreach(OfferItem* item, offer_items) {
                QString offname = item->offer();
                if (offname == "") {
                    continue;
                }

                // 1: app name 2: adx
                QString url = offname + "&idfa=" + info[0] + "&aff_sub=" + info[1] +
                        "&aff_sub2=" + info[2] +
                        "&aff_sub3=" + QDateTime::currentDateTime().toString("yyyy-MM-dd hh");
                qDebug() << "====== url:" << url;
                // qDebug() << "====== ua:" << info[6];
                ClickRunnable* click = new ClickRunnable(this);
                click->setUrl(url);
                click->setAutoDelete(true);

                while (m_thread_pool->activeThreadCount() == pool_size) {
                    // qDebug() << "wait pool";
                    QThread::sleep(1);
                }

                qDebug() << "total click: " << ++total_click;
                m_thread_pool->start(click);
                item->logClick();

                QDateTime dt = QDateTime::currentDateTime();
                static bool written = false;
                if (dt.time().minute() == 0 && !written) {
                    writeLog();
                    written = true;
                } else if(dt.time().minute() == 1) {
                    written = false;
                }

//                if (total_click % 500 == 0) {
//                    writeLog();
//                }
            }
        }
    }

    getIdFromServer(false);
}

void Click::onTimeout()
{
    getIdFromServer(false);
}

void Click::onReloadID()
{
    QNetworkReply* reply;
    QString reply_str;
    while (1) {
        QString strurl = "http://" + server_ip + "/ioslogs/" + server_index + ".id";
        qDebug() << "get server id file:" << strurl;
        QEventLoop eventLoop;
        QNetworkAccessManager mgr;
        QUrl url(strurl);
        QNetworkRequest qnr(url);
        reply = mgr.get(qnr);
        QObject::connect(reply, &QNetworkReply::finished, &eventLoop, &QEventLoop::quit);
        eventLoop.exec();

        if (reply->error() != QNetworkReply::NoError) {
            qDebug() << "reply error: " << reply->errorString();
            continue;
        }

        reply_str = reply->readAll();
        qDebug() << "download finished";
        break;
    }

    QString filepath = QCoreApplication::applicationDirPath() + "/ioslogs/" + server_index + ".id";
    QFile file(filepath);
    if (file.exists()) {
        file.remove();
    }

    file.open(QIODevice::WriteOnly);
    QTextStream out(&file);
    out << reply_str;
    out.flush();

    should_quit = false;
    startRequest();
}

void Click::getIdFromServer(bool no_id)
{
    QString md5file = QCoreApplication::applicationDirPath() + "/md5";
    QFile file(md5file);
    file.open(QIODevice::ReadWrite);
    QTextStream out(&file);
    if (file.exists()) {
        md5_str = file.readAll();
    }

    QNetworkReply* reply;
    QString reply_str;

    while (1) {
        QString strurl = "http://" + server_ip + "/ioslogs/md5";
        qDebug() << "get md5:" << strurl;
        QEventLoop eventLoop;
        QNetworkAccessManager mgr;
        QUrl url(strurl);
        QNetworkRequest qnr(url);
        reply = mgr.get(qnr);
        QObject::connect(reply, &QNetworkReply::finished, &eventLoop, &QEventLoop::quit);
        eventLoop.exec();

        if (reply->error() != QNetworkReply::NoError) {
            qDebug() << "reply error: " << reply->errorString();
            continue;
        }

        reply_str = reply->readAll();
        if (reply_str == md5_str && !no_id) {
            return;
        }
        break;
    }

    md5_str = reply_str;
    out << md5_str;
    out.flush();
    this->should_quit = true;
    emit reloadID();
}

void Click::writeLog()
{
    QFile file(QCoreApplication::applicationDirPath() + "/out.log");
    file.open(QIODevice::WriteOnly | QIODevice::Append);
    QTextStream output(&file);
    output << QDateTime::currentDateTime().toString("yyyy-MM-dd hh ") << "click:" << total_click
           << " log md5: " << md5_str
           << "\n";
#if 0
    foreach(OfferItem* item, offer_items) {
        QString output_str = QString("%1 %2 %3\r\n")
                .arg(QDateTime::currentDateTime().toString())
                .arg(item->name())
                .arg(item->clickInfo());
        output << output_str;
    }
#endif
}

OfferItem::OfferItem(QString name) :
    offer_name(name),
    curOffer(0),
    click_counter(0)
{
}

QString OfferItem::name()
{
    return offer_name;
}

void OfferItem::addOffer(QString offer)
{
    offers.append(offer);

    offer_map[offers.size() - 1] = 0;
}

QStringList OfferItem::getOffers()
{
    return offers;
}

QString OfferItem::offer()
{
    // TODO: 此处可恢复offer的有效性
    // if (预算充足) curOffer = 0;
    if (curOffer >= offers.size()) {
        return "";
    }

    return offers[curOffer];
}

void OfferItem::advancedIndex()
{
    ++curOffer;
}

void OfferItem::logClick()
{
    // ++click_counter;
    ++offer_map[curOffer];
}

QString OfferItem::clickInfo()
{
    // return click_counter;
    QString ret;
    QMap<int, qint64>::iterator iter = offer_map.begin();
    while (iter != offer_map.end()) {
        ret.append("offer:" + QString("%1").arg(iter.key())
                   + " click:" +
                   QString("%1").arg(iter.value()) + " ");
        iter++;
    }

    return ret;
}


TimerThread::TimerThread()
{

}

void TimerThread::run()
{
    QEventLoop ev;

    ev.exec();
}
