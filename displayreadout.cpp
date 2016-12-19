#include "displayreadout.h"
#include "ui_displayreadout.h"

#include <QPainter>

#include <iostream>
#include <fstream>
#include <sstream>
#include <QMouseEvent>
#include <QColor>
#include "mainwindow.h"
#include <vector>
#include <qvector.h>
#include <QCloseEvent>

//Painting Parameters
#define widthA 6    //Pixel width
#define heightA 15  //Pixel height
#define xnull 80    //origin x
#define ynull 10    //origin y

DisplayReadout::DisplayReadout(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::DisplayReadout)
{
    ui->setupUi(this);

    ui->label->setGeometry(10,ynull+8*heightA-10,60,13);
    ui->label_2->setGeometry(10,ynull+8*heightA+10,60,13);

    ui->label_3->setGeometry(10,ynull+heightA*25-10, 60, 13);
    ui->label_4->setGeometry(10,ynull+heightA*25+10, 60, 13);
    for (int icol = 0; icol < 300; icol++)
    {
        for (int irow = 0; irow <16; irow++)
        {
            pixels[icol][irow] = 0;
        }
    }
    ui->Speed_spinbox->setVisible(0);
    ui->Speed_label->setVisible(0);
    ui->Persistence_label->setVisible(0);
    ui->Persistence_spinbox->setVisible(0);
    ui->Running_checkBox->setVisible(0);
}

DisplayReadout::~DisplayReadout()
{
    emit DisplayReadoutClosed();
    delete ui;
}


void DisplayReadout::closeEvent (QCloseEvent *event)
{
    ui->Speed_spinbox->setVisible(0);
    ui->Speed_label->setVisible(0);
    ui->Persistence_label->setVisible(0);
    ui->Persistence_spinbox->setVisible(0);
    ui->Running_checkBox->setVisible(0);
    ui->Running_checkBox->setChecked(0);
}

void DisplayReadout::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);

    //Columns 0 - 149:
    painter.fillRect(xnull-5,ynull-5,widthA*150+10, heightA*16+10,  QBrush(Qt::white));

    //draw active pixels
    for(int i=0;i<150;++i)
        for(int j=0;j<16;++j)
            if(pixels[i][j])
                painter.fillRect(xnull+1+widthA*i, ynull+1+heightA*j, widthA-1, heightA-1, QBrush(Qt::red));

    //Draw Matrix outlines
    for (int i = 0; i < 151; i++)
    {
        painter.drawLine(xnull+(i*widthA), ynull, xnull+(i*widthA), ynull+heightA*16);
    }
    for (int i = 0; i < 17; i++)
    {
        painter.drawLine(xnull, ynull+(i*heightA), xnull+(150*widthA), ynull+(i*heightA));
    }

    //Columns 150 - 299:
    painter.fillRect(xnull-5, ynull-5+10+heightA*(16+1), widthA*150+10, heightA*16+10,  QBrush(Qt::white));

    //draw active pixels
    for(int i=0;i<150;++i)
        for(int j=0;j<16;++j)
            if(pixels[i+150][j])
                painter.fillRect(xnull+1+widthA*i, ynull+10+heightA*(17+j)+1, widthA-1, heightA-1, QBrush(Qt::red));

    //Draw Matrix outlines
    for (int i = 0; i < 151; i++)
    {
        painter.drawLine(xnull+(i*widthA), ynull+10+heightA*17, xnull+(i*widthA), ynull+10+heightA*17+heightA*16);
    }
    for (int i = 0; i < 17; i++)
    {
        painter.drawLine(xnull, ynull+10+heightA*17+(i*heightA), xnull+(150*widthA), ynull+10+heightA*17+(i*heightA));
    }

}

