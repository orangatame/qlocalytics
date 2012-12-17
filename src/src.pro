QLOCALYTICS_BASE = ..
QLOCALYTICS_SRCBASE = .

TEMPLATE = lib

QT -= gui
QT += sql network
TARGET = qlocalytics
DESTDIR = $$QLOCALYTICS_BASE/lib
#CONFIG += create_prl # ???
LIBS += -lz
VERSION = 0.0.1

QLOCALYTICS_CPP = $$QLOCALYTICS_SRCBASE

INCLUDEPATH += $$QLOCALYTICS_CPP


PUBLIC_HEADERS += \
  localyticsdatabase.h \
  localyticssession.h \
  localyticsuploader.h \
  webserviceconstants.h

HEADERS += $$PRIVATE_HEADERS $$PUBLIC_HEADERS

SOURCES += \
  localyticsdatabase.cpp \
  localyticssession.cpp \
  localyticsuploader.cpp

