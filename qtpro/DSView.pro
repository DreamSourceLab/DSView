#-------------------------------------------------
#
# Project created by QtCreator 2013-07-24T17:32:25
#
#-------------------------------------------------

QT += core gui
CONFIG -= lib_bundle
CONFIG += app_bundle
macx {
QT += svg
}
CONFIG += exceptions
CONFIG += object_parallel_to_source

#QT += core5compat
#able to show console log on widnows
#CONFIG += console thread

CONFIG += c++11
QT += widgets

#QMAKE_CFLAGS_ISYSTEM = -I


TARGET = DSView
TEMPLATE = app

TRANSLATIONS = my_CN.ts

CONFIG += decoders
DEFINES += decoders

unix:!macx {
INCLUDEPATH += /usr/include/glib-2.0
INCLUDEPATH += /usr/lib/x86_64-linux-gnu/glib-2.0/include
INCLUDEPATH += /usr/include/libusb-1.0
INCLUDEPATH += /usr/include/boost
INCLUDEPATH += /usr/include/python3.8
INCLUDEPATH += ..

LIBS += /usr/lib/x86_64-linux-gnu/libglib-2.0.so
LIBS += /usr/lib/x86_64-linux-gnu/libusb-1.0.so
LIBS += /usr/lib/x86_64-linux-gnu/libboost_thread.so
LIBS += /usr/lib/x86_64-linux-gnu/libboost_system.so
LIBS += /usr/lib/x86_64-linux-gnu/libboost_filesystem.so
LIBS += /usr/lib/x86_64-linux-gnu/libpython3.8.so
LIBS += /usr/lib/x86_64-linux-gnu/libfftw3.so
LIBS += /usr/local/lib/libz.so.1.2.11
LIBS += /usr/local/lib/libzip.so.5.4
}

macx: {
INCLUDEPATH += /usr/local/include/
INCLUDEPATH += /usr/local/include/glib-2.0
INCLUDEPATH += /usr/local/lib/glib-2.0/include
INCLUDEPATH += /usr/local/include/libusb-1.0
INCLUDEPATH += /usr/local/include/boost
INCLUDEPATH += /Library/Frameworks/Python.framework/Versions/3.4/include/python3.4m
INCLUDEPATH += ..

#LIBS += -framework CoreFoundation
#LIBS += -framework CoreServices
#LIBS += /usr/lib/libiconv.2.dylib
#LIBS += /usr/local/lib/libpcre.a
#LIBS += /usr/local/opt/gettext/lib/libintl.a
#LIBS += /usr/lib/libz.1.dylib
#LIBS += /usr/lib/libobjc.dylib

LIBS += /usr/local/lib/libglib-2.0.dylib
LIBS += /usr/local/lib/libusb-1.0.dylib
LIBS += /usr/local/lib/libboost_atomic-mt.a
LIBS += /usr/local/lib/libboost_thread-mt.a
LIBS += /usr/local/lib/libboost_system-mt.a
LIBS += /usr/local/lib/libboost_filesystem-mt.a
LIBS += /Library/Frameworks/Python.framework/Versions/3.4/lib/libpython3.4.dylib
#LIBS += /opt/local/lib/libsetupapi.a
LIBS += /usr/local/lib/libfftw3.a
}

