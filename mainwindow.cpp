#include "mainwindow.h"
#include "hbselect.h"
#include "ui_mainwindow.h"
#include "libs/geniobase.h"
#include "libs/ftdi.h"
#include "libs/func.h"
#include <iostream>
#include "libs/nexysio.h"

#include <QTimer>
#include "libs/HaCOM.h"
#include <bitset>

#include <stdlib.h>
#include <sstream>
#include "libs/nexysio.h"
#include <iomanip>
#include <vector>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QVector3D>

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_DEPRECATE)
/* Functions like strcpy are technically not secure because they do */
/* not contain a 'length'. But we disable this warning for the VISA */
/* examples since we never copy more than the actual buffer size.   */
#define _CRT_SECURE_NO_DEPRECATE
#endif

#include <stdio.h>
#include <string.h>
#include <fstream>
#include <Windows.h>
#include "visa.h"
#include <QString>
#include <math.h>
#include <QThread>





static ViSession defaultRM = VI_NULL;
static ViSession instr = VI_NULL;
static ViStatus status;
static char instrResourceString[VI_FIND_BUFLEN];



bool isuxibo;

//ASIC Config
std::vector<bool> MatrixAconfig, MatrixBconfig, Matrix_config, MatrixNconfig;
int SensorADAC[17], SensorBDAC[17], Sensor_DAC[17], SensorNDAC[17];
int PixelASel[3], PixelBSel[3], PixelNSel[3], Pixel_Sel[3]; //Col, Row, RowToOut
bool ModeASel[6], ModeBSel[6], ModeNSel[6], Mode_Sel[6]; // Checkboxes
bool Unlock[17]= {0, //unlock                   //Some bits are negative!
                  1, //unlock
                  0,0,0,0,0,0,0, //????
                  0, //SyncGen
                  0, //Sel4Bit
                  1, //EnCCPD
                  0, //BufferEn
                  1, //SelSlowTS
                  0, //ExtCnt
                  0, //BufferEn
                  1  //unlock
                 };
bool Lock[17] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

bool RamPix0[300][16], RamPix1[300][16];
bool RamDig0[120][40], RamDig1[120][40], RamDig2[120][40], RamDig3[120][40];

bool Matrixinjection = 0; // marks all pixels of a matrix for injection
bool ColInjection = 1;
bool RowInjection = 0;
bool InjEnCol[300];
bool InjEnRow[23];

//Thresholdscanstuff
double bestTh = 1.0;
double bestEffi = 1.0;


int currentMatrix;

bool MaskedPixels_NL[150][16];

//PCB Config
std::vector<bool> PCBConfig;
float PCBDAC[10];


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    HBsel = new HBselect();
    DigselUI = new DigSelect();
    GeburtstagUI = new Geburtstag();
    DisplayReadoutUI = new DisplayReadout();

    player = new QMediaPlayer();


    //Injection Timer
    //QTimer *timer = new QTimer(this);
    //connect(timer, SIGNAL(timeout()), this, SLOT(on_timer_update()));
    //timer->start(1000);
    ftdi = new FTDI();
    ftdi->Close();
    genio = new GenioBase();
    nexys = new NexysIO();
    //FT_STATUS ftStatus;
    //FT_HANDLE ftHandleTemp;

    MatrixAconfig.resize(1411); //1292 config + 119 DACs
    MatrixBconfig.resize(1411);
    MatrixNconfig.resize(1463);
    Matrix_config.resize(1411);

    PCBConfig.resize(160);

    //GetASICConfig(0);   //takes DAC values from Gui to vectors. To be replaced when save and load is introduced
    GetASICConfig(1);
    GetASICConfig(2);
    GetASICConfig(3);

    ui->progressBar->hide();


    currentMatrix = ui->MatrixSelect->currentIndex();

    setallRamPix(0);

    for (int j = 0; j < 40; j++)
    {
        for (int i = 0; i < 120; i++)
        {

            RamDig0[i][j] = 0; /// hier ist vram unlock
            RamDig1[i][j] = 0;
            RamDig2[i][j] = 0;//(j==32)&&(i==0); col 0
            RamDig3[i][j] = 0;
        }
    }

    for (int col = 0; col<150; col++)
        InjEnCol[col] = 0;

    for (int row = 0; row<23; row++)
        InjEnRow[row] = 0;

    for (int i = 0; i < 40; i++)
    {
        HBenRow[i] = 0;
    }
    for (int j = 0; j < 60; j++)
    {
        HBenCol[j] = 0;
        HBenCol[j+60] = 0;
    }


    //Connections
    //Hitbus
    connect(this, SIGNAL(MainToHB(QVector<bool>)), HBsel, SLOT(MainToHB(QVector<bool>)));
    connect(HBsel, SIGNAL(HBToMain(QVector<bool>)), this, SLOT(HBToMain(QVector<bool>)));
    //Digital Select
    connect(this, SIGNAL(MainToDigSel(QVector<std::pair<unsigned int, unsigned int> >)), DigselUI, SLOT(MainToDigSel(QVector<std::pair<unsigned int, unsigned int> >)));
    connect(DigselUI, SIGNAL(DigSelToMain(QVector<std::pair<unsigned int, unsigned int> >)), this, SLOT(DigSelToMain(QVector<std::pair<unsigned int, unsigned int> >)));
    //IPE Gebrutstag
    connect(GeburtstagUI, SIGNAL(GeburtstagtoMain(QVector<std::pair<unsigned int, unsigned int> >)), this, SLOT(GeburtstagtoMain(QVector<std::pair<unsigned int, unsigned int> >)));
    connect(this,SIGNAL(MainToGeburtstag(QVector<std::pair<unsigned int, unsigned int> >)),GeburtstagUI, SLOT(MainToGeburtstag(QVector<std::pair<unsigned int, unsigned int> >)));
    //Display Readout Results
    connect(this, SIGNAL(MainToReadout(std::string)), DisplayReadoutUI, SLOT(MainToReadout(std::string)));
    connect(this, SIGNAL(LiveReadout(std::pair<std::string, bool>)), DisplayReadoutUI, SLOT(LiveReadout(std::pair<std::string, bool>)));
    connect(this, SIGNAL(DisplayReadoutRepaint()), DisplayReadoutUI, SLOT(DisplayReadoutRepaint()));
    connect(DisplayReadoutUI, SIGNAL(DisplayReadoutClosed()), this, SLOT(DisplayReadoutClosed()));
}

MainWindow::~MainWindow()
{
    ui->ReadoutRunning->setChecked(0);
    delete ui;
    delete genio;
    delete nexys;
    ftdi->Close();
    delete ftdi;
    delete HBsel;
    delete DigselUI;
    delete GeburtstagUI;
    delete DisplayReadoutUI;
    delete player;
}

void MainWindow::on_WriteRamDig_clicked()  // vram unlock
{
    for (int j = 0; j < 40; j++)
    {
        for (int i = 0; i < 120; i++)
        {
            RamDig0[i][j] = 0;
            RamDig1[i][j] = 0;
            RamDig2[i][j] = 0;//(j==32)&&(i==0); col 0
            RamDig3[i][j] = 0;
            if (ui->NumRamDig->value() == 0)
            {
                if ((ui->iRamDig->value() == 120) && (ui->jRamDig->value() == j))
                    RamDig0[i][j]=1;
                if ((ui->iRamDig->value() == i) && (ui->jRamDig->value() == 40))
                    RamDig0[i][j]=1;
                if ((ui->iRamDig->value() == 120) && (ui->jRamDig->value() == 40))
                    RamDig0[i][j]=1;
            }
            if (ui->NumRamDig->value() == 1)
            {
                if ((ui->iRamDig->value() == 120) && (ui->jRamDig->value() == j))
                    RamDig1[i][j]=1;
                if ((ui->iRamDig->value() == i) && (ui->jRamDig->value() == 40))
                    RamDig1[i][j]=1;
                if ((ui->iRamDig->value() == 120) && (ui->jRamDig->value() == 40))
                    RamDig1[i][j]=1;
            }
            if (ui->NumRamDig->value() == 2)
            {
                if ((ui->iRamDig->value() == 120) && (ui->jRamDig->value() == j))
                    RamDig2[i][j]=1;
                if ((ui->iRamDig->value() == i) && (ui->jRamDig->value() == 40))
                    RamDig2[i][j]=1;
                if ((ui->iRamDig->value() == 120) && (ui->jRamDig->value() == 40))
                    RamDig2[i][j]=1;
            }
            if (ui->NumRamDig->value() == 3)
            {
                if ((ui->iRamDig->value() == 120) && (ui->jRamDig->value() == j))
                    RamDig3[i][j]=1;
                if ((ui->iRamDig->value() == i) && (ui->jRamDig->value() == 40))
                    RamDig3[i][j]=1;
                if ((ui->iRamDig->value() == 120) && (ui->jRamDig->value() == 40))
                    RamDig3[i][j]=1;
            }


        }
    }
    if((ui->iRamDig->value() != 120) && (ui->jRamDig->value() != 40))
    {
        if (ui->NumRamDig->value() == 0)
            RamDig0[ui->iRamDig->value()][ui->jRamDig->value()] = 1;
        if (ui->NumRamDig->value() == 1)
            RamDig1[ui->iRamDig->value()][ui->jRamDig->value()] = 1;
        if (ui->NumRamDig->value() == 2)
            RamDig2[ui->iRamDig->value()][ui->jRamDig->value()] = 1;
        if (ui->NumRamDig->value() == 3)
            RamDig3[ui->iRamDig->value()][ui->jRamDig->value()] = 1;
    }
}

void MainWindow::logit(std::string logstream)
{
    std::cout << logstream << std::endl;
    QString buffer= QString::fromStdString(logstream);
    ui->Log->append(buffer);
    QTextCursor c =  ui->Log->textCursor();
    c.movePosition(QTextCursor::End);
    ui->Log->setTextCursor(c);
}


void MainWindow::on_Update_clicked()
{
    /*Funkcije* Usb;
    Usb = new Funkcije(this->genio);

     // SendH35Config
     for (int icol=0; icol<300; icol++)
     {
       if( (299-icol) == ui->H35Col->value())// FormMain -> EditH35Col -> Text.ToInt() )
       {
           Usb->SendBitSensor(((299-icol) == 0)&&(ui->checkbox_AOutToMon->isChecked() == true));//MuxEn
           Usb->SendBitSensor(ui->checkbox_InjToPixel->isChecked());//InjToCol
           Usb->SendBitSensor(ui->checkbox_TestToMon->isChecked());//TestToMon
           Usb->SendBitSensor(ui->checkbox_InjToTest->isChecked());//InjToTest
       }
       else
       {
           Usb->SendBitSensor(((299-icol) == 0)&&(ui->checkbox_AOutToMon->isChecked() == true));//
           Usb->SendBitSensor(false);//
           Usb->SendBitSensor(false);//
           Usb->SendBitSensor(false);//
       }
       Usb->SendBuffer();//Ivan
     }//cols
     for (int irow=0; irow<23; irow++) {

       if( (22-irow) == ui->H35Row->value())//FormMain -> EditH35Row -> Text.ToInt() )
       {
           Usb->SendBitSensor(false);//nn
           Usb->SendBitSensor((22-irow) == ui->RowToAOut->value());//SF
           Usb->SendBitSensor(ui->checkbox_PixelToTest->isChecked());//Test IVAN
           Usb->SendBitSensor(ui->checkbox_InjToPixel->isChecked());//Inj

       }
       else
       {
           Usb->SendBitSensor(false);//nn
           Usb->SendBitSensor((22-irow) == ui->RowToAOut->value());//SF
           Usb->SendBitSensor(false);//
           Usb->SendBitSensor(false);//
       }
       Usb->SendBuffer();//Ivan

     }//rows

     int  H35DAC[17];

     H35DAC[0] = ui->spinBox_HBDig->value();
     H35DAC[1] = ui->spinBox_PDelDig->value();
     H35DAC[2] = ui->spinBox_NDelDig->value();
     H35DAC[3] = ui->spinBox_PTrimDig->value();
     H35DAC[4] = ui->spinBox_NCompDig->value();
     H35DAC[5] = ui->spinBox_BLResDig->value();
     H35DAC[6] = ui->spinBox_BLRes->value();
     H35DAC[7] = ui->spinBox_NBiasRes->value();
     H35DAC[8] = ui->spinBox_NFB->value();
     H35DAC[9] = ui->spinBox_PTrim->value();
     H35DAC[10] = ui->spinBox_NTWDown->value();
     H35DAC[11] = ui->spinBox_NTW->value();
     H35DAC[12] = ui->spinBox_NLogic->value();
     H35DAC[13] = ui->spinBox_PAmpLoad->value();
     H35DAC[14] = ui->spinBox_NSF->value();
     H35DAC[15] = ui->spinBox_PAmp->value();
     H35DAC[16] = ui->spinBox_PAB->value();

     Usb -> SendDACSensor(H35DAC[16], false );
     Usb -> SendDACSensor(H35DAC[15], true );
      Usb -> SendDACSensor(H35DAC[14], true );
       Usb -> SendDACSensor(H35DAC[13], true );
        Usb -> SendDACSensor(H35DAC[12], true );
         Usb -> SendDACSensor(H35DAC[11], true );
          Usb -> SendDACSensor(H35DAC[10], true );
           Usb -> SendDACSensor(H35DAC[9], true );
            Usb -> SendDACSensor(H35DAC[8], true );
             Usb -> SendDACSensor(H35DAC[7], true );
              Usb -> SendDACSensor(H35DAC[6], true );
               Usb -> SendDACSensor(H35DAC[5], true );
                Usb -> SendDACSensor(H35DAC[4], true );
                 Usb -> SendDACSensor(H35DAC[3], true );
                  Usb -> SendDACSensor(H35DAC[2], true );
                   Usb -> SendDACSensor(H35DAC[1], false );
                    Usb -> SendDACSensor(H35DAC[0], true );

     Usb -> SendLoadDAC();
     Usb->SendBuffer();
     logit("Updated");
     delete Usb;*/
    GetASICConfig(ui->MatrixSelect->currentIndex());
    /*for (int i=0; i<1411;i++)
        {
            std::cout << MatrixAconfig[i];
        }
        std::cout << std::endl;*/
    if (isuxibo)
    {
        Funkcije* Usb;
        Usb = new Funkcije(this->genio);
        for (int iMat = 0; iMat<1292; iMat++)
        {
            Usb->SendBitSensor(MatrixAconfig[iMat]);
        }
        //Usb->SendBuffer();

        for (int iDAC = 0; iDAC<17; iDAC++)
            Usb -> SendDACSensor(SensorADAC[16-iDAC], Unlock[16-iDAC] );

        Usb -> SendLoadDAC();
        Usb->SendBuffer();
        delete Usb;
    }
    else
    {
        if (ui->MatrixSelect->currentIndex() == 0) //NMatrix
            nexys->WriteASIC(0x00, MatrixNconfig, 0);
        if (ui->MatrixSelect->currentIndex() == 1) //AMatrix
            nexys->WriteASIC(0x00, MatrixAconfig, 1);
        if (ui->MatrixSelect->currentIndex() == 2) //BMatrix
            nexys->WriteASIC(0x00, MatrixBconfig, 2);
        if (ui->MatrixSelect->currentIndex() == 3) //_Matrix
            nexys->WriteASIC(0x00, Matrix_config, 3);
        nexys->flush();
    }
    if (!quiet)
        logit("Sensor configuration updated");
}

//---------------------------------------------\\
//Get Config for Sensor from GUI to bool-vector\\
//---------------------------------------------\\

void MainWindow::GetASICConfig(int Matrix)
{
    if (Matrix == 1)
    {
        PixelASel[0] = ui->H35Col->value();
        PixelASel[1] = ui->H35Row->value();
        PixelASel[2] = ui->RowToAOut->value();
        ModeASel[0] = ui->checkbox_AOutToMon->isChecked();
        ModeASel[1] = ui->checkbox_PixelToTest->isChecked();
        ModeASel[2] = ui->checkbox_InjToPixel->isChecked();
        ModeASel[3] = ui->checkbox_TestToMon->isChecked();
        ModeASel[4] = ui->checkbox_InjToTest->isChecked();
        ModeASel[5] = ui->checkBox->isChecked();
        //columns
        for (int icol=0; icol<300; icol++)
        {
            if( ((299-icol) == ui->H35Col->value()) || ((299-icol) == ui->H35Col_2->value()) || ((299-icol) == ui->H35Col_3->value()))
            {
                MatrixAconfig[icol*4] = (ui->checkbox_AOutToMon->isChecked());//MuxEn
                MatrixAconfig[icol*4+1] = (ui->checkbox_InjToPixel->isChecked());//InjToCol
                MatrixAconfig[icol*4+2] = (ui->checkbox_TestToMon->isChecked());//TestToMon
                MatrixAconfig[icol*4+3] = (ui->checkbox_InjToTest->isChecked());//InjToTest
            }
            else
            {
                MatrixAconfig[icol*4] = (false);
                MatrixAconfig[icol*4+1] = (false);
                MatrixAconfig[icol*4+2] = (false);
                MatrixAconfig[icol*4+3] = (false);
            }
        }

        //rows
        for (int irow=0; irow<23; irow++) {

            if( (22-irow) == (ui->H35Row->value()))
            {
                MatrixAconfig[irow*4+1200] = (false);//nn
                MatrixAconfig[irow*4+1201] = (ui->RowToAOut->value());//SF
                MatrixAconfig[irow*4+1202] = (ui->checkbox_PixelToTest->isChecked());//Test IVAN
                MatrixAconfig[irow*4+1203] = (ui->checkbox_InjToPixel->isChecked());//Inj
            }
            else
            {
                MatrixAconfig[irow*4+1200] = (false);
                MatrixAconfig[irow*4+1201] = (false);
                MatrixAconfig[irow*4+1202] = (false);
                MatrixAconfig[irow*4+1203] = (false);
            }
        }

        //Sensor DACs
        /*SensorADAC[0] = ui->spinBox_HBDig->value();
        SensorADAC[1] = ui->spinBox_PDelDig->value();
        SensorADAC[2] = ui->spinBox_NDelDig->value();
        SensorADAC[3] = ui->spinBox_PTrimDig->value();
        SensorADAC[4] = ui->spinBox_NCompDig->value();
        SensorADAC[5] = ui->spinBox_BLResDig->value();
        SensorADAC[6] = ui->spinBox_BLRes->value();
        SensorADAC[7] = ui->spinBox_NBiasRes->value();
        SensorADAC[8] = ui->spinBox_NFB->value();
        SensorADAC[9] = ui->spinBox_PTrim->value();
        SensorADAC[10] = ui->spinBox_NTWDown->value();
        SensorADAC[11] = ui->spinBox_NTW->value();
        SensorADAC[12] = ui->spinBox_NLogic->value();
        SensorADAC[13] = ui->spinBox_PAmpLoad->value();
        SensorADAC[14] = ui->spinBox_NSF->value();
        SensorADAC[15] = ui->spinBox_PAmp->value();
        SensorADAC[16] = ui->spinBox_PAB->value();*/

        SensorADAC[16] = ui->spinBox_HBDig->value();
        SensorADAC[15] = ui->spinBox_PDelDig->value();
        SensorADAC[14] = ui->spinBox_NDelDig->value();
        SensorADAC[13] = ui->spinBox_PTrimDig->value();
        SensorADAC[12] = ui->spinBox_NCompDig->value();
        SensorADAC[11] = ui->spinBox_BLResDig->value();
        SensorADAC[10] = ui->spinBox_BLRes->value();
        SensorADAC[9] = ui->spinBox_NBiasRes->value();
        SensorADAC[8] = ui->spinBox_NFB->value();
        SensorADAC[7] = ui->spinBox_PTrim->value();
        SensorADAC[6] = ui->spinBox_NTWDown->value();
        SensorADAC[5] = ui->spinBox_NTW->value();
        SensorADAC[4] = ui->spinBox_NLogic->value();
        SensorADAC[3] = ui->spinBox_PAmpLoad->value();
        SensorADAC[2] = ui->spinBox_NSF->value();
        SensorADAC[1] = ui->spinBox_PAmp->value();
        SensorADAC[0] = ui->spinBox_PAB->value();

        for (int iDAC = 0; iDAC < 17; iDAC++)
        {
            if (ui->IsUnlocked->isChecked())
                MatrixAconfig[1292+iDAC*7] = Unlock[iDAC];
            else
                MatrixAconfig[1292+iDAC*7] = Lock[iDAC];
            for (int ibit=0; ibit<6; ibit++)
            {
                MatrixAconfig[1292+iDAC*7+1+ibit] = (SensorADAC[iDAC] & (0x01<<(ibit))) != 0x00;
            }
        }
        //std::cout << "Matrix A" << std::endl;
    }

    if (Matrix == 2)
    {
        PixelBSel[0] = ui->H35Col->value();
        PixelBSel[1] = ui->H35Row->value();
        PixelBSel[2] = ui->RowToAOut->value();
        ModeBSel[0] = ui->checkbox_AOutToMon->isChecked();
        ModeBSel[1] = ui->checkbox_PixelToTest->isChecked();
        ModeBSel[2] = ui->checkbox_InjToPixel->isChecked();
        ModeBSel[3] = ui->checkbox_TestToMon->isChecked();
        ModeBSel[4] = ui->checkbox_InjToTest->isChecked();
        ModeBSel[5] = ui->checkBox->isChecked();
        //columns
        for (int icol=0; icol<300; icol++)
        {
            if( ((299-icol) == ui->H35Col->value()) || ((299-icol) == ui->H35Col_2->value()) || ((299-icol) == ui->H35Col_3->value()))
            {
                MatrixBconfig[icol*4] = (((299-icol) == 0)&&(ui->checkbox_AOutToMon->isChecked() == true));//MuxEn
                MatrixBconfig[icol*4+1] = (ui->checkbox_InjToPixel->isChecked());//InjToCol
                MatrixBconfig[icol*4+2] = (ui->checkbox_TestToMon->isChecked());//TestToMon
                MatrixBconfig[icol*4+3] = (ui->checkbox_InjToTest->isChecked());//InjToTest
            }
            else
            {
                MatrixBconfig[icol*4] = (((299-icol) == 0)&&(ui->checkbox_AOutToMon->isChecked() == true));
                MatrixBconfig[icol*4+1] = (false);
                MatrixBconfig[icol*4+2] = (false);
                MatrixBconfig[icol*4+3] = (false);
            }
        }

        //rows
        for (int irow=0; irow<23; irow++) {

            if( (22-irow) == ui->H35Row->value())
            {
                MatrixBconfig[irow*4+1200] = (false);//nn
                MatrixBconfig[irow*4+1201] = ((22-irow) == ui->RowToAOut->value());//SF
                MatrixBconfig[irow*4+1202] = (ui->checkbox_PixelToTest->isChecked());//Test IVAN
                MatrixBconfig[irow*4+1203] = (ui->checkbox_InjToPixel->isChecked());//Inj
            }
            else
            {
                MatrixBconfig[irow*4+1200] = (false);
                MatrixBconfig[irow*4+1201] = ((22-irow) == ui->RowToAOut->value());
                MatrixBconfig[irow*4+1202] = (false);
                MatrixBconfig[irow*4+1203] = (false);
            }
        }

        //Sensor DACs
        SensorBDAC[16] = ui->spinBox_HBDig->value();
        SensorBDAC[15] = ui->spinBox_PDelDig->value();
        SensorBDAC[14] = ui->spinBox_NDelDig->value();
        SensorBDAC[13] = ui->spinBox_PTrimDig->value();
        SensorBDAC[12] = ui->spinBox_NCompDig->value();
        SensorBDAC[11] = ui->spinBox_BLResDig->value();
        SensorBDAC[10] = ui->spinBox_BLRes->value();
        SensorBDAC[9] = ui->spinBox_NBiasRes->value();
        SensorBDAC[8] = ui->spinBox_NFB->value();
        SensorBDAC[7] = ui->spinBox_PTrim->value();
        SensorBDAC[6] = ui->spinBox_NTWDown->value();
        SensorBDAC[5] = ui->spinBox_NTW->value();
        SensorBDAC[4] = ui->spinBox_NLogic->value();
        SensorBDAC[3] = ui->spinBox_PAmpLoad->value();
        SensorBDAC[2] = ui->spinBox_NSF->value();
        SensorBDAC[1] = ui->spinBox_PAmp->value();
        SensorBDAC[0] = ui->spinBox_PAB->value();

        for (int iDAC = 0; iDAC < 17; iDAC++)
        {
            if (ui->IsUnlocked->isChecked())
                MatrixBconfig[1292+iDAC*7] = Unlock[iDAC];
            else
                MatrixBconfig[1292+iDAC*7] = Lock[iDAC];
            for (int ibit=0; ibit<6; ibit++)
            {
                MatrixBconfig[1292+iDAC*7+1+ibit] = (SensorBDAC[iDAC] & (0x01<<(ibit))) != 0x00;
            }
        }
    }

    //NMOS Standalone Matrix
    if (Matrix == 0)
    {
        PixelNSel[0] = ui->H35Col->value();
        PixelNSel[1] = ui->H35Row->value();
        PixelNSel[2] = ui->RowToAOut->value();
        ModeNSel[0] = ui->checkbox_AOutToMon->isChecked();
        ModeNSel[1] = ui->checkbox_PixelToTest->isChecked();
        ModeNSel[2] = ui->checkbox_InjToPixel->isChecked();
        ModeNSel[3] = ui->checkbox_TestToMon->isChecked();
        ModeNSel[4] = ui->checkbox_InjToTest->isChecked();
        ModeNSel[5] = ui->checkBox->isChecked();

        for (int irow = 0; irow<16; irow++)
        {
            SendConfig(1,0,irow,0,1,0);
            nexys->WriteASIC(0x00, MatrixNconfig, 0);
            nexys->flush();
            SendConfig(1,0,irow,0,0,0);
            nexys->WriteASIC(0x00, MatrixNconfig, 0);
            nexys->flush();
        }
        for (int dig=0; dig<40;dig++)
        {
            SendConfig(0, 1, 0, dig, 0, 1);
            nexys->WriteASIC(0x00, MatrixNconfig, 0);
            nexys->flush();
            SendConfig(0, 1, 0, dig, 0, 0);
            nexys->WriteASIC(0x00, MatrixNconfig, 0);
            nexys->flush();
        }
        SendConfig(0, 0, 0, 0, 0, 0);
        //nexys->flush();


    }
    if (Matrix == 3)
    {
        Pixel_Sel[0] = ui->H35Col->value();
        Pixel_Sel[1] = ui->H35Row->value();
        Pixel_Sel[2] = ui->RowToAOut->value();
        Mode_Sel[0] = ui->checkbox_AOutToMon->isChecked();
        Mode_Sel[1] = ui->checkbox_PixelToTest->isChecked();
        Mode_Sel[2] = ui->checkbox_InjToPixel->isChecked();
        Mode_Sel[3] = ui->checkbox_TestToMon->isChecked();
        Mode_Sel[4] = ui->checkbox_InjToTest->isChecked();
        Mode_Sel[5] = ui->checkBox->isChecked();
        //columns
        for (int icol=0; icol<300; icol++)
        {
            if( (299-icol) == ui->H35Col->value())
            {
                Matrix_config[icol*4] = (((299-icol) == 0)&&(ui->checkbox_AOutToMon->isChecked() == true));//MuxEn
                Matrix_config[icol*4+1] = (ui->checkbox_InjToPixel->isChecked());//InjToCol
                Matrix_config[icol*4+2] = (ui->checkbox_TestToMon->isChecked());//TestToMon
                Matrix_config[icol*4+3] = (ui->checkbox_InjToTest->isChecked());//InjToTest
            }
            else
            {
                Matrix_config[icol*4] = (((299-icol) == 0)&&(ui->checkbox_AOutToMon->isChecked() == true));
                Matrix_config[icol*4+1] = (false);
                Matrix_config[icol*4+2] = (false);
                Matrix_config[icol*4+3] = (false);
            }
        }

        //rows
        for (int irow=0; irow<16; irow++) {

            if( (15-irow) == ui->H35Row->value())
            {
                Matrix_config[irow*4+1200] = (ui->checkbox_RAM->isChecked());//RAM
                Matrix_config[irow*4+1201] = ((15-irow) == ui->RowToAOut->value());//SF
                Matrix_config[irow*4+1202] = (ui->checkbox_PixelToTest->isChecked());//Test IVAN
                Matrix_config[irow*4+1203] = (ui->checkbox_InjToPixel->isChecked());//Inj
            }
            else
            {
                Matrix_config[irow*4+1200] = (false);
                Matrix_config[irow*4+1201] = ((15-irow) == ui->RowToAOut->value());
                Matrix_config[irow*4+1202] = (false);
                Matrix_config[irow*4+1203] = (false);
            }
        }

        //Sensor DACs
        Sensor_DAC[16] = ui->spinBox_HBDig->value();
        Sensor_DAC[15] = ui->spinBox_PDelDig->value();
        Sensor_DAC[14] = ui->spinBox_NDelDig->value();
        Sensor_DAC[13] = ui->spinBox_PTrimDig->value();
        Sensor_DAC[12] = ui->spinBox_NCompDig->value();
        Sensor_DAC[11] = ui->spinBox_BLResDig->value();
        Sensor_DAC[10] = ui->spinBox_BLRes->value();
        Sensor_DAC[9] = ui->spinBox_NBiasRes->value();
        Sensor_DAC[8] = ui->spinBox_NFB->value();
        Sensor_DAC[7] = ui->spinBox_PTrim->value();
        Sensor_DAC[6] = ui->spinBox_NTWDown->value();
        Sensor_DAC[5] = ui->spinBox_NTW->value();
        Sensor_DAC[4] = ui->spinBox_NLogic->value();
        Sensor_DAC[3] = ui->spinBox_PAmpLoad->value();
        Sensor_DAC[2] = ui->spinBox_NSF->value();
        Sensor_DAC[1] = ui->spinBox_PAmp->value();
        Sensor_DAC[0] = ui->spinBox_PAB->value();

        for (int iDAC = 0; iDAC < 17; iDAC++)
        {
            if (ui->IsUnlocked->isChecked())
                Matrix_config[1292+iDAC*7] = Unlock[iDAC];
            else
                Matrix_config[1292+iDAC*7] = Lock[iDAC];
            for (int ibit=0; ibit<6; ibit++)
            {
                Matrix_config[1292+iDAC*7+1+ibit] = (Sensor_DAC[iDAC] & (0x01<<(ibit))) != 0x00;
            }
        }
    }
}

