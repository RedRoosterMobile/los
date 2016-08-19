//=========================================================
//  LOS
//  Libre Octave Studio
//
//  (C) Copyright 1999/2000 Werner Schweer (ws@seh.de)
//  (C) Copyright 2011-2012 The Open Octave Project <info@openoctave.org>
//=========================================================

#ifndef __SONG_H__
#define __SONG_H__

#include <QObject>
#include <QStringList>
#include <QHash>

#include "pos.h"
#include "globaldefs.hpp"
#include "tempo.h"
#include "sig.h"
#include "undo.h"
#include "track.h"
#include "trackview.h"

class QAction;
class QFont;
class QMenu;
class QMessageBox;
class QUndoStack;

struct MidiMsg;
struct AudioMsg;
class Event;
class Xml;
class Sequencer;
class Part;
class MidiPart;
class PartList;
class MPEventList;
class EventList;
class MarkerList;
class Marker;
class SNode;
class LOSCommand;

class MidiPort;
class MidiDevice;
class AudioPort;
class AudioDevice;

#define SC_TRACK_INSERTED     1
#define SC_TRACK_REMOVED      2
#define SC_TRACK_MODIFIED     4
#define SC_TRACK_INSTRUMENT   5
#define SC_PART_INSERTED      8
#define SC_PART_REMOVED       0x10
#define SC_PART_MODIFIED      0x20
#define SC_PART_COLOR_MODIFIED      0x30
#define SC_EVENT_INSERTED     0x40
#define SC_EVENT_REMOVED      0x80
#define SC_EVENT_MODIFIED     0x100
#define SC_SIG                0x200       // timing signature
#define SC_TEMPO              0x400       // tempo map changed
#define SC_MASTER             0x800       // master flag changed
#define SC_SELECTION          0x1000
#define SC_MIDI_CONTROLLER    0x2000      // must update midi mixer
#define SC_MUTE               0x4000
#define SC_SOLO               0x8000
#define SC_RECFLAG            0x10000
#define SC_ROUTE              0x20000
#define SC_CHANNELS           0x40000
#define SC_CONFIG             0x80000     // midiPort-midiDevice
#define SC_DRUMMAP            0x100000    // must update drumeditor
#define SC_MIXER_VOLUME       0x200000
#define SC_MIXER_PAN          0x400000
#define SC_AUTOMATION         0x800000
#define SC_CLIP_MODIFIED      0x4000000
#define SC_MIDI_CONTROLLER_ADD 0x8000000   // a hardware midi controller was added or deleted
#define SC_MIDI_TRACK_PROP    0x10000000   // a midi track's properties changed (channel, compression etc)
#define SC_SONG_TYPE          0x20000000   // the midi song type (mtype) changed
#define SC_TRACKVIEW_INSERTED 0x30000000
#define SC_TRACKVIEW_REMOVED  0x40000000
#define SC_TRACKVIEW_MODIFIED 0x50000000
#define SC_PATCH_UPDATED      0X60000000
#define SC_VIEW_CHANGED		   0X70000000
#define SC_VIEW_ADDED		   0X80000000
#define SC_VIEW_DELETED		   0X90000000

/*
enum
{
    SC_TRACK_INSERTED = 1, SC_TRACK_REMOVED, SC_TRACK_MODIFIED,
    SC_PART_INSERTED, SC_PART_REMOVED, SC_PART_MODIFIED,
    SC_PART_COLOR_MODIFIED, SC_EVENT_INSERTED,SC_EVENT_REMOVED,
    SC_EVENT_MODIFIED, SC_SIG, SC_TEMPO, SC_MASTER,  SC_SELECTION,
    SC_MIDI_CONTROLLER, SC_MUTE, SC_SOLO, SC_RECFLAG, SC_ROUTE,
    SC_CHANNELS, SC_CONFIG, SC_DRUMMAP, SC_MIXER_VOLUME, SC_MIXER_PAN,
    SC_AUTOMATION, SC_CLIP_MODIFIED, SC_MIDI_CONTROLLER_ADD,
    SC_MIDI_TRACK_PROP, SC_SONG_TYPE, SC_TRACKVIEW_INSERTED, SC_TRACKVIEW_REMOVED,
    SC_TRACKVIEW_MODIFIED, SC_PATCH_UPDATED, SC_VIEW_CHANGED, SC_VIEW_ADDED,
    SC_VIEW_DELETED, SC_TRACK_INSTRUMENT
};
*/

