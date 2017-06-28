#include <QCoreApplication>
#include <QObject>
#include <QString>
#include <QDebug>

#include "click.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    Click click;
    click.startRequest();

    return a.exec();
}