//generates configuration for the Standalone MatriX N
void MainWindow::SendConfig(bool ModeRamPix, bool ModeRamDig, int LdPixIndex, int LdDigIndex, bool LdPixEn, bool LdDigEn)
{
    //Digital horizontal control register
    for (int dig = 0; dig <40; dig++)
    {
        MatrixNconfig[dig*2] = HBenRow[dig];                      //HB En
        MatrixNconfig[dig*2+1] = ((dig==LdDigIndex) && LdDigEn);  //RAM Ld
    }

    //columns
    for (int icol=0; icol<300; icol++)      //MUSS ICOL RÜCKWÄRTS LAUFEN? BZW ICOL -> 299-ICOL, genauso IROW UNTEN && DIG OBEN
    {

        //ModeRamPix
        if (ModeRamPix)
        {
            //std::cout << "rampix" << std::endl;
            MatrixNconfig[icol*4+80] = RamPix1[(299-icol)][LdPixIndex]; //Ram(1)
            MatrixNconfig[icol*4+81] = RamPix0[(299-icol)][LdPixIndex]; //InjToCol  &&  RAM(0)
            MatrixNconfig[icol*4+82] = 0; //TestToMon
            MatrixNconfig[icol*4+83] = 0; //InjToTest
        }
        //ModeRamDig
        else if (ModeRamDig)
        {
            //std::cout << "ramdig" << std::endl;
            if (icol%5 == 0)
            {
                MatrixNconfig[icol*4+81] = RamDig1[int(((299-icol)/5)*2+1)][LdDigIndex]; //InjToCol  &&  RAM(0)
                MatrixNconfig[icol*4+80] = RamDig3[int(((299-icol)/5)*2+1)][LdDigIndex]; //Ram(1)
            }
            if (icol%5 == 1)
            {
                MatrixNconfig[icol*4+81] = RamDig0[int(((299-icol)/5)*2+1)][LdDigIndex];
                MatrixNconfig[icol*4+80] = RamDig2[int(((299-icol)/5)*2+1)][LdDigIndex];
            }
            if (icol%5 == 2)
            {
                MatrixNconfig[icol*4+81] = RamDig3[int(((299-icol)/5)*2)][LdDigIndex];
                MatrixNconfig[icol*4+80] = 0;
            }
            if (icol%5 == 3)
            {
                MatrixNconfig[icol*4+81] = RamDig2[int(((299-icol)/5)*2)][LdDigIndex];
                MatrixNconfig[icol*4+80] = RamDig1[int(((299-icol)/5)*2)][LdDigIndex];
            }
            if (icol%5 == 4)
            {
                MatrixNconfig[icol*4+81] = RamDig0[int(((299-icol)/5)*2)][LdDigIndex];
                MatrixNconfig[icol*4+80] = 0;
            }
            MatrixNconfig[icol*4+82] = 0; //TestToMon
            MatrixNconfig[icol*4+83] = 0; //InjToTest
        }
        //Mode NormalConfig
        else
        {
            //std::cout << "normal" << std::endl;
            if ((299-icol)%5 == 0)
                MatrixNconfig[icol*4+80] = 0;
            if ((299-icol)%5 == 1)
                MatrixNconfig[icol*4+80] = 0;
            if ((299-icol)%5 == 2)
                MatrixNconfig[icol*4+80] = HBenCol[int(((299-icol)/5)*2)];
            if ((299-icol)%5 == 3)
                MatrixNconfig[icol*4+80] = 0;
            if ((299-icol)%5 == 4)
                MatrixNconfig[icol*4+80] = HBenCol[int(((299-icol)/5)*2+1)];
            if( ((299-icol) == ui->H35Col->value()) || InjEnCol[icol] || Matrixinjection || RowInjection)
            {
                //MatrixNconfig[icol*4+80] = (((299-icol) == 0)&&(ui->checkbox_AOutToMon->isChecked() == true)); //HB
                MatrixNconfig[icol*4+81] = (ui->checkbox_InjToPixel->isChecked());//InjToCol
                MatrixNconfig[icol*4+82] = (ui->checkbox_TestToMon->isChecked());//TestToMon
                MatrixNconfig[icol*4+83] = (ui->checkbox_InjToTest->isChecked());//InjToTest
            }
            else
            {
                //MatrixNconfig[icol*4+80] = (((299-icol) == 0)&&(ui->checkbox_AOutToMon->isChecked() == true));
                MatrixNconfig[icol*4+81] = (false);
                MatrixNconfig[icol*4+82] = (false);
                MatrixNconfig[icol*4+83] = (false);
            }
        }

    }

    //rows
    for (int irow=0; irow<16; irow++)
    {
        MatrixNconfig[irow*4+1280] = (((15-irow) == LdPixIndex)&&(LdPixEn));          //RAM ld
        MatrixNconfig[irow*4+1281] = ((15-irow) == ui->RowToAOut->value());                   //Amp Out
        MatrixNconfig[irow*4+1282] = (((15-irow) == ui->RowToAOut->value()) && ui->checkbox_PixelToTest->isChecked());    //Amp to test
        MatrixNconfig[irow*4+1283] = (((15-irow) == ui->H35Row->value()) && ui->checkbox_InjToPixel->isChecked()) || InjEnRow[irow] || Matrixinjection || ColInjection;     //Inj En
    }

    //Sensor DACs
    SensorNDAC[16] = ui->spinBox_HBDig->value();
    SensorNDAC[15] = ui->spinBox_PDelDig->value();
    SensorNDAC[14] = ui->spinBox_NDelDig->value();
    SensorNDAC[13] = ui->spinBox_PTrimDig->value();
    SensorNDAC[12] = ui->spinBox_NCompDig->value();
    SensorNDAC[11] = ui->spinBox_BLResDig->value();
    SensorNDAC[10] = ui->spinBox_BLRes->value();
    SensorNDAC[9] = ui->spinBox_NBiasRes->value();
    SensorNDAC[8] = ui->spinBox_NFB->value();
    SensorNDAC[7] = ui->spinBox_PTrim->value();
    SensorNDAC[6] = ui->spinBox_NTWDown->value();
    SensorNDAC[5] = ui->spinBox_NTW->value();
    SensorNDAC[4] = ui->spinBox_NLogic->value();
    SensorNDAC[3] = ui->spinBox_PAmpLoad->value();
    SensorNDAC[2] = ui->spinBox_NSF->value();
    SensorNDAC[1] = ui->spinBox_PAmp->value();
    SensorNDAC[0] = ui->spinBox_PAB->value();

    for (int iDAC = 0; iDAC < 17; iDAC++)
    {
        if (ui->IsUnlocked->isChecked())
        {
            //1292+iDAC*7
            MatrixNconfig[1344+iDAC*7] = Unlock[iDAC];
            //std::cout << "dac unlocked" << std::endl;
        }
        else
        {
            MatrixNconfig[1344+iDAC*7] = Lock[iDAC];
            //std::cout << "dac locked" << std::endl;
        }
        for (int ibit=0; ibit<6; ibit++)
        {
            MatrixNconfig[1344+iDAC*7+1+ibit] = (SensorNDAC[iDAC] & (0x01<<(ibit))) != 0x00;
        }
    }
    /*for (int i = 0; i < 150; i++)
    {
        std::cout << MatrixNconfig[i] ;
        if (i%10 == 9)
                     std::cout << std::endl;
    }*/
}




void MainWindow::on_Slider_HBDig_valueChanged(int value)
{
    ui->spinBox_HBDig->setValue(value);
}

void MainWindow::on_Slider_PDelDig_valueChanged(int value)
{
    ui->spinBox_PDelDig->setValue(value);
}

void MainWindow::on_Slider_NDelDig_valueChanged(int value)
{
    ui->spinBox_NDelDig->setValue(value);
}

void MainWindow::on_Slider_PTrimDig_valueChanged(int value)
{
    ui->spinBox_PTrimDig->setValue(value);
}

void MainWindow::on_Slider_NCompDig_valueChanged(int value)
{
    ui->spinBox_NCompDig->setValue(value);
}

void MainWindow::on_Slider_BLResDig_valueChanged(int value)
{
    ui->spinBox_BLResDig->setValue(value);
}

void MainWindow::on_Slider_BLRes_valueChanged(int value)
{
    ui->spinBox_BLRes->setValue(value);
}

void MainWindow::on_Slider_NBiasRes_valueChanged(int value)
{
    ui->spinBox_NBiasRes->setValue(value);
}

void MainWindow::on_Slider_NFB_valueChanged(int value)
{
    ui->spinBox_NFB->setValue(value);
}

void MainWindow::on_Slider_PTrim_valueChanged(int value)
{
    ui->spinBox_PTrim->setValue(value);
}

void MainWindow::on_Slider_NTWDown_valueChanged(int value)
{
    ui->spinBox_NTWDown->setValue(value);
}

void MainWindow::on_Slider_NTW_valueChanged(int value)
{
    ui->spinBox_NTW->setValue(value);
}

void MainWindow::on_Slider_NLogic_valueChanged(int value)
{
    ui->spinBox_NLogic->setValue(value);
}

void MainWindow::on_Slider_PAmpLoad_valueChanged(int value)
{
    ui->spinBox_PAmpLoad->setValue(value);
}

void MainWindow::on_Slider_NSF_valueChanged(int value)
{
    ui->spinBox_NSF->setValue(value);
}

void MainWindow::on_Slider_PAmp_valueChanged(int value)
{
    ui->spinBox_PAmp->setValue(value);
}

void MainWindow::on_Slider_PAB_valueChanged(int value)
{
    ui->spinBox_PAB->setValue(value);
}

void MainWindow::on_spinBox_HBDig_valueChanged(int arg1)
{
    ui->Slider_HBDig->setValue(arg1);
}

void MainWindow::on_spinBox_PDelDig_valueChanged(int arg1)
{
    ui->Slider_PDelDig->setValue(arg1);
}

void MainWindow::on_spinBox_NDelDig_valueChanged(int arg1)
{
    ui->Slider_NDelDig->setValue(arg1);
}

void MainWindow::on_spinBox_PTrimDig_valueChanged(int arg1)
{
    ui->Slider_PTrimDig->setValue(arg1);
}

void MainWindow::on_spinBox_NCompDig_valueChanged(int arg1)
{
    ui->Slider_NCompDig->setValue(arg1);
}

void MainWindow::on_spinBox_BLResDig_valueChanged(int arg1)
{
    ui->Slider_BLResDig->setValue(arg1);
}

void MainWindow::on_spinBox_BLRes_valueChanged(int arg1)
{
    ui->Slider_BLRes->setValue(arg1);
}

void MainWindow::on_spinBox_NBiasRes_valueChanged(int arg1)
{
    ui->Slider_NBiasRes->setValue(arg1);
}

void MainWindow::on_spinBox_NFB_valueChanged(int arg1)
{
    ui->Slider_NFB->setValue(arg1);
}

void MainWindow::on_spinBox_PTrim_valueChanged(int arg1)
{
    ui->Slider_PTrim->setValue(arg1);
    ui->spinBox_PTrim_2->setValue(arg1);
}

void MainWindow::on_spinBox_NTWDown_valueChanged(int arg1)
{
    ui->Slider_NTWDown->setValue(arg1);
}

void MainWindow::on_spinBox_NTW_valueChanged(int arg1)
{
    ui->Slider_NTW->setValue(arg1);
}

void MainWindow::on_spinBox_NLogic_valueChanged(int arg1)
{
    ui->Slider_NLogic->setValue(arg1);
}

void MainWindow::on_spinBox_PAmpLoad_valueChanged(int arg1)
{
    ui->Slider_PAmpLoad->setValue(arg1);
}

void MainWindow::on_spinBox_NSF_valueChanged(int arg1)
{
    ui->Slider_NSF->setValue(arg1);
}

void MainWindow::on_spinBox_PAmp_valueChanged(int arg1)
{
    ui->Slider_PAmp->setValue(arg1);
}

void MainWindow::on_spinBox_PAB_valueChanged(int arg1)
{
    ui->Slider_PAB->setValue(arg1);
}

void MainWindow::on_Injection_clicked()
{
    if (isuxibo)
    {
        Funkcije* funcc = new Funkcije(genio);
    funcc->InitPatternHitbus();
    funcc->StartPattern();
    funcc->SendBuffer();
    logit("Analog Injection sent.");
    delete funcc;
    }
    else
    {
       /* nexys->PatGenPeriod(4);
        nexys->PatGenFlags(0x03);
        nexys->PatGenRunLength(9);
        nexys->PatGenClockDiv(2048);
        nexys->PatGenInitialDelay(0);*/

        nexys->PatGen();

        nexys->PatGenReset(1);
        nexys->PatGenReset(0);
        nexys->PatGenSuspend(0);
        nexys->flush();
        Sleep(1000);
        //nexys->PatGenReset(1);
        nexys->flush();
    }
}

void MainWindow::on_Start_Pattern_clicked()
{
    // Start Pattern Generator
    Funkcije* funcc = new Funkcije(genio);
    // 21ms for 128 pulses
    funcc->InitPatternHitbus(60000); // 65535 is  max. 16 bit int
    //funcc->InitCounter();
    funcc->StartPattern();
    funcc->SendBuffer();
    ui->checkBox->setChecked(true); // Start Pattern generation as long as checkbox is active.
    logit("Injections started.");
    delete funcc;
}

void MainWindow::on_timer_update()
{
    if (ui->checkBox->isChecked())
    {
        Funkcije* funcc = new Funkcije(genio);
        funcc->StartPattern();
        funcc->SendBuffer();
        delete funcc;
    }
}

void MainWindow::on_SearchDevices_clicked()
{
    ftdi->Close();
    ui->Devices->clear();
    FT_STATUS ftStatus;
    FT_HANDLE ftHandleTemp;
    DWORD numDevs;
    DWORD Flags;
    DWORD ID;
    DWORD Type;
    DWORD LocId;
    char SerialNumber[16];
    char Description[64];

    // create the device information list
    ftStatus = FT_CreateDeviceInfoList(&numDevs);
    if (ftStatus == FT_OK)
    {
        std::cout << "Number of devices is " << numDevs << std::endl;
    }
    //if (numDevs > 0) {
    std::string Descrip;
    for(unsigned int i=0; i<numDevs; i++)
    {
        // get information for all devices
        ftStatus = FT_GetDeviceInfoDetail(i, &Flags, &Type, &ID, &LocId, SerialNumber, Description, &ftHandleTemp);
        if (ftStatus == FT_OK)
        {
            std::cout << "Dev " << i << std::endl;
            std::cout << " Flags  =" << Flags << std::endl;
            std::cout << " Type = " << Type << std::endl;
            std::cout << " ID = " << ID << std::endl;
            std::cout << " LocId = " << LocId << std::endl;
            std::cout << " SerialNumber = " << SerialNumber << std::endl;
            std::cout << " Description = " << Description << std::endl;
            std::cout << " ftHandle = " << ftHandleTemp << std::endl;
            Descrip = QString::number(i).toStdString();
            Descrip.append(" ");
            Descrip.append(std::string(Description));
            ui->Devices->addItem(QString::fromStdString(Descrip));           
        }
    }
    if (ui->Devices->itemText(0) == "")
    {
        ui->Devices->addItem("No device detected");
    }
}

void MainWindow::on_OpenDevice_clicked()
{
    FT_HANDLE ftHandleTemp = NULL;
    bool open = false;
    if (ui->Devices->currentText() != "No device detected")
    {
        ftdi->Close();
        //ftdi->purge();
        open = ftdi->Open(QString::fromStdString(ui->Devices->currentText().toStdString().substr(0,1)).toInt());
        if(!open)
            logit("Board could not be initialized.");
        else
            logit("Board successfully initialized.");

        //Select FTDI synchronous mode for NexysVideo and asynchronous for Uxibo
        if (ui->FTModus->currentIndex() == 0)
        {

            ftdi->setBitMode(0xFF,0x00);
            ftdi->setBitMode(0xFF,0x40);
            FT_SetLatencyTimer(ftHandleTemp,2);
            FT_SetUSBParameters(ftHandleTemp, 0x10000, 0x10000);
            nexys->initializeFtdi(ftdi);
            isuxibo = false;

        }
        else
        {
            genio->initializeFtdi(ftdi);
            isuxibo = true;
        }

        logit("Channel " + QString::fromStdString(ui->Devices->currentText().toStdString().substr(0,1)).toStdString() + " opened.");
    }
    else
        logit("No Device selected");

}

void MainWindow::on_Devices_currentIndexChanged(int index)
{
    if (ui->Devices->currentText().mid(2,5) == "Uxibo")
    {
        if (ui->Devices->currentText().mid(18,1)=="A")
        {
            ui->OpenDevice->setEnabled(false);
        }
        if (ui->Devices->currentText().mid(18,1)=="B")
        {
            ui->OpenDevice->setEnabled(true);
        }
        ui->FTModus->setCurrentIndex(1);
        return;
    }
    if (ui->Devices->currentText().mid(2,8) == "Digilent")
    {
        if (ui->Devices->currentText().mid(22,1)=="B")
        {
            ui->OpenDevice->setEnabled(false);
        }
        if (ui->Devices->currentText().mid(22,1)=="A")
        {
            ui->OpenDevice->setEnabled(true);
        }
        ui->FTModus->setCurrentIndex(0);
        return;
    }
    ui->OpenDevice->setEnabled(false);
}

void MainWindow::on_CloseDevice_clicked()
{
    ftdi->Close();
}

void MainWindow::on_SendPair_clicked()
{
    int addr, val;

    std::stringstream s, r;
    std::string str;

    str = ui->SendAddr->toPlainText().toStdString();
    s << str;
    s >> std::hex >> addr;
    str = ui->SendVal->toPlainText().toStdString();
    r << str;
    r >> std::hex >> val;
    std::cout << addr << " = " << std::bitset<8>(addr) << std::endl;
    std::cout << val << " = " << std::bitset<8>(val) << std::endl;
    if (isuxibo)
    {
        genio->addpair(addr, val);
        genio->sendbuf();
    }
    else
    {
        nexys->AddByte(0x01);
        nexys->AddByte(addr);
        nexys->AddByte(0x00);
        nexys->AddByte(0x01);



        nexys->AddByte(val);

        nexys->flush();

    }


}


