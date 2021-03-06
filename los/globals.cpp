//=========================================================
//  LOS
//  Libre Octave Studio
//  $Id: globals.cpp,v 1.15.2.11 2009/11/25 09:09:43 terminator356 Exp $
//
//  (C) Copyright 1999-2004 Werner Schweer (ws@seh.de)
//=========================================================

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

#include <QActionGroup>
#include <QColor>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QPixmap>

#include "globals.h"
#include "config.h"
#include "TrackManager.h"

int sampleRate       = 44100;
unsigned segmentSize = 1024U; // segmentSize in frames (set by JACK)
unsigned fifoLength  = 128;   // 131072/segmentSize
                              // 131072 - magic number that gives a sufficient buffer size

QTimer* heartBeatTimer;

bool hIsB = true;             // call note h "b"

const signed char sharpTab[14][7] = {
      { 0, 3, -1, 2, 5, 1, 4 },
      { 0, 3, -1, 2, 5, 1, 4 },
      { 0, 3, -1, 2, 5, 1, 4 },
      { 0, 3, -1, 2, 5, 1, 4 },
      { 2, 5,  1, 4, 7, 3, 6 },
      { 2, 5,  1, 4, 7, 3, 6 },
      { 2, 5,  1, 4, 7, 3, 6 },
      { 4, 0,  3, -1, 2, 5, 1 },
      { 7, 3,  6, 2, 5, 1, 4 },
      { 5, 8,  4, 7, 3, 6, 2 },
      { 3, 6,  2, 5, 1, 4, 7 },
      { 1, 4,  0, 3, 6, 2, 5 },
      { 6, 2,  5, 1, 4, 0, 3 },
      { 0, 3, -1, 2, 5, 1, 4 },
      };
const signed char flatTab[14][7]  = {
      { 4, 1, 5, 2, 6, 3, 7 },
      { 4, 1, 5, 2, 6, 3, 7 },
      { 4, 1, 5, 2, 6, 3, 7 },
      { 4, 1, 5, 2, 6, 3, 7 },
      { 6, 3, 7, 4, 8, 5, 9 },
      { 6, 3, 7, 4, 8, 5, 9 },
      { 6, 3, 7, 4, 8, 5, 9 },

      { 1, 5, 2, 6, 3, 7, 4 },
      { 4, 1, 5, 2, 6, 3, 7 },
      { 2, 6, 3, 7, 4, 8, 5 },
      { 7, 4, 1, 5, 2, 6, 3 },
      { 5, 2, 6, 3, 7, 4, 8 },
      { 3, 0, 4, 1, 5, 2, 6 },
      { 4, 1, 5, 2, 6, 3, 7 },
      };

QString losGlobalLib;
QString losGlobalShare;
QString losUser;
QString losProject;
QString losProjectFile;
QString losProjectInitPath("./");
QString configPath = QString(getenv("HOME")) + QString("/.config/LOS");
QString configName = configPath + QString("/LOS-").append(VERSION).append(".cfg");
QString routePath = configPath + QString("/routes");
QString losInstruments;
QString losUserInstruments;
QString lastMidiPath(".");

bool debugMsg = false;
bool midiInputTrace = false;
bool midiOutputTrace = false;
bool realTimeScheduling = false;
int realTimePriority = 40;  // 80
int midiRTPrioOverride = -1;

const QStringList midi_file_pattern =
      QT_TRANSLATE_NOOP("@default",
      QString("Midi/Kar (*.mid *.MID *.kar *.KAR *.mid.gz *.mid.bz2);;"
              "Midi (*.mid *.MID *.mid.gz *.mid.bz2);;"
              "Karaoke (*.kar *.KAR *.kar.gz *.kar.bz2);;"
              "All Files (*)")).split(";;");

const QStringList midi_file_save_pattern =
      QT_TRANSLATE_NOOP("@default",
      QString("Midi (*.mid);;"
              "Karaoke (*.kar);;"
              "All Files (*)")).split(";;");

const QStringList med_file_pattern =
      QT_TRANSLATE_NOOP("@default",
      QString("los Files (*.los *.los.gz *.los.bz2);;"
              "Uncompressed los Files (*.los);;"
              "gzip compressed los Files (*.los.gz);;"
              "bzip2 compressed los Files (*.los.bz2);;"
              "All Files (*)")).split(";;");