SOURCES += \
    ../DSView/main.cpp \
    ../DSView/pv/sigsession.cpp \
    ../DSView/pv/mainwindow.cpp \
    ../DSView/pv/devicemanager.cpp \
    ../DSView/pv/data/snapshot.cpp \
    ../DSView/pv/data/signaldata.cpp \
    ../DSView/pv/data/logicsnapshot.cpp \
    ../DSView/pv/data/logic.cpp \
    ../DSView/pv/data/analogsnapshot.cpp \
    ../DSView/pv/data/analog.cpp \
    ../DSView/pv/dialogs/deviceoptions.cpp \
    ../DSView/pv/prop/property.cpp \
    ../DSView/pv/prop/int.cpp \
    ../DSView/pv/prop/enum.cpp \
    ../DSView/pv/prop/double.cpp \
    ../DSView/pv/prop/bool.cpp \
    ../DSView/pv/prop/binding/binding.cpp \
    ../DSView/pv/toolbars/samplingbar.cpp \
    ../DSView/pv/view/viewport.cpp \
    ../DSView/pv/view/view.cpp \
    ../DSView/pv/view/timemarker.cpp \
    ../DSView/pv/view/signal.cpp \
    ../DSView/pv/view/ruler.cpp \
    ../DSView/pv/view/logicsignal.cpp \
    ../DSView/pv/view/header.cpp \
    ../DSView/pv/view/cursor.cpp \
    ../DSView/pv/view/analogsignal.cpp \
    ../DSView/pv/prop/binding/deviceoptions.cpp \
    ../DSView/pv/toolbars/trigbar.cpp \
    ../DSView/pv/toolbars/filebar.cpp \
    ../DSView/pv/dock/triggerdock.cpp \
    ../DSView/pv/dock/measuredock.cpp \
    ../DSView/pv/dock/searchdock.cpp \
    ../DSView/pv/toolbars/logobar.cpp \
    ../DSView/pv/data/groupsnapshot.cpp \
    ../DSView/pv/view/groupsignal.cpp \
    ../DSView/pv/data/group.cpp \
    ../DSView/pv/dialogs/about.cpp \
    ../DSView/pv/dialogs/search.cpp \
    ../DSView/pv/data/dsosnapshot.cpp \
    ../DSView/pv/data/dso.cpp \
    ../DSView/pv/view/dsosignal.cpp \
    ../DSView/pv/view/dsldial.cpp \
    ../DSView/pv/dock/dsotriggerdock.cpp \
    ../DSView/pv/view/trace.cpp \
    ../DSView/pv/view/selectableitem.cpp \
    ../DSView/pv/widgets/fakelineedit.cpp \
    ../DSView/pv/prop/string.cpp \
    ../DSView/pv/device/sessionfile.cpp \
    ../DSView/pv/device/inputfile.cpp \
    ../DSView/pv/device/file.cpp \
    ../DSView/pv/device/devinst.cpp \
    ../DSView/pv/dialogs/storeprogress.cpp \
    ../DSView/pv/storesession.cpp \
    ../DSView/pv/view/devmode.cpp \
    ../DSView/pv/device/device.cpp \
    ../DSView/pv/dialogs/waitingdialog.cpp \
    ../DSView/pv/dialogs/dsomeasure.cpp \
    ../DSView/pv/dialogs/calibration.cpp \
    ../DSView/pv/dialogs/fftoptions.cpp \
    ../DSView/dsapplication.cpp \
    ../DSView/pv/toolbars/titlebar.cpp \
    ../DSView/pv/mainframe.cpp \
    ../DSView/pv/widgets/border.cpp \
    ../DSView/pv/dialogs/dsmessagebox.cpp \
    ../DSView/pv/dialogs/shadow.cpp \
    ../DSView/pv/dialogs/dsdialog.cpp \
    ../DSView/pv/dialogs/interval.cpp \
    ../DSView/pv/prop/binding/probeoptions.cpp \
    ../DSView/pv/view/xcursor.cpp \
    ../DSView/pv/view/viewstatus.cpp \
    ../DSView/pv/dialogs/lissajousoptions.cpp \
    ../DSView/pv/view/lissajoustrace.cpp \
    ../DSView/pv/view/spectrumtrace.cpp \
    ../DSView/pv/data/spectrumstack.cpp \
    ../DSView/pv/view/mathtrace.cpp \
    ../DSView/pv/dialogs/mathoptions.cpp \
    ../DSView/pv/data/mathstack.cpp \
    ../DSView/pv/dialogs/regionoptions.cpp \
    ../DSView/pv/ZipMaker.cpp \
    ../DSView/pv/data/decode/AnnotationResTable.cpp \
    ../DSView/pv/ui/msgbox.cpp \
    ../DSView/pv/ui/dscombobox.cpp \
    ../DSView/pv/dock/protocolitemlayer.cpp \
    ../DSView/pv/config/appconfig.cpp \
    ../DSView/pv/dsvdef.cpp \
    ../DSView/pv/minizip/zip.c \
    ../DSView/pv/minizip/unzip.c \
    ../DSView/pv/minizip/ioapi.c \
    ../DSView/pv/dialogs/applicationpardlg.cpp \
    ../DSView/pv/appcontrol.cpp \
    ../DSView/pv/eventobject.cpp \
    ../DSView/pv/dstimer.cpp

