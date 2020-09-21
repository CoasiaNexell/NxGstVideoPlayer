#-------------------------------------------------
#
# Project created by QtCreator 2016-10-25T11:08:06
#
#-------------------------------------------------

QT       += core gui
QT       += network     \
            xml         \
            multimedia  \
            multimediawidgets \
            widgets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = NxGstVideoPlayer
TEMPLATE = app

# Add Playler Tool Library
LIBS += -L${SDKTARGETSYSROOT}/usr/lib/ -lnx_video_api -lnx_renderer
LIBS += -L$$PWD/../libnxgstvplayer/lib/32bit -lnxgstvplayer

# Add icu libraries
LIBS += -licuuc -licui18n

exists($(SDKTARGETSYSROOT)) {
    INCLUDEPATH += -I$(SDKTARGETSYSROOT)/usr/include/drm
    INCLUDEPATH += -I$(SDKTARGETSYSROOT)/usr/include
}

CONFIG += link_pkgconfig
PKGCONFIG += glib-2.0 gstreamer-1.0 gstreamer-pbutils-1.0 libdrm libudev alsa

INCLUDEPATH += ../libnxgstvplayer/include

SOURCES += main.cpp\
        mainwindow.cpp \
        CNX_GstMoviePlayer.cpp \
        NX_CFileList.cpp \
        playlistwindow.cpp \
        CNX_SubtitleParser.cpp \
        CNX_DrmInfo.cpp

HEADERS  += mainwindow.h \
        CNX_GstMoviePlayer.h \
        NX_CFileList.h \
        CNX_Util.h \
        playlistwindow.h \
        CNX_SubtitleParser.h \
        CNX_DrmInfo.h

# 'media' directory
SOURCES += \
        media/CNX_UeventManager.cpp

HEADERS += \
        media/CNX_UeventManager.h


FORMS    += mainwindow.ui \
            playlistwindow.ui
