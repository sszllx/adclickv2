#include "click.h"
#include "clickrunnable.h"

#include <QDateTime>
#include <QTimer>

#define MAX_THREAD_NUM 16

Click::Click(QObject *parent) : QObject(parent),
    m_thread_pool(QThreadPool::globalInstance()),
    total_click(0),
    pool_size(MAX_THREAD_NUM),
    idfa_counter(0)
{
    qDebug() << QCoreApplication::applicationDirPath() + "/config.txt";
    QFile config(QCoreApplication::applicationDirPath() + "/config.txt");
    if (config.open(QIODevice::ReadOnly)) {
        QString ps = config.readLine();
        QStringList ps_pars = ps.split("=");
        if (ps_pars[0] == "pool_size") {
            bool ok =false;
            uint size = ps_pars[1].toUInt(&ok);
            if (ok) {
                pool_size = size;
                qDebug() << "pool size:" << size;
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
    }

    foreach (QFileInfo fi, file_list) {
        QFile id_file(fi.absoluteFilePath());
        if (!id_file.open(QIODevice::ReadOnly)) {
            continue;
        }

        while (!id_file.atEnd()) {
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
                } else {
                    written = false;
                }

//                if (total_click % 500 == 0) {
//                    writeLog();
//                }
            }
        }
    }
}

void Click::writeLog()
{
    QFile file(QCoreApplication::applicationDirPath() + "/out.log");
    file.open(QIODevice::WriteOnly | QIODevice::Append);
    QTextStream output(&file);
    output << QDateTime::currentDateTime().toString("yyyy-MM-dd hh ") << "click:" << total_click << "\n";
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