HEADERS  += \
    ../DSView/pv/extdef.h \
    ../DSView/config.h \
    ../DSView/pv/sigsession.h \
    ../DSView/pv/mainwindow.h \
    ../DSView/pv/devicemanager.h \
    ../DSView/pv/data/snapshot.h \
    ../DSView/pv/data/signaldata.h \
    ../DSView/pv/data/logicsnapshot.h \
    ../DSView/pv/data/logic.h \
    ../DSView/pv/data/analogsnapshot.h \
    ../DSView/pv/data/analog.h \
    ../DSView/pv/dialogs/deviceoptions.h \
    ../DSView/pv/prop/property.h \
    ../DSView/pv/prop/int.h \
    ../DSView/pv/prop/enum.h \
    ../DSView/pv/prop/double.h \
    ../DSView/pv/prop/bool.h \
    ../DSView/pv/prop/binding/deviceoptions.h \
    ../DSView/pv/prop/binding/binding.h \
    ../DSView/pv/toolbars/samplingbar.h \
    ../DSView/pv/view/viewport.h \
    ../DSView/pv/view/view.h \
    ../DSView/pv/view/timemarker.h \
    ../DSView/pv/view/signal.h \
    ../DSView/pv/view/ruler.h \
    ../DSView/pv/view/logicsignal.h \
    ../DSView/pv/view/header.h \
    ../DSView/pv/view/cursor.h \
    ../DSView/pv/view/analogsignal.h \
    ../DSView/pv/toolbars/trigbar.h \
    ../DSView/pv/toolbars/filebar.h \
    ../DSView/pv/dock/triggerdock.h \
    ../DSView/pv/dock/measuredock.h \
    ../DSView/pv/dock/searchdock.h \
    ../DSView/pv/toolbars/logobar.h \
    ../DSView/pv/data/groupsnapshot.h \
    ../DSView/pv/view/groupsignal.h \
    ../DSView/pv/data/group.h \
    ../DSView/pv/dialogs/about.h \
    ../DSView/pv/dialogs/search.h \
    ../DSView/pv/data/dso.h \
    ../DSView/pv/data/dsosnapshot.h \
    ../DSView/pv/view/dsosignal.h \
    ../DSView/pv/view/dsldial.h \
    ../DSView/pv/dock/dsotriggerdock.h \
    ../DSView/pv/view/trace.h \
    ../DSView/pv/view/selectableitem.h \
    ../DSView/pv/widgets/fakelineedit.h \
    ../DSView/pv/prop/string.h \
    ../DSView/pv/device/sessionfile.h \
    ../DSView/pv/device/inputfile.h \
    ../DSView/pv/device/file.h \
    ../DSView/pv/device/devinst.h \
    ../DSView/pv/dialogs/storeprogress.h \
    ../DSView/pv/storesession.h \
    ../DSView/pv/view/devmode.h \
    ../DSView/pv/device/device.h \
    ../DSView/pv/dialogs/waitingdialog.h \
    ../DSView/pv/dialogs/dsomeasure.h \
    ../DSView/pv/dialogs/calibration.h \
    ../DSView/pv/dialogs/fftoptions.h \
    ../DSView/dsapplication.h \
    ../DSView/pv/toolbars/titlebar.h \
    ../DSView/pv/mainframe.h \
    ../DSView/pv/widgets/border.h \
    ../DSView/pv/dialogs/dsmessagebox.h \
    ../DSView/pv/dialogs/shadow.h \
    ../DSView/pv/dialogs/dsdialog.h \
    ../DSView/pv/dialogs/interval.h \
    ../DSView/config.h \
    ../libsigrok4DSL/config.h \
    ../DSView/pv/prop/binding/probeoptions.h \
    ../libsigrok4DSL/hardware/demo/demo.h \
    ../DSView/pv/view/xcursor.h \
    ../DSView/pv/view/viewstatus.h \
    ../DSView/pv/dialogs/lissajousoptions.h \
    ../DSView/pv/view/lissajoustrace.h \
    ../DSView/pv/view/spectrumtrace.h \
    ../DSView/pv/data/spectrumstack.h \
    ../DSView/pv/view/mathtrace.h \
    ../DSView/pv/dialogs/mathoptions.h \
    ../DSView/pv/data/mathstack.h \
    ../DSView/pv/dialogs/regionoptions.h \
    ../DSView/mystyle.h \
    ../DSView/pv/ZipMaker.h \
    ../DSView/pv/data/decode/AnnotationResTable.h \
    ../DSView/pv/ui/msgbox.h \
    ../DSView/pv/ui/dscombobox.h \
    ../DSView/pv/dock/protocolitemlayer.h \
    ../DSView/pv/config/appconfig.h \
    ../DSView/pv/dsvdef.h \
    ../DSView/pv/minizip/zip.h \
    ../DSView/pv/minizip/unzip.h \
    ../DSView/pv/minizip/ioapi.h \
    ../DSView/pv/dialogs/applicationpardlg.h \
    ../DSView/pv/appcontrol.h \
    ../DSView/pv/eventobject.h \
    ../DSView/pv/dstimer.h

