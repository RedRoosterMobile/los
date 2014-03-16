//=========================================================
//  LOS
//  Libre Octave Studio
//  $Id: audio.h,v 1.25.2.13 2009/12/20 05:00:35 terminator356 Exp $
//
//  (C) Copyright 2001 Werner Schweer (ws@seh.de)
//=========================================================

#ifndef __AUDIO_H__
#define __AUDIO_H__

#include "thread.h"
#include "pos.h"
#include "mpevent.h"
#include "route.h"
#include "event.h"
#include <QList>

class MidiDevice;
class AudioDevice;
class Part;
class Event;
class MidiPlayEvent;
class Event;
class MidiPort;
class EventList;
class MidiInstrument;
class MidiTrack;

//---------------------------------------------------------
//   Msg
//---------------------------------------------------------

struct AudioMsg : public ThreadMsg
{ // this should be an union
    int serialNo;
    qint64 sid;
    Route sroute, droute;
    AudioDevice* device;
    int ival;
    int iival;
    double dval;
    Part* spart;
    Part* dpart;
    MidiTrack* track;

    const void *p1, *p2, *p3;
    Event ev1, ev2;
    char port, channel, ctrl;
    int a, b, c;
    Pos pos;
    QList<qint64> list;
    QList<Part*> plist;
    QList<void*> objectList;
};

//---------------------------------------------------------
//   AudioMsgId
//    this are the messages send from the GUI thread to
//    the midi thread
//---------------------------------------------------------

enum
{
    SEQM_ADD_TRACK, SEQM_REMOVE_TRACK, SEQM_CHANGE_TRACK, SEQM_MOVE_TRACK,
    SEQM_ADD_PART, SEQM_REMOVE_PART, SEQM_REMOVE_PART_LIST, SEQM_CHANGE_PART,
    SEQM_ADD_EVENT, SEQM_ADD_EVENT_CHECK, SEQM_REMOVE_EVENT, SEQM_CHANGE_EVENT,
    SEQM_ADD_TEMPO, SEQM_SET_TEMPO, SEQM_REMOVE_TEMPO, SEQM_REMOVE_TEMPO_RANGE, SEQM_ADD_SIG, SEQM_REMOVE_SIG,
    SEQM_SET_GLOBAL_TEMPO,
    SEQM_UNDO, SEQM_REDO,
    SEQM_RESET_DEVICES, SEQM_INIT_DEVICES, SEQM_PANIC,
    SEQM_MIDI_LOCAL_OFF,
    SEQM_SET_MIDI_DEVICE,
    SEQM_PLAY_MIDI_EVENT,
    SEQM_SET_HW_CTRL_STATE,
    SEQM_SET_HW_CTRL_STATES,
    SEQM_SET_TRACK_OUT_PORT,
    SEQM_SET_TRACK_OUT_CHAN,
    SEQM_SCAN_ALSA_MIDI_PORTS,
    SEQM_UPDATE_SOLO_STATES,
    AUDIO_RECORD,
    AUDIO_ROUTEADD, AUDIO_ROUTEREMOVE, AUDIO_REMOVEROUTES,
    AUDIO_VOL, AUDIO_PAN,
    AUDIO_SET_SEG_SIZE,
    AUDIO_SET_PREFADER, AUDIO_SET_CHANNELS,
    AUDIO_SWAP_CONTROLLER_IDX,
    AUDIO_CLEAR_CONTROLLER_EVENTS,
    AUDIO_SEEK_PREV_AC_EVENT,
    AUDIO_SEEK_NEXT_AC_EVENT,
    AUDIO_ERASE_AC_EVENT,
    AUDIO_ERASE_RANGE_AC_EVENTS,
    AUDIO_ADD_AC_EVENT,
    AUDIO_SET_SOLO,
    MS_PROCESS, MS_STOP, MS_SET_RTC, MS_UPDATE_POLL_FD,
    SEQM_IDLE, SEQM_SEEK, SEQM_PRELOAD_PROGRAM, SEQM_REMOVE_TRACK_GROUP
};

extern const char* seqMsgList[]; // for debug

//---------------------------------------------------------
//  Struct for controll preload processing
//---------------------------------------------------------
struct ProcessList {
    int port;
    int channel;
    int dataB;
};

//---------------------------------------------------------
//   Audio
//---------------------------------------------------------

class Audio
{
public:

    enum State
    {
        STOP, START_PLAY, PLAY, LOOP1, LOOP2, SYNC, PRECOUNT
    };

private:
    bool _running; // audio is active
    bool recording; // recording is active
    bool idle; // do nothing in idle mode
    bool _freewheel;
    bool _bounce;
    //bool loopPassed;
    unsigned _loopFrame; // Startframe of loop if in LOOP mode. Not quite the same as left marker !
    int _loopCount; // Number of times we have looped so far

    Pos _pos; // current play position

    unsigned curTickPos; // pos at start of frame during play/record
    unsigned nextTickPos; // pos at start of next frame during play/record

    double syncTime; // wall clock at last sync point
    unsigned syncFrame; // corresponding frame no. to syncTime
    int frameOffset; // offset to free running hw frame counter

    State state;
    AudioMsg* msg;

    int fromThreadFdw, fromThreadFdr; // message pipe

    int sigFd; // pipe fd for messages to gui

    // record values:
    Pos startRecordPos;
    Pos endRecordPos;

