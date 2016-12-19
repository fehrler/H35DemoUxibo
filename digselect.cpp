#include "digselect.h"
#include "ui_digselect.h"

#include <QPainter>

#include <iostream>
#include <QMouseEvent>
#include <QColor>
#include "mainwindow.h"
#include <vector>
#include <qvector.h>

//Painting Parameters
#define widthA 6    //Pixel width
#define heightA 15  //Pixel height
#define xnull 80    //origin x
#define ynull 10    //origin y

DigSelect::DigSelect(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::DigSelect)
{
    ui->setupUi(this);

    ui->label->setGeometry(10,ynull+8*heightA-10,60,13);
    ui->label_2->setGeometry(10,ynull+8*heightA+10,60,13);

    ui->label_3->setGeometry(10,ynull+heightA*25-10, 60, 13);
    ui->label_4->setGeometry(10,ynull+heightA*25+10, 60, 13);

}

DigSelect::~DigSelect()
{
    delete ui;
}

void DigSelect::paintEvent(QPaintEvent *event)
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

void DigSelect::mousePressEvent(QMouseEvent *MouseEvent)
{
    QPoint mousepos =  MouseEvent->pos();
    int xpos = mousepos.rx();
    int ypos = mousepos.ry();

    //upper half
    if(ypos > ynull && ypos < ynull+heightA*16
                && xpos > xnull && xpos < xnull+widthA*150)
    {
        xpos -= xnull;
        ypos -= ynull;
    }
    //lower half
    else if(ypos > ynull+heightA*17+10 && ypos < ynull+heightA*(32+1)+10
            && xpos > xnull && xpos < xnull+widthA*150)
    {
        xpos -= xnull - widthA*150;
        ypos -= ynull + heightA*17+10;
    }
    else
        return;


    if (MouseEvent->button() ==  Qt::LeftButton)
        pixels[(int)(xpos/widthA)][(int)(ypos/heightA)] = true;
    else if (MouseEvent->button() ==  Qt::RightButton)
        pixels[(int)(xpos/widthA)][(int)(ypos/heightA)] = false;

    this->repaint();
}

void DigSelect::MainToDigSel(QVector<std::pair<unsigned int, unsigned int> > activepixels)
{
    std::cout << "Got Length: " << activepixels.length() << std::endl;

    for(int i=0;i<300;++i)
        for(int j=0;j<16;++j)
            pixels[i][j] = false;

    for(auto it: activepixels)
        pixels[it.first][it.second] = true;
}

void DigSelect::on_selAll_clicked()
{
    for(int i=0;i<300;++i)
        for(int j=0;j<16;++j)
            pixels[i][j] = true;

    this->repaint();
}

void DigSelect::on_selNone_clicked()
{
    for(int i=0;i<300;++i)
        for(int j=0;j<16;++j)
            pixels[i][j] = false;

    this->repaint();

}

void DigSelect::on_discardButton_clicked()
{
    this->hide();
}

void DigSelect::on_acceptButton_clicked()
{
    QVector<std::pair<unsigned int, unsigned int> >  activepixels;
    for (int i = 0; i<300; i++)
        for(int j=0;j<16;++j)
            if(pixels[i][j])
                activepixels.push_back(std::make_pair<unsigned int, unsigned int>(i,j));

    for(auto it: activepixels)
        std::cout << "(" << it.first << " | " << it.second << ")" << std::endl;

    emit DigSelToMain(activepixels);
    this->hide();
}

void DigSelect::on_selLeft_clicked()
{
    for(int i=0;i<150;++i)
        for(int j=0;j<16;++j)
            pixels[i][j] = true;

    this->repaint();
}

void DigSelect::on_selRight_clicked()
{
    for(int i=150;i<300;++i)
        for(int j=0;j<16;++j)
            pixels[i][j] = true;

    this->repaint();
}