void MainWindow::GetPCBConfig()
{
    PCBDAC[9] = ui->DAC1->text().toFloat();
    PCBDAC[8] = ui->DAC2->text().toFloat();
    PCBDAC[7] = ui->DAC3->text().toFloat();
    PCBDAC[6] = ui->DAC4->text().toFloat();
    PCBDAC[5] = ui->DAC5->text().toFloat();
    PCBDAC[4] = ui->DAC6->text().toFloat();
    PCBDAC[3] = ui->DAC7->text().toFloat();
    PCBDAC[2] = ui->DAC8->text().toFloat();
    PCBDAC[1] = ui->DAC9->text().toFloat();
    PCBDAC[0] = ui->DAC10->text().toFloat();
}

void MainWindow::on_Update_2_clicked()
{
    GetPCBConfig();
    for (int i = 0; i<10; i++)
    {
        int Steps = floor(16383 * PCBDAC[i]/3.3);
        for (int ibit=0; ibit<14; ibit++)
          {
              PCBConfig[i*16+ibit] = (Steps & (0x01<<(13-ibit))) != 0x00;
          }
        PCBConfig[i*16+14] = 0;
        PCBConfig[i*16+15] = 0;
    }

   /* for (int i = 0; i <10; i++)
    {
        for (int j = 0; j<16; j++)
        {
            std::cout << PCBConfig[i*16+j];
        }
        std::cout << std::endl;
    }*/
    nexys->WritePCB(0x01, PCBConfig);
    nexys->flush();
}

void MainWindow::on_MatrixSelect_currentIndexChanged(int index)
{
    if (index < 0)
    {
        GetASICConfig(currentMatrix);
        currentMatrix = index;
        SetASICConfig(currentMatrix);
    }
    else
    {
        currentMatrix = index;
    }
    if (index == 1)
    {
        ui->GetFullMatrix->setText("Scan Signal of Matrix A");
        ui->GetFullMatrix->setEnabled(1);
    }
    else
    {
        if (index == 2)
        {
            ui->GetFullMatrix->setText("Scan Signal of Matrix B");
            ui->GetFullMatrix->setEnabled(1);
        }
        else
        {
            ui->GetFullMatrix->setText("Scan Signal disabled\nSelect Matrix A or B");
            ui->GetFullMatrix->setEnabled(0);
        }
    }
}

//-------------------------------------------\\
//Put Config for Sensor to GUI to bool-vector\\
//-------------------------------------------\\

void MainWindow::SetASICConfig(int Matrix)
{
    if (Matrix == 0)
    {
        ui->spinBox_HBDig->setValue(SensorNDAC[16]);
        ui->spinBox_PDelDig->setValue(SensorNDAC[15]);
        ui->spinBox_NDelDig->setValue(SensorNDAC[14]);
        ui->spinBox_PTrimDig->setValue(SensorNDAC[13]);
        ui->spinBox_NCompDig->setValue(SensorNDAC[12]);
        ui->spinBox_BLResDig->setValue(SensorNDAC[11]);
        ui->spinBox_BLRes->setValue(SensorNDAC[10]);
        ui->spinBox_NBiasRes->setValue(SensorNDAC[9]);
        ui->spinBox_NFB->setValue(SensorNDAC[8]);
        ui->spinBox_PTrim->setValue(SensorNDAC[7]);
        ui->spinBox_NTWDown->setValue(SensorNDAC[6]);
        ui->spinBox_NTW->setValue(SensorNDAC[5]);
        ui->spinBox_NLogic->setValue(SensorNDAC[4]);
        ui->spinBox_PAmpLoad->setValue(SensorNDAC[3]);
        ui->spinBox_NSF->setValue(SensorNDAC[2]);
        ui->spinBox_PAmp->setValue(SensorNDAC[1]);
        ui->spinBox_PAB->setValue(SensorNDAC[0]);

        ui->H35Col->setValue(PixelNSel[0]);
        ui->H35Row->setValue(PixelNSel[1]);
        ui->RowToAOut->setValue(PixelNSel[2]);

        ui->checkbox_AOutToMon->setChecked(ModeNSel[0]);
        ui->checkbox_PixelToTest->setChecked(ModeNSel[1]);
        ui->checkbox_InjToPixel->setChecked(ModeNSel[2]);
        ui->checkbox_TestToMon->setChecked(ModeNSel[3]);
        ui->checkbox_InjToTest->setChecked(ModeNSel[4]);
        ui->checkBox->setChecked(ModeNSel[5]);
    }
    if (Matrix == 1)
    {
        ui->spinBox_HBDig->setValue(SensorADAC[16]);
        ui->spinBox_PDelDig->setValue(SensorADAC[15]);
        ui->spinBox_NDelDig->setValue(SensorADAC[14]);
        ui->spinBox_PTrimDig->setValue(SensorADAC[13]);
        ui->spinBox_NCompDig->setValue(SensorADAC[12]);
        ui->spinBox_BLResDig->setValue(SensorADAC[11]);
        ui->spinBox_BLRes->setValue(SensorADAC[10]);
        ui->spinBox_NBiasRes->setValue(SensorADAC[9]);
        ui->spinBox_NFB->setValue(SensorADAC[8]);
        ui->spinBox_PTrim->setValue(SensorADAC[7]);
        ui->spinBox_NTWDown->setValue(SensorADAC[6]);
        ui->spinBox_NTW->setValue(SensorADAC[5]);
        ui->spinBox_NLogic->setValue(SensorADAC[4]);
        ui->spinBox_PAmpLoad->setValue(SensorADAC[3]);
        ui->spinBox_NSF->setValue(SensorADAC[2]);
        ui->spinBox_PAmp->setValue(SensorADAC[1]);
        ui->spinBox_PAB->setValue(SensorADAC[0]);

        ui->H35Col->setValue(PixelASel[0]);
        ui->H35Row->setValue(PixelASel[1]);
        ui->RowToAOut->setValue(PixelASel[2]);

        ui->checkbox_AOutToMon->setChecked(ModeASel[0]);
        ui->checkbox_PixelToTest->setChecked(ModeASel[1]);
        ui->checkbox_InjToPixel->setChecked(ModeASel[2]);
        ui->checkbox_TestToMon->setChecked(ModeASel[3]);
        ui->checkbox_InjToTest->setChecked(ModeASel[4]);
        ui->checkBox->setChecked(ModeASel[5]);
    }
    if (Matrix == 2)
    {
        ui->spinBox_HBDig->setValue(SensorBDAC[16]);
        ui->spinBox_PDelDig->setValue(SensorBDAC[15]);
        ui->spinBox_NDelDig->setValue(SensorBDAC[14]);
        ui->spinBox_PTrimDig->setValue(SensorBDAC[13]);
        ui->spinBox_NCompDig->setValue(SensorBDAC[12]);
        ui->spinBox_BLResDig->setValue(SensorBDAC[11]);
        ui->spinBox_BLRes->setValue(SensorBDAC[10]);
        ui->spinBox_NBiasRes->setValue(SensorBDAC[9]);
        ui->spinBox_NFB->setValue(SensorBDAC[8]);
        ui->spinBox_PTrim->setValue(SensorBDAC[7]);
        ui->spinBox_NTWDown->setValue(SensorBDAC[6]);
        ui->spinBox_NTW->setValue(SensorBDAC[5]);
        ui->spinBox_NLogic->setValue(SensorBDAC[4]);
        ui->spinBox_PAmpLoad->setValue(SensorBDAC[3]);
        ui->spinBox_NSF->setValue(SensorBDAC[2]);
        ui->spinBox_PAmp->setValue(SensorBDAC[1]);
        ui->spinBox_PAB->setValue(SensorBDAC[0]);

        ui->H35Col->setValue(PixelBSel[0]);
        ui->H35Row->setValue(PixelBSel[1]);
        ui->RowToAOut->setValue(PixelBSel[2]);

        ui->checkbox_AOutToMon->setChecked(ModeBSel[0]);
        ui->checkbox_PixelToTest->setChecked(ModeBSel[1]);
        ui->checkbox_InjToPixel->setChecked(ModeBSel[2]);
        ui->checkbox_TestToMon->setChecked(ModeBSel[3]);
        ui->checkbox_InjToTest->setChecked(ModeBSel[4]);
        ui->checkBox->setChecked(ModeBSel[5]);
    }
    if (Matrix == 3)
    {
        ui->spinBox_HBDig->setValue(Sensor_DAC[16]);
        ui->spinBox_PDelDig->setValue(Sensor_DAC[15]);
        ui->spinBox_NDelDig->setValue(Sensor_DAC[14]);
        ui->spinBox_PTrimDig->setValue(Sensor_DAC[13]);
        ui->spinBox_NCompDig->setValue(Sensor_DAC[12]);
        ui->spinBox_BLResDig->setValue(Sensor_DAC[11]);
        ui->spinBox_BLRes->setValue(Sensor_DAC[10]);
        ui->spinBox_NBiasRes->setValue(Sensor_DAC[9]);
        ui->spinBox_NFB->setValue(Sensor_DAC[8]);
        ui->spinBox_PTrim->setValue(Sensor_DAC[7]);
        ui->spinBox_NTWDown->setValue(Sensor_DAC[6]);
        ui->spinBox_NTW->setValue(Sensor_DAC[5]);
        ui->spinBox_NLogic->setValue(Sensor_DAC[4]);
        ui->spinBox_PAmpLoad->setValue(Sensor_DAC[3]);
        ui->spinBox_NSF->setValue(Sensor_DAC[2]);
        ui->spinBox_PAmp->setValue(Sensor_DAC[1]);
        ui->spinBox_PAB->setValue(Sensor_DAC[0]);

        ui->H35Col->setValue(Pixel_Sel[0]);
        ui->H35Row->setValue(Pixel_Sel[1]);
        ui->RowToAOut->setValue(Pixel_Sel[2]);

        ui->checkbox_AOutToMon->setChecked(Mode_Sel[0]);
        ui->checkbox_PixelToTest->setChecked(Mode_Sel[1]);
        ui->checkbox_InjToPixel->setChecked(Mode_Sel[2]);
        ui->checkbox_TestToMon->setChecked(Mode_Sel[3]);
        ui->checkbox_InjToTest->setChecked(Mode_Sel[4]);
        ui->checkBox->setChecked(Mode_Sel[5]);
    }
}

void MainWindow::on_scanButton_clicked()
{
    unsigned int i;

    if(instr != VI_NULL)
        return;

    /*
    * First we must call viOpenDefaultRM to get the manager
    * handle.  We will store this handle in defaultRM.
    */
   status=viOpenDefaultRM(&defaultRM);
   if (status < VI_SUCCESS)
   {
      logit("Could not open a session to the VISA Resource Manager!\n");
      exit(EXIT_FAILURE);
   }

   /* Find all the USB TMC VISA resources in our system and store the  */
   /* number of resources in the system in numInstrs.                  */
   ViUInt32 numInstrs;
   ViFindList findList;
   status = viFindRsrc(defaultRM, (ViString)"USB?*INSTR", &findList, &numInstrs,
                       instrResourceString);

   if (status < VI_SUCCESS)
   {
      logit("An error occurred while finding resources.\nHit enter to continue.");
      //fflush(stdin);
      //getchar();
      viClose(defaultRM);
      //return(status);
   }

   /*
    * Now we will open VISA sessions to all USB TMC instruments.
    * We must use the handle from viOpenDefaultRM and we must
    * also use a string that indicates which instrument to open.  This
    * is called the instrument descriptor.  The format for this string
    * can be found in the function panel by right clicking on the
    * descriptor parameter. After opening a session to the
    * device, we will get a handle to the instrument which we
    * will use in later VISA functions.  The AccessMode and Timeout
    * parameters in this function are reserved for future
    * functionality.  These two parameters are given the value VI_NULL.
    */

   for (i=0; i<numInstrs; i++)
   {
      if (i > 0)
         viFindNext(findList, instrResourceString);

      status = viOpen(defaultRM, instrResourceString, VI_NULL, VI_NULL, &instr);

      if (status < VI_SUCCESS)
      {
         logit("Cannot open a session to the device" + QString::number(i+1).toStdString());
         continue;
      }

      /*
       * At this point we now have a session open to the USB TMC instrument.
       * We will now use the viWrite function to send the device the string "*IDN?\n",
       * asking for the device's identification.
       */
      ViUInt32 writeCount;
      char stringinput[512];

      strcpy(stringinput,"*IDN?\n");
      status = viWrite(instr, (ViBuf)stringinput, (ViUInt32)strlen(stringinput), &writeCount);
      if (status < VI_SUCCESS)
      {
         logit("Error writing to the device" + QString::number(i+1).toStdString());
         status = viClose(instr);
         continue;
      }

       /*
       * Now we will attempt to read back a response from the device to
       * the identification query that was sent.  We will use the viRead
       * function to acquire the data.  We will try to read back 100 bytes.
       * This function will stop reading if it finds the termination character
       * before it reads 100 bytes.
       * After the data has been read the response is displayed.
       */
      ViUInt32 retCount;

      unsigned char buffer[100];

      status = viRead(instr, buffer, 100, &retCount);
      if (status < VI_SUCCESS)
      {
         logit("Error reading a response from the device" + QString::number(i+1).toStdString());
      }
      else
      {
         //printf ("\nDevice %d: %*s\n", i+1, retCount, buffer);
         logit("Device" + QString::number(i+1).toStdString() + ": " + QString::number(retCount).toStdString() +"\n"+ std::string(reinterpret_cast<char*>(buffer)));// + QString(QChar(buffer)).toStdString() ;// + QString(buffer).toStdString());
         //strcpy(stringinput,"HIStogram:COUNt RESET\n");
         //status = viWrite(instr, (ViBuf)stringinput, (ViUInt32)strlen(stringinput), &writeCount);
      }
      status = viClose(instr);
   }


   /*
    * Now we will close the session to the instrument using
    * viClose. This operation frees all system resources.
    */
   status = viClose(defaultRM);
   //logit("Hit enter to continue.");
   //fflush(stdin);
   //getchar();

   //return 0;
}

void MainWindow::on_openButton_clicked()
{
    ViUInt32 numInstrs;
    ViFindList findList;
    status = viOpenDefaultRM(&defaultRM);
    viFindRsrc(defaultRM, (ViString)"USB?*INSTR", &findList, &numInstrs,
            instrResourceString);

    //set limits to the IndexBox range
    //ui->IndexBox->setMinimum(1);
    ui->IndexBox->setMaximum(numInstrs);

    //do not open anything if the requested index is out of range
    if(unsigned (ui->IndexBox->value()) > numInstrs || ui->IndexBox->value() <= 0)
    {
        status = viClose(defaultRM);
        return;
    }

    //get the descriptor for the requested resource
    for(int i=0;i < ui->IndexBox->value()-1; ++i)
    {
        viFindNext(findList,instrResourceString);
    }

    status = viOpen(defaultRM,instrResourceString,VI_NULL, VI_NULL,&instr);

    logit("Channel to osci opened");

}

void MainWindow::on_histogramButton_clicked()
{
    if(instr == VI_NULL)
        return;

    ViUInt32 writtenchars;
    ViUInt32 readchars;

    logit("getHistogram");

    //--- get the parameters to transform to voltage ---
    float yoffset;
    status = viQueryf(instr, (ViString)"Math1:VERTICAL:POSITION?\n", (ViString)"%f",
                      &yoffset);
    logit("yoffset:" + QString::number(yoffset).toStdString());
    if(status < VI_SUCCESS)
    {
        logit("Error getting yoffset");
        return;
    }
    float ymult;
    status = viQueryf(instr, (ViString)"Math1:VERTICAL:SCALE?\n", (ViString)"%f",
                      &ymult);
    logit("scale:" + QString::number(ymult).toStdString());
    if(status < VI_SUCCESS)
    {
        logit("Error getting ymult");
        return;
    }

    //yoffset = -4;
    //ymult= 0.05;
    status = viWrite(instr,(ViBuf)"HISTOGRAM:DATA?\n",16, &writtenchars);

    ViChar buffer[100];

    std::fstream f;
    int index = 0;
    do
    {
        f.close();
        ++index;
        f.open(("histogram_" + QString::number(index).toStdString()
                + ".dat").c_str(), std::ios::in);
    }while(f.is_open());
    f.close();
    f.open(("histogram_" + QString::number(index).toStdString()
            + ".dat").c_str(), std::ios::out);

    logit("write Histogram data to \"histogram_" + QString::number(index).toStdString()
          + ".dat\"");

    readchars = 100;
    index = 0;          //now the variable will be used as index for the bin,
                        // not the file index
    long entries = 0;

    while(readchars == 100)
    {
        status = viRead(instr, (ViBuf)buffer, 100, &readchars);
        //std::cout << buffer << std::endl;
        unsigned int i=0;



        while(i < readchars)
        {
            //building of the number of entries for a bin
            while(i < readchars && buffer[i] != ',')
            {
                entries = entries * 10 + static_cast<long>(buffer[i] - '0');
                //std::cout << buffer[i] << " " << entries << std::endl;
                ++i;
            }

            //at the end of the buffer, it is not clear whether the number was
            //  complete or not, so it will be ccontinued with the next buffer
            if(i != readchars)
            {
                //add the line to the file
                //f << (((126.5 - yoffset) -index )* ymult) << "\t" << entries << std::endl;
                f << ((5.0-yoffset - (index-0.5)/25.0)*ymult) << "\t" << entries << std::endl;
                //prepare for the next bin:
                entries = 0;
                ++i;    //buffer[i] = ',' now
                ++index;
            }
        }
    }
    if(entries > 0)
    {
        f << ((5.0-yoffset - (index-0.5)/25.0)*ymult) << "\t" << entries << std::endl;
        ++index;
    }

    logit("wrote histogram with " + QString::number(index).toStdString()
          + " entries");

    f.close();
}

void MainWindow::on_GetFullMatrix_clicked()
{
    on_Update_2_clicked();
    ui->tabWidget->setCurrentIndex(0);
    ui->tabWidget->show();
    Sleep(1);
    ui->checkbox_TestToMon->setChecked(true);
    ui->checkbox_PixelToTest->setChecked(true);
    ui->checkbox_AOutToMon->setChecked(false);
    ui->checkbox_InjToPixel->setChecked(false);

    logit("Matrix scan for " + ui->MatrixSelect->currentText().toStdString() + "started");

    ViUInt32 readchars;
    std::string Mon0 = "HIST:SOU MATH1", Mon1 = "HIST:SOU MATH2", Mon2 = "HIST:SOU MATH3";

    ui->RowToAOut->setValue(0);
    ui->H35Row->setValue(0);

    //ui->MatrixSelect->setCurrentIndex(1);
    ui->IsUnlocked->setChecked(true);

    ui->H35Col->setValue(0);
    ui->H35Col_2->setValue(1);
    ui->H35Col_3->setValue(2);

    int icolstart = ui->MMcolStart->value()/3;
    int icolmax = (ui->MMcolEnd->value()+2)/3;
    int irowstart = ui->MMrowStart->value();
    int irowmax = ui->MMRowEnd->value();
    int PBnow = 0;
    int PBmax = (1+icolmax)*(irowmax+1)*3;
    //std::cout << icol << " " << irow << " " << icolmax << " " << irowmax << std::endl;
    set_ProgressBar(PBmax, PBnow, 1);

    for (int icol = icolstart; icol < icolmax; icol++)
    {
        //std::cout << irow << " " << icol << std::endl;
        for (int irow = irowstart; irow <= irowmax; irow++)
        {
            //Config Chip
            ui->RowToAOut->setValue(irow);
            ui->H35Row->setValue(irow);
            ui->H35Col->setValue(icol*3);
            ui->H35Col_2->setValue((icol*3)+1);
            ui->H35Col_3->setValue((icol*3)+2);



            std::string nome1 = "hist_Fe55_"+ui->MatrixSelect->currentText().toStdString()+QString::number(irow).toStdString()+"_"+QString::number(icol*3).toStdString() +".dat";
            QFileInfo check_file1(QString::fromStdString(nome1));
            std::string nome2 = "hist_Fe55_"+ui->MatrixSelect->currentText().toStdString()+QString::number(irow).toStdString()+"_"+QString::number((icol*3)+1).toStdString() +".dat";
            QFileInfo check_file2(QString::fromStdString(nome2));
            std::string nome3 = "hist_Fe55_"+ui->MatrixSelect->currentText().toStdString()+QString::number(irow).toStdString()+"_"+QString::number((icol*3)+2).toStdString() +".dat";
            QFileInfo check_file3(QString::fromStdString(nome3));

            //std::cout << nome1 << nome2 << nome3<< std::endl;
            //std::cout << irow << " " << icol << std::endl;

            if (!check_file1.exists() || !check_file2.exists() || !check_file3.exists() )
                on_Update_clicked();
            int acq = 300;
            int k = 0;
            int loopcount = 0;


            PBnow++;
            set_ProgressBar(PBmax, PBnow, 1);


            if (!check_file1.exists())
            {

            //Config Osci
            status = viWrite(instr, (ViBuf)(Mon0.c_str()), 14, &readchars);
            status = viWrite(instr, (ViBuf)("ACQ:STOPA SEQ"), 13, &readchars);
            status = viWrite(instr, (ViBuf)("ACQ:STOPA RUNST"), 15, &readchars);
            status = viWrite(instr, (ViBuf)("ACQ:STATE RUN"), 13, &readchars);
            status = viWrite(instr, (ViBuf)("HIST:COUN RESET"), 15, &readchars);
            status = viWrite(instr, (ViBuf)("TRIGGER:A:EDGE:SOURCE CH1"), 25, &readchars);

            Sleep(1000);

                //wait maybe check acq
                do
                {
                    status = viQueryf(instr,(ViString)"ACQ:NUMACq?", (ViString)"%i", &k);
                    //std::cout << k << std::endl;
                    QApplication::processEvents();
                    Sleep(20);
                    loopcount++;
                }while ((k < acq) && (loopcount < 50000));
                if (k >= acq-1)
                    logit("5000 acq");
                if (loopcount >= 49900)
                {
                    logit("timeout");
                    on_Update_clicked();
                    on_Update_2_clicked();
                }

                //read Histogram
                on_histogramButton_clicked();
                Sleep(100);

                rename("histogram_1.dat", nome1.c_str());
            }

            k = 0;
            loopcount = 0;

            PBnow++;
            set_ProgressBar(PBmax, PBnow, 1);


            if (!check_file2.exists())
            {
                //Config Osci
                status = viWrite(instr, (ViBuf)(Mon1.c_str()), 14, &readchars);
                status = viWrite(instr, (ViBuf)("ACQ:STOPA SEQ"), 13, &readchars);
                status = viWrite(instr, (ViBuf)("ACQ:STOPA RUNST"), 15, &readchars);
                status = viWrite(instr, (ViBuf)("ACQ:STATE RUN"), 13, &readchars);
                status = viWrite(instr, (ViBuf)("HIST:COUN RESET"), 15, &readchars);
                status = viWrite(instr, (ViBuf)("TRIGGER:A:EDGE:SOURCE CH2"), 25, &readchars);
                Sleep(1000);

                //wait maybe check acq
                do
                {
                    status = viQueryf(instr,(ViString)"ACQ:NUMACq?", (ViString)"%i", &k);
                    //std::cout << k << std::endl;
                    QApplication::processEvents();
                    Sleep(20);
                    loopcount++;
                }while ((k < acq) && (loopcount < 50000));
                if (k >= acq-1)
                    logit("5000 acq");
                if (loopcount >= 49900)
                {
                    logit("timeout");
                    on_Update_clicked();
                    on_Update_2_clicked();
                }

                //read Histogram
                on_histogramButton_clicked();
                Sleep(100);
                rename("histogram_1.dat", nome2.c_str());
            }

            k = 0;
            loopcount = 0;

            PBnow++;
            set_ProgressBar(PBmax, PBnow, 1);


            if (!check_file3.exists())
            {
                //Config Osci
                status = viWrite(instr, (ViBuf)(Mon2.c_str()), 14, &readchars);
                status = viWrite(instr, (ViBuf)("ACQ:STOPA SEQ"), 13, &readchars);
                status = viWrite(instr, (ViBuf)("ACQ:STOPA RUNST"), 15, &readchars);
                status = viWrite(instr, (ViBuf)("ACQ:STATE RUN"), 13, &readchars);
                status = viWrite(instr, (ViBuf)("HIST:COUN RESET"), 15, &readchars);
                status = viWrite(instr, (ViBuf)("TRIGGER:A:EDGE:SOURCE CH3"), 25, &readchars);
                Sleep(1000);

                //wait maybe check acq
                do
                {
                    status = viQueryf(instr,(ViString)"ACQ:NUMACq?", (ViString)"%i", &k);
                    //std::cout << k << std::endl;
                    QApplication::processEvents();
                    loopcount++;
                    Sleep(20);
                }while ((k < acq) && (loopcount < 50000));
                if (k >= acq-1)
                    logit("5000 acq");
                if (loopcount >= 49900)
                {
                    logit("timeout");
                    on_Update_clicked();
                    on_Update_2_clicked();
                }

                //read Histogram
                on_histogramButton_clicked();
                Sleep(20);

                rename("histogram_1.dat", nome3.c_str());
            }
        }
    }


    ui->IsUnlocked->setChecked(false);
    on_Update_clicked();
    //ui->DAC8->setText("0");
    //ui->DAC10->setText("0");
    on_Update_2_clicked();

    set_ProgressBar(PBmax, PBnow, 0);
    //status = viWrite(instr, (ViBuf)(ui->query->text().toStdString().c_str()), 14, &readchars);
    logit("Matrix scan for " + ui->MatrixSelect->currentText().toStdString() + "completed");
}


