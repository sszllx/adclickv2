#include <QCoreApplication>
#include <QObject>
#include <QString>
#include <QDebug>

#include "click.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    Click *click = Click::getInstance();
    click->setWangmeng(argv[1]);
    click->startRequest();

    return a.exec();
}