void DisplayReadout::MainToReadout(std::string filename)
{
    std::fstream infile ;
    infile.open(filename.c_str(), std::ios::in);
    int ReadoutBlock, TrigTS1, TrigTS2, ExtTime, TSL, ADDRL, TSR, ADDRR, TrigTS, EventTS, Delay; // input
    std::string ReadoutBlocks, TrigTS1s, TrigTS2s, ExtTimes, TSLs, ADDRLs, TSRs, ADDRRs, TrigTSs, EventTSs, Delays; // input
    bool left, right;
    int col, row;   //decoded
    int persistence = ui->Persistence_spinbox->value();
    int speed = ui->Speed_spinbox->value();

    ui->Speed_spinbox->setVisible(1);
    ui->Speed_label->setVisible(1);
    ui->Persistence_label->setVisible(1);
    ui->Persistence_spinbox->setVisible(1);
    ui->Running_checkBox->setVisible(1);
    ui->Running_checkBox->setChecked(1);

    for (int icol = 0; icol < 300; icol++)
    {
        for (int irow = 0; irow <16; irow++)
        {
            pixels[icol][irow] = 0;
        }
    }
    while(!infile.eof())
    {
        while (!ui->Running_checkBox->isChecked())
            QApplication::processEvents();
        persistence = ui->Persistence_spinbox->value();
        speed = ui->Speed_spinbox->value();
        for (int icol = 0; icol < 300; icol++)
        {
            for (int irow = 0; irow <16; irow++)
            {
                if (pixels[icol][irow] < 1)
                    pixels[icol][irow] = 0;
                else
                    pixels[icol][irow] = pixels[icol][irow] - 1;
            }
        }
        infile >> ReadoutBlocks >> TrigTS1s >> TrigTS2s >> ExtTimes >> TSLs >> ADDRLs >> TSRs >> ADDRRs >> TrigTSs >> EventTSs >> Delays;
        if (ReadoutBlocks != "ROBind")
        {
            QVector<int> temp;
            temp.clear();
            temp = DecodeStrings(ReadoutBlocks, TrigTS1s, TrigTS2s, ExtTimes, TSLs, ADDRLs, TSRs, ADDRRs, TrigTSs, EventTSs, Delays);
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

            //std::cout << " " << ReadoutBlock << " " << TrigTS1 << " " << TrigTS2 << " " << ExtTime << " " << TSL << " " << ADDRL << " " << TSR << " " << ADDRR << " " << TrigTS << " " << EventTS << " " << Delay << " " << std::endl;
            //std::cout << " " << ReadoutBlocks << " " << TrigTS1s << " " << TrigTS2s << " " << ExtTimes << " " << TSLs << " " << ADDRLs << " " << TSRs << " " << ADDRRs << " " << TrigTSs << " " << EventTSs << " " << Delays << " " << std::endl;

            if (left == 1)
            {
                col = ((int)(ReadoutBlock*40+40-ADDRL))/16;
                row = 15 - ((ReadoutBlock*40+40-ADDRL)%16);
                pixels[col][row] = persistence;
            }
            else
            {
                if (right == 1)
                {
                    col = 150+((int)(ReadoutBlock*40+40-ADDRR))/16;
                    row = 15 - ((ReadoutBlock*40+40-ADDRR)%16);
                    pixels[col][row] = persistence;
                }
            }

            this->repaint();
            QApplication::processEvents();
            Sleep(speed);
        }
    }
    infile.close();
    ui->Speed_spinbox->setVisible(0);
    ui->Speed_label->setVisible(0);
    ui->Persistence_label->setVisible(0);
    ui->Persistence_spinbox->setVisible(0);
    ui->Running_checkBox->setVisible(0);
    ui->Running_checkBox->setChecked(0);
}