void MainWindow::on_coincidenceButton_clicked()
{
    float min1, min2, min3;
    std::string filename = "mins.dat";
    std::ofstream outfile ((filename).c_str());;
    if(instr == VI_NULL)
        return;

    logit("Aquiring Coincidence Wave Forms ...");

    ViUInt32 writtenchars;
    ViUInt32 readchars;


    //--- set transmission format --
    status = viWrite(instr, (ViBuf)"DATA:ENCDG RIBINARY\n",
                     20, &writtenchars);
    status = viWrite(instr, (ViBuf)"WFMOutpre:BYT_Nr 1\n",
                     19, &writtenchars);

    ui->runningcheckBox->setChecked(true);
    int acq = 1;
    int k = 0;

    while(ui->runningcheckBox->checkState())
    {
        //wait for an event:

        /*int acqs = -1;
        int startacqs = -1;
        while(acqs == startacqs)
        {
            status = viQueryf(instr,(ViString)"ACQ:NUMACq?", (ViString)"%i", &acqs);
            if(status < VI_SUCCESS)
            {
                logit("Error getting acquisitions");
                return;
            }
            if(startacqs == -1)
                startacqs = acqs;
        }

        //set input source: Ch1 = Scintillator
        status = viWrite(instr, (ViBuf)"DATA:Source Ch1",15, &writtenchars);

        if(status < VI_SUCCESS)
        {
            if (ui->CoincCount->value() == 2)
                logit("Error getting \"Scintillator\" WaveForm");
            else
                logit("Error getting \"Signal1\" WaveForm");
            return;
        }*/
        k = 0;
        //loopcount = 0;
        status = viWrite(instr, (ViBuf)("ACQ:STOPA SEQ"), 13, &readchars);
        status = viWrite(instr, (ViBuf)("ACQ:STATE 1"), 11, &readchars);
        do
        {
            status = viQueryf(instr,(ViString)"ACQ:NUMACq?", (ViString)"%i", &k);
            //std::cout << k << std::endl;
        }while (k < acq);

        //Scintillator + Signal
        if (ui->CoincCount->value() == 2)
        {
             status = viWrite(instr, (ViBuf)"DATA:Source Ch1",15, &writtenchars);
             if(status < VI_SUCCESS)
             {
                 logit("Error getting \"Signal2\" WaveForm");
                 return;
             }


            if(getwaveform("Scintillator", false))
            {
                //set input source: Ch2 = Signal
                status = viWrite(instr, (ViBuf)"DATA:Source Ch2",15, &writtenchars);

                if(status < VI_SUCCESS)
                {
                    logit("Error getting \"Signal\" WaveForm");
                    return;
                }

                getwaveform("Signal", true);
            }
        }

        //3x Signal
        if (ui->CoincCount->value() == 3)
        {
            status = viWrite(instr, (ViBuf)"DATA:Source Ch1",15, &writtenchars);
            if(status < VI_SUCCESS)
            {
                logit("Error getting \"Signal1\" WaveForm");
                return;
            }

            getwaveform("Signal1", true);

            status = viWrite(instr, (ViBuf)"DATA:Source Ch2",15, &writtenchars);
            if(status < VI_SUCCESS)
            {
                logit("Error getting \"Signal2\" WaveForm");
                return;
            }
            getwaveform("Signal2", true);

            status = viWrite(instr, (ViBuf)"DATA:Source Ch3",15, &writtenchars);
            if(status < VI_SUCCESS)
            {
                logit("Error getting \"Signal3\" WaveForm");
                return;
            }
            getwaveform("Signal3", true);

            getmin("waveform_0_Signal1.dat");
            getmin("waveform_0_Signal2.dat");
            getmin("waveform_0_Signal3.dat");


            //std::ofstream outfile ((filename).c_str());;
            min1 = getmin("waveform_0_Signal1.dat");
            min2 = getmin("waveform_0_Signal2.dat");
            min3 = getmin("waveform_0_Signal3.dat");
            outfile << min1 << std::endl << min2 << std::endl << min3 << std::endl;
            std::remove("waveform_0_Signal1.dat");
            std::remove("waveform_0_Signal2.dat");
            std::remove("waveform_0_Signal3.dat");
        }


        //3x Pix + Scintillator
        if (ui->CoincCount->value() == 4)
        {
            status = viWrite(instr, (ViBuf)"DATA:Source Ch4",15, &writtenchars);
            if(status < VI_SUCCESS)
            {
                logit("Error getting \"Scintillator\" WaveForm");
                return;
            }
            if (getwaveform("Scintillator", false))
            {

                status = viWrite(instr, (ViBuf)"DATA:Source Ch1",15, &writtenchars);
                if(status < VI_SUCCESS)
                {
                    logit("Error getting \"Signal1\" WaveForm");
                    return;
                }
                getwaveform("Signal1", true);

                status = viWrite(instr, (ViBuf)"DATA:Source Ch2",15, &writtenchars);
                if(status < VI_SUCCESS)
                {
                    logit("Error getting \"Signal2\" WaveForm");
                    return;
                }
                getwaveform("Signal2", true);

                status = viWrite(instr, (ViBuf)"DATA:Source Ch3",15, &writtenchars);
                if(status < VI_SUCCESS)
                {
                    logit("Error getting \"Signal3\" WaveForm");
                    return;
                }
                getwaveform("Signal3", true);
            }
        }


        QApplication::processEvents();
        //Sleep(100);
    }
    outfile.close();
}

bool MainWindow::getwaveform(std::string suffix, bool save)
{
    bool returnvalue = save;
    std::vector<double> waveformdata;

    ViUInt32 writtenchars;
    ViUInt32 readchars;


    //--- get the parameters to transform to time and voltage ---
    float timescale;
    status = viQueryf(instr, (ViString)"WFMOutpre:XINcr?\n", (ViString)"%f",
                      &timescale);
    if(status < VI_SUCCESS)
    {
        logit("Error getting timescale");
        return false;
    }
    float yoffset;
    status = viQueryf(instr, (ViString)"WFMOutpre:YOFF?\n", (ViString)"%f",
                      &yoffset);
    if(status < VI_SUCCESS)
    {
        logit("Error getting yoffset");
        return false;
    }
    float ymult;
    status = viQueryf(instr, (ViString)"WFMOutpre:YMULT?\n", (ViString)"%f",
                      &ymult);
    if(status < VI_SUCCESS)
    {
        logit("Error getting ymult");
        return false;
    }

    //--- getting the curve ---
    status = viWrite(instr, (ViBuf)"CURVE?\n", 7, &writtenchars);

    ViChar buffer[50];

    status = viRead(instr, (ViBuf)buffer, 2, &readchars);

    if(buffer[0] != '#')
    {
        logit("error reading Wave Form");
        return false;
    }
    int length = buffer[1] - '0';

    status = viRead(instr, (ViBuf)buffer, length, &readchars);

    long bufferlength = 0;

    for(int i=0;i<length; ++i)
        bufferlength = bufferlength * 10 + (buffer[i] - '0');

    ViChar* waveform = new ViChar[bufferlength];

    status = viRead(instr, (ViBuf)waveform, bufferlength, &readchars);

    logit("WaveForm transmitted: " + QString::number(readchars).toStdString()
          + " points.");


    //--- transform data to voltage and save to a file ---
    for(unsigned long i=0;i<readchars;++i)
        waveformdata.push_back((static_cast<double>(waveform[i])-yoffset) * ymult);

    if(save || ((fabs((waveformdata.front()) - (waveformdata.back())) > 0.1) && (ui->CoincCount->value()==2)) || ((fabs((waveformdata.front()) - (waveformdata.back())) > 0.03) && (ui->CoincCount->value()==4)))
    {
        returnvalue = true;

        std::fstream f;

        //search for unused filename "waveform_%i_`suffix`.dat":
        static int index = 0;
        f.open(("waveform_" + QString::number(index).toStdString()
                + "_" + suffix + ".dat").c_str(), std::ios::in);
        while(f.is_open())
        {
            f.close();
            ++index;
            f.open(("waveform_" + QString::number(index).toStdString()
                    + "_" + suffix + ".dat").c_str(), std::ios::in);
        }
        f.close();
        f.open(("waveform_" + QString::number(index).toStdString()
                + "_" + suffix + ".dat").c_str(), std::ios::out | std::ios::app);

        f << "# Channel: " << suffix << std::endl
          << "# timescale: " << timescale << std::endl;

        //write transformed data into the file:
        /*
        for(unsigned long i=0;i<readchars;++i)
        {
            //dataset datum;
            //datum.x = i*timescale;
            //datum.y = (static_cast<double>(waveform[i])-yoffset) * ymult;

            f //<< (i*timescale) << "\t"
              << ((static_cast<double>(waveform[i])-yoffset) * ymult) << std::endl;
        }
        */

        std::vector<double>::iterator it = waveformdata.begin();
        while(it != waveformdata.end())
        {
            f << *it << std::endl;
            ++it;
        }

        f.close();

        logit("WaveForm saved to \"waveform_" + QString::number(index).toStdString()
              + "_" + suffix + ".dat\"");

    }

    delete waveform;

    return returnvalue;
}

QVector<std::pair<double, double> > MainWindow::getwaveform(int channel, int lengtha)
{
    int acq = 1;
    int k = 0;
    ViUInt32 writtenchars;
    ViUInt32 readchars;

    status = viWrite(instr, (ViBuf)("ACQ:STOPA SEQ"), 13, &readchars);
    status = viWrite(instr, (ViBuf)("ACQ:STATE 1"), 11, &readchars);
    do
    {
        status = viQueryf(instr,(ViString)"ACQ:NUMACq?", (ViString)"%i", &k);
    }while (k < acq);

    if (channel == 1)
        status = viWrite( instr, (ViBuf)"DATA:Source Ch1",15, &writtenchars);
    if (channel == 2)
        status = viWrite( instr, (ViBuf)"DATA:Source Ch2",15, &writtenchars);
    if (channel == 3)
        status = viWrite( instr, (ViBuf)"DATA:Source Ch3",15, &writtenchars);
    if (channel == 4)
        status = viWrite( instr, (ViBuf)"DATA:Source Ch4",15, &writtenchars);

    if(status < VI_SUCCESS)
    {
     logit("Error getting \"Signal2\" WaveForm");
    }

    std::vector<double> waveformdata;
    QVector<std::pair<double, double> > returnvalue;




    //--- get the parameters to transform to time and voltage ---
    float timescale;
    status = viQueryf(instr, (ViString)"WFMOutpre:XINcr?\n", (ViString)"%f",
                      &timescale);
    if(status < VI_SUCCESS)
    {
        logit("Error getting timescale");
        //return false;
    }
    float yoffset;
    status = viQueryf(instr, (ViString)"WFMOutpre:YOFF?\n", (ViString)"%f",
                      &yoffset);
    if(status < VI_SUCCESS)
    {
        logit("Error getting yoffset");
        //return false;
    }
    float ymult;
    status = viQueryf(instr, (ViString)"WFMOutpre:YMULT?\n", (ViString)"%f",
                      &ymult);
    if(status < VI_SUCCESS)
    {
        logit("Error getting ymult");
        //return false;
    }

    //--- getting the curve ---
    status = viWrite(instr, (ViBuf)"CURVE?\n", 7, &writtenchars);

    ViChar buffer[50];

    status = viRead(instr, (ViBuf)buffer, 2, &readchars);

    if(buffer[0] != '#')
    {
        logit("error reading Wave Form");
        //return false;
    }
    int length = buffer[1] - '0';

    status = viRead(instr, (ViBuf)buffer, length, &readchars);

    long bufferlength = 0;

    for(int i=0;i<length; ++i)
        bufferlength = bufferlength * 10 + (buffer[i] - '0');

    ViChar* waveform = new ViChar[bufferlength];

    status = viRead(instr, (ViBuf)waveform, bufferlength, &readchars);

    logit("WaveForm transmitted: " + QString::number(readchars).toStdString()
          + " points.");


    //--- transform data to voltage and save to a file ---
    for(unsigned long i=0;i<readchars;++i)
        waveformdata.push_back((static_cast<double>(waveform[i])-yoffset) * ymult);


      //  f << "# Channel: " << suffix << std::endl
       //   << "# timescale: " << timescale << std::endl;


    std::vector<double>::iterator it = waveformdata.begin();
    double time = 0.0;
    while(it != waveformdata.end())
    {
        //temp = *it;
        returnvalue.append(std::make_pair<double, double > ((double)time, (double)*it));
        std::cout << time << "\t" << *it << std::endl;
        ++it;
        time = time + timescale;
    }

    delete waveform;

    return returnvalue;
}


void MainWindow::on_InjectionButton_clicked()
{
    if(ui->InjectionButton->text() == "Start Injections")
    {
        ui->InjectionButton->setText("Stop Injections");

        nexys->PatGen(ui->PeriodSpinBox->value(),ui->NumInjectionSpinBox->value(),ui->ClockDivSpinBox->value(),0);

        nexys->PatGenReset(1);
        nexys->PatGenReset(0);
        nexys->PatGenSuspend(0);
        //nexys->flush();
        //Sleep(1000);
        //nexys->PatGenReset(1);
        //nexys->flush();

    }
    else
    {
        nexys->PatGenReset(true);
        ui->InjectionButton->setText("Start Injections");
    }
    nexys->flush();
}

float MainWindow::getmax(std::string filename)
{
    std::string line;
    std::ifstream myfile ((filename).c_str());
    float f = 0.0;
    if (myfile.is_open())
    {
      while ( getline (myfile,line) )
      {
        float g = QString::fromStdString(line).toFloat();
        if (f<g)
            f = g;

      }
      //cout << i << "," << f << endl;
      myfile.close();

      return f;
    }
}

float MainWindow::getmin(std::string filename)
{
    std::string line;
    std::ifstream myfile ((filename).c_str());
    float f = 0.0;
    if (myfile.is_open())
    {
      while ( getline (myfile,line) )
      {
        float g = QString::fromStdString(line).toFloat();
        if (f>g)
            f = g;

      }
      //cout << i << "," << f << endl;
      myfile.close();

      return f;
    }
}



void MainWindow::on_Hitbus_clicked()
{
    logit("HitBus selection started");
    QVector<bool> HBConfig;
    for (int i = 0; i<60; i++)
    {
        HBConfig.push_back(HBenCol[i]);
    }
    for (int i = 0; i<40; i++)
    {
        HBConfig.push_back(HBenRow[i]);
    }
    emit MainToHB(HBConfig);
    HBsel->show();
}

void MainWindow::HBToMain(QVector<bool> HBConfig)
{
    logit("HitBus selection accepted");
    for (int i = 0; i<60; i++)
    {
        HBenCol[i] = HBConfig[i];
    }
    for (int i = 0; i<40; i++)
    {
        HBenRow[i] = HBConfig[60+i];
    }
}

void MainWindow::DigSelToMain(QVector<std::pair<unsigned int, unsigned int> > activepixels)
{
    std::stringstream s("");
    s << "Active Pixels: " << activepixels.length();
    if (!quiet)
        logit(s.str());

    for(int i=0;i<120;++i)
    {
        for(int j=0;j<40;++j)
        {
            RamDig1[i][j] = false;
            RamDig2[i][j] = false;
        }
    }
    for(auto it: activepixels)
    {
        unsigned int pixelindex = 16*it.first + (15-it.second);

        unsigned int configblock = pixelindex / 40;

       if(configblock & 1)
           RamDig1[configblock][39-(pixelindex % 40)] = true;
       else
           RamDig2[configblock][39-(pixelindex % 40)] = true;

       if (!quiet)
           std::cout << "config: (" << configblock << " | " << (39-(pixelindex % 40)) << ")" << std::endl;
    }

}

void MainWindow::GeburtstagtoMain(QVector<std::pair<unsigned int, unsigned int> > activepixels)
{
    QVector<std::pair<unsigned int, unsigned int> > hitpixels;

    //set up Injections and turn them off:
    ui->NumInjectionSpinBox->setValue(1);
    if(ui->InjectionButton->text() != "Start Injections")
        on_InjectionButton_clicked();

    //turn on fast readout:
    if(ui->FastReadoutEnable->text() == "Start Fast Readout")
        on_FastReadoutEnable_clicked();

    ui->FastReadoutFIFO->setChecked(true);
    ui->FastReadoutStateMachine->setChecked(false);

    on_Update_clicked();
    on_Update_2_clicked();

    srand (time(NULL));

    while(activepixels.size() > 0) //for(auto it: activepixels)
    {
        QVector<std::pair<unsigned int, unsigned int> >::iterator it;
        it = activepixels.begin();

        if(ui->GeburtstagRandom->checkState() == 2)
        {
            int index;
            index = rand() % activepixels.size();
            for(int i = 0; i < index; ++i)
                ++it;
        }

        std::cout << "Testing Pixel (" << it->first << " | " << it->second << ")" << std::endl;

        //set up pixel for injections:
        ui->H35Col->setValue(it->first);
        ui->H35Row->setValue(it->second);
        on_Update_clicked();
        //SendConfig(0, 0, 0, 0, 0, 0);
        //reset fast readout:
        //on_FastReadoutEnable_clicked();  //turn fast clock back on
        QApplication::processEvents();

        on_FastReadoutReset_clicked();
        on_InjectionButton_clicked();

        Sleep(10);

        //read out the data:
        nexys->AddByte(0x00);   //read
        nexys->AddByte(0x0B);   //address
        nexys->AddByte(0);   //length high
        nexys->AddByte(48);   //length low
        nexys->flush();
        Sleep(10);
        std::string answer = nexys->read(48);

        for(int i=0; (unsigned) i<answer.length(); i+=8)
        {
            int pixelindex = answer[i]*40-1 + answer[i+5];

            if(answer[i] != -1 && answer[i+5] != -1)
            {
                hitpixels.push_back(std::make_pair<unsigned int, unsigned int>(pixelindex / 16, 15-(pixelindex%16)));

                std::cout << "Raw Data: " <<(int) answer.c_str()[i] << " " << (int)answer.c_str()[i+5] << std::endl;
                std::cout << "Decoded:  " << hitpixels.back().first << " " << hitpixels.back().second << std::endl;
            }
        }

        on_InjectionButton_clicked();   //turn off injections

        //on_FastReadoutEnable_clicked(); //turn fast readout off again

        if(hitpixels.size() > 0)
            emit MainToGeburtstag(hitpixels);
        QApplication::processEvents();

        activepixels.erase(it);
    }
}

void MainWindow::DisplayReadoutClosed()
{
    ui->liveDisplay->setChecked(0);
}


void MainWindow::on_DigSel_clicked()
{

    QVector<std::pair<unsigned int, unsigned int> > activepixels;

    for(int i=0;i<120;++i)
    {
        for(int j=0;j<40;++j)
        {
            if(RamDig1[i][j] || RamDig2[i][j])
            {
                unsigned int pixelindex = i*40 + 39-j;
                unsigned int x = pixelindex / 16;
                unsigned int y = 15-(pixelindex % 16);

                activepixels.push_back(std::make_pair(x,y));
            }
        }
    }

    for(auto it: activepixels)
        std::cout << "(" << it.first << " | " << it.second << ")" << std::endl;

    DigselUI->show();

    emit MainToDigSel(activepixels);
    logit("Digital Select Started");
}


void MainWindow::on_ReadoutButton_clicked()
{
    unsigned int length = ui->NumReadout->value()*8;
    unsigned int lengthH = length / 256;
    unsigned int lengthL = length % 256;
    nexys->AddByte(0x00);   //read
    nexys->AddByte(0x0B);   //address
    nexys->AddByte(lengthH);   //length high
    nexys->AddByte(lengthL);   //length low
    nexys->flush();
    Sleep(10);
    std::string answer = nexys->read(length);
    std::cout << "Read Length: " << answer.length() << std::endl;
    std::cout << "\"" << answer << "\"" << std::endl;

    std::cout << std::setw(9) << "ROBind" << std::setw(9) << "trigTS1"  << std::setw(9) << "trigTS2"  << std::setw(9) << "exttime "  << std::setw(9) << "TSL"  << std::setw(9) << "ADDRL"  << std::setw(9) << "TSR"  << std::setw(9) << "ADDRR" << std::endl;
    for(int i=0; (unsigned) i<answer.length();  ++i)
    {
        //Readout Block
        if(i%8 == 0)
        {
            if ((unsigned short)answer.c_str()[i] > 60)
                std::cout << std::setw(8) << "-" << " ";
            else
                std::cout << std::setw(8) << (unsigned short)answer.c_str()[i] << " ";
        }

        //Trigger Timestamp (2 byte)
        if(i%8 == 1)
        {
            if ((unsigned short)answer.c_str()[i-1] > 60)
                std::cout << std::setw(8) << "-" << " ";
            else
            {
                if ((unsigned short)answer.c_str()[i] > 255)
                    std::cout << std::setw(8) << (unsigned short)answer.c_str()[i]-65280 << " ";
                else
                    std::cout << std::setw(8) << (unsigned short)answer.c_str()[i] << " ";
            }
        }
        if(i%8 == 2)
        {
            if ((unsigned short)answer.c_str()[i-2] > 60)
                std::cout << std::setw(8) << "-" << " ";
            else
            {
                if ((unsigned short)answer.c_str()[i] > 255)
                    std::cout << std::setw(8) << (unsigned short)answer.c_str()[i]-65280 << " ";
                else
                    std::cout << std::setw(8) << (unsigned short)answer.c_str()[i] << " ";
            }
        }

        //Extended Timestamp (generated in FPGA)
        if(i%8 == 3)
        {
            if ((unsigned short)answer.c_str()[i-3] > 60)
                std::cout << std::setw(8) << "-" << " ";
            else
            {
                if ((unsigned short)answer.c_str()[i] > 255)
                    std::cout << std::setw(8) << (unsigned short)answer.c_str()[i]-65280 << " ";
                else
                    std::cout << std::setw(8) << (unsigned short)answer.c_str()[i] << " ";
            }
        }

        //Timestamp left
        if(i%8 == 4)
        {
            if ((unsigned short)answer.c_str()[i-4] > 60)
                std::cout << std::setw(8) << "-" << " ";
            else
            {
                if ((unsigned short)answer.c_str()[i+1] > 0)
                {
                    if ((unsigned short)answer.c_str()[i] > 255)
                        std::cout << std::setw(8) << (unsigned short)answer.c_str()[i]-65280 << " ";
                    else
                        std::cout << std::setw(8) << (unsigned short)answer.c_str()[i] << " ";
                }
                else
                    std::cout << std::setw(8) << "-" << " ";
            }
        }

        //Address left
        if(i%8 == 5)
        {
            if ((unsigned short)answer.c_str()[i-5] > 60)
                std::cout << std::setw(8) << "-" << " ";
            else
            {
                if ((unsigned short)answer.c_str()[i] > 0)
                {
                    if ((unsigned short)answer.c_str()[i] > 255)
                        std::cout << std::setw(8) << (unsigned short)answer.c_str()[i]-65280 << " ";
                    else
                        std::cout << std::setw(8) << (unsigned short)answer.c_str()[i] << " ";
                }
                else
                    std::cout << std::setw(8) << "-" << " ";
            }
        }

        //Timestamp right
        if(i%8 == 6)
        {
            if ((unsigned short)answer.c_str()[i-6] > 60)
                std::cout << std::setw(8) << "-" << " ";
            else
            {
                if ((unsigned short)answer.c_str()[i+1] > 0)
                {
                    if ((unsigned short)answer.c_str()[i] > 255)
                        std::cout << std::setw(8) << (unsigned short)answer.c_str()[i]-65280 << " ";
                    else
                        std::cout << std::setw(8) << (unsigned short)answer.c_str()[i] << " ";
                }
                else
                    std::cout << std::setw(8) << "-" << " ";
            }
        }

        //Address right
        if(i%8 == 7)
        {
            if ((unsigned short)answer.c_str()[i-7] > 60)
                std::cout << std::setw(8) << "-" << " ";
            else
            {
                if ((unsigned short)answer.c_str()[i] > 0)
                {
                    if ((unsigned short)answer.c_str()[i] > 255)
                        std::cout << std::setw(8) << (unsigned short)answer.c_str()[i]-65280 << " ";
                    else
                        std::cout << std::setw(8) << (unsigned short)answer.c_str()[i] << " ";
                }
                else
                    std::cout << std::setw(8) << "-" << " ";
            }
            std::cout << std::endl;
        }
    }
    std::cout << std::endl;
}



