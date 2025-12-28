QT += quick core gui widgets

CONFIG += c++17

SOURCES += \
    main.cpp \
    gamecontroller.cpp \
    shiritorigame.cpp

HEADERS += \
    gamecontroller.h \
    shiritorigame.h

RESOURCES += resources.qrc

# Additional import path used to resolve QML modules
QML_IMPORT_PATH =

# Additional import path used to resolve QML modules
QML_DESIGNER_IMPORT_PATH =

# Default rules for deployment
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
