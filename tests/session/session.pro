include(../../buildInfo.pri)

QT += qtestlib
CONFIG += qtestlib

include(../../libraryIncludes.pri)

DESTDIR = $${TESTS_DIRECTORY}/session
OBJECTS_DIR = $${TESTS_DIRECTORY}/session
MOC_DIR = $${TESTS_DIRECTORY}/session

SOURCES += testsession.cpp