#define REC_NOTE_FIFO_SIZE    16

//---------------------------------------------------------
//    Song
//---------------------------------------------------------

class Song : public QObject {

    Q_OBJECT

public:
    enum POS {
        CPOS = 0, LPOS, RPOS
    };

    enum FollowMode {
        NO, JUMP, CONTINUOUS
    };

    enum {
        REC_OVERDUP, REC_REPLACE
    };

    enum {
        CYCLE_NORMAL, CYCLE_MIX, CYCLE_REPLACE
    };

    enum {
        MARKER_CUR, MARKER_ADD, MARKER_REMOVE, MARKER_NAME,
        MARKER_TICK, MARKER_LOCK
    };
    static void movePlaybackToPart(Part*);

private:
    // fifo for note-on events
    //    - this events are read by the heart beat interrupt
    //    - used for single step recording in midi editors

    int recNoteFifo[REC_NOTE_FIFO_SIZE];
    volatile int noteFifoSize;
    int noteFifoWindex;
    int noteFifoRindex;

    int updateFlags;

    QHash<qint64, MidiTrack*> m_tracks; //New indexed list of tracks
    QHash<qint64, MidiTrack*> m_arrangerTracks;
    QHash<qint64, MidiTrack*> m_viewTracks;

    qint64 m_workingViewId;
    qint64 m_inputViewId;
    qint64 m_outputViewId;
    qint64 m_commentViewId;

    //For maintaining the track order and track view order
    QList<qint64> m_trackIndex;
    QList<qint64> m_arrangerTrackIndex;
    QList<qint64> m_trackViewIndex;
    QList<qint64> m_autoTrackViewIndex;

    //This list forms a compatibility layer between the old name based
    //track system and should be removed when all things use track ID
    //At that time track will be able to have the same name without a problem
    QStringList m_tracknames;

    MidiTrackList _tracks; // tracklist as seen by globally
    MidiTrackList _artracks; // tracklist as seen by Arranger

    TrackViewList _tviews; // trackviewlist as seen by Arranger
    TrackViewList _autotviews;

    MidiTrackList _midis;
    MidiTrackList _viewtracks;

    UndoList* undoList;
    UndoList* redoList;
    QUndoStack* m_undoStack;
    Pos pos[3];
    Pos _vcpos; // virtual CPOS (locate in progress)
    MarkerList* _markerList;

    bool _masterFlag;
    bool loopFlag;
    bool punchinFlag;
    bool punchoutFlag;
    bool recordFlag;
    bool soloFlag;
    MidiType _mtype;
    int _recMode;
    int _cycleMode;
    //bool _click;
    bool _quantize;
    int _arrangerRaster; // Used for audio rec new part snapping. Set by Arranger snap combo box.
    unsigned _len; // song len in ticks
    FollowMode _follow;
    int _globalPitchShift;
    void readMarker(Xml&);

    QString songInfoStr; // contains user supplied song information, stored in song file.
    QStringList deliveredScriptNames;
    QStringList userScriptNames;
    QMessageBox *jackErrorBox;

    //Audition feature
    bool _replay;
    unsigned _replayPos;

    //key to note mapping
    QHash<int, QString> m_midiKeys;

public:
    Song(QUndoStack *stack, const char* name = 0);
    ~Song();

    void putEvent(int pv);
    void endMsgCmd();
    void processMsg(AudioMsg* msg);
    void pushToHistoryStack(LOSCommand* cmd);
    void undoFromQtUndoStack();
    void redoFromQtUndoStack();

    void setMidiType(MidiType t);

    MidiType midiType() const {
        return _mtype;
    }

    void setFollow(FollowMode m) {
        _follow = m;
    }

    FollowMode follow() const {
        return _follow;
    }

    bool dirty;
    bool invalid; //used to deturmin is song is valid
    bool viewselected;
    bool hasSelectedParts;
    QString associatedRoute;
    void updatePos();

    void read(Xml&);
    void write(int, Xml&) const;
    void writeFont(int level, Xml& xml, const char* name,
            const QFont& font) const;
    QFont readFont(Xml& xml, const char* name);

    QString getSongInfo() {
        return songInfoStr;
    }

    void setSongInfo(QString info) {
        if(songInfoStr != info)
            dirty = true;
        songInfoStr = info;
    }

