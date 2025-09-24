TEMPLATE = app
CONFIG += console c++17
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += $$PWD/Descomprimir.cpp \
           $$PWD/../Desencriptado/desencriptacion.cpp \
           $$PWD/../Lectura_archivos.txt/lectoryalmacenatxt.cpp \
           $$PWD/../main.cpp

HEADERS += $$PWD/../Desencriptado/desencriptacion.h
