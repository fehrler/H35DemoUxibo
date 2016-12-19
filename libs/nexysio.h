#ifndef NEXYSIO_H
#define NEXYSIO_H

#include "ftdi.h"
#include <QBitArray>

const int FTDIBuffSize = 1024*128;

class NexysIO
{
private:
    FTDI* ftdi;
public:
    NexysIO();
    ~NexysIO();

    unsigned char FTDIBuff[FTDIBuffSize];
    unsigned int FTDIBuffPos;

    bool initializeFtdi(FTDI*);

    bool AddByte(unsigned char Order);

    bool flush();

    void write(unsigned char address, std::vector<byte> values);

    bool WriteASIC(unsigned char address, std::vector<bool> values, int Matrix);

    bool WritePCB(unsigned char address, std::vector<bool> values);

    void PatGenReset(bool reset);

    void PatGenSuspend(bool suspend);

    void PatGen(int period, int cycle, int clkdiv, int initdelay);

    void PatGen();

    void TransByte(int address, int value);

    void TransByte(int address, std::vector<int> value);

    enum Header
    {
        Read = 0x00,
        Write = 0x01,
        //Pattern Generator Stuff
        PGreset = 0x02,
        PGsuspend = 0x03,
        PGwrite = 0x04,
        PGunhandled = 0x05,
        PGaddress = 0x06,
        PGdata = 0x07
        //Reset = 0xFF
    };

    enum Value
    {
        Ck1 = 0b00000001,
        Ck2 = 0b00000010,
        SinA = 0b00000100,
        Ld  = 0b00001000,
        SinB = 0b00010000,
        Sin_ = 0b00100000,
        SinN = 0b01000000,
        PCB_Ck =  0b00000001,
        PCB_SIN = 0b00000010,
        PCB_Ld =  0b00000100
    };

    enum behaviour
    {
        RstState           =   1,
        InitState          =   2,   //(while start delay)
        RstStateOnDone     =   4,   //(otherwise keep last state)
        InitStateOnDone    =   8,   //(otherwise keep last state)
        RstStateOnSuspend  =  16,   //(otherwise keep last state)
        InitStateOnSuspend =  32,   //(otherwise keep last state)
        NoResetOnPeriod    = 128    //do not reset timestamp pointer on period reach
    };
    std::string read(int count);
};

#endif // NEXYSIO_H
