#include "hbselect.h"
#include "ui_hbselect.h"
#include <QPainter>

#include <iostream>
#include <QMouseEvent>
#include <QColor>
#include "mainwindow.h"
#include <vector>

bool Matrix[150][16];   //PixelMatrix
bool ConfigX[60];       //RamCellCols
bool ConfigY[40];       //RamCellRows

//Drawing stuff
int widthA = 6;
int heightA = 15;
int widthC = widthA*150/60;
int heightC = heightA*16/40;
int xnull = 10;
int ynull = 10;

HBselect::HBselect(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::HBselect)
{
    ui->setupUi(this);
    clear(1,1,1);
}


HBselect::~HBselect()
{
    delete ui;

}

//set all Pixels 0 or 1
void HBselect::clear(bool MatrixDel, bool ConfigDel, bool empty)
{
    if (MatrixDel)
    {
        for (int i = 0; i <150; i++)
        {
            for (int j = 0; j < 16; j++)
            {
                if (empty)
                    Matrix[i][j] = 0;
                else
                    Matrix[i][j] = 1;
            }
        }
    }
    if (ConfigDel)
    {
        for (int i = 0; i <60; i++)
        {
            if (empty)
                ConfigX[i] = 0;
            else
                ConfigX[i] = 1;
        }
        for (int j = 0; j < 40; j++)
        {
            if (empty)
                ConfigY[j] = 0;
            else
                ConfigY[j] = 1;
        }
    }
}

//Paint Matrices with selected pixels
void HBselect::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);


    painter.fillRect(xnull-5, ynull-5, widthA*150+10, heightA*16+10+heightC*40,  QBrush(Qt::white));


    //Colorize Matrix
    for (int i = 0; i < 150; i++)
    {
        for (int j = 0; j < 16; j++)
        {
            if(Matrix[i][j])
            {
                painter.fillRect(xnull+1+widthA*i, ynull+1+heightA*j, widthA-1, heightA-1, QBrush(Qt::red));
            }
        }
    }

    //Colorize Config
    for (int i = 0; i < 60; i++)
    {
        if(ConfigX[i])
            painter.fillRect(xnull+1+widthC*i, ynull+1+(16*heightA)+10, widthC-1, (heightC*40)-1, QBrush(Qt::yellow));
    }
    for (int j = 0; j < 40; j++)
    {
        if(ConfigY[j])
            painter.fillRect(xnull+1, ynull+1+(16*heightA)+10+(j*heightC), (widthC*60)-1, heightC-1, QBrush(Qt::yellow));
    }
    for (int i = 0; i < 60; i++)
    {
        for (int j = 0; j < 40; j++)
        {
            if(ConfigX[i] && ConfigY[j])
            {
                painter.fillRect(xnull+1+widthC*i, ynull+1+heightC*j+(16*heightA)+10, widthC-1, heightC-1, QBrush(Qt::red));
            }
        }
    }

    //Draw Matrix outlines
    for (int i = 0; i < 151; i++)
    {
        painter.drawLine(xnull+(i*widthA), ynull, xnull+(i*widthA), ynull+heightA*16);
    }
    for (int i = 0; i < 17; i++)
    {
        painter.drawLine(xnull, ynull+(i*heightA), xnull+(150*widthA), ynull+(i*heightA));
    }

    //Draw Configuration outlines
    for (int i = 0; i < 61; i++)
    {
        painter.drawLine(xnull+(i*widthC), ynull+heightA*16+10, xnull+(i*widthC), ynull+heightA*16+10+heightC*40);
    }
    for (int i = 0; i < 41; i++)
    {
        painter.drawLine(xnull, ynull+(i*heightC)+(16*heightA)+10, xnull+(60*widthC), ynull+(i*heightC)+(16*heightA)+10);
    }

}