    void clear(bool signal);
    void update(int flags = -1);
    void cleanupForQuit();

    int globalPitchShift() const {
        return _globalPitchShift;
    }

    void setGlobalPitchShift(int val) {
        _globalPitchShift = val;
    }

    //-----------------------------------------
    //   Marker
    //-----------------------------------------

    MarkerList* marker() const {
        return _markerList;
    }
    Marker* addMarker(const QString& s, int t, bool lck);
    Marker* getMarkerAt(int t);
    void removeMarker(Marker*);
    Marker* setMarkerName(Marker*, const QString&);
    Marker* setMarkerTick(Marker*, int);
    Marker* setMarkerLock(Marker*, bool);
    void setMarkerCurrent(Marker* m, bool f);

    //-----------------------------------------
    //   transport
    //-----------------------------------------

    void setPos(int, const Pos&, bool sig = true, bool isSeek = true,
            bool adjustScrollbar = false);

    const Pos& cPos() const {
        return pos[0];
    }

    const Pos& lPos() const {
        return pos[1];
    }

    const Pos& rPos() const {
        return pos[2];
    }

    unsigned cpos() const {
        return pos[0].tick();
    }

    unsigned vcpos() const {
        return _vcpos.tick();
    }

    const Pos& vcPos() const {
        return _vcpos;
    }

    unsigned lpos() const {
        return pos[1].tick();
    }

    unsigned rpos() const {
        return pos[2].tick();
    }

    bool loop() const {
        return loopFlag;
    }

    bool record() const {
        return recordFlag;
    }

    bool punchin() const {
        return punchinFlag;
    }

    bool punchout() const {
        return punchoutFlag;
    }

    bool masterFlag() const {
        return _masterFlag;
    }

    void setRecMode(int val) {
        _recMode = val;
    }

    int recMode() const {
        return _recMode;
    }

    void setCycleMode(int val) {
        _cycleMode = val;
    }

    int cycleMode() const {
        return _cycleMode;
    }

//    bool click() const {
//        return _click;
//    }

    bool quantize() const {
        return _quantize;
    }
    void setStopPlay(bool);
    void stopRolling();
    void abortRolling();

    //-----------------------------------------
    //    access tempomap/sigmap  (Mastertrack)
    //-----------------------------------------

    unsigned len() const {
        return _len;
    }
    void setLen(unsigned l); // set songlen in ticks
    int roundUpBar(int tick) const;
    int roundUpBeat(int tick) const;
    int roundDownBar(int tick) const;
    void initLen();
    void tempoChanged();

    //-----------------------------------------
    //   event manipulations
    //-----------------------------------------

    void cmdAddRecordedEvents(MidiTrack*, EventList*, unsigned);
    bool addEvent(Event&, Part*);
    void changeEvent(Event&, Event&, Part*);
    void deleteEvent(Event&, Part*);

    //-----------------------------------------
    //   part manipulations
    //-----------------------------------------

    void cmdResizePart(Track* t, Part* p, unsigned int size);
    void cmdSplitPart(Track* t, Part* p, int tick);
    void cmdGluePart(Track* t, Part* p);

    void addPart(Part* part);
    void removePart(Part* part);
    void changePart(Part*, Part*);
    PartList* getSelectedMidiParts() const;
    bool msgRemoveParts();

    //void cmdChangePart(Part* oldPart, Part* newPart);
    void cmdChangePart(Part* oldPart, Part* newPart, bool doCtrls, bool doClones);
    void cmdRemovePart(Part* part);
    void cmdAddPart(Part* part);

    int arrangerRaster() {
        return _arrangerRaster;
    } // Used by Song::cmdAddRecordedWave to snap new wave parts

    void setArrangerRaster(int r) {
        _arrangerRaster = r;
    } // Used by Arranger snap combo box

    //-----------------------------------------
    //   track manipulations
    //-----------------------------------------

    QStringList *trackNames()
    {
        return &m_tracknames;
    }
    MidiTrackList* tracks() {
        return &_tracks;
    }

    MidiTrackList* artracks() {
        return &_artracks;
    }

    MidiTrackList* visibletracks() {
            return &_viewtracks;
    }

    QList<qint64> selectedTracks()
    {
        QList<qint64> list;
        foreach(Track* t, m_viewTracks)
        {
            if(t->selected())
                list.append(t->id());
        }
        return list;
    }
    QList<Part*> selectedParts();

