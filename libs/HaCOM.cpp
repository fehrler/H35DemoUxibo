#include <QByteArray>
#include <QString>
#include "HaCOM.h"
#include <stdlib.h>
#include <sstream>
#include <unistd.h>
#include <string.h>


QByteArray HM7044::CreateCommand(std::string command)
{
    QByteArray data;
    for(unsigned int i=0; i<command.size(); i++)
    {
        data.push_back(command.at(i));
    }
#if defined(Q_OS_WIN)
    data.push_back(0x0D);
#endif
#if defined(Q_OS_LINUX)
    data.push_back(0x0D);
#endif
    //usleep(50000);
    return data;
}

QByteArray HM7044::select(int value)
{
    std::stringstream command;
    command << value;
    return CreateCommand("SEL " + command.str());
}

QByteArray HM7044::select(std::string value)
{
    return CreateCommand("SEL "+value);
}

QByteArray HM7044::set_amp(double value)
{
    std::stringstream command;
    command << "SET " << value <<"A";
    return CreateCommand(command.str());
}

QByteArray HM7044::set_volt(double value)
{
    std::stringstream command;
    command << "SET " << value <<"V";
    return CreateCommand(command.str());
}

QByteArray HM7044::fuse(int value)
{
    std::stringstream command;
    command << "FUSE " << value;
    return CreateCommand(command.str());
}

QByteArray HM7044::fuse(std::string value)
{
    std::stringstream command;
    command << "FUSE " << value;
    return CreateCommand(command.str());
}

QByteArray HM7044::read()
{
    return CreateCommand("READ");
}

QByteArray HM7044::lock(bool value)
{
    if (value)
        return CreateCommand("LOCK ON");
    else
        return CreateCommand("LOCK OFF");
}

QByteArray HM7044::ON()
{
    return CreateCommand("ON");
}

QByteArray HM7044::OFF()
{
    return CreateCommand("OFF");
}

QByteArray HM7044::SetOutput(bool value)
{
    if (value)
        return CreateCommand("ENABLE OUTPUT");
    else
        return CreateCommand("DISABLE OUTPUT");
}
