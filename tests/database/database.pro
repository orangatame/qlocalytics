include(../../buildInfo.pri)

QT += qtestlib
CONFIG += qtestlib

include(../../libraryIncludes.pri)

DESTDIR = $${TESTS_DIRECTORY}/database
OBJECTS_DIR = $${TESTS_DIRECTORY}/database
MOC_DIR = $${TESTS_DIRECTORY}/database

SOURCES += testdatabase.cpp