//Click event
void HBselect::mousePressEvent(QMouseEvent *MouseEvent)
{

    QPoint mousepos =  MouseEvent->pos();
    int xpos = mousepos.rx();
    int ypos = mousepos.ry();
    int xcoord;
    int ycoord;

    //Click on Matrix
    if ((xpos >=xnull) && (xpos<=xnull+(150*widthA)) && (ypos>=ynull) && (ypos < ynull+heightA*16))
    {
        xcoord = (xpos-xnull)/widthA;
        ycoord = (ypos-ynull)/heightA;
        if (MouseEvent->button() ==  Qt::LeftButton)
        {
            Matrix[xcoord][ycoord] = 1;
            MatrixToConfig();
        }
        if (MouseEvent->button() ==  Qt::RightButton)
        {
            Matrix[xcoord][ycoord] = 0;
            MatrixToConfig(xcoord, ycoord);
        }
    }
    else //Click on Config
    if ((xpos >=xnull) && (xpos<=xnull+(60*widthC)) && (ypos>= ynull+heightA*16+10) && (ypos <  ynull+heightA*16+10+heightC*40))
    {
        xcoord = (xpos-xnull)/widthC;
        ycoord = (ypos-(ynull+heightA*16+10))/heightC;

        if (MouseEvent->button() ==  Qt::LeftButton)
        {
            ConfigX[xcoord] = 1;
            ConfigY[ycoord] = 1;
        }
        if (MouseEvent->button() ==  Qt::RightButton)
        {
            ConfigX[xcoord] = 0;
            ConfigY[ycoord] = 0;
        }
        ConfigToMatrix();
    }
    this->repaint();
}

//Calculate PixelMatrix from RamCells
void HBselect::ConfigToMatrix()
{
    for (int i = 0; i <60; i++)
    {
        for (int j = 0; j < 40; j++)
        {
            if (i%2 == 0)
            {
                //first block
                if ((j+8)/16 == 2)
                {
                    //col0
                    Matrix[(i/2)*5][j-24] = (ConfigX[i] && ConfigY[j]);
                }
                else if ((j+8)/16 ==1)
                {
                    //col1
                    Matrix[(i/2)*5+1][j-8] = (ConfigX[i] && ConfigY[j]);
                }
                else
                {
                    //col2_0
                    Matrix[(i/2)*5+2][j+8] = (ConfigX[i] && ConfigY[j]);
                }
            }
            else
            {
                //second block
                if (j/16 ==0)
                {
                    //col4
                    Matrix[(i/2)*5+4][j] = (ConfigX[i] && ConfigY[j]);
                }
                else if (j/16 == 1)
                {
                    //col3
                    Matrix[(i/2)*5+3][j-16] = (ConfigX[i] && ConfigY[j]);
                }
                else
                {
                    Matrix[(i/2)*5+2][j-32] = (ConfigX[i] && ConfigY[j]);
                    //col2_1
                }
            }
        }
    }
}

//Left click on Pixel Matrix, calculates which Cols/Rows in RamCell have to be activated
void HBselect::MatrixToConfig()
{
    clear(0,1,1);
    for (int i = 0; i<150; i++)
    {
        for (int j = 0; j <16; j++)
        {
            if (i%5 == 0)
            {
                //RO 0, 24-39
                if (!ConfigX[(i/5)*2])
                    ConfigX[(i/5)*2] = Matrix[i][j];
                if (!ConfigY[j+24])
                    ConfigY[j+24] = Matrix[i][j];
            }
            if (i%5 == 1)
            {
                //RO 0, 8-23
                if (!ConfigX[(i/5)*2])
                    ConfigX[(i/5)*2] = Matrix[i][j];
                if (!ConfigY[j+8])
                    ConfigY[j+8] = Matrix[i][j];
            }
            if (i%5 == 2)
            {
                //RO 0, 0-8 & RO 1, 32-39
                if (j/8 == 1)
                {
                    if (!ConfigX[(i/5)*2])
                        ConfigX[(i/5)*2] = Matrix[i][j];
                    if (!ConfigY[j-8])
                        ConfigY[j-8] = Matrix[i][j];
                }
                else
                {
                    if (!ConfigX[(i/5)*2+1])
                        ConfigX[(i/5)*2+1] = Matrix[i][j];
                    if (!ConfigY[j+32])
                        ConfigY[j+32] = Matrix[i][j];
                }
            }
            if (i%5 == 3)
            {
                //RO 1, 16-33
                if (!ConfigX[(i/5)*2+1])
                    ConfigX[(i/5)*2+1] = Matrix[i][j];
                if (!ConfigY[j+16])
                    ConfigY[j+16] = Matrix[i][j];
            }
            if (i%5 == 4)
            {
                //RO 1, 0-15
                if (!ConfigX[(i/5)*2+1])
                    ConfigX[(i/5)*2+1] = Matrix[i][j];
                if (!ConfigY[j])
                    ConfigY[j] = Matrix[i][j];
            }
        }
    }
    ConfigToMatrix();
}