void MainWindow::on_get_efficiency_button_clicked()
{
    QVector<std::pair<unsigned int, unsigned int> >  activepixels;
    QVector<std::pair<unsigned int, double> >  result;
    activepixels.clear();
    activepixels.push_back(std::make_pair<unsigned int, unsigned int>((unsigned) ui->H35Col->value(), (unsigned) ui->H35Row->value()));
    result = get_efficiencyV2(activepixels, ui->DAC4->text().toDouble(), 128, 1);
    bool failed =1;
    while ((result.length() != 0) &&(failed ==1))
    {
        if ((result.back().first/16 == ui->H35Col->value())&&(15- (result.back().first % 16) == ui->H35Row->value()))
        {
            failed = 0;
        }
        else
            result.pop_back();
    }
    if (failed)
        logit("Efficiency of Pixel (" +ui->H35Col->text().toStdString() + " " + ui->H35Row->text().toStdString() + ") with current settings is 0");
    else
        logit("Efficiency of Pixel (" + std::to_string(((int)result.back().first)/16) + " " + std::to_string(15-((int)result.back().first)%16) + ") with current settings is " + " " +std::to_string(result.back().second));



    /*if (result.length()!=0)
        logit("Efficiency of Pixel (" + std::to_string(((int)result.back().first)/16) + " " + std::to_string(15-((int)result.back().first)%16) + ") with current settings is " + " " +std::to_string(result.back().second));
    else
        logit("Efficiency of Pixel (" +ui->H35Col->text().toStdString() + " " + ui->H35Row->text().toStdString() + ") with current settings is 0");*/
}

//pixelindex / 16;
               // unsigned int y = 15-(pixelindex % 16);


void MainWindow::on_get_delay_button_clicked()
{
    QVector<std::pair<unsigned int, unsigned int> >  activepixels;
    QVector<std::pair<unsigned int, double> >  result;
    activepixels.clear();
    activepixels.push_back(std::make_pair<unsigned int, unsigned int>((unsigned) ui->H35Col->value(), (unsigned) ui->H35Row->value()));
    result = get_delay(activepixels, ui->DAC4->text().toDouble(), 1024, 1);
    bool failed =1;
    while ((result.length() != 0) &&(failed ==1))
    {
        if ((result.back().first/16 == ui->H35Col->value())&&(15- (result.back().first % 16) == ui->H35Row->value()))
        {
            failed = 0;
        }
        else
            result.pop_back();
    }
    if (failed)
        logit("Delay of Pixel (" +ui->H35Col->text().toStdString() + " " + ui->H35Row->text().toStdString() + ") with current settings could not be measured.");
    else
        logit("Delay of Pixel (" + std::to_string(((int)result.back().first)/16) + " " + std::to_string(15-((int)result.back().first)%16) + ") with current settings is " + std::to_string(result.back().second));
}

void MainWindow::on_get_delayCurve_clicked()
{
    logit("Delay Curve started");
    quiet = 1;
    QVector<std::pair<unsigned int, unsigned int> >  activepixels;
    QVector<std::pair<unsigned int, double> >  result;
    std::string filename;
    std::ofstream outfile ;
    filename = "DelayCurve (" + std::to_string(ui->H35Col->value()) + " " + std::to_string(ui->H35Row->value()) + ").dat";
    outfile.open(((filename).c_str()));
    outfile << "";
    activepixels.clear();
    activepixels.push_back(std::make_pair<unsigned int, unsigned int>((unsigned) ui->H35Col->value(), (unsigned) ui->H35Row->value()));

    double Th2start = ui->Thstart->text().toDouble();
    double Th2end   = ui->Thend->text().toDouble();
    double Th2step  = ui->Thstep->text().toDouble();

    double injstart = ui->injstart->text().toDouble();
    double injend   = ui->injend->text().toDouble();
    double injstep  = ui->injstep->text().toDouble();

    int PBnow = 0;
    int PBmax = ((injstart-injend)/injstep)*((Th2start-Th2end)/Th2step);
    set_ProgressBar(PBmax, PBnow, 1);

    for (double Th2 = Th2start; Th2 > Th2end; Th2= Th2-Th2step)
    {
        ui->DAC2->setText(QString::number(Th2));
        outfile << Th2;
        for (double inj = injstart; inj >injend; inj = inj-injstep)
        {
            PBnow++;

            set_ProgressBar(PBmax, PBnow, 1);
            ui->DAC4->setText(QString::number(inj));
            on_Update_2_clicked();
            result = get_delay(activepixels, ui->DAC4->text().toDouble(), 1024, 1);
            bool failed =1;
            while ((result.length() != 0) &&(failed ==1))
            {
                if ((result.back().first/16 == ui->H35Col->value())&&(15- (result.back().first % 16) == ui->H35Row->value()))
                {
                    failed = 0;
                }
                else
                    result.pop_back();
            }
            if (!failed)
                outfile << "\t" << result.back().second;
        }
        outfile << std::endl;
    }


    outfile.close();
    activepixels.clear();
    quiet = 0;
    logit("Delay Curve completed");
    set_ProgressBar(PBmax, PBnow, 0);
}


void MainWindow::on_FastReadoutEnable_clicked()
{
    //Important: Notice that the Caption of this button is also used in other methods, so be careful changing the caption

    //Turn on Fast Readout Clock:
    if(ui->FastReadoutEnable->text() == "Start Fast Readout")
    {
        ui->FastReadoutEnable->setText("Stop Fast Readout");

        nexys->AddByte(0x01);   //Write Header
        nexys->AddByte(0x09);   //Address (Fast Readout Control)
        nexys->AddByte(0x00);   //Length High Byte
        nexys->AddByte(0x01);   //Length Low Byte
        nexys->AddByte(0x01);   //Data: Enable Fast Readout

        nexys->flush();

        logit("Fast Readout Enabled");
    }
    //Turn off Fast Readout Clock:
    else
    {
        ui->FastReadoutEnable->setText("Start Fast Readout");

        nexys->AddByte(0x01);   //Write Header
        nexys->AddByte(0x09);   //Address (Fast Readout Control)
        nexys->AddByte(0x00);   //Length High Byte
        nexys->AddByte(0x01);   //Length Low Byte
        nexys->AddByte(0x00);   //Data: Disable Fast Readout

        nexys->flush();

        logit("Fast Readout Disabled");
    }
}

void MainWindow::on_FastReadoutReset_clicked()
{
    unsigned char value = 0;    //value to write to the FPGA

    //leave the enable bit turned on (=> 1):
    if(ui->FastReadoutEnable->text() == "Stop Fast Readout")
        value += 1;
    //reset the Fast Readout FIFO (=> 2):
    if(ui->FastReadoutFIFO->checkState() == 2)
        value += 2;
    //reset the Fast Readout state / counter machine (=> 4):
    if(ui->FastReadoutStateMachine->checkState() == 2)
        value += 4;

    if(value < 2)
    {
        logit("Nothing to reset selected");
        return;
    }

    //neyxs command header, address, length (2x):
    nexys->AddByte(0x01);
    nexys->AddByte(0x09);
    nexys->AddByte(0x00);
    nexys->AddByte(0x05);



    for(int i=0;i<4;++i)
        nexys->AddByte(value);

    //only leave the enable bit turned on if it was before:
    nexys->AddByte(value & 0x01);

    nexys->flush();

    //write out what has been done:
    /*switch(value)
    {
    case(2):
    case(3):
        logit("Fast Readout FIFO has been reset");
        break;
    case(4):
    case(5):
        logit("Fast Readout State / Counter Machine has been reset");
        break;
    case(6):
    case(7):
        logit("Fast Readout has been completely reset");
        break;
    default:
        logit("Nothing to reset selected");
        break;
    }*/
}


void MainWindow::on_IPEGeburtstagsButton_clicked()
{
    GeburtstagUI->show();
}


void MainWindow::on_SCurveV2_Button_clicked()
{
    quiet = 0;
    logit("Starting single S-curve scan");
    ui->IsUnlocked->setChecked(1);
    QVector<std::pair<unsigned int, unsigned int> >  activepixels;


    activepixels.push_back(std::make_pair<unsigned int, unsigned int>((unsigned) ui->H35Col->value(), (unsigned) ui->H35Row->value()));

    //generate output file
    std::string filename;
    std::ofstream outfile ;
    filename = "SCurve(" + QString::number(ui->H35Col->value()).toStdString() + " " + QString::number(ui->H35Row->value()).toStdString() + ") RamPix" + QString::number(ui->TrimRamPix->value()).toStdString() + ".dat";
    outfile.open(((filename).c_str()));
    outfile << "";
    double startval = ui->SCurveStart->text().toDouble();
    double endval = ui->SCurveEnd->text().toDouble()-ui->SCurveStep->text().toDouble();
    double step = ui->SCurveStep->text().toDouble();
    if (endval < 0)
        endval = 0;
    if (startval<endval)
        startval = endval;
    if (step <= 0)
        step = 0.1;

    for (double inj = startval; inj >= endval; inj = inj-step)
    {        
        QVector<std::pair<unsigned int, double> >  result = get_efficiencyV2(activepixels, inj, ui->PrecisionSpinBox->value(), 1);
        bool failed =1;
        while ((result.length() != 0) &&(failed ==1))
        {
            if ((result.back().first/16 == ui->H35Col->value())&&(15- (result.back().first % 16) == ui->H35Row->value()))
            {
                failed = 0;
            }
            else
                result.pop_back();
        }
        if (failed)
            outfile << std::to_string(inj) << " 0" << std::endl;
        else
            outfile << std::to_string(inj) << " " << std::to_string(result.back().second) << std::endl;
        /*if (result.length() != 0)
        {
            std::cout << 15- (result.back().first % 16) << " " << result.back().first/16 << std::endl;
            outfile << std::to_string(inj) << " " << std::to_string(result.back().second) << std::endl;
        }
        else
            outfile << std::to_string(inj) << " 0" << std::endl;*/
    }
    outfile.close();
    std::cout << "Single S-curve scan completed" << std::endl;
    quiet = 0;
}


void MainWindow::on_SCurveallV2_Button_clicked()
{
    RowInjection = 1;
    logit("Starting all S-curve scan");
    //progress Bar
    int PBmax = 16;
    int PBnow = 0;
    set_ProgressBar(PBmax, PBnow, 1);

    QVector<std::pair<unsigned int, unsigned int> >  activepixels;
    QVector<std::pair<unsigned int, double> >  result;
    result.clear();
    int precision = 70;

    for (int row = 0; row < 16; row++)
    {
        if (ui->SCurveAllPixAllRamval_Button->text() == "Aborting all \nSCurve run")
            break;
        ui->H35Row->setValue(row); //just to be sure
        for (int col = 0; col<150; col++)
        {
            InjEnCol[col] = 1;
        }
        InjEnRow[row] = 1;

        bool newpixset = 1;
        for (int col = 0; col <150; col++)
        {
            activepixels.push_back(std::make_pair<unsigned int, unsigned int>(col, row));
        }

        //generate output file
        std::string filename;
        std::ofstream outfile[150] ;
        for (int col = 0; col<150; col++)
        {
            ui->H35Col_Meas->setValue(col);
            filename = "SCurve(" + QString::number(col).toStdString() + " " + QString::number(row).toStdString() + ") RamPix" + QString::number(ui->TrimRamPix->value()).toStdString() + ".dat";
            outfile[col].open(((filename).c_str()));
            outfile[col] << "";
        }

        //get SCurves
        for (double inj = ui->SCurveStart->text().toDouble(); inj >= ui->SCurveEnd->text().toDouble(); inj = inj-ui->SCurveStep->text().toDouble())
        {
            result = get_efficiencyV2( activepixels, inj, precision, 1);
            newpixset = 0;
            //std::cout << "Current Row: " << row << ", Current Injection: "<< inj << ", Current PTrim: " << trimval << std::endl;
            if (result.length() !=0 )
            {
                for (auto it: result)
                {
                    if (!quiet)
                        std::cout << "pix " << it.first << ", effi " << it.second << std::endl;
                    if ((15-(((int)it.first) % 16) == row) && (it.first <2400))
                        outfile[((int)it.first)/16] << std::to_string(inj) << " " << std::to_string(it.second) << std::endl;
                }
            }
            result.clear();
        }

        //close files
        for (int col = 0; col<150; col++)
        {
            outfile[col] << std::to_string(0) << " " << std::to_string(0) << std::endl;
            outfile[col].close();
        }
        //std::cout << "finished" << std::endl;

        for (int col = 0; col<150; col++)
        {
            InjEnCol[col] = 0;
        }
        InjEnRow[row] = 0;
        activepixels.clear();
        PBnow++;
        set_ProgressBar(PBmax, PBnow, 1);
    }
    set_ProgressBar(PBnow, PBnow, 0);
    ui->IsUnlocked->setChecked(0);
    on_Update_clicked();
    logit("All SCurve-Scan completed");
    RowInjection = 0;
}


void MainWindow::on_H35Col_Meas_valueChanged(int arg1)
{
    ui->H35Col->setValue(arg1);
    ui->TrimRamPix->setValue(2*RamPix1[ui->H35Col->value()][ui->H35Row->value()]+RamPix0[ui->H35Col->value()][ui->H35Row->value()]);
}

void MainWindow::on_H35Row_Meas_valueChanged(int arg1)
{
    ui->H35Row->setValue(arg1);
    ui->TrimRamPix->setValue(2*RamPix1[ui->H35Col->value()][ui->H35Row->value()]+RamPix0[ui->H35Col->value()][ui->H35Row->value()]);
}

void MainWindow::on_H35Col_valueChanged(int arg1)
{
    ui->H35Col_Meas->setValue(arg1);
    ui->TrimRamPix->setValue(2*RamPix1[ui->H35Col->value()][ui->H35Row->value()]+RamPix0[ui->H35Col->value()][ui->H35Row->value()]);
}

void MainWindow::on_H35Row_valueChanged(int arg1)
{
    ui->H35Row_Meas->setValue(arg1);
    ui->TrimRamPix->setValue(2*RamPix1[ui->H35Col->value()][ui->H35Row->value()]+RamPix0[ui->H35Col->value()][ui->H35Row->value()]);
}

void MainWindow::on_TrimRamPix_valueChanged(int arg1)
{
    RamPix0[ui->H35Col->value()][ui->H35Row->value()] = arg1%2;
    RamPix1[ui->H35Col->value()][ui->H35Row->value()] = arg1/2;
    /*for (int i = 0; i < 300; i++)
    {
        for (int j = 0; j < 16; j++)
        {
            RamPix0[i][j] = arg1%2;;
            RamPix1[i][j] = arg1/2;
        }
    }*/
    //std::cout << "rampix" << RamPix0[ui->H35Col->value()][ui->H35Row->value()] << " " << RamPix1[ui->H35Col->value()][ui->H35Row->value()] << std::endl;
}

void MainWindow::set_ProgressBar(int max, int now, bool visible)
{
    ui->progressBar->setMaximum(max);
    ui->progressBar->setValue(now);
    if (ui->progressBar->isVisible() == 1 && !visible)
    {
        player->setMedia(QUrl::fromLocalFile("Alert.wav"));
        player->setVolume(5000);
        player->play();
        this->raise();
    }
    ui->progressBar->setVisible(visible);
}

QVector<float> MainWindow::get_efficiency(double injection, int precision)
{


    QVector<std::pair<unsigned int, unsigned int> > hitpixels;
    // save settings
    int prev_NumInjections = ui->NumInjectionSpinBox->value();
    float prev_injection = ui->DAC4->text().toFloat();


    //set injections and turn them off:
    ui->NumInjectionSpinBox->setValue(precision);
    if(ui->InjectionButton->text() != "Start Injections")
        on_InjectionButton_clicked();

    //set and turn off fast readout:
    if(ui->FastReadoutEnable->text() == "Start Fast Readout")
        on_FastReadoutEnable_clicked();
    ui->FastReadoutFIFO->setChecked(true);
    ui->FastReadoutStateMachine->setChecked(false);


    ui->DAC4->setText(QString::number(injection));
    on_Update_2_clicked();
    QApplication::processEvents();
    Sleep(1);
    on_FastReadoutReset_clicked();
    on_InjectionButton_clicked();
    QApplication::processEvents();
    Sleep(5+((double)ui->ClockDivSpinBox->value()/245.0)); // wait all injections

    //read out the data:
    std::string answer;
    answer.clear();
    int hitcount[150];
    //int failcount=0;
    for (int i = 0; i<150; i++)
    {
        hitcount[i] = 0;
    }

    //number of simultaneously read out pixels
    int SCurvePixels = 1;
    if (RowInjection)
        SCurvePixels = 16;
    if (ColInjection)
        SCurvePixels = 150;


    unsigned int lengthAux = precision*(8+2)*SCurvePixels;
    unsigned int length = lengthAux;
    int counter = (length/1024)+1;
    for (int i = 0; i<counter; i++)// (lengthAux>1024)
    {
        if (length > 1024)
            length = 1024;
        unsigned int lengthH = length / 256;
        unsigned int lengthL = length % 256;
        QApplication::processEvents();
        //Sleep(10);
        nexys->AddByte(0x00);   //read
        nexys->AddByte(0x0B);   //address
        nexys->AddByte(lengthH);   //length high
        nexys->AddByte(lengthL);   //length low
        nexys->flush();
        QApplication::processEvents();
        //Sleep(10);
        answer = nexys->read(length);
        QApplication::processEvents();


        for(int i=0; (unsigned) i<answer.length(); i+=8)
        {
            int pixelindex = answer[i]*40-1 + answer[i+5];
            if(answer[i] != -1 && answer[i+5] != -1)
            {
                hitpixels.push_back(std::make_pair<unsigned int, unsigned int>(pixelindex / 16, 15-(pixelindex%16)));
                //std::cout << "Raw Data: " <<(int) answer.c_str()[i] << " " << (int)answer.c_str()[i+5] << std::endl;
                //std::cout << "Decoded:  " << hitpixels.back().first << " " << hitpixels.back().second << std::endl;
            }
        }
        lengthAux = lengthAux-1024;
        length = lengthAux;

    }

    //Analyze result
    for(auto it: hitpixels)
    {
        if (RowInjection)
            hitcount[it.second]++;
        if (ColInjection)
            hitcount[it.first]++;
        if (!RowInjection && !ColInjection && ui->H35Col->value() == it.first)
            hitcount[it.second]++;
//        if (it.first == ui->H35Col->value())
//            hitcount[it.second]++;
//        else
//            failcount++;
    }

    QVector<float> efficiency;
    if (!ColInjection)
    {
        for (int i = 0; i<16; i++)
        {
            if (RowInjection || i == ui->H35Row->value())
            {
                efficiency.push_back((float)hitcount[i]/(float)precision);
            }
        }
    }
    else
    {
        for (int i = 0; i< 150 ; i++)
        {
            efficiency.push_back((float)hitcount[i]/(float)precision);
        }
    }

    //aufräumen
    hitpixels.clear();
    on_InjectionButton_clicked();
    QApplication::processEvents();

    //restore settings
    ui->NumInjectionSpinBox->setValue(prev_NumInjections);
    ui->DAC4->setText(QString::number(prev_injection));
    return efficiency;
}

QVector<std::pair<unsigned int, double> > MainWindow::get_delay( QVector<std::pair<unsigned int, unsigned int> >  activepixels, double injection, int precision, bool newpixset)
{
    ui->IsUnlocked->setChecked(1);
    //save settings
    QString prev_DAC1 = ui->DAC1->text();
    int prev_NumInjections = ui->NumInjectionSpinBox->value();
    float prev_injection = ui->DAC4->text().toFloat();

    //activate Pixels

    if (newpixset)
    {
        DigSelToMain(activepixels);
        on_Update_clicked();
    }

    QVector<std::pair<unsigned int, double> > hitpixels;    //hit pixels in order of appearance
    QVector<std::pair<unsigned int, double> > delay; //sorted pixel index and corresponding delay
    hitpixels.clear();
    delay.clear();

    //set injections and turn them off:
    ui->NumInjectionSpinBox->setValue(precision);
    if(ui->InjectionButton->text() != "Start Injections")
        on_InjectionButton_clicked();

    //set and turn off fast readout:
    if(ui->FastReadoutEnable->text() == "Start Fast Readout")
        on_FastReadoutEnable_clicked();
    ui->FastReadoutFIFO->setChecked(true);
    ui->FastReadoutStateMachine->setChecked(false);

    ui->DAC4->setText(QString::number(injection));
    on_Update_2_clicked();

    QApplication::processEvents();
    Sleep(1);
    on_FastReadoutReset_clicked();
    on_InjectionButton_clicked();
    QApplication::processEvents();
    Sleep(5+((double)ui->ClockDivSpinBox->value()/245.0)); // wait all injections.



    //read out the data:
    std::string answer;
    answer.clear();

    //number of simultaneously read out pixels
    int SCurvePixels = activepixels.length();
    //std::cout << SCurvePixels << std::endl;

    unsigned int lengthAux = precision*(8+2)*SCurvePixels;
    unsigned int length = lengthAux;
    int counter = (length/1024)+1;
    for (int i = 0; i<counter; i++)// (lengthAux>1024)
    {
        if (length > 1024)
            length = 1024;
        unsigned int lengthH = length / 256;
        unsigned int lengthL = length % 256;
        QApplication::processEvents();
        nexys->AddByte(0x00);   //read
        nexys->AddByte(0x0B);   //address
        nexys->AddByte(lengthH);   //length high
        nexys->AddByte(lengthL);   //length low
        nexys->flush();
        QApplication::processEvents();
        answer = nexys->read(length);
        QApplication::processEvents();

        for(int i=0; (unsigned) i<answer.length(); i+=8)
        {
            int pixelindex;
            bool left;

            if (answer[i+7] > 0 && answer[i+7] <=40)
            {
                pixelindex = answer[i]*40 + answer[i+7]-1+2400;
                left = 0;
            }

            if (answer[i+5] > 0 && answer[i+5] <=40)
            {
                pixelindex = answer[i]*40 + answer[i+5]-1;
                left = 1;
            }

            if(answer[i] != -1 && (answer[i+5] != -1 || answer[i+7] != -1))
            {
                //+48, because the counters in fpga dont start syncronously, due to chip design
                double tempdelay = ((((unsigned short)answer[i+3]%256)*240) + ((unsigned short)answer[i+4]%256) - (((unsigned short)answer[i+1]%256)*256) - ((unsigned short)answer[i+2]%256)); //+48.0
                if(!left)
                    tempdelay = ((((unsigned short)answer[i+3]%256)*240) + ((unsigned short)answer[i+6]%256) - (((unsigned short)answer[i+1]%256)*256) - ((unsigned short)answer[i+2]%256)); //+48.0
                if ((tempdelay < 50) && (tempdelay > -50))
                    hitpixels.push_back(std::make_pair<unsigned int, double>(pixelindex, (double)tempdelay));
                //std::cout << tempdelay << std::endl;
            }
        }
        lengthAux = lengthAux-1024;
        length = lengthAux;
    }

    qSort(hitpixels);
    int prev = -1;
    int divisor = 1;
    if (hitpixels.length() != 0)
    {
        for(auto it: hitpixels)
        {
            //std::cout << it.first << " " << it.second << std::endl;
            if (prev == it.first)
            {
                delay.back().second = (delay.back().second*divisor + it.second)/(divisor+1);
                divisor++;
            }
            else
            {
                prev = it.first;
                divisor = 1;
                delay.push_back(std::make_pair<unsigned int, double>((unsigned int)it.first, (double)it.second));
            }
        }
    }


    //aufräumen
    hitpixels.clear();
    on_InjectionButton_clicked();
    QApplication::processEvents();

    //restore settings
    ui->NumInjectionSpinBox->setValue(prev_NumInjections);
    ui->DAC4->setText(QString::number(prev_injection));
    ui->DAC1->setText(prev_DAC1);

    //result
    return delay;
}


