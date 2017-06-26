#include <QCoreApplication>
#include <QObject>

#include "click.h"

#if 0
void handle(int sig)
{
    Q_UNUSED(sig)
    qApp->exit();
}
#endif

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    signal(SIGINT, NULL);

    Click click;
    click.startRequest();

    return a.exec();
}
