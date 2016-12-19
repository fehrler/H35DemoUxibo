#include "mainwindow.h"
#include <QApplication>
#include "hbselect.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    //qRegisterMetaType<QVector<float> >("QVector<bool>");


    return a.exec();
}
