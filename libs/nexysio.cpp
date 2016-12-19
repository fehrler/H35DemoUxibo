#include "nexysio.h"
#include <stdio.h>
#include <iostream>
#include <QBitArray>
#include <sstream>

//#################################################################
//### This Class provides functions for efficient communication ###
//### with the NexysVideo FPGA board via FTDI. It also includes ###
//### functions for configuration of connected Sensors and PCB. ###
//### by Felix Ehrler                                           ###
//#################################################################

NexysIO::NexysIO()
{
    this->ftdi = NULL;
    FTDIBuffPos = 0;
}

NexysIO::~NexysIO()
{

}

//Inizialize FTDI communication (Pointer to FTDI used in main program)
bool NexysIO::initializeFtdi(FTDI* extftdi)
{
  this->ftdi = extftdi;
  if(this->ftdi == NULL)
    return false;
  else
    return true;
}

//Add a single byte to the buffer
bool NexysIO::AddByte(unsigned char Order)
{
    if (FTDIBuffPos > FTDIBuffSize-1)
    {
        flush(); //does it help?
        return false;
    }
    FTDIBuff[FTDIBuffPos++] = Order;
    return true;
}

//Send the buffer to the FPGA
bool NexysIO::flush()
{
    //std::cout << FTDIBuff[i]<< std::endl;
    bool ftStatus = false;
    unsigned long int BytesWritten = 0;
    //std::cout << "Bytes written to FTDI: " << BytesWritten << std::endl;
    if (FTDIBuffPos > 0 )
      {
        ftStatus = ftdi->Write( (unsigned char*)FTDIBuff, FTDIBuffPos, &BytesWritten );
        //std::cout << "Bytes written to FTDI: " << BytesWritten << std::endl;
      }
    //std::cout << "Bytes written to FTDI: " << BytesWritten << std::endl;
    FTDIBuffPos = 0;
    return ftStatus;
}

std::string NexysIO::read(int count)
{
    unsigned char* text = new unsigned char[count];
    unsigned long readbytes = 0;
    ftdi->Read(text, count, &readbytes);
    std::string data="";    
    for (int i= 0; (unsigned)i< readbytes;i++)
    {
        data+=text[i];
    }
    delete text;
    return data;
}

//########################
//### Sensor Functions ###
//########################


void NexysIO::write(unsigned char address, std::vector<byte> values)
{

}


bool NexysIO::WriteASIC(unsigned char address, std::vector<bool> values, int Matrix)
{
    int length = values.size();
    int clockdiv = 8;
    int lengthA = ((length+5)*clockdiv*5) / 256;
    int lengthB = ((length+5)*clockdiv*5) % 256;
    if (length*clockdiv > 65535)
        return 0;


    std::stringstream s, r;
    unsigned char c_lengthA, c_lengthB;

    unsigned char pattern = 0x00;
    unsigned char Sin = 0x00;

    if (Matrix == 0)
        Sin = SinN;
    if (Matrix == 1)
        Sin = SinA;
    if (Matrix == 2)
        Sin = SinB;
    if (Matrix == 3)
        Sin = Sin_;
    //Sin = SinB;

    s << lengthA;
    s >> std::hex >> c_lengthA;
    r << lengthB;
    r >> std::hex >> c_lengthB;

    AddByte(Write);                     //Send Header
    AddByte(address);                   //Send Address
    AddByte(lengthA);                   //Send Length
    AddByte(lengthB);

    for (int i= 0; i< length; i++)      //Send Values
    {
        pattern = 0x00;
        if (values[i])
        {
            pattern = 0x00 | Sin;
            //pattern = 0x00 | SinA | SinB | SinN | Sin_;
        }
        for (int j = 0; j<clockdiv; j++)
            AddByte(pattern);
        for (int j = 0; j<clockdiv; j++)
            AddByte(pattern | Ck1);
        for (int j = 0; j<clockdiv; j++)
            AddByte(pattern);
        for (int j = 0; j<clockdiv; j++)
            AddByte(pattern | Ck2);
        for (int j = 0; j<clockdiv; j++)
            AddByte(pattern);
    }


    for (int i= 0; i< 5; i++)      //Send Values
    {
        pattern = 0x00;
        if (i<=3)
        {
            pattern = 0x00 | Ld;
        }
        for (int j = 0; j<clockdiv; j++)
            AddByte(pattern);
        for (int j = 0; j<clockdiv; j++)
            AddByte(pattern);
        for (int j = 0; j<clockdiv; j++)
            AddByte(pattern);
        for (int j = 0; j<clockdiv; j++)
            AddByte(pattern);
        for (int j = 0; j<clockdiv; j++)
            AddByte(pattern);
    }
    //std::cout << length<<  std::endl;
    return 1;
}

//#####################
//### PCB Functions ###
//#####################