SOURCES += \
    ../libsigrok4DSL/version.c \
    ../libsigrok4DSL/strutil.c \
    ../libsigrok4DSL/std.c \
    ../libsigrok4DSL/session_file.c \
    ../libsigrok4DSL/session_driver.c \
    ../libsigrok4DSL/session.c \
    ../libsigrok4DSL/log.c \
    ../libsigrok4DSL/hwdriver.c \
    ../libsigrok4DSL/error.c \
    ../libsigrok4DSL/backend.c \
    ../libsigrok4DSL/output/output.c \
    ../libsigrok4DSL/input/input.c \
    ../libsigrok4DSL/hardware/demo/demo.c \
    ../libsigrok4DSL/input/in_binary.c \
    ../libsigrok4DSL/input/in_vcd.c \
    ../libsigrok4DSL/input/in_wav.c \
    ../libsigrok4DSL/output/csv.c \
    ../libsigrok4DSL/output/gnuplot.c \
    ../libsigrok4DSL/output/srzip.c \
    ../libsigrok4DSL/output/vcd.c \
    ../libsigrok4DSL/hardware/DSL/dslogic.c \
    ../libsigrok4DSL/hardware/common/usb.c \
    ../libsigrok4DSL/hardware/common/ezusb.c \
    ../libsigrok4DSL/trigger.c \
    ../libsigrok4DSL/dsdevice.c \
    ../libsigrok4DSL/hardware/DSL/dscope.c \
    ../libsigrok4DSL/hardware/DSL/command.c \
    ../libsigrok4DSL/hardware/DSL/dsl.c

HEADERS  += \
    ../libsigrok4DSL/version.h \
    ../libsigrok4DSL/proto.h \
    ../libsigrok4DSL/libsigrok-internal.h \
    ../libsigrok4DSL/libsigrok.h \
    ../libsigrok4DSL/config.h \
    ../libsigrok4DSL/hardware/DSL/command.h \
    ../libsigrok4DSL/hardware/DSL/dsl.h

decoders {
DEFINES += ENABLE_DECODE
SOURCES += \
    ../DSView/pv/data/decoderstack.cpp \
    ../DSView/pv/data/decode/rowdata.cpp \
    ../DSView/pv/data/decode/row.cpp \
    ../DSView/pv/data/decode/decoder.cpp \
    ../DSView/pv/data/decode/annotation.cpp \
    ../DSView/pv/view/decodetrace.cpp \
    ../DSView/pv/prop/binding/decoderoptions.cpp \
    ../DSView/pv/dock/protocoldock.cpp \
    ../DSView/pv/dialogs/protocollist.cpp \
    ../DSView/pv/dialogs/protocolexp.cpp \
    ../DSView/pv/widgets/decodermenu.cpp \
    ../DSView/pv/widgets/decodergroupbox.cpp \
    ../DSView/pv/data/decodermodel.cpp

HEADERS  += \
    ../DSView/pv/data/decoderstack.h \
    ../DSView/pv/data/decode/rowdata.h \
    ../DSView/pv/data/decode/row.h \
    ../DSView/pv/data/decode/decoder.h \
    ../DSView/pv/data/decode/annotation.h \
    ../DSView/pv/view/decodetrace.h \
    ../DSView/pv/prop/binding/decoderoptions.h \
    ../DSView/pv/dock/protocoldock.h \
    ../DSView/pv/dialogs/protocollist.h \
    ../DSView/pv/dialogs/protocolexp.h \
    ../DSView/pv/widgets/decodermenu.h \
    ../DSView/pv/widgets/decodergroupbox.h \
    ../DSView/pv/data/decodermodel.h


#unix:!macx {
#}else{
SOURCES += \
    #../libsigrokdecode4DSL/type_logic.c \
    ../libsigrokdecode4DSL/type_decoder.c \
    ../libsigrokdecode4DSL/srd.c \
    ../libsigrokdecode4DSL/module_sigrokdecode.c \
    ../libsigrokdecode4DSL/decoder.c \
    ../libsigrokdecode4DSL/error.c \
    ../libsigrokdecode4DSL/exception.c \
    ../libsigrokdecode4DSL/instance.c \
    ../libsigrokdecode4DSL/log.c \
    ../libsigrokdecode4DSL/session.c \
    ../libsigrokdecode4DSL/util.c \
    ../libsigrokdecode4DSL/version.c

HEADERS  += \
    ../libsigrokdecode4DSL/libsigrokdecode-internal.h \
    ../libsigrokdecode4DSL/libsigrokdecode.h \
    ../libsigrokdecode4DSL/config.h \
    ../libsigrokdecode4DSL/version.h
#}
}


RESOURCES += \
    ../DSView/DSView.qrc \
    ../DSView/themes/breeze.qrc \
    language.qrc

ICON = DSView.icns

# make app logo on windows
#RC_FILE += ../applogo.rc

MOC_DIR  = ../../DSView_tmp/DSView_moc
RCC_DIR  = ../../DSView_tmp/RCC_DIR
UI_HEADERS_DIR  = ../../DSView_tmp/UI_HEADERS_DIR
UI_SOURCES_DIR = ../../DSView_tmp/UI_SOURCES_DIR
UI_DIR   = ../../DSView_tmp/UI_DIR
OBJECTS_DIR = ../../DSView_tmp/OBJECTS_DIR

DISTFILES += \
    ../applogo.rc