    void sendLocalOff();
    bool filterEvent(const MidiPlayEvent* event, int type, bool thru);

    void startRolling();
    void stopRolling();

    void panic();
    void processMsg(AudioMsg* msg);
    void process1(unsigned samplePos, unsigned offset, unsigned samples);

    void collectEvents(MidiTrack*, unsigned int startTick, unsigned int endTick);

public:
    Audio();

    virtual ~Audio()
    {
    }

    void process(unsigned frames);
    bool sync(int state, unsigned frame);
    void shutdown();

    // transport:
    bool start();
    void stop(bool);
    void seek(const Pos& pos);

    bool isPlaying() const
    {
        return state == PLAY || state == LOOP1 || state == LOOP2;
    }

    bool isRecording() const
    {
        return state == PLAY && recording;
    }

    void setRunning(bool val)
    {
        _running = val;
    }

    bool isRunning() const
    {
        return _running;
    }

    //-----------------------------------------
    //   message interface
    //-----------------------------------------

    void msgSeek(const Pos&);
    void msgPlay(bool val);

    void msgRemoveTrack(MidiTrack*, bool u = true);
    void msgRemoveTracks();
    void msgRemoveTrackGroup(QList<qint64>, bool undo = true);
    void msgChangeTrack(MidiTrack* oldTrack, MidiTrack* newTrack, bool u = true);
    void msgMoveTrack(int idx1, int dx2, bool u = true);
    void msgAddPart(Part*, bool u = true);
    void msgRemovePart(Part*, bool u = true);
    void msgRemoveParts(QList<Part*>, bool u = true);
    void msgChangePart(Part* oldPart, Part* newPart, bool u = true, bool doCtrls = true, bool doClones = false);
    //void msgAddEvent(Event&, Part*, bool u = true);
    void msgAddEvent(Event&, Part*, bool u = true, bool doCtrls = true, bool doClones = false, bool waitRead = true);
    void msgAddEventCheck(MidiTrack*, Event&, bool u = true, bool doCtrls = true, bool doClones = false, bool waitRead = true);
    //void msgDeleteEvent(Event&, Part*, bool u = true);
    void msgDeleteEvent(Event&, Part*, bool u = true, bool doCtrls = true, bool doClones = false, bool waitRead = true);
    //void msgChangeEvent(Event&, Event&, Part*, bool u = true);
    void msgChangeEvent(Event&, Event&, Part*, bool u = true, bool doCtrls = true, bool doClones = false, bool waitRead = true);
    void msgScanAlsaMidiPorts();
    void msgAddTempo(int tick, int tempo, bool doUndoFlag = true);
    void msgSetTempo(int tick, int tempo, bool doUndoFlag = true);
    void msgUpdateSoloStates();
    void msgSetGlobalTempo(int val);
    void msgDeleteTempo(int tick, int tempo, bool doUndoFlag = true);
    void msgDeleteTempoRange(QList<void*> tempo, bool doUndoFlag = true);
    void msgAddSig(int tick, int z, int n, bool doUndoFlag = true);
    void msgRemoveSig(int tick, int z, int n, bool doUndoFlag = true);
    void msgPanic();
    void sendMsg(AudioMsg*, bool waitRead = true);
    bool sendMessage(AudioMsg* m, bool doUndo, bool waitRead = true);
    void msgRemoveRoute(Route, Route);
    void msgRemoveRoute1(Route, Route);
    void msgRemoveRoutes(Route, Route); // p3.3.55
    void msgRemoveRoutes1(Route, Route); // p3.3.55
    void msgAddRoute(Route, Route);
    void msgAddRoute1(Route, Route);
    void msgSetSegSize(int, int);
    void msgUndo();
    void msgRedo();
    void msgLocalOff();
    void msgInitMidiDevices();
    void msgResetMidiDevices();
    void msgIdle(bool);
    void msgBounce();
    void msgSetSolo(MidiTrack*, bool);
    void msgSetHwCtrlState(MidiPort*, int, int, int);
    void msgSetHwCtrlStates(MidiPort*, int, int, int, int);
    void msgSetTrackOutChannel(MidiTrack*, int);
    void msgSetTrackOutPort(MidiTrack*, int);

    void msgPlayMidiEvent(const MidiPlayEvent* event);
    void msgPreloadCtrl();
    void rescanAlsaPorts();

    void midiPortsChanged();

    const Pos& pos() const
    {
        return _pos;
    }

    const Pos& getStartRecordPos() const
    {
        return startRecordPos;
    }

    const Pos& getEndRecordPos() const
    {
        return endRecordPos;
    }

    int loopCount()
    {
        return _loopCount;
    } // Number of times we have looped so far

    unsigned loopFrame()
    {
        return _loopFrame;
    }

    int tickPos() const
    {
        return curTickPos;
    }
    int timestamp() const;
    void processMidi();
    unsigned curFrame() const;
    void recordStop();
    void preloadControllers();

    bool freewheel() const
    {
        return _freewheel;
    }
    void setFreewheel(bool val);

    int getFrameOffset() const
    {
        return frameOffset;
    }
    void initDevices();

    void sendMsgToGui(char c);

    bool bounce() const
    {
        return _bounce;
    }
};

extern int processAudio(unsigned long, void*);
extern void processAudio1(void*, void*);

extern Audio* audio;
extern AudioDevice* audioDevice; // current audio device in use
#endif