QVector<std::pair<unsigned int, double> > MainWindow::get_efficiencyV2( QVector<std::pair<unsigned int, unsigned int> >  activepixels, double injection, int precision, bool newpixset)
{
    ui->IsUnlocked->setChecked(1);
    //save settings
    QString prev_DAC1 = ui->DAC1->text();
    int prev_NumInjections = ui->NumInjectionSpinBox->value();
    float prev_injection = ui->DAC4->text().toFloat();

    //activate Pixels

    if (newpixset)
    {
        DigSelToMain(activepixels);
        on_Update_clicked();
    }

    QVector<std::pair<unsigned int, unsigned int> > hitpixels;    //hit pixels in order of appearance
    QVector<std::pair<unsigned int, double> > efficiency; //sorted pixel index and corresponding effi
    hitpixels.clear();
    efficiency.clear();

    //set injections and turn them off:
    ui->NumInjectionSpinBox->setValue(precision);
    if(ui->InjectionButton->text() != "Start Injections")
        on_InjectionButton_clicked();

    //set and turn off fast readout:
    if(ui->FastReadoutEnable->text() == "Start Fast Readout")
        on_FastReadoutEnable_clicked();
    ui->FastReadoutFIFO->setChecked(true);
    ui->FastReadoutStateMachine->setChecked(false);

    ui->DAC4->setText(QString::number(injection));
    on_Update_2_clicked();

    QApplication::processEvents();
    Sleep(1);
    on_FastReadoutReset_clicked();
    on_InjectionButton_clicked();
    QApplication::processEvents();
    Sleep(5+((double)ui->ClockDivSpinBox->value()/245.0)); // wait all injections.



    //read out the data:
    std::string answer;
    answer.clear();

    //number of simultaneously read out pixels
    int SCurvePixels = activepixels.length();
    //std::cout << SCurvePixels << std::endl;

    unsigned int lengthAux = precision*(8+2)*SCurvePixels;
    unsigned int length = lengthAux;
    int counter = (length/1024)+1;
    for (int i = 0; i<counter; i++)// (lengthAux>1024)
    {
        if (length > 1024)
            length = 1024;
        unsigned int lengthH = length / 256;
        unsigned int lengthL = length % 256;
        QApplication::processEvents();
        nexys->AddByte(0x00);   //read
        nexys->AddByte(0x0B);   //address
        nexys->AddByte(lengthH);   //length high
        nexys->AddByte(lengthL);   //length low
        nexys->flush();
        QApplication::processEvents();
        answer = nexys->read(length);
        QApplication::processEvents();

        for(int i=0; (unsigned) i<answer.length(); i+=8)
        {
            //39-(pixelindex % 40

            int pixelindex;

            if (answer[i+7] > 0 && answer[i+7] <=40)
                pixelindex = answer[i]*40 + answer[i+7]-1+2400;

            if (answer[i+5] > 0 && answer[i+5] <=40)
                pixelindex = answer[i]*40 + answer[i+5]-1;

            if(answer[i] != -1 && (answer[i+5] != -1 || answer[i+7] != -1))
            {
                hitpixels.push_back(std::make_pair<unsigned int, unsigned int>(pixelindex, 0));
            }
        }
        lengthAux = lengthAux-1024;
        length = lengthAux;
    }


    if (hitpixels.length() != 0)
    {
        qSort(hitpixels);
        int prev = -1;
        for(auto it: hitpixels)
        {

            if (prev == it.first)
                efficiency.back().second++;
            else
            {
                prev = it.first;
                efficiency.push_back(std::make_pair<unsigned int, double>((unsigned int)it.first, (double)1)); //std::make_pair<unsigned int, double>(it.first, 1));
            //std::make_pair<unsigned int, unsigned int>((unsigned) ui->H35Col->value(), (unsigned) ui->H35Row->value())
            }
        }


        for (QVector<std::pair<unsigned int, double> >::iterator it = efficiency.begin(); it!=efficiency.end(); it++)
        {
            //std::cout << it->second << std::endl;
            it->second = it->second/precision;
            //std::cout << it->second << std::endl;
        }
    }


    //aufräumen
    hitpixels.clear();
    on_InjectionButton_clicked();
    QApplication::processEvents();

    //restore settings
    ui->NumInjectionSpinBox->setValue(prev_NumInjections);
    ui->DAC4->setText(QString::number(prev_injection));
    ui->DAC1->setText(prev_DAC1);

    //result
    return efficiency;
    efficiency.clear();
}

double MainWindow::ThresholdScanV2(double tolerance = 0.0)
{
    //save settings
    QString prev_DAC1 = ui->DAC1->text();
    QVector<std::pair<unsigned int, unsigned int> >  activepixels;
    QVector<std::pair<unsigned int, double> > result;
    activepixels.push_back(std::make_pair<unsigned int, unsigned int>((unsigned) ui->H35Col->value(), (unsigned) ui->H35Row->value()));

    double nextTh = ui->ThresholdStart->text().toDouble();
    double step = 0.5;
    ui->DAC1->setText("0");
    bestTh = 1.0;
    bestEffi = 1.0;
    double effi;
    bool newset = 1;
    while (step > 0.00002)
    {
        ui->DAC1->setText(QString::number(nextTh));
        result.clear();
        result = get_efficiencyV2(activepixels,ui->ThresholdInj->text().toDouble(), ui->ThresholdPrecision->text().toInt(),newset);
        newset = 0;
        if (result.length() == 0)
            effi = 0;
        else
            effi = result.back().second;
        //std::cout << "Effi: "  << effi << " at nTh = " << nextTh << std::endl;
        //std::cout << effi << " " << nextTh << std::endl;
        if (std::abs(effi-0.5) < std::abs(bestEffi-0.5))
        {
            bestTh = nextTh;
            bestEffi = effi;
        }

        //next iteration
        step = step/2;
        if (effi<0.5-tolerance)
            nextTh = nextTh-step;
        else
            if (effi>0.5+tolerance)
                nextTh = nextTh+step;
        else
                step=0;
    }
    //std::cout << "Best Effi: " << bestEffi << "; Best Th: " << bestTh << std::endl;
    logit("Best 50% efficiency found for Pixel ("+ ui->H35Col->text().toStdString() + "|" + ui->H35Row->text().toStdString() + ") at threshold: " + std::to_string(bestTh) + " with " + std::to_string(bestEffi) + " efficiency" ) ;


    //restore settings
    ui->DAC1->setText(prev_DAC1);
    return bestTh;
}



void MainWindow::on_Thresholdall_Button_clicked()
{
    //progress Bar
    int PBmax = 150*16;
    int PBnow = 0;
    set_ProgressBar(PBmax, PBnow, 1);
    //generate output file
    std::string filename;
    std::ofstream outfile ;
    filename = "ThPix_Inj" + ui->ThresholdInj->text().toStdString() + ".dat";
    outfile.open(((filename).c_str()));
    outfile << "";
    for (int col = 0; col < 150; col++)
    {
        if (!(((col == 15) || (col == 16) || (col == 17) || (col == 18) || (col == 19)) && ui->MaskPixels->isChecked()))
        {
            ui->H35Col->setValue(col);
            for (int row = 0; row < 16; row++)
            {
                if(!((row == 1) && ui->MaskPixels->isChecked()))
                {
                    ui->H35Row->setValue(row);
                    ThresholdScanV2();
                    outfile << ui->H35Col->text().toStdString() << " " << ui->H35Row->text().toStdString() << " " << bestTh << " " << bestEffi << std::endl;
                }
                PBnow++;
                set_ProgressBar(PBmax, PBnow,1);
            }
        }
        else
        {
            PBnow = PBnow + 16;
            set_ProgressBar(PBmax, PBnow,1);
        }
    }
    outfile.close();
    ui->IsUnlocked->setChecked(0);
    on_Update_clicked();
    set_ProgressBar(PBmax, PBnow,0);
}

void MainWindow::setRam(double Thmax, double Thmin)
{
    //Find PTrim
    int PTrim, RamPix, bestTrim, col, row;
    double Th, DeltaTh, Th0, bestTh=5, effi;
    std::fstream g;
    std::string filename = "TrimScan.dat";
    g.open(filename.c_str());
    while(!g.eof())
    {
        g >> PTrim >> RamPix >> Th;
        if (RamPix == 0)
            Th0 = Th;
        if (RamPix == 3)
        {
            DeltaTh = Th0-Th;
            if (std::fabs(bestTh-(Thmax-Thmin)) > std::fabs(DeltaTh-(Thmax-Thmin)))
            {
                bestTh = DeltaTh;
                bestTrim = PTrim;
            }
            //std::cout << "best PTrim: " << PTrim << " " << bestTh << std::endl;
        }
    }
    g.close();
    ui->spinBox_PTrim->setValue(bestTrim);
    std::cout << "best PTrim: " << bestTrim << std::endl;



    filename = "ThPix_Inj0.6.dat";
    g.open(filename.c_str());
    while(!g.eof())
    {
        g >> col >> row >> Th >> effi;
        if (Th < (Thmin+(bestTh/4)))
        {
            RamPix0[col][row] = 0;
            RamPix1[col][row] = 0;
            std::cout << "Ram of pixel (" << col << "|" << row << ") set to 0" << std::endl;
        }
        else
        {
            if (Th < (Thmin+(bestTh/2)))
            {
                RamPix0[col][row] = 1;
                RamPix1[col][row] = 0;
                std::cout << "Ram of pixel (" << col << "|" << row << ") set to 1" << std::endl;
            }
            else
            {
                if (Th < (Thmin+bestTh/4*3))
                {
                    RamPix0[col][row] = 0;
                    RamPix1[col][row] = 1;
                    std::cout << "Ram of pixel (" << col << "|" << row << ") set to 2" << std::endl;
                }
                else
                {
                    RamPix0[col][row] = 1;
                    RamPix1[col][row] = 1;
                    std::cout << "Ram of pixel (" << col << "|" << row << ") set to 3" << std::endl;
                }
            }
        }
    }
    g.close();
}

void MainWindow::on_Write_RamPix_Button_clicked()
{
    int PBmax = 2400;
    int PBnow = 0;
    set_ProgressBar(PBmax, PBnow, 1);
    for (int col = 0; col < 150; col++)
    {
        if (!(((col == 15) || (col == 16) || (col == 17) || (col == 18) || (col == 19)) && ui->MaskPixels->isChecked()))
        {
            ui->H35Col->setValue(col);
            for (int row = 0; row <16; row++)
            {
                if (!((row == 1) && ui->MaskPixels->isChecked()))
                {
                    ui->H35Row->setValue(row);
                    findRam(0.01);
                }
                PBnow++;
                set_ProgressBar(PBmax, PBnow, 1);
            }
        }
        else
        {
            PBnow = PBnow + 16;
            set_ProgressBar(PBmax, PBnow, 1);
        }
    }
    set_ProgressBar(PBmax, PBnow, 0);
    ui->IsUnlocked->setChecked(false);
    on_Update_clicked();
    on_saveRamPix_Button_clicked();
}

void MainWindow::on_Clear_RamPix_Button_clicked()
{
    setallRamPix(ui->TrimRamPix_2->value());
}

void MainWindow::setallRamPix(int value)
{
    for (int i = 0; i < 150; i++)
    {
        for (int j = 0; j < 16; j++)
        {
            RamPix0[i][j] = value%2;
            RamPix1[i][j] = value/2;
        }
    }
    ui->TrimRamPix->setValue(value);
}

void MainWindow::findRam(double tolerance = 0)
{
    int bestRamVal = 4;
    double bestThVal = 3.0, currentTh;
    for (int i = 0; i<4; i++)
    {
        ui->TrimRamPix->setValue(i);
        currentTh = ThresholdScanV2(tolerance);
        if (std::fabs(currentTh - ui->ThTarget->text().toDouble()) < std::fabs(bestThVal - ui->ThTarget->text().toDouble()))
        {
            bestRamVal = i;
            bestThVal = currentTh;
        }
    }
    std::cout << "Best RamPix value for Pixel (" << ui->H35Col->value() << "|" << ui->H35Row->value() << ") is " << bestRamVal << " with Th 50% = " << bestThVal << std::endl;
    ui->TrimRamPix->setValue(bestRamVal);
}

void MainWindow::on_TrimScan_clicked()
{
    int PBmax = 256;
    int PBnow = 0;
    set_ProgressBar(PBmax,PBnow, 1);
    std::ofstream outfile;
    QString filename = "PTrim_RamPix_Scan_(" + ui->H35Col->text() + " " + ui->H35Row->text() + ").dat";
    outfile.open(filename.toStdString().c_str());
    for (int j = 0; j < 4; j++)
    {
        setallRamPix(j);
        for (int i = 0; i <64; i=i+1)
        {
            set_ProgressBar(PBmax,PBnow, 1);
            ui->spinBox_PTrim->setValue(i);
            outfile << i << " " << j << " " << ThresholdScanV2() << std::endl;
            PBnow++;
        }
    }
    set_ProgressBar(PBmax,PBmax, 1);
    set_ProgressBar(PBmax,PBmax, 0);
    outfile.close();
}

void MainWindow::on_spinBox_PTrim_2_valueChanged(int arg1)
{
    ui->spinBox_PTrim->setValue(arg1);
}

void MainWindow::on_saveRamPix_Button_clicked()
{
    std::ofstream outfile;
    outfile.open("RamPix.dat");
    for (int i = 0; i <150; i++)
    {
        for (int j = 0; j < 16; j++)
        {
            outfile << i << " " << j << " " << (2*RamPix1[i][j] + RamPix0[i][j]) << std::endl;
        }
    }
    outfile.close();
    logit("RamPix values saved");
}



void MainWindow::on_ThButton_clicked()
{
    ThresholdScanV2();
}

void MainWindow::on_SelectRam_Button_clicked()
{
    int found0 = 0;
    int found1 = 0;
    int found2 = 0;
    int found3 = 0;
    int notfound = 0;
    int PBmax = 2400;
    int PBnow = 0;

    int risetime;

    set_ProgressBar(PBmax, PBnow, 1);
    std::string ramfilename;
    std::ofstream ramfile[5] ;
    for (int ramval = 0; ramval<5; ramval++)
    {
        if (ramval == 4)
        {
            ramfilename = "05Points_SCurve_bestRamPix.dat";
        }
        else
        {
        ramfilename = "05Points_SCurve_RamPix" + QString::number(ramval).toStdString() + ".dat";
        }
        ramfile[ramval].open(((ramfilename).c_str()));
        ramfile[ramval] << "";
    }



    double InjTarget = ui->InjTarget->text().toDouble();
    std::ofstream outfile;
    outfile.open("RamPix.dat");
    for (int col = 0; col < 150; col++)
    {
        for (int row = 0; row < 16; row++)
        {
            PBnow++;
            set_ProgressBar(PBmax, PBnow, 1);
            int BestRam = 4;
            double BestInj = 10.0;
            for (int RamVal = 0; RamVal <4; RamVal++)
            {
                QVector< std::pair<double, double> > points;
                std::pair<double, double> regline;

                std::fstream f;
                std::string filename = "SCurve(" + QString::number(col).toStdString() + " " + QString::number(row).toStdString() + ") RamPix" + QString::number(RamVal).toStdString() + ".dat";
                f.open(filename.c_str(), std::ios::in);
                if (f.good())
                {
                    double max = 0;
                    double min = 2;
                    double x, y;
                    int n = 0;
                    double injmin = 5.0;
                    double injmax = 0.0;
                    while(!f.eof())
                    {
                        f >> x >> y;

                        n++;
                        if (x > injmax)
                            injmax = x;
                        if ((x < injmin) && (x !=0))
                            injmin = x;

                        if(f.eof())
                            break;
                        if ((y < 0.9) && (y > 0.1))
                        {
                            points.push_back(std::pair<double, double>(x,y));
                            if (y > max)
                                max = y;
                            if (y < min)
                                min = y;
                        }
                    }
                    risetime = 0.3/((injmax-injmin)/n);
                    //std::cout << risetime << " = 0.15/((" << injmax << "-" << injmin << " )/ " << n << ")" << std::endl;
                    f.close();
                    if ((points.length() <= risetime) && (max>0.6) && (min<0.4))
                    {
                        regline = LinReg(points);
                        double fifty = (0.5 - regline.second)/regline.first;
                        //std::cout << "a = " << regline.first << " b = " << regline.second << std::endl;
                        ramfile[RamVal] << col << " " << row << " " << fifty << std::endl;

                        if (std::fabs(InjTarget-fifty) < std::fabs(InjTarget-BestInj))
                        {
                            BestRam = RamVal;
                            BestInj = fifty;
                        }
                    }
                    //std::cout << "max " << max << " min " << min << std::endl;
                }
            }
            outfile << col << " " << row << " " << BestRam << std::endl;
            ramfile[4] << col << " " << row << " " << BestInj << " " << BestRam << std::endl;

            if (BestRam == 4)
                notfound++;
            if (BestRam == 0)
                found0++;
            if (BestRam == 1)
                found1++;
            if (BestRam == 2)
                found2++;
            if (BestRam == 3)
                found3++;
        }
    }
    outfile.close();

    for (int ramval = 0; ramval<5; ramval++)
    {
        ramfile[ramval].close();
    }
    logit(("50% points NOT found for " + QString::number(notfound) + " SCurves").toStdString() );
    logit(("RamPix 0 used " + QString::number(found0) + " times").toStdString());
    logit(("RamPix 1 used " + QString::number(found1) + " times").toStdString());
    logit(("RamPix 2 used " + QString::number(found2) + " times").toStdString());
    logit(("RamPix 3 used " + QString::number(found3) + " times").toStdString());
    //std::cout << "max " << max << " min " << min << std::endl;

    set_ProgressBar(PBmax, PBmax, 0);

}

void MainWindow::on_SCurveAllPixAllRamval_Button_clicked()
{
    if (ui->SCurveAllPixAllRamval_Button->text() == "SCurves for all \npixels and RamVals")
        ui->SCurveAllPixAllRamval_Button->setText("SCurves for all pixels\nand RamVals running");
    else
        ui->SCurveAllPixAllRamval_Button->setText("Aborting all \nSCurve run");
    for (int i = 0; i<4; i++)
    {
        if (ui->SCurveAllPixAllRamval_Button->text() == "Aborting all \nSCurve run")
            break;
        setallRamPix(i);
        on_SCurveallV2_Button_clicked();
    }
    Sleep(100);
    ui->SCurveAllPixAllRamval_Button->setText("SCurves for all \npixels and RamVals");
}

std::pair<double, double> MainWindow::LinReg(QVector< std::pair<double, double> > points)
{
    double avgx = 0;
    double avgy = 0;
    double suma = 0;
    double sumb = 0;
    for (auto it: points)
    {
        avgx += it.first;
        avgy += it.second;
    }
    avgx = avgx/points.length();
    avgy = avgy/points.length();
    for (auto it: points)
    {
        suma += (it.first - avgx)*(it.second - avgy);
        sumb += (it.first - avgx)*(it.first - avgx);
    }

    std::pair <double, double> result;
    result.first = suma/sumb;
    result.second = avgy - (suma/sumb*avgx);
    return result;
}


void MainWindow::on_loadRamPix_Button_clicked()
{
    int PBmax = 2400;
    int PBnow = 0;
    set_ProgressBar(PBmax, PBnow, 1);
    std::fstream input;
    input.open("RamPix.dat",std::ios::in);
    int col, row, ramval;

    for (int i = 0; i<150;i++)
        for (int j =0; j<16;j++)
            MaskedPixels_NL[i][j] = ui->MaskPixels->isChecked();

    //load and write rampix values from file
    while(!input.eof())
    {
        input >> col >> row >> ramval;
        if (!((ramval == 0) || (ramval == 1) || (ramval == 2) || (ramval == 3)))
            ramval = 3;
        else
            MaskedPixels_NL[col][row] = 0;
        ui->H35Col_Meas->setValue(col);
        ui->H35Row_Meas->setValue(row);
        ui->TrimRamPix->setValue(ramval);
        PBnow++;
        set_ProgressBar(PBmax, PBnow, 1);
    }

    //masking pixels with invalid ramval
    QVector<std::pair<unsigned int, unsigned int> >  activepixels;
    activepixels.clear();

    for (int i = 0; i<150;i++)
    {
        for (int j =0; j<16;j++)
        {
            if (!MaskedPixels_NL[i][j])
            {
                activepixels.push_back(std::make_pair<unsigned int, unsigned int>(i,j));
            }
        }
    }
    DigSelToMain(activepixels);


    set_ProgressBar(PBmax, PBmax, 1);
    set_ProgressBar(PBmax, PBnow, 0);
    logit("Values for RamPix loaded from file \'RamPix.dat\' ");
}



void MainWindow::on_SaveTrim_Button_clicked()
{
    int PBmax = 2400;
    int PBnow = 0;
    set_ProgressBar(PBmax, PBnow, 1);
    QDir dir;
    QString folder = "tuned_" + ui->InjTarget->text() + "Inj";
    dir.remove(folder);
    dir.mkdir(folder);
    QFile file;
    file.copy("DrawSCurves.cpp", folder + "/DrawSCurves.cpp");
    std::fstream f;
    std::string filename = "05Points_SCurve_bestRamPix.dat";
    f.open(filename.c_str(), std::ios::in);
    if (f.good())
    {
        int col, row, ramval;
        double effi;
        while(!f.eof())
        {
            PBnow++;
            set_ProgressBar(PBmax, PBnow, 1);
            f >> col >> row >> effi >> ramval;
            if (ramval != 4)
            {
                QString currentfile;
                currentfile = "SCurve(" + QString::number(col) + " " + QString::number(row) + ") RamPix" + QString::number(ramval) + ".dat";
                file.copy(currentfile, folder + "/"+currentfile);
            }
        }
    }

    set_ProgressBar(PBmax, PBmax, 1);
    set_ProgressBar(PBmax, PBmax, 0);
    logit(("Best SCurves in " + folder + " stored.").toStdString());
}


unsigned short MainWindow::gray_to_bin(unsigned short gray = 0b11101010)
{
    unsigned short bin = 0;
    for (int i = 0; i<8; i++)
    {
       bin = (bin * 2) + (((gray & (128>>i)) >> (7-i)) ^ (bin & 1));
    }

    return bin;
}

void MainWindow::on_DelayCurve_button_clicked()
{
    QString prevInj = ui->DAC4->text();
    double DelayStart = ui->DelayStart->text().toDouble();
    double DelayEnd = ui->DelayEnd->text().toDouble();
    double DelayStep = ui->DelayStep->text().toDouble();
    //if (DelayStart > DelayEnd)

    int PBmax = (int)((DelayStart - DelayEnd)/DelayStep);
    //std::cout << PBmax << std::endl;
    int PBnow = 0;
    std::string filename;
    std::ofstream outfile;
    filename = "DelayOverInjection.dat";
    outfile.open((filename).c_str());
    for (double i = DelayStart; i> DelayEnd; i=i-DelayStep)
    {
        set_ProgressBar(PBmax, PBnow, 1);
        ui->DAC4->setText(QString::number(i));
        QVector<std::pair<unsigned int, unsigned int> >  activepixels;
        QVector<std::pair<unsigned int, double> >  result;
        result.clear();
        activepixels.push_back(std::make_pair<unsigned int, unsigned int>(ui->H35Col->value(), ui->H35Row->value()));
        result = get_delay(activepixels, ui->DAC4->text().toDouble(), 100, 1);
        if (result.length() != 0) // convert to logit
        {
            std::cout << result.back().first << " " << result.back().second << std::endl;
            outfile << i << " " << result.back().second << std::endl;
        }
        PBnow++;
    }
    set_ProgressBar(PBmax, PBmax, 1);
    set_ProgressBar(PBmax, PBmax, 0);
    outfile.close();
    ui->DAC4->setText(prevInj);
}




