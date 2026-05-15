QT += core gui widgets multimedia
CONFIG += c++17 warn_on

TEMPLATE = app
TARGET = BigSmallFish

SOURCES += \
    main.cpp \
    fish.cpp \
    playerfish.cpp \
    enemyfish.cpp \
    gamewidget.cpp

HEADERS += \
    fish.h \
    playerfish.h \
    enemyfish.h \
    gamewidget.h

RESOURCES += skins.qrc