    MidiTrackList getSelectedTracks();
    void setTrackHeights(MidiTrackList& list, int height);

    MidiTrackList* midis() {
        return &_midis;
    }

    void cmdRemoveTrack(MidiTrack* track);
    void removeTrack(MidiTrack* track);
    void removeTrack1(MidiTrack* track);
    void removeTrackRealtime(MidiTrack* track);
    void removeMarkedTracks();
    void changeTrack(MidiTrack* oldTrack, MidiTrack* newTrack);
    MidiTrack* findTrack(const Part* part) const;
    MidiTrack* findTrack(const QString& name) const;
    MidiTrack* findTrackById(qint64 id) const;
    void swapTracks(int i1, int i2);
    void setChannelMute(int channel, bool flag);
    void setRecordFlag(MidiTrack*, bool, bool monitor = false);
    void insertTrack(MidiTrack*, int idx);
    void insertTrack1(MidiTrack*, int idx);
    void insertTrackRealtime(MidiTrack*, int idx);
    void deselectTracks();
    void deselectAllParts();
    void disarmAllTracks();
    void readRoute(Xml& xml);
    void recordEvent(MidiTrack*, Event&);
    void recordEvent(MidiPart*, Event&);
    void msgInsertTrack(MidiTrack* track, int idx, bool u = true);
    void updateSoloStates();

    // TrackView

    TrackViewList* trackviews() {
        return &_tviews;
    }
    TrackViewList* autoviews() {
        return &_autotviews;
    }
    QList<qint64>* autoTrackViewIndexList()
    {
        return &m_autoTrackViewIndex;
    }
    int autoTrackViewIndex(qint64 id)
    {
        return m_autoTrackViewIndex.isEmpty() ? -1 : m_autoTrackViewIndex.indexOf(id);
    }
    QList<qint64>* trackViewIndexList()
    {
        return &m_trackViewIndex;
    }
    int trackViewIndex(qint64 id)
    {
        return m_trackViewIndex.isEmpty() ? -1 : m_trackViewIndex.indexOf(id);
    }
    QList<qint64>* trackIndexList()
    {
        return &m_trackIndex;
    }
    int trackIndex(qint64 id)
    {
        return m_trackIndex.isEmpty() ? -1 : m_trackIndex.indexOf(id);
    }
    qint64 workingViewId()
    {
        return m_workingViewId;
    }
    qint64 inputViewId()
    {
        return m_inputViewId;
    }
    qint64 outputViewId()
    {
        return m_outputViewId;
    }
    qint64 commentViewId()
    {
        return m_commentViewId;
    }

    TrackView* addNewTrackView();
    TrackView* addTrackView();

    TrackView* findAutoTrackView(const QString& name) const;
    TrackView* findAutoTrackViewById(qint64) const;

    TrackView* findTrackViewById(qint64) const;
    TrackView* findTrackViewByTrackId(qint64);

    void insertTrackView(TrackView*, int idx);
    void removeTrackView(qint64);
    void cmdRemoveTrackView(qint64);
    void msgInsertTrackView(TrackView*, int idx, bool u = true);

    //midikeys
    QString key2note(int key)
    {
        if(m_midiKeys.contains(key))
        {
            return m_midiKeys.value(key);
        }
        return QString("");
    }

    //-----------------------------------------
    //   undo, redo
    //-----------------------------------------

    void startUndo();
    void endUndo(int);
    //void undoOp(UndoOp::UndoType, Track* oTrack, Track* nTrack);
    void undoOp(UndoOp::UndoType, int n, MidiTrack* oTrack, MidiTrack* nTrack);
    void undoOp(UndoOp::UndoType, int, MidiTrack*);
    void undoOp(UndoOp::UndoType, int, int, int = 0);
    void undoOp(UndoOp::UndoType, Part*);
    void undoOp(UndoOp::UndoType, LOSCommand*);
    //void undoOp(UndoOp::UndoType, Event& nevent, Part*);
    void undoOp(UndoOp::UndoType, Event& nevent, Part*, bool doCtrls, bool doClones);
    //void undoOp(UndoOp::UndoType, Event& oevent, Event& nevent, Part*);
    void undoOp(UndoOp::UndoType, Event& oevent, Event& nevent, Part*, bool doCtrls, bool doClones);
    void undoOp(UndoOp::UndoType, SigEvent* oevent, SigEvent* nevent);
    void undoOp(UndoOp::UndoType, int channel, int ctrl, int oval, int nval);
    //void undoOp(UndoOp::UndoType, Part* oPart, Part* nPart);
    void undoOp(UndoOp::UndoType, Part* oPart, Part* nPart, bool doCtrls, bool doClones);
    void undoOp(UndoOp::UndoType type, const char* changedFile, const char* changeData, int startframe, int endframe);
    void undoOp(UndoOp::UndoType type, Marker* copyMarker, Marker* realMarker);
    bool doUndo1();
    void doUndo2();
    void doUndo3();
    bool doRedo1();
    void doRedo2();
    void doRedo3();