bool NexysIO::WritePCB(unsigned char address, std::vector<bool> values)
{
    int length = values.size();
    int clockdiv = 8;
    int lengthA = ((length+5)*3*clockdiv) / 256;
    int lengthB = ((length+5)*3*clockdiv) % 256;
    if (length*clockdiv > 65535)
        return 0;
    std::stringstream s, r;
    unsigned char c_lengthA, c_lengthB;

    unsigned char pattern = 0x00;

    s << lengthA;
    s >> std::hex >> c_lengthA;
    r << lengthB;
    r >> std::hex >> c_lengthB;

    AddByte(Write);                     //Send Header
    AddByte(address);                   //Send Address
    AddByte(lengthA);                   //Send Length
    AddByte(lengthB);

    for (int i= 0; i< length; i++)      //Send Values
    {
        pattern = 0x00;
        if (values[i])
        {
            pattern = 0x00 | PCB_SIN;
        }
        for (int j = 0; j<clockdiv; j++)
            AddByte(pattern);
        for (int j = 0; j<clockdiv; j++)
            AddByte(pattern | PCB_Ck);
        for (int j = 0; j<clockdiv; j++)
            AddByte(pattern);
    }

    for (int i= 0; i< 5; i++)      //Send Values
    {
        pattern = 0x00;
        if (i<=3)
        {
            pattern = 0x00 | PCB_Ld;
        }
        for (int j = 0; j<clockdiv; j++)
            AddByte(pattern);
        for (int j = 0; j<clockdiv; j++)
            AddByte(pattern | PCB_Ck);
        for (int j = 0; j<clockdiv; j++)
            AddByte(pattern);
    }
    return 1;
}


//#########################
//### Pattern Generator ###
//#########################

void NexysIO::TransByte(int address, std::vector<int> value)
{
    AddByte(Write);                     //Send Header
    AddByte(address);                   //Send Address
    AddByte(value.size() >> 8);                   //Send Length
    AddByte(value.size());
    for(std::vector<int>::iterator it=value.begin();it != value.end();++it)
        AddByte(*it);
}

void NexysIO::TransByte(int address, int value)
{
    std::vector<int> vec;
    vec.push_back(value);

    TransByte(address,vec);
}

void NexysIO::PatGenReset(bool reset)
{
    int length = 1;
    int lengthA = length / 256;
    int lengthB = length % 256;


    AddByte(Write);                     //Send Header
    AddByte(PGreset);                   //Send Address
    AddByte(lengthA);                   //Send Length
    AddByte(lengthB);
    if (reset)
        AddByte(0x01);
    else
        AddByte(0x00);
}

void NexysIO::PatGenSuspend(bool suspend)
{
    int length = 1;
    int lengthA = length / 256;
    int lengthB = length % 256;


    AddByte(Write);                     //Send Header
    AddByte(PGsuspend);                   //Send Address
    AddByte(lengthA);                   //Send Length
    AddByte(lengthB);
    if (suspend)
        AddByte(0x01);
    else
        AddByte(0x00);
}

void NexysIO::PatGen()
{
    int period = 4;
    int cycle = 5;
    int clkdiv = 10;
    int initdelay = 0;
    PatGen(period, cycle, clkdiv, initdelay);
}

void NexysIO::PatGen(int period, int cycle, int clkdiv, int initdelay)
{

    /*
        pattern generator register file and description:

        $addr:	(8) toggle timestamps
            when subcounter reaches toggle timestamp,
            signal is toggled, and next toggle timestamp is loaded
        $addr+8: period
            when subcounter reaches period, subcounter
            is resetted (and first toggle timestamp loaded if desired)
        $addr+9: bitwise behaviour control
            0: rststate (0/1/tri?)
            1: initstate (while in start delay)
            2: rststate on done  (otherwise, keep last state)
            3: initstate on done (otherwise, keep last state)
            4: rststate on suspend  (otherwise, keep last state)
            5: initstate on suspend (otherwise, keep last state)
            6:
            7: do not reset timestamp ptr on period reach
        $addr+10/11:	runlen
            count of patternticks this generator shall run
            0: run infinite
        $addr+12/13:	fastclock initial delay
            start delay: count of fastticks until patternticks are generated
        $addr+14/15:	clkfac
            fire patterntick each clkfac fastticks*/



    std::vector <int> timestamps;
    timestamps.push_back(0);
    timestamps.push_back(3);
    timestamps.push_back(0);
    timestamps.push_back(0);
    timestamps.push_back(0);
    timestamps.push_back(0);
    timestamps.push_back(0);
    timestamps.push_back(0);


    int runlength = period* cycle;

    //Write Strobe
    std::vector <int> strobe;
    strobe.push_back(1);
    strobe.push_back(0);


    //Timestamps
    TransByte(PGaddress, 0);
    TransByte(PGdata, 1);
    TransByte(PGwrite, strobe);

    TransByte(PGaddress, 1);
    TransByte(PGdata, 3);
    TransByte(PGwrite, strobe);

    for (int i = 2; i<8; ++i)
    {
        TransByte(PGaddress, i);
        TransByte(PGdata, 0);
        TransByte(PGwrite, strobe);
    }


    //Period
    TransByte(PGaddress, 8);
    TransByte(PGdata, period);
    TransByte(PGwrite, strobe);

    //Flags
    TransByte(PGaddress, 9);
    TransByte(PGdata, 0);   //initstate = 0
    TransByte(PGwrite, strobe);

    //runlength
    TransByte(PGaddress, 10);
    TransByte(PGdata, (runlength >>8));
    TransByte(PGwrite, strobe);
    TransByte(PGaddress, 11);
    TransByte(PGdata, runlength);
    TransByte(PGwrite, strobe);

    //initial delay
    TransByte(PGaddress, 12);
    TransByte(PGdata, (initdelay >>8));
    TransByte(PGwrite, strobe);
    TransByte(PGaddress, 13);
    TransByte(PGdata, initdelay);
    TransByte(PGwrite, strobe);

    //clkdiv
    TransByte(PGaddress, 14);
    TransByte(PGdata, ((clkdiv) >>8));
    TransByte(PGwrite, strobe);
    TransByte(PGaddress, 15);
    TransByte(PGdata, clkdiv);
    TransByte(PGwrite, strobe);
}


