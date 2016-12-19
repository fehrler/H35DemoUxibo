#ifndef HACOM
#define HACOM

#include <QByteArray>
#include <stdlib.h>

class HM7044
{
    public:
    //Commands


        QByteArray select(std::string value);
        QByteArray select(int value);
        QByteArray set_amp(double value);
        QByteArray set_volt(double value);
        QByteArray fuse(int value);
        QByteArray fuse(std::string value);
        QByteArray read();
        QByteArray lock(bool value);
        QByteArray ON();
        QByteArray OFF();
        QByteArray SetOutput(bool value);
    private:
        QByteArray CreateCommand(std::string command);
};

#endif // HACOM