void DisplayReadout::LiveReadout(std::pair<std::string, bool> data)
{
    std::string ReadoutBlocks, TrigTS1s, TrigTS2s, ExtTimes, TSLs, ADDRLs, TSRs, ADDRRs, TrigTSs, EventTSs, Delays;
    int ReadoutBlock, TrigTS1, TrigTS2, ExtTime, TSL, ADDRL, TSR, ADDRR, TrigTS, EventTS, Delay; // input
    int col, row, left, right;   //decoded
    int persistence = ui->Persistence_spinbox->value();
    ui->Persistence_label->setVisible(1);
    ui->Persistence_spinbox->setVisible(1);

    if (data.second)
    {
        for (int icol = 0; icol < 300; icol++)
        {
            for (int irow = 0; irow <16; irow++)
            {
                if (pixels[icol][irow] < 1)
                    pixels[icol][irow] = 0;
                else
                    pixels[icol][irow] = pixels[icol][irow] - 1;
            }
        }
    }
    std::stringstream templine;
    templine.str("");
    templine << (data.first);
    templine >> ReadoutBlocks >> TrigTS1s >> TrigTS2s >> ExtTimes >> TSLs >> ADDRLs >> TSRs >> ADDRRs >> TrigTSs >> EventTSs >> Delays;
    QVector<int> temp;
    temp.clear();
    temp = DecodeStrings(ReadoutBlocks, TrigTS1s, TrigTS2s, ExtTimes, TSLs, ADDRLs, TSRs, ADDRRs, TrigTSs, EventTSs, Delays);
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
        col = ((int)(ReadoutBlock*40+ADDRL-1))/16;
        row = 15 - ((ReadoutBlock*40+ADDRL-1)%16);
        pixels[col][row] = persistence;
    }
    else
    {
        if (right ==1)
        {
            col = 150+((int)(ReadoutBlock*40+ADDRR-1))/16;
            row = 15 - ((ReadoutBlock*40+ADDRR-1)%16);
            pixels[col][row] = persistence;
        }
    }


    if (data.second)
        this->repaint();
}

void DisplayReadout::DisplayReadoutRepaint()
{
    this->repaint();
    for (int icol = 0; icol < 300; icol++)
    {
        for (int irow = 0; irow <16; irow++)
        {
            if (pixels[icol][irow] < 1)
                pixels[icol][irow] = 0;
            else
                pixels[icol][irow] = pixels[icol][irow] - 1;
        }
    }
}

QVector<int>  DisplayReadout::DecodeStrings(std::string ReadoutBlocks, std::string TrigTS1s, std::string TrigTS2s, std::string ExtTimes, std::string TSLs, std::string ADDRLs, std::string TSRs, std::string ADDRRs, std::string TrigTSs, std::string EventTSs, std::string Delays)
{
    QVector<int> result;
    result.clear();
    bool left = 1;
    bool right = 1;
    if (ReadoutBlocks == "-")
    {
        result.append(0);
        left = 0;
        right = 0;
    }
    else
        result.append(std::stoi(ReadoutBlocks));

    if (TrigTS1s == "-")
    {
        result.append(0);
        left = 0;
        right = 0;
    }
    else
        result.append(std::stoi(TrigTS1s));

    if (TrigTS2s == "-")
    {
        result.append(0);
        left = 0;
        right = 0;
    }
    else
        result.append(std::stoi(TrigTS2s));

    if (ExtTimes == "-")
    {
        result.append(0);
        left = 0;
        right = 0;
    }
    else
        result.append(std::stoi(ExtTimes));

    if (TSLs == "-")
    {
        result.append(0);
        left = 0;
    }
    else
        result.append(std::stoi(TSLs));

    if (ADDRLs == "-")
    {
        result.append(0);
        left = 0;
    }
    else
        result.append(std::stoi(ADDRLs));

    if (TSRs == "-")
    {
        result.append(0);
        right = 0;
    }
    else
        result.append(std::stoi(TSRs));

    if (ADDRRs == "-")
    {
        result.append(0);
        right = 0;
    }
    else
        result.append(std::stoi(ADDRRs));

    if (TrigTSs == "-")
    {
        result.append(0);
        right = 0;
        left = 0;
    }
    else
        result.append(std::stoi(TrigTSs));

    if (EventTSs == "-")
    {
        result.append(0);
        right = 0;
        left = 0;
    }
    else
        result.append(std::stoi(EventTSs));

    if (Delays == "-")
    {
        result.append(0);
        right = 0;
        left = 0;
    }
    else
        result.append(std::stoi(Delays));

    if (left)
        result.append(1);
    else
        result.append(0);

    if (right)
        result.append(1);
    else
        result.append(0);

    return result;
}