    void addUndo(UndoOp& i);

    //-----------------------------------------
    //   Configuration
    //-----------------------------------------

    void rescanAlsaPorts();

    //-----------------------------------------
    //   Debug
    //-----------------------------------------

    void dumpMaster();

    void addUpdateFlags(int f) {
        updateFlags |= f;
    }

    //-----------------------------------------
    //   Python bridge related
    //-----------------------------------------
    void executeScript(const char* scriptfile, PartList* parts, int quant, bool onlyIfSelected);

public slots:
    void beat();

    void undo();
    void redo();

    void setTempo(int t);
    void setSig(int a, int b);
    void setSig(const TimeSignature&);

    void setTempo(double tempo) {
        setTempo(int(60000000.0 / tempo));
    }

    void setMasterFlag(bool flag);

    bool getReplay() {
        return _replay;
    }
    void setReplay(bool f);
    unsigned replayPos() {
        return _replayPos;
    }
    void updateReplayPos();
    bool getLoop() {
        return loopFlag;
    }
    void setLoop(bool f);
    void setRecord(bool f, bool autoRecEnable = true);
    void clearTrackRec();
    void setPlay(bool f);
    void setStop(bool);
    void forward();
    void rewindStart();
    void rewind();
    void setPunchin(bool f);
    void setPunchout(bool f);
    void setQuantize(bool val);
    void panic();
    void seqSignal(int fd);
    void playMonitorEvent(int fd);
    void processMonitorMessage(const void*);
    MidiTrack* addTrack(bool doUndo = true);
    MidiTrack* addTrackByName(QString name, int pos, bool doUndo);
    MidiTrack* addNewTrack();
    QString getScriptPath(int id, bool delivered);
    void populateScriptMenu(QMenu* menuPlugins, QObject* receiver);
    void updateTrackViews();
    void closeJackBox();
    void toggleFeedback(bool);
    void newTrackAdded(qint64);

signals:
    void replayChanged(bool, unsigned);
    void songChanged(int);
    void posChanged(int, unsigned, bool);
    void loopChanged(bool);
    void recordChanged(bool);
    void playChanged(bool);
    void playbackStateChanged(bool);
    void punchinChanged(bool);
    void punchoutChanged(bool);
    void quantizeChanged(bool);
    void markerChanged(int);
    void midiPortsChanged();
    void midiNote(int pitch, int velo);
    void midiLearned(int port, int chan, int cc, int lsb = -1);
    void trackViewChanged();
    void trackViewAdded();
    void trackViewDeleted();
    void arrangerViewChanged();
    void segmentSizeChanged(int);
    void trackOrderChanged();
    void trackModified(qint64);
    void trackRemoved(qint64);
    void trackAdded(qint64);
    void partInserted(qint64 trackId, unsigned int pos);
    void partRemoved(qint64);
    void partModified(qint64, unsigned);
    void eventInserted();
    void eventRemoved();
    void eventModified();
    void sigmapChanged();
    void tempoModified();
    void masterFlagChanged();
    void selectionChanged(int type);
    void midiControllerChanged();
    void muteChanged(qint64);
    void soloChanged(qint64);
    void recordFlagChanged(qint64);
    void routeChanged();
    void channelsModified();
    void configModified();

//#define SC_CLIP_MODIFIED      0x4000000
//#define SC_MIDI_CONTROLLER_ADD 0x8000000   // a hardware midi controller was added or deleted
//#define SC_MIDI_TRACK_PROP    0x10000000   // a midi track's properties changed (channel, compression etc)
//#define SC_SONG_TYPE          0x20000000   // the midi song type (mtype) changed
//#define SC_PATCH_UPDATED      0X60000000
};

extern Song* song;

#endif

