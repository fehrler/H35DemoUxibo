#include "geburtstag.h"
#include "ui_geburtstag.h"

#include <QPainter>

#include <iostream>
//#include <QMouseEvent>
#include <QColor>
#include "mainwindow.h"
#include <vector>
#include <qvector.h>
#include <fstream>

//Painting Parameters
#define widthA 20    //Pixel width
#define heightA 20  //Pixel height
#define xnull 10    //origin x
#define ynull 10    //origin y

Geburtstag::Geburtstag(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Geburtstag)
{
    ui->setupUi(this);
}

Geburtstag::~Geburtstag()
{
    delete ui;
}


void Geburtstag::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);

    //Columns 0 - 149:
    painter.fillRect(xnull-5,ynull-5,widthA*75+10, heightA*32+10,  QBrush(Qt::white));

    //draw active pixels
    for(int i=0;i<75;++i)
        for(int j=0;j<32;++j)
            if(pixels[75*(j>=16)+i][j%16])
                painter.fillRect(xnull+1+widthA*i, ynull+1+heightA*j, widthA-1, heightA-1, QBrush(Qt::red));

    //Draw Matrix outlines
    for (int i = 0; i < 76; i++)
    {
        painter.drawLine(xnull+(i*widthA), ynull, xnull+(i*widthA), ynull+heightA*32);
    }
    for (int i = 0; i < 33; i++)
    {
        painter.drawLine(xnull, ynull+(i*heightA), xnull+(75*widthA), ynull+(i*heightA));
    }

}

void Geburtstag::on_pushButton_clicked()
{
    on_ClearButton_clicked();
    std::fstream f;
    f.open((ui->LogoSelect->currentText().toStdString() + ".txt").c_str(), std::ios::in);

    QVector<std::pair<unsigned int, unsigned int> > activepixels;

    int x, y;

    while(!f.eof())
    {
        f >> y >> x;


        if(f.eof())
            break;

        activepixels.push_back(std::make_pair<unsigned int, unsigned int>(x+((y >=16)?75:0),y%16));
    }

    f.close();

    std::cout << "Number of Pixels to test: " << activepixels.size() << std::endl;

    emit GeburtstagtoMain(activepixels);
}

void Geburtstag::on_ClearButton_clicked()
{
    for(int i=0;i<150;++i)
        for(int j=0;j<16;++j)
            pixels[i][j] = false;

    this->repaint();
}

void Geburtstag::MainToGeburtstag(QVector<std::pair<unsigned int, unsigned int> > hitpixels)
{
    for(auto it: hitpixels)
        pixels[it.first][it.second] = true;

    this->repaint();
}
