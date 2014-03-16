//=========================================================
//  LOS
//  Libre Octave Studio
//  $Id: undo.h,v 1.6.2.5 2009/05/24 21:43:44 terminator356 Exp $
//
//  (C) Copyright 1999/2000 Werner Schweer (ws@seh.de)
//=========================================================

#ifndef __UNDO_H__
#define __UNDO_H__

#include <list>

#include "event.h"
#include "marker/marker.h"

class QString;

class TEvent;
class SigEvent;
class Part;
class LOSCommand;
class MidiTrack;

// XXX remove
extern std::list<QString> temporaryWavFiles; //!< Used for storing all tmp-files, for cleanup on shutdown

//---------------------------------------------------------
//   UndoOp
//---------------------------------------------------------

struct UndoOp
{

    enum UndoType
    {
        AddTrack, DeleteTrack, ModifyTrack,
        AddPart, DeletePart, ModifyPart,
        AddEvent, DeleteEvent, ModifyEvent,
        AddTempo, DeleteTempo,
        AddSig, DeleteSig,
        SwapTrack,
        /*ModifyClip,*/
        ModifyMarker,
        AddTrackView, DeleteTrackView, ModifyTrackView,
        AddAutomation, DeleteAutomation, ModifyAutomation,
        AddOOMCommand
    };
    UndoType type;

    union
    {
        struct {
            int a;
            int b;
            int c;
        };

        struct {
            MidiTrack* oTrack;
            MidiTrack* nTrack;
            int trackno;
        };

        struct {
            Part* oPart;
            Part* nPart;
        };

        struct {
            Part* part;
        };

        struct {
            LOSCommand* cmd;
        };

        struct {
            SigEvent* nSignature;
            SigEvent* oSignature;
        };

        struct {
            int channel;
            int ctrl;
            int oVal;
            int nVal;
        };

        struct {
            int startframe; //!< Start frame of changed data
            int endframe; //!< End frame of changed data
            const char* filename; //!< The file that is changed
            const char* tmpwavfile; //!< The file with the changed data
        };

        struct {
            Marker* realMarker;
            Marker* copyMarker;
        };

        struct {
            int d;
            int e;
            int f;
        };
    };

    Event oEvent;
    Event nEvent;
    bool doCtrls;
    bool doClones;
    const char* typeName();
    void dump();
};

class Undo : public std::list<UndoOp>
{
    void undoOp(UndoOp::UndoType, int data);
};

typedef Undo::iterator iUndoOp;
typedef Undo::reverse_iterator riUndoOp;

class UndoList : public std::list<Undo>
{
public:
    void clearDelete();
};

typedef UndoList::iterator iUndo;

#endif // __UNDO_H__
