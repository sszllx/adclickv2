#include "click.h"
#include "clickrunnable.h"

#include <QTime>
#include <QTimer>

#define MAX_THREAD_NUM 16

Click::Click(QObject *parent) : QObject(parent),
    m_thread_pool(QThreadPool::globalInstance()),
    total_click(0),
    pool_size(MAX_THREAD_NUM)
{
    QFile config("config.txt");
    if (config.open(QIODevice::ReadOnly)) {
        QString ps = config.readLine();
        QStringList ps_pars = ps.split("=");
        if (ps_pars[0] == "pool_size") {
            bool ok =false;
            uint size = ps_pars[1].toUInt(&ok);
            if (ok) {
                pool_size = size;
            }
        }
    }

    m_thread_pool->setMaxThreadCount(pool_size);

    QFile offer_file("offers.txt");
    if (!offer_file.open(QIODevice::ReadOnly)) {
        offers << "https://global.ymtracking.com/trace?offer_id=5107479&aff_id=104991";
        offers << "https://global.ymtracking.com/trace?offer_id=5065577&aff_id=104991";
        offers << "http://svr.dotinapp.com/ics?sid=1217&adid=4006512";
    } else {
        while (!offer_file.atEnd()) {
            QString f = offer_file.readLine().trimmed();
            offers << f;
        }
    }

    QFile ua_file("ua.txt");
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
            QString idfa = id_file.readLine().trimmed();

            foreach(QString offer, offers) {
                QString url = offer + "&idfa=" + idfa;
                ClickRunnable* click = new ClickRunnable(this);
                click->setUrl(url);
                click->setAutoDelete(true);

                while (m_thread_pool->activeThreadCount() == pool_size) {
                    qDebug() << "wait pool";
                    QThread::sleep(1);
                }

                total_click++;
                qDebug() << "total click: " << total_click;
                m_thread_pool->start(click);
            }
        }
    }
}
