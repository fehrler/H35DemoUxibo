#-------------------------------------------------
#
# Project created by QtCreator 2015-03-16T16:04:09
#
#-------------------------------------------------

QT          += core gui
QT          += multimedia

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets printsupport

TARGET = H35Demo
TEMPLATE = app

QMAKE_CXXFLAGS += -std=c++11

SOURCES += main.cpp\
        mainwindow.cpp \
    libs/ftdi.cpp \
    libs/func.cpp \
    libs/geniobase.cpp \
    libs/HaCOM.cpp \
    libs/highlevel.cpp \
    libs/plot.cpp \
    libs/qcustomplot.cpp \
    libs/nexysio.cpp \
    hbselect.cpp \
    digselect.cpp \
    geburtstag.cpp \
    displayreadout.cpp \
    alglib/alglibinternal.cpp \
    alglib/alglibmisc.cpp \
    alglib/ap.cpp \
    alglib/dataanalysis.cpp \
    alglib/diffequations.cpp \
    alglib/fasttransforms.cpp \
    alglib/integration.cpp \
    alglib/interpolation.cpp \
    alglib/linalg.cpp \
    alglib/optimization.cpp \
    alglib/solvers.cpp \
    alglib/specialfunctions.cpp \
    alglib/statistics.cpp

HEADERS  += mainwindow.h \
    libs/geniobase.h \
    libs/ftdi.h \
    libs/func.h \
    libs/geniobase.h \
    libs/HaCOM.h \
    libs/highlevel.h \
    libs/plot.h \
    libs/qcustomplot.h \
    ftd2xx.h \
    libs/nexysio.h \
    hbselect.h \
    digselect.h \
    geburtstag.h \
    displayreadout.h \
    alglib/alglibinternal.h \
    alglib/alglibmisc.h \
    alglib/ap.h \
    alglib/dataanalysis.h \
    alglib/diffequations.h \
    alglib/fasttransforms.h \
    alglib/integration.h \
    alglib/interpolation.h \
    alglib/linalg.h \
    alglib/optimization.h \
    alglib/solvers.h \
    alglib/specialfunctions.h \
    alglib/statistics.h \
    alglib/stdafx.h
FORMS    += mainwindow.ui \
    hbselect.ui \
    digselect.ui \
    geburtstag.ui \
    displayreadout.ui

LIBS += -LC:\github\H35DemoUxibo -lftd2xx
LIBS     += "C:/Windows/System32/visa32.dll"

INCLUDEPATH += "C:\Program Files\IVI Foundation\VISA\Win64\Include"
QMAKE_CXXFLAGS += -pthread -fopenmp -Wunused-parameter

DISTFILES +=
