#include "gamewidget.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationDisplayName(QStringLiteral("大鱼吃小鱼"));

    GameWidget w;
    w.resize(800, 600);
    w.show();

    return app.exec();
}