/*double MainWindow::ThresholdScan(double tolerance = 0.0)
{
    ui->IsUnlocked->setChecked(1);
    //save settings
    QString prev_DAC1 = ui->DAC1->text();

    QVector<std::pair<unsigned int, unsigned int> >  activepixels;
    activepixels.push_back(std::make_pair<unsigned int, unsigned int>((unsigned) ui->H35Col->value(), (unsigned) ui->H35Row->value()));
    DigSelToMain(activepixels);
    on_Update_clicked();

    double nextTh = ui->ThresholdStart->text().toDouble();
    double step = 0.5;
    ui->DAC1->setText("0");
    bestTh = 1.0;
    bestEffi = 1.0;

    while (step > 0.00002)
    {
        ui->DAC1->setText(QString::number(nextTh));
        QVector<float> efficiency = get_efficiency(ui->ThresholdInj->text().toDouble(), ui->ThresholdPrecision->text().toInt());
        std::cout << "Effi: "  << efficiency[0] << " at nTh = " << nextTh << std::endl;
        std::cout << efficiency[0] << " " << nextTh << std::endl;
        if (std::abs(efficiency[0]-0.5) < std::abs(bestEffi-0.5))
        {
            bestTh = nextTh;
            bestEffi = efficiency[0];
        }

        //next iteration
        step = step/2;
        if (efficiency[0]<0.5-tolerance)
            nextTh = nextTh-step;
        else
            if (efficiency[0]>0.5+tolerance)
                nextTh = nextTh+step;
        else
                step=0;
    }
    std::cout << "Best Effi: " << bestEffi << "; Best Th: " << bestTh << std::endl;
    logit("("+ ui->H35Col->text().toStdString() + "|" + ui->H35Row->text().toStdString() + ") threshold: " + std::to_string(bestTh) + " with " + std::to_string(bestEffi) + " efficiency" ) ;





    //restore settings
    ui->DAC1->setText(prev_DAC1);
    return bestTh;
}*/

void MainWindow::on_Continue_Readout_clicked()
{
    ui->ReadoutRunning->setChecked(1);
    std::ofstream outfile;
    std::string filename = ui->ReadoutFile->text().toStdString() + ".dat";
    QFileInfo check_file(QString::fromStdString(filename));
    if (!check_file.exists())
    {
        outfile.open(filename.c_str());
        outfile << "ROBind" << std::setw(9) << "TrigTS1"  << std::setw(9) << "TrigTS2"  << std::setw(9) << "ExtTime "  << std::setw(9) << "TSL"  << std::setw(9) << "ADDRL"  << std::setw(9) << "TSR"  << std::setw(9) << "ADDRR" << std::setw(9) << "TrigTS"  << std::setw(9) << "EventTS"  << std::setw(9) << "Delay" << std::endl;
        logit("Readout started");
        std::stringstream templine;
        if (ui->liveDisplay->isChecked())
            DisplayReadoutUI->show();
        while (ui->ReadoutRunning->isChecked())
        {
            templine.str("");
            bool valid=0;
            int TriggerTS=100000;
            int EventTS=100000;
            unsigned int length = ui->NumReadout->value()*8;
            unsigned int lengthH = length / 256;
            unsigned int lengthL = length % 256;
            nexys->AddByte(0x00);   //read
            nexys->AddByte(0x0B);   //address
            nexys->AddByte(lengthH);   //length high
            nexys->AddByte(lengthL);   //length low
            nexys->flush();
            Sleep(10);
            std::string answer = nexys->read(length);
            //logit("Read Length: " + answer.length());
            //std::cout << "\"" << answer << "\"" << std::endl;
            if (answer.length() != 0)
            {
            for(int i=0; (unsigned) i<answer.length();  ++i)
            {

                //Readout Block
                if(i%8 == 0)
                {
                    if ((unsigned short)answer.c_str()[i] > 60)
                    {
                        valid = 0;
                        templine << "-" << " ";
                    }
                    else
                    {
                        templine << (unsigned short)answer.c_str()[i] << " ";
                        valid = 1;
                    }
                }

                //Trigger Timestamp (2 byte)
                if(i%8 == 1)
                {
                    if ((unsigned short)answer.c_str()[i-1] > 60)
                        templine << "-" << " ";
                    else
                    {
                        if ((unsigned short)answer.c_str()[i] > 255)
                        {
                            templine << (unsigned short)answer.c_str()[i]-65280 << " ";
                            TriggerTS = 256*((unsigned short)answer.c_str()[i]-65280);
                        }
                        else
                        {
                            templine << (unsigned short)answer.c_str()[i] << " ";
                            TriggerTS = 256*((unsigned short)answer.c_str()[i]);
                        }
                    }
                }
                if(i%8 == 2)
                {
                    if ((unsigned short)answer.c_str()[i-2] > 60)
                        templine << "-" << " ";
                    else
                    {
                        if ((unsigned short)answer.c_str()[i] > 255)
                        {
                            templine << (unsigned short)answer.c_str()[i]-65280 << " ";
                            TriggerTS = TriggerTS + (unsigned short)answer.c_str()[i]-65280;
                        }
                        else
                        {
                            templine << (unsigned short)answer.c_str()[i] << " ";
                            TriggerTS = TriggerTS + (unsigned short)answer.c_str()[i];
                        }
                    }
                }

                //Extended Timestamp (generated in FPGA)
                if(i%8 == 3)
                {
                    if ((unsigned short)answer.c_str()[i-3] > 60)
                        templine << "-" << " ";
                    else
                    {
                        if ((unsigned short)answer.c_str()[i] > 255)
                        {
                            templine << (unsigned short)answer.c_str()[i]-65280 << " ";
                            EventTS = 240*((unsigned short)answer.c_str()[i]-65280);
                        }
                        else
                        {
                            templine << (unsigned short)answer.c_str()[i] << " ";
                            EventTS = 240*((unsigned short)answer.c_str()[i]);
                        }
                    }
                }

                //Timestamp left
                if(i%8 == 4)
                {
                    if ((unsigned short)answer.c_str()[i-4] > 60)
                        templine << "-" << " ";
                    else
                    {
                        if ((unsigned short)answer.c_str()[i+1] > 0)
                        {
                            if ((unsigned short)answer.c_str()[i] > 255)
                            {
                                templine << (unsigned short)answer.c_str()[i]-65280 << " ";
                                EventTS = EventTS + (unsigned short)answer.c_str()[i]-65280;
                            }
                            else
                            {
                                templine << (unsigned short)answer.c_str()[i] << " ";
                                EventTS = EventTS + (unsigned short)answer.c_str()[i];
                            }
                        }
                        else
                           templine << "-" << " ";
                    }
                }

                //Address left
                if(i%8 == 5)
                {
                    if ((unsigned short)answer.c_str()[i-5] > 60)
                        templine << "-" << " ";
                    else
                    {
                        if ((unsigned short)answer.c_str()[i] > 0)
                        {
                            if ((unsigned short)answer.c_str()[i] > 255)
                                templine << (unsigned short)answer.c_str()[i]-65280 << " ";
                            else
                                templine << (unsigned short)answer.c_str()[i] << " ";
                        }
                        else
                            templine << "-" << " ";
                    }
                }

                //Timestamp right
                if(i%8 == 6)
                {
                    if ((unsigned short)answer.c_str()[i-6] > 60)
                        templine << "-" << " ";
                    else
                    {
                        if ((unsigned short)answer.c_str()[i+1] > 0)
                        {
                            if ((unsigned short)answer.c_str()[i] > 255)
                            {
                                templine << (unsigned short)answer.c_str()[i]-65280 << " ";
                                EventTS = EventTS + (unsigned short)answer.c_str()[i]-65280;
                            }
                            else
                            {
                                templine << (unsigned short)answer.c_str()[i] << " ";
                                EventTS = EventTS + (unsigned short)answer.c_str()[i];
                            }
                        }
                        else
                            templine << "-" << " ";
                    }
                }

                //Address right
                if(i%8 == 7)
                {
                    if ((unsigned short)answer.c_str()[i-7] > 60)
                        templine << "-" << " ";
                    else
                    {
                        if ((unsigned short)answer.c_str()[i] > 0)
                        {
                            if ((unsigned short)answer.c_str()[i] > 255)
                                templine << (unsigned short)answer.c_str()[i]-65280 << " ";
                            else
                                templine << (unsigned short)answer.c_str()[i] << " ";
                        }
                        else
                            templine << "-" << " ";
                    }

                    //Combined Timestamps and Delay
                    templine << TriggerTS << " " << EventTS << " " << TriggerTS-EventTS << " " << std::endl;
                    if (valid)
                    {
                        valid = 0;
                        outfile << templine.str();

                        //std::cout << templine.str();
                        if (ui->liveDisplay->isChecked())
                            emit LiveReadout(std::make_pair<std::string, bool> (templine.str(), 0));
                        templine.str("");
                    }
                    valid = 0;
                    templine.str("");
                }
            }
            }
            if (ui->liveDisplay->isChecked())
                emit DisplayReadoutRepaint();
            QApplication::processEvents();
            answer = "";
            Sleep(10);
        }
        outfile.close();
        logit("Readout stopped");
    }
    else
        logit("File exists already!");
}

void MainWindow::on_Display_Readout_Results_clicked()
{
    std::string filename = ui->ReadoutFile->text().toStdString() + ".dat";
    std::ifstream infile ((filename).c_str());
    if (infile.good())
    {
        infile.close();
        DisplayReadoutUI->show();
        emit MainToReadout(filename);
    }
    else
        logit("File not found!");
}

void MainWindow::on_liveDisplay_clicked()
{
    if (ui->liveDisplay->isChecked())
        DisplayReadoutUI->show();
    else
        DisplayReadoutUI->hide();
}

void MainWindow::on_Analyze_Readout_Results_clicked()
{
    logit("Cluster analysis of " + ui->ReadoutFile->text().toStdString() + ".dat started.");

    QVector<std::array<int, 6> > indata, outdata; //Col, Row, TriggerTS, EventTS, Delay, side(left=1,right=2)
    indata.clear();
    outdata.clear();
    set_ProgressBar(0, 1, 1);
    QApplication::processEvents();
    indata = LoadReadoutData();

    int PBnow = indata.length()*0.1;
    int PBmax = indata.length()*1.2;
    set_ProgressBar(PBmax, PBnow, 1);
    std::array<int,6> TempA, TempB;
    TempA = indata.front();
    QApplication::processEvents();
    for (QVector<std::array<int, 6> >::iterator it = indata.begin()+1; it < indata.end(); it++)
    {
        TempB = *it;
        PBnow++;
        set_ProgressBar(PBmax, PBnow, 1);
        QApplication::processEvents();
        if (TempA[2] == TempB[2]) // same TriggerTS
        {
            if (TempB[3] < TempA[3])
            {
                TempA = TempB;
            }
        }
        else
        {
            outdata.append(TempA);
            //std::cout << TempA[0] << " " << TempA[1] << " "<< TempA[2] << " "<< TempA[3] << " "<< TempA[4] << " "<< TempA[5] << " "<< std::endl;
            TempA = TempB;
        }
    }
    outdata.append(TempA);
    //std::cout << TempA[0] << " " << TempA[1] << " "<< TempA[2] << " "<< TempA[3] << " "<< TempA[4] << " "<< TempA[5] << " "<< std::endl;
    indata.clear();
    SaveReadoutData(outdata);
    set_ProgressBar(PBmax, PBmax, 1);
    set_ProgressBar(PBmax, PBmax, 0);
    logit("Cluster analysis of " + ui->ReadoutFile->text().toStdString() + ".dat finished. Result saved in " + ui->ReadoutFile->text().toStdString() + "analyzed.dat.");



    /*
    QVector<std::array<int, 6> > indata, ClusterCandidate, outdata; //Col, Row, TriggerTS, EventTS, Delay, side(left=1,right=2)
    indata.clear();
    ClusterCandidate.clear();
    outdata.clear();
    indata = LoadReadoutData();
    std::array<int,6> TempA, TempB;

    while (indata.length() >= 2)
    {
        TempA = indata.front();
        //indata.pop_front();
        //ClusterCandidate.push_back(TempA);

        //TempB = TempA;

        bool sameTS = 1;

        while (sameTS)
        {
            TempB = indata.front();
            if (TempA[2] == TempB[2])
            {
                indata.pop_front();
                ClusterCandidate.push_back(TempB);
            }
            else
            {
                sameTS = 0;
            }
        }
        std::cout << ClusterCandidate.length() << std::endl;
        //ClusterCandidat analysieren
        QVector<std::array<int, 6> >::iterator it = ClusterCandidate.begin();
        if (ClusterCandidate.length() == 1)
            outdata.append(ClusterCandidate.back());
        else
        {
            //write back to indata, which dont belong in the Cluster
            while (it != ClusterCandidate.end())
            {
                TempA = *it;
                it++;
                TempB = *it;
                if (!((std::abs(TempA[0]-TempB[0]) <= 3) && (std::abs(TempA[1]-TempB[1]) <= 1)))
                {

                    std::cout << "debug2" << std::endl;
                    indata.push_front(TempB);

                    if (it != ClusterCandidate.end())
                    {
                        ClusterCandidate.erase(it);
                        it--;
                    }
                }
            }

            //select the element with lowest delay and write it to outdata, others delete
            it = ClusterCandidate.begin();
            QVector<std::array<int, 6> >::iterator bestdelay = it;
            while (it != ClusterCandidate.end())
            {
                TempA = *bestdelay;
                TempB = *it;
                if (TempA[4]>TempB[4])
                    bestdelay = it;
                it++;
            }
            TempA = *bestdelay;
            outdata.append(TempA);
        }
        ClusterCandidate.clear();
    }
    std::stringstream outline;

    QVector<std::array<int, 6> >::iterator it = outdata.begin();
    while (it != outdata.end())
    {
        TempA = *it;
        outline << TempA[0] << " " << TempA[1] << " " << TempA[2] << " " << TempA[3] << " " << TempA[4] << " " << TempA[5] << std::endl;
        std::cout << "bla" << outline;
        it++;
    }*/

}

void MainWindow::SaveReadoutData(QVector<std::array<int, 6> > outdata)
{
    std::string filename = ui->ReadoutFile->text().toStdString() + "analyzed.dat";
    std::ofstream outfile;
    outfile.open(filename.c_str());
    std::array<int,6> TempA;
    for (QVector<std::array<int, 6> >::iterator it = outdata.begin(); it != outdata.end(); it++)
    {
        TempA = *it;
        outfile << TempA[0] << " " << TempA[1] << " "<< TempA[2] << " "<< TempA[3] << " "<< TempA[4] << " "<< TempA[5] << std::endl;
    }
    outfile.close();
    outdata.clear();
}

QVector<std::array<int, 6> > MainWindow::LoadReadoutData()
{
    std::string filename = ui->ReadoutFile->text().toStdString() + ".dat";
    std::fstream infile ;
    infile.open(filename.c_str(), std::ios::in);
    int ReadoutBlock, TrigTS1, TrigTS2, ExtTime, TSL, ADDRL, TSR, ADDRR, TrigTS, EventTS, Delay; // input
    std::string ReadoutBlocks, TrigTS1s, TrigTS2s, ExtTimes, TSLs, ADDRLs, TSRs, ADDRRs, TrigTSs, EventTSs, Delays; // input
    int col, row, Side;   //decoded
    QVector<std::array<int, 6> > indata; //Col, Row, TriggerTS, EventTS, Delay, side(left=1,right=2)
    int k=0;
    bool analyzed = 0;
    if (infile.good())
    {

        while (!infile.eof())
        {

        infile >> ReadoutBlocks >> TrigTS1s >> TrigTS2s >> ExtTimes >> TSLs >> ADDRLs;
        if ((k==0) && (ReadoutBlocks != "ROBind"))  //analyzed files have no header
            analyzed = 1;

        if (!analyzed)
            infile >> TSRs >> ADDRRs >> TrigTSs >> EventTSs >> Delays;

        k++;

        if (!analyzed)
        {
        if (ReadoutBlocks != "ROBind")  //not analyzed file
        {
            QVector<int> temp;
            temp.clear();
            bool left = 1;
            bool right = 1;
            if (ReadoutBlocks == "-")
            {
                temp.append(0);
                left = 0;
                right = 0;
            }
            else
                temp.append(std::stoi(ReadoutBlocks));

            if (TrigTS1s == "-")
            {
                temp.append(0);
                left = 0;
                right = 0;
            }
            else
                temp.append(std::stoi(TrigTS1s));

            if (TrigTS2s == "-")
            {
                temp.append(0);
                left = 0;
                right = 0;
            }
            else
                temp.append(std::stoi(TrigTS2s));

            if (ExtTimes == "-")
            {
                temp.append(0);
                left = 0;
                right = 0;
            }
            else
                temp.append(std::stoi(ExtTimes));

            if (TSLs == "-")
            {
                temp.append(0);
                left = 0;
            }
            else
                temp.append(std::stoi(TSLs));

            if (ADDRLs == "-")
            {
                temp.append(0);
                left = 0;
            }
            else
                temp.append(std::stoi(ADDRLs));

            if (TSRs == "-")
            {
                temp.append(0);
                right = 0;
            }
            else
                temp.append(std::stoi(TSRs));

            if (ADDRRs == "-")
            {
                temp.append(0);
                right = 0;
            }
            else
                temp.append(std::stoi(ADDRRs));

            if (TrigTSs == "-")
            {
                temp.append(0);
                right = 0;
                left = 0;
            }
            else
                temp.append(std::stoi(TrigTSs));

            if (EventTSs == "-")
            {
                temp.append(0);
                right = 0;
                left = 0;
            }
            else
                temp.append(std::stoi(EventTSs));

            if (Delays == "-")
            {
                temp.append(0);
                right = 0;
                left = 0;
            }
            else
                temp.append(std::stoi(Delays));

            if (left)
                temp.append(1);
            else
                temp.append(0);

            if (right)
                temp.append(1);
            else
                temp.append(0);

            ReadoutBlock =  temp[0];
            TrigTS1 =       temp[1];
            TrigTS2 =       temp[2];
            ExtTime =       temp[3];
            TSL =           temp[4];
            ADDRL =         temp[5];
            TSR =           temp[6];
            ADDRR =         temp[7];
            TrigTS =        temp[8];
            EventTS =       temp[9];
            Delay =         temp[10];
            left =          temp[11];
            right =         temp[12];

            if (left == 1)
            {
                col = ((int)floor((ReadoutBlock*40+ADDRL-1)))/16;
                row = 15 - ((ReadoutBlock*40+ADDRL-1)%16);
            }
            else
            {
                if (right == 1)
                {
                    col = 150+((int)floor((ReadoutBlock*40+ADDRR-1)))/16;
                    row = 15 - ((ReadoutBlock*40+ADDRR-1)%16);
                }
            }

            if (left)
                Side = 1;
            if (right)
                Side = 2;

            if (left || right)
            {
                std::array<int, 6> templine;
                templine = {col, row, TrigTS, EventTS, Delay, Side};
                indata.append(templine);
            }
            QApplication::processEvents();
        }
        }
        else
        {   //analyzed-file case
            std::array<int, 6> templine;
            templine[0] = (std::stoi(ReadoutBlocks));  //actually column
            templine[1] = (std::stoi(TrigTS1s));       //actually row
            templine[2] = (std::stoi(TrigTS2s));       //actually TriggerTS
            templine[3] = (std::stoi(ExtTimes));       //actually EventTS
            templine[4] = (std::stoi(TSLs));           //actually Delay
            templine[5] = (std::stoi(ADDRLs));         //actually isLeft
            indata.append(templine);
            QApplication::processEvents();
        }
        }
    }
    else
        logit("File not found!");

    infile.close();
    return indata;
    indata.clear();
}

/*QVector<std::array<int, 6> > MainWindow::LoadReadoutData()
{
    std::string filename = ui->ReadoutFile->text().toStdString() + ".dat";
    std::fstream infile ;
    infile.open(filename.c_str(), std::ios::in);
    int ReadoutBlock, TrigTS1, TrigTS2, ExtTime, TSL, ADDRL, TSR, ADDRR, TrigTS, EventTS, Delay; // input
    std::string ReadoutBlocks, TrigTS1s, TrigTS2s, ExtTimes, TSLs, ADDRLs, TSRs, ADDRRs, TrigTSs, EventTSs, Delays; // input
    int col, row, Side;   //decoded
    QVector<std::array<int, 6> > indata; //Col, Row, TriggerTS, EventTS, Delay, side(left=1,right=2)
    int k=0;
    if (infile.good())
    {

        while (!infile.eof())
        {
            k++;
        infile >> ReadoutBlocks >> TrigTS1s >> TrigTS2s >> ExtTimes >> TSLs >> ADDRLs >> TSRs >> ADDRRs >> TrigTSs >> EventTSs >> Delays;
        if (ReadoutBlocks != "ROBind")
        {
            QVector<int> temp;
            temp.clear();
            bool left = 1;
            bool right = 1;
            if (ReadoutBlocks == "-")
            {
                temp.append(0);
                left = 0;
                right = 0;
            }
            else
                temp.append(std::stoi(ReadoutBlocks));

            if (TrigTS1s == "-")
            {
                temp.append(0);
                left = 0;
                right = 0;
            }
            else
                temp.append(std::stoi(TrigTS1s));

            if (TrigTS2s == "-")
            {
                temp.append(0);
                left = 0;
                right = 0;
            }
            else
                temp.append(std::stoi(TrigTS2s));

            if (ExtTimes == "-")
            {
                temp.append(0);
                left = 0;
                right = 0;
            }
            else
                temp.append(std::stoi(ExtTimes));

            if (TSLs == "-")
            {
                temp.append(0);
                left = 0;
            }
            else
                temp.append(std::stoi(TSLs));

            if (ADDRLs == "-")
            {
                temp.append(0);
                left = 0;
            }
            else
                temp.append(std::stoi(ADDRLs));

            if (TSRs == "-")
            {
                temp.append(0);
                right = 0;
            }
            else
                temp.append(std::stoi(TSRs));

            if (ADDRRs == "-")
            {
                temp.append(0);
                right = 0;
            }
            else
                temp.append(std::stoi(ADDRRs));

            if (TrigTSs == "-")
            {
                temp.append(0);
                right = 0;
                left = 0;
            }
            else
                temp.append(std::stoi(TrigTSs));

            if (EventTSs == "-")
            {
                temp.append(0);
                right = 0;
                left = 0;
            }
            else
                temp.append(std::stoi(EventTSs));

            if (Delays == "-")
            {
                temp.append(0);
                right = 0;
                left = 0;
            }
            else
                temp.append(std::stoi(Delays));

            if (left)
                temp.append(1);
            else
                temp.append(0);

            if (right)
                temp.append(1);
            else
                temp.append(0);

            ReadoutBlock =  temp[0];
            TrigTS1 =       temp[1];
            TrigTS2 =       temp[2];
            ExtTime =       temp[3];
            TSL =           temp[4];
            ADDRL =         temp[5];
            TSR =           temp[6];
            ADDRR =         temp[7];
            TrigTS =        temp[8];
            EventTS =       temp[9];
            Delay =         temp[10];
            left =          temp[11];
            right =         temp[12];

            if (left == 1)
            {
                col = ((int)floor((ReadoutBlock*40+ADDRL-1)))/16;
                row = 15 - ((ReadoutBlock*40+ADDRL-1)%16);
            }
            else
            {
                if (right == 1)
                {
                    col = 150+((int)floor((ReadoutBlock*40+ADDRR-1)))/16;
                    row = 15 - ((ReadoutBlock*40+ADDRR-1)%16);
                }
            }

            if (left)
                Side = 1;
            if (right)
                Side = 2;

            if (left || right)
            {
                std::array<int, 6> templine;
                templine = {col, row, TrigTS, EventTS, Delay, Side};
                indata.append(templine);
            }
        }
        }
    }
    else
        logit("File not found!");

    infile.close();
    return indata;
    indata.clear();
}*/

void MainWindow::on_Make_Histogram_clicked()
{
    int PBmax=500000;
    int PBnow = 0;
    set_ProgressBar(PBmax, PBnow, 1);
    QVector<std::array<int, 6> > indata;
    indata.clear();

    indata = LoadReadoutData();
    PBnow+80000;
    set_ProgressBar(PBmax, PBnow, 1);
    int Hist[140000];
    std::array<int, 6> Temp;
    QVector<std::array<int, 6> >::iterator it = indata.begin();

    for (int i = 0; i < 140000; i++)
    {
        set_ProgressBar(PBmax, PBnow, 1);
        Hist[i] = 0;
        PBnow++;
    }

    while (it != indata.end())
    {
        Temp = *it;
        //std::cout << Temp[4] << std::endl;
        Hist[Temp[4]+70000]++;
        it++;
        PBnow++;
        set_ProgressBar(PBmax, PBnow, 1);
        QApplication::processEvents();
    }

    std::string filename = ui->ReadoutFile->text().toStdString() + "_hist.dat";
    std::ofstream outfile;
    outfile.open(filename.c_str());
    for (int i = 0; i < 140000; i++)
    {
        outfile << (i-70000) << " " << Hist[i] << std::endl;
        PBnow++;
        set_ProgressBar(PBmax, PBnow, 1);
        QApplication::processEvents();
    }
    outfile.close();
    delete Hist;
    indata.clear();
    set_ProgressBar(PBmax, PBnow, 0);
}