const QStringList med_file_save_pattern =
      QT_TRANSLATE_NOOP("@default",
      QString("Uncompressed los Files (*.los);;"
              "gzip compressed los Files (*.los.gz);;"
              "bzip2 compressed los Files (*.los.bz2);;"
              "All Files (*)")).split(";;");

const QStringList image_file_pattern =
      QT_TRANSLATE_NOOP("@default",
      QString("(*.jpg *.gif *.png);;"
              "(*.jpg);;"
              "(*.gif);;"
              "(*.png);;"
              "All Files (*)")).split(";;");

const QStringList part_file_pattern =
      QT_TRANSLATE_NOOP("@default",
      QString("part Files (*.mpt);;"
              "All Files (*)")).split(";;");

const QStringList part_file_save_pattern =
      QT_TRANSLATE_NOOP("@default",
      QString("part Files (*.mpt);;"
              "All Files (*)")).split(";;");

const QStringList preset_file_pattern =
      QT_TRANSLATE_NOOP("@default",
      QString("Presets (*.pre *.pre.gz *.pre.bz2);;"
              "All Files (*)")).split(";;");

const QStringList preset_file_save_pattern =
      QT_TRANSLATE_NOOP("@default",
      QString("Presets (*.pre);;"
              "gzip compressed presets (*.pre.gz);;"
              "bzip2 compressed presets (*.pre.bz2);;"
              "All Files (*)")).split(";;");

const QStringList drum_map_file_pattern =
      QT_TRANSLATE_NOOP("@default",
      QString("Presets (*.map *.map.gz *.map.bz2);;"
              "All Files (*)")).split(";;");

const QStringList drum_map_file_save_pattern =
      QT_TRANSLATE_NOOP("@default",
      QString("Presets (*.map);;"
              "gzip compressed presets (*.map.gz);;"
              "bzip2 compressed presets (*.map.bz2);;"
              "All Files (*)")).split(";;");

const QStringList audio_file_pattern =
      QT_TRANSLATE_NOOP("@default",
      QString("Wave/Binary (*.wav *.ogg *.bin);;"
              "Wave (*.wav *.ogg);;"
              "Binary (*.bin);;"
              "All Files (*)")).split(";;");

Qt::KeyboardModifiers globalKeyState;

// Midi Filter Parameter
int midiInputPorts   = 0;    // receive from all devices
int midiInputChannel = 0;    // receive all channel
int midiRecordType   = 0;    // receive all events
int midiThruType = 0;        // transmit all events
int midiFilterCtrl1 = 0;
int midiFilterCtrl2 = 0;
int midiFilterCtrl3 = 0;
int midiFilterCtrl4 = 0;

QActionGroup* undoRedo;
QAction* undoAction;
QAction* redoAction;
QActionGroup* transportAction;
QAction* playAction;
QAction* startAction;
QAction* stopAction;
QAction* rewindAction;
QAction* forwardAction;
QAction* loopAction;
QAction* replayAction;
QAction* punchinAction;
QAction* punchoutAction;
QAction* recordAction;
QAction* panicAction;
QAction* feedbackAction;
QAction* pcloaderAction;
QAction* noteAlphaAction;
QAction* multiPartSelectionAction;
QAction* masterEnableAction;

LOS* los;

bool rcEnable = false;
unsigned char rcStopNote = 28;
unsigned char rcRecordNote = 31;
unsigned char rcGotoLeftMarkNote = 33;
unsigned char rcPlayNote = 29;

QObject* gRoutingPopupMenuMaster = 0;
RouteMenuMap gRoutingMenuMap;
bool gIsOutRoutingPopupMenu = false;

bool midiSeqRunning = false;

int lastTrackPartColorIndex = 1;

QList<QIcon> partColorIcons;
QList<QIcon> partColorIconsSelected;
QHash<int, QColor> g_trackColorListLine;
QHash<int, QColor> g_trackColorList;
QHash<int, QColor> g_trackColorListSelected;
QHash<int, QPixmap> g_trackDragImageList;
int vuColorStrip = 0; //default vuColor is gradient
TrackManager* trackManager;

QList<QPair<int, QString> > gInputList;
QList<int> gInputListPorts;