//Right click on Pixel Matrix, calculates which Cols/Rows in RamCell have to be deactivated
void HBselect::MatrixToConfig(int i, int j)
{
    if (i%5 == 0)
    {
        //RO 0, 24-39
        if (ConfigX[(i/5)*2])
            ConfigX[(i/5)*2] = Matrix[i][j];
        if (ConfigY[j+24])
            ConfigY[j+24] = Matrix[i][j];
    }
    if (i%5 == 1)
    {
        //RO 0, 8-23
        if (ConfigX[(i/5)*2])
            ConfigX[(i/5)*2] = Matrix[i][j];
        if (ConfigY[j+8])
            ConfigY[j+8] = Matrix[i][j];
    }
    if (i%5 == 2)
    {
        //RO 0, 0-8 & RO 1, 32-39
        if (j/8 == 1)
        {
            if (ConfigX[(i/5)*2])
                ConfigX[(i/5)*2] = Matrix[i][j];
            if (ConfigY[j-8])
                ConfigY[j-8] = Matrix[i][j];
        }
        else
        {
            if (!ConfigX[(i/5)*2+1])
                ConfigX[(i/5)*2+1] = Matrix[i][j];
            if (!ConfigY[j+32])
                ConfigY[j+32] = Matrix[i][j];
        }
    }
    if (i%5 == 3)
    {
        //RO 1, 16-33
        if (ConfigX[(i/5)*2+1])
            ConfigX[(i/5)*2+1] = Matrix[i][j];
        if (ConfigY[j+16])
            ConfigY[j+16] = Matrix[i][j];
    }
    if (i%5 == 4)
    {
        //RO 1, 0-15
        if (ConfigX[(i/5)*2+1])
            ConfigX[(i/5)*2+1] = Matrix[i][j];
        if (ConfigY[j])
            ConfigY[j] = Matrix[i][j];
    }
    ConfigToMatrix();
}

//Receives config from MainProgram
void HBselect::MainToHB(QVector<bool> HBConfig)
{
    for (int i = 0; i<60; i++)
    {
        ConfigX[i] = HBConfig[i];
    }
    for (int i = 0; i<40; i++)
    {
        ConfigY[i] = HBConfig[60+i];
    }
    ConfigToMatrix();
}

//Clears the GUI
void HBselect::on_ClearSelection_clicked()
{
    clear(1,1,1);
    this->repaint();
}

//Send selection to MainProgram
void HBselect::on_AcceptSelection_clicked()
{
    QVector<bool> HBConfig;
    for (int i = 0; i<60; i++)
    {
        HBConfig.push_back(ConfigX[i]);
    }
    for (int i = 0; i<40; i++)
    {
        HBConfig.push_back(ConfigY[i]);
    }
    emit HBToMain(HBConfig);
    this->hide();
}

//Close GUI
void HBselect::on_AcceptSelection_2_clicked()
{
    this->hide();
}

//Select all pixels
void HBselect::on_SelectAll_clicked()
{
    clear(1,1,0);
    this->repaint();
}