void MainWindow::on_InjCalib_clicked()
{
    ViUInt32 readchars;
    int acq = 1000;
    int PBnow = 0;
    int PBmax = 80;
    ui->H35Col->setValue(0);
    ui->H35Col_2->setValue(0);
    ui->H35Col_3->setValue(0);
    ui->NumInjectionSpinBox->setValue(0);
    set_ProgressBar(PBmax, PBnow, 1);
    if(ui->InjectionButton->text() != "Start Injections")
        on_InjectionButton_clicked();
    on_InjectionButton_clicked();
    QFile file;
    for (int irow = 0; irow < 16; irow++)
    {
        ui->H35Row->setValue(irow);
        ui->RowToAOut->setValue(irow);
        on_Update_clicked();
        for (double inj = 0.5; inj < 2.5; inj = inj+0.5)
        {
            int k = 0;
            ui->DAC4->setText(QString::number(inj));
            on_Update_2_clicked();
            status = viWrite(instr, (ViBuf)("ACQ:STOPA SEQ"), 13, &readchars);
            status = viWrite(instr, (ViBuf)("ACQ:STOPA RUNST"), 15, &readchars);
            status = viWrite(instr, (ViBuf)("ACQ:STATE RUN"), 13, &readchars);
            status = viWrite(instr, (ViBuf)("HIST:COUN RESET"), 15, &readchars);
            Sleep(20000);
            do
            {
                status = viQueryf(instr,(ViString)"ACQ:NUMACq?", (ViString)"%i", &k);
                QApplication::processEvents();
                Sleep(20);
            }while (k < acq);
            on_histogramButton_clicked();
            QString newname = "Historgram_Row" + QString::number(irow) + "_Inj" + QString::number(inj) + ".dat";
            file.rename("histogram_1.dat", newname);
            PBnow++;
            set_ProgressBar(PBmax, PBnow, 1);
        }
    }
    set_ProgressBar(PBmax, PBnow, 1);
    set_ProgressBar(PBmax, PBnow, 0);
}

void MainWindow::on_Make_Histogram_2_clicked()
{
    int PBmax=2400+2400+2400+2400;
    int PBnow = 1;
    set_ProgressBar(PBmax, PBnow, 1);
    QApplication::processEvents();
    QVector<std::array<int, 6> > indata;
    indata.clear();

    indata = LoadReadoutData();
    PBnow = PBnow+2400;
    set_ProgressBar(PBmax, PBnow, 1);
    int Hist[150][16];
    std::array<int, 6> Temp;
    QVector<std::array<int, 6> >::iterator it = indata.begin();

    for (int i = 0; i < 150; i++)
    {
        for (int j = 0; j < 16; j++)
        {
            Hist[i][j] = 0;
            PBnow++;
            set_ProgressBar(PBmax, PBnow, 1);
        }
    }

    while (it != indata.end())
    {
        Temp = *it;//Col, Row, TriggerTS, EventTS, Delay, side(left=1,right=2)
        //std::cout << Temp[4] << std::endl;
        Hist[Temp[0]][Temp[1]]++;
        it++;
        QApplication::processEvents();
    }

    PBnow = PBnow+2400;
    set_ProgressBar(PBmax, PBnow, 1);

    std::string filename = ui->ReadoutFile->text().toStdString() + "_Matrix_hist.dat";
    std::ofstream outfile;
    outfile.open(filename.c_str());
    for (int i = 0; i < 16; i++)
    {
        for (int j = 0; j < 150; j++)
        {
            outfile << Hist[j][i] << " ";
            PBnow++;
            set_ProgressBar(PBmax, PBnow, 1);
            //QApplication::processEvents();
        }
        outfile << std::endl;
    }
    outfile.close();
    delete Hist;
    indata.clear();
    set_ProgressBar(PBmax, PBmax, 1);
    set_ProgressBar(PBmax, PBnow, 0);
}

void MainWindow::on_Make_Histogram_3_clicked()
{
    std::ifstream infile ;
    std::string  filename = ui->ReadoutFile->text().toStdString() + ".dat";
    std::string line;
    std::ofstream outfile;
    infile.open( filename.c_str(), std::ios::in);
    int counter = 0;
    int k = 0;
    if (infile)
    {
        while(infile)
        {
            counter = 0;
            k++;
            filename = ui->ReadoutFile->text().toStdString() + "_Part" + std::to_string(k) + ".dat";
            outfile.open(filename.c_str());
            while ((counter < 2000000) && (getline(infile, line)))
            {
                counter++;
                outfile<<line<<std::endl;
            }
            outfile.close();
        }
    }
    infile.close();
}

void MainWindow::on_Make_Histogram_4_clicked()
{
    std::ifstream infile ;
    std::string  filename = "ReadoutData_finefocus_mid_Part1_Matrix_hist.dat";
    std::string line;
    std::ofstream outfile;
    int Hist[150][16];
    int temp;
    for (int col = 0; col < 150; col++)
    {
        for (int row = 0; row < 16; row++)
        {
            Hist[col][row] = 0;
        }
    }

    for (int k = 1; k<=8; k++)
    {
        filename = "ReadoutData_finefocus_mid_Part" + std::to_string(k) + "_Matrix_hist.dat" ;
        infile.open(filename.c_str());
        for (int row = 0; row < 16; row++)
        {
            for (int col = 0; col < 150; col++)
            {
                infile >> temp;
                Hist[col][row] = Hist[col][row] + temp;
            }
        }
        infile.close();
    }

    outfile.open("ReadoutData_finefocus_mid_Matrix_hist.dat");
    for (int row = 0; row < 16; row++)
    {
        for (int col = 0; col < 150; col++)
        {
            outfile << Hist[col][row] << " ";
        }
        outfile << std::endl;
    }
    outfile.close();
}

//---------------------
//S-Curve fitting stuff
//---------------------

int Srow, Scol, SRamVal;

//cumulative distribution function
double MainWindow::CDF(double x, double x0, double width)
{
    return (1.0 + erf((x - x0)/width)) / 2.0;
    std::cout << "CDF" << std::endl;
}

void function1_fvec(const alglib::real_1d_array &x, alglib::real_1d_array &fi, void *ptr)
{
    //std::cout << "fvec" << std::endl;
    if(ptr != 0){}

    std::fstream f;
    double X;
    double Y;
    int i=0;
    std::string filename = "SCurve(" + QString::number(Scol).toStdString() + " " + QString::number(Srow).toStdString() + ") RamPix" + QString::number(SRamVal).toStdString() + ".dat";
    f.open(filename.c_str(), std::ios::in);
    double scale=0;
    if (f.good())
    {
        while(!f.eof())
        {
            f >> X >> Y;
            if (scale == 0)
            {
                if (Y<0.9)
                    scale = 1/Y;
                else
                    scale = 1;
            }
            if (X>=0)
            {
                fi[i] = MainWindow::CDF(X,x[0], x[1]) - (Y*scale);
                i++;
            }
        }
    }
    f.close();
    //std::cout << "fvec" << std::endl;
}

void function1_jac(const alglib::real_1d_array &x, alglib::real_1d_array &fi, alglib::real_2d_array &jac, void *ptr)
{
    //std::cout<< "jac" << std::endl;
    if(ptr != 0){}

    std::fstream f;
    double X;
    double Y;
    int i=0;
    std::string filename = "SCurve(" + QString::number(Scol).toStdString() + " " + QString::number(Srow).toStdString() + ") RamPix" + QString::number(SRamVal).toStdString() + ".dat";
    f.open(filename.c_str(), std::ios::in);
    double scale = 0;
    if (f.good())
    {
        while(!f.eof())
        {
            f >> X >> Y;
            if (scale == 0)
            {
                if (Y<0.9)
                    scale = 1/Y;
                else
                    scale = 1;
            }
            if (X>=0)
            {
                //std::cout << i << X << Y << std::endl;
                fi[i] = MainWindow::CDF(X,x[0], x[1]) - (Y*scale);
                jac[i][0] = -exp(pow((X - x[0])/x[1],2)) / sqrt(3.1415926);
                jac[i][1] = jac[i][0] * (X - x[0])/x[1];
                jac[i][0] /= x[1];
                i++;
            }
            X=0;
        }
    }
    f.close();
    //std::cout<< "jac" << std::endl;
}

QVector<double> MainWindow::SCurveFit(int col, int row, int RamVal)
{
    Srow = row;
    Scol = col;
    SRamVal = RamVal;
    QVector<double> result;
    result.clear();
   // std::cout << "test " << std::endl;

    std::fstream f;
    double X;
    double Y;
    int Size = 0;
    std::string filename = "SCurve(" + QString::number(Scol).toStdString() + " " + QString::number(Srow).toStdString() + ") RamPix" + QString::number(SRamVal).toStdString() + ".dat";
    f.open(filename.c_str(), std::ios::in);
    //std::cout << filename << std::endl;
    bool validA = 0;
    bool validB = 0;
    if (f.good())
    {
        while(!f.eof())
        {
            f >> X >> Y;
            if (X>=0)
            {
                if (Y > 0.5 && X >0)
                    validA = 1;
                if (Y < 0.2 && X >0)
                    validB = 1;
                Size++;
            }
        }
    }
    f.close();
    //std::cout << "test " << Size << std::endl;
    //
    // This example demonstrates minimization of F(x0,x1) = f0^2+f1^2, where
    //
    //     f0(x0,x1) = 10*(x0+3)^2
    //     f1(x0,x1) = (x1-3)^2
    //
    // using "VJ" mode of the Levenberg-Marquardt optimizer.
    //
    // Optimization algorithm uses:
    // * function vector f[] = {f1,f2}
    // * Jacobian matrix J = {dfi/dxj}.
    //
    if (validA && validB)
    {
        alglib::real_1d_array x;
        x.setlength(2);
        x[0] = 0.5;
        x[1] = 0.5;
        double epsg = 0;//0.0000000001;
        double epsf = 1e-4;//0;
        double epsx = 0;
        alglib::ae_int_t maxits = 100000;
        alglib::minlmstate state;
        alglib::minlmreport rep;

        //alglib::minlmcreatevj(Size, x, 1.e-4, state);
        alglib::minlmcreatevj(Size, x, state);    //0.0001, state);
        alglib::minlmsetcond(state, epsg, epsf, epsx, maxits);
        alglib::minlmoptimize(state, function1_fvec, function1_jac);
        alglib::minlmresults(state, x, rep);

        //std::cout << "Fit Results:" << std::endl;
        //std::cout << "    Iterations:   " << rep.iterationscount << std::endl;
        //std::cout << "    Termination:  " << int(rep.terminationtype) << std::endl;
        //std::cout << "    Parameters:   x0 = " << x[0] << ";  width = " << x[1] << std::endl;
        std::cout << x[1] << std::endl;
        if (x[1]<0.38)
        {
            result.append(x[0]);
            result.append(x[1]);
        }
        else
        {
            result.append(0);
            result.append(0);
        }

    }
    else
    {
        result.append(0);
        result.append(0);
    }
    return result;
    result.clear();
}

void MainWindow::on_SCurveFit_clicked()
{
    int found0 = 0;
    int found1 = 0;
    int found2 = 0;
    int found3 = 0;
    int notfound = 0;
    int PBmax = 2400;
    int PBnow = 0;

    logit("SCurve fits started.");
    set_ProgressBar(PBmax, PBnow, 1);
    std::string ramfilename;
    std::ofstream ramfile[5] ;
    for (int ramval = 0; ramval<5; ramval++)
    {
        if (ramval == 4)
        {
            ramfilename = "05Points_SCurve_bestRamPix.dat";
        }
        else
        {
        ramfilename = "05Points_SCurve_RamPix" + QString::number(ramval).toStdString() + ".dat";
        }
        ramfile[ramval].open(((ramfilename).c_str()));
        ramfile[ramval] << "";
    }



    double InjTarget = ui->InjTarget->text().toDouble();
    std::ofstream outfile;
    outfile.open("RamPix.dat");
    for (int col = 0; col < 150; col++)
    {
        for (int row = 0; row < 16; row++)
        {
            PBnow++;
            set_ProgressBar(PBmax, PBnow, 1);
            int BestRam = 4;
            double BestInj = 10.0;
            for (int RamVal = 0; RamVal <4; RamVal++)
            {
                QVector<double> result;
                result.clear();
                result = SCurveFit(col,row,RamVal);

                ramfile[RamVal] << col << " " << row << " " << result[0] << std::endl;

                if ((std::fabs(InjTarget-result[0]) < std::fabs(InjTarget-BestInj)) && (result[0] != 0))
                {
                    BestRam = RamVal;
                    BestInj = result[0];
                }

            }
            outfile << col << " " << row << " " << BestRam << std::endl;
            ramfile[4] << col << " " << row << " " << BestInj << " " << BestRam << std::endl;

            if (BestRam == 4)
                notfound++;
            if (BestRam == 0)
                found0++;
            if (BestRam == 1)
                found1++;
            if (BestRam == 2)
                found2++;
            if (BestRam == 3)
                found3++;
            QApplication::processEvents();
        }
    }
    outfile.close();

    for (int ramval = 0; ramval<5; ramval++)
    {
        ramfile[ramval].close();
    }
    logit(("50% points NOT found for " + QString::number(notfound) + " SCurves").toStdString() );
    logit(("RamPix 0 used " + QString::number(found0) + " times").toStdString());
    logit(("RamPix 1 used " + QString::number(found1) + " times").toStdString());
    logit(("RamPix 2 used " + QString::number(found2) + " times").toStdString());
    logit(("RamPix 3 used " + QString::number(found3) + " times").toStdString());
    //std::cout << "max " << max << " min " << min << std::endl;
    set_ProgressBar(PBmax, PBmax, 1);
    set_ProgressBar(PBmax, PBmax, 0);
    logit("SCurve fits completed.");
}

void MainWindow::on_SCurveRefit_clicked()
{
    QVector<double> dataRam[4];
    int PBmax = 2400;
    int PBnow = 0;

    set_ProgressBar(PBmax, PBnow, 1);
    std::fstream f;
    std::string filename;
    int row, col;
    double fifty;

    for (int RamVal = 0; RamVal<4; RamVal++)
    {
        dataRam[RamVal].clear();
        filename = "05Points_SCurve_RamPix" + QString::number(RamVal).toStdString() + ".dat";
        f.open(filename.c_str(), std::ios::in);
        if (f.good())
        {
            while(!f.eof())
            {
                f >> col >> row >> fifty;
                dataRam[RamVal].append(fifty);
            }
        }
        f.close();
    }

    double InjTarget = ui->InjTarget->text().toDouble();
    std::ofstream outfile, outfile2;
    outfile.open("RamPix.dat");
    outfile2.open("05Points_SCurve_bestRamPix.dat");
    QVector<double>::iterator it[4];
    for (int i = 0; i<4; i++)
        it[i] = dataRam[i].begin();

    int found0 = 0;
    int found1 = 0;
    int found2 = 0;
    int found3 = 0;
    int notfound = 0;

    for (int i = 0; i<2400; i++)
    {
        int BestRam = 4;
        double BestInj = 10.0;
        PBnow++;
        set_ProgressBar(PBmax, PBnow, 1);
        for (int RamVal = 0; RamVal<4; RamVal++)
        {

            if ((std::fabs(InjTarget- *(it[RamVal]+i)) < std::fabs(InjTarget-BestInj)) && (*(it[RamVal]+i) != 0))
            {
                BestRam = RamVal;
                BestInj = *(it[RamVal]+i);
            }
        }
        outfile << i/16 << " " << i%16 << " " << BestRam << std::endl;
        outfile2 << i/16 << " " << i%16 << " " << BestInj << " " << BestRam << std::endl;

        if (BestRam == 4)
            notfound++;
        if (BestRam == 0)
            found0++;
        if (BestRam == 1)
            found1++;
        if (BestRam == 2)
            found2++;
        if (BestRam == 3)
            found3++;
        QApplication::processEvents();
    }
    logit(("50% points NOT found for " + QString::number(notfound) + " SCurves").toStdString() );
    logit(("RamPix 0 used " + QString::number(found0) + " times").toStdString());
    logit(("RamPix 1 used " + QString::number(found1) + " times").toStdString());
    logit(("RamPix 2 used " + QString::number(found2) + " times").toStdString());
    logit(("RamPix 3 used " + QString::number(found3) + " times").toStdString());
    outfile.close();
    outfile2.close();
    set_ProgressBar(PBmax, PBmax, 1);
    set_ProgressBar(PBmax, PBnow, 0);
}

void MainWindow::on_pushButton_clicked()
{
    quiet =1;
    //low res SCurves for all RamVal and Pix
    ui->SCurveStep->setText("0.1");
    on_SCurveAllPixAllRamval_Button_clicked();
    Sleep(1);

    //ScurveFit
    on_SCurveFit_clicked();
    on_SaveTrim_Button_clicked();
    on_loadRamPix_Button_clicked();
    MoveSCurves("untuned");
    Sleep(1);

    //HighRes Scurves for all Pix
    ui->SCurveStep->setText("0.01");
    on_SCurveallV2_Button_clicked();
    Sleep(1);

    //save SCurves
    MoveSCurves("tuned");
    quiet = 0;
}

void MainWindow::MoveSCurves(QString folder)
{
    int PBmax = 2400;
    int PBnow = 0;
    set_ProgressBar(PBmax, PBnow, 1);
    QDir dir;
    dir.remove(folder);
    dir.mkdir(folder);
    QFile file;
    file.copy("DrawSCurves.cpp", folder + "/DrawSCurves.cpp");

    for (int col = 0; col < 150; col++)
    {
        for (int row = 0; row <16; row++)
        {
            for (int ramval = 0; ramval<4; ramval++)
            {
                QString currentfile;
                currentfile = "SCurve(" + QString::number(col) + " " + QString::number(row) + ") RamPix" + QString::number(ramval) + ".dat";
                if (file.exists(currentfile))
                {
                    file.copy(currentfile, folder + "/"+ currentfile);
                    file.remove(currentfile);
                    PBnow++;
                    set_ProgressBar(PBmax, PBnow, 1);
                    QApplication::processEvents();
                }
            }
        }
    }

    set_ProgressBar(PBmax, PBmax, 1);
    set_ProgressBar(PBmax, PBmax, 0);
    logit(("SCurves in " + folder + " stored.").toStdString());
}




void MainWindow::on_Sr90Injection_clicked()
{
    int PBnow = 0;
    int PBmax = 1010;
    set_ProgressBar(PBmax,PBnow,1);
    std::string filename = "Sr90Distribution.dat";
    std::fstream infile;
    infile.open(filename.c_str(), std::ios::in);
    QVector<std::pair<double, int> > SrDist;
    double signal, inj;
    int count;
    int maxcount = 0;
    on_Update_clicked();
    QVector<std::pair<unsigned int, unsigned int> > activepixels;
    activepixels.clear();
    activepixels.push_back(std::make_pair<unsigned int, unsigned int>((unsigned) ui->H35Col->value(), (unsigned) ui->H35Row->value()));
    QVector<std::pair<unsigned int, double> >  result;

    std::ofstream outfile ;
    outfile.open("StrontiumInjectionDelay.dat");
    outfile << "Inj[V] Delay[200MHz]" <<std::endl;
    SrDist.clear();

    if (infile.good())
    {
        while (!infile.eof())
        {
            infile >> signal >> count;
            //signal in V_out --> signal in V_inj
            //0.12150369266077Vout/Vinj +0.00737642340222Vout
            inj = (signal-0.00737642340222)/0.12150369266077;
            //SrDist.push_back(std::make_pair<double, int>(inj, count));
            SrDist.push_back(std::make_pair<double, int>((double)inj, (int)count));
            if (count > maxcount)
                maxcount = count;
        }
        std::cout << SrDist.length() << std::endl;
    }
    PBnow = 10;
    set_ProgressBar(PBmax,PBnow,1);
    infile.close();
    for(int i = 0; i <1000; i++)
    {
        PBnow++;
        set_ProgressBar(PBmax,PBnow,1);
        int randelement = qrand() % (SrDist.length());
        std::cout << randelement << std::endl;
        int randcount = qrand() % (maxcount + 1);
        if (SrDist[randelement].second <= randcount && SrDist[randelement].first >= 0.5 && SrDist[randelement].first <= 2 );
        {
            result = get_delay(activepixels, SrDist[randelement].first, ui->NumInjectionSpinBox->value(), 0);
            bool failed = 1;
            while ((result.length() != 0) &&(failed ==1))
            {
                if ((result.back().first/16 == ui->H35Col->value())&&(15- (result.back().first % 16) == ui->H35Row->value()))
                {
                    failed = 0;
                }
                else
                    result.pop_back();
            }
            if(!failed)
            {
                //logit("Delay of Pixel (" + std::to_string(((int)result.back().first)/16) + " " + std::to_string(15-((int)result.back().first)%16) + ") with current settings is " + std::to_string(result.back().second));

                outfile << SrDist[randelement].first << " " << result.back().second << std::endl;
            }
        }
    }

    set_ProgressBar(PBmax,PBnow,0);
    outfile.close();
}

void MainWindow::on_InjDelay_clicked()
{
    QVector<std::pair<double, double> > waveform;
    waveform = getwaveform(1,1000);

    QVector<std::pair<double, double> >::iterator it = waveform.begin();
    double min = 1;
    while(it != waveform.end())
    {
        if (min > it->second)
            min = it->second;
        it++;
    }
}

void MainWindow::on_get_delayCurve_2_clicked()
{

/*
    //for (int RamVal = 3; RamVal < 4; RamVal++)
    {


        for (int col = 150; col < 153; col++)
        {
            ui->H35Col->setValue(col);
            for (int row = 0; row < 3; row++)
            {
               ui->H35Row->setValue(row);
               //ui->TrimRamPix->setValue(RamVal);
               on_Update_clicked();
               on_get_delayCurve_clicked();
               //on_SCurveV2_Button_clicked();
            }
        }
    }
    //
    //    ui->TrimRamPix*/


    logit("Cluster analysis of " + ui->ReadoutFile->text().toStdString() + ".dat started.");

    QVector<std::array<int, 6> > indata, outdata; //Col, Row, TriggerTS, EventTS, Delay, side(left=1,right=2)
    indata.clear();
    outdata.clear();
    set_ProgressBar(0, 1, 1);
    QApplication::processEvents();
    indata = LoadReadoutData(); //Col, Row, TriggerTS, EventTS, Delay, side(left=1,right=2)

    int PBnow = indata.length()*0.1;
    int PBmax = indata.length()*1.2;
    set_ProgressBar(PBmax, PBnow, 1);
    std::array<int,6> TempA, TempB;
    //TempA = indata.front();
    QApplication::processEvents();
    for (QVector<std::array<int, 6> >::iterator it = indata.begin()+1; it < indata.end(); it++)
    {
        TempA =*(it-1);
        TempB = *it;
        PBnow++;
        set_ProgressBar(PBmax, PBnow, 1);
        QApplication::processEvents();
        if (TempA[2] != TempB[2])
            outdata.append(TempA);

        while ((TempA[2] == TempB[2])&&(it < indata.end()))
        {
            it++;
            TempB = *it;
        }

            //std::cout << TempA[0] << " " << TempA[1] << " "<< TempA[2] << " "<< TempA[3] << " "<< TempA[4] << " "<< TempA[5] << " "<< std::endl;


    }
    //std::cout << TempA[0] << " " << TempA[1] << " "<< TempA[2] << " "<< TempA[3] << " "<< TempA[4] << " "<< TempA[5] << " "<< std::endl;
    indata.clear();
    SaveReadoutData(outdata);
    //set_ProgressBar(PBmax, PBmax, 1);
    set_ProgressBar(PBmax, PBmax, 0);
    logit("Cluster analysis of " + ui->ReadoutFile->text().toStdString() + ".dat finished. Result saved in " + ui->ReadoutFile->text().toStdString() + "analyzed.dat.");


}
