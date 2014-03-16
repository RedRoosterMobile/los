//=========================================================
//  LOS
//  Libre Octave Studio
//  $Id: part.cpp,v 1.12.2.17 2009/06/25 05:13:02 terminator356 Exp $
//
//  (C) Copyright 1999/2000 Werner Schweer (ws@seh.de)
//=========================================================

//FIXME: Find all files with assert(foo) in them and add the following above the include for assert.h
#define NDEBUG
#include <stdio.h>
#include <assert.h>
#include <cmath>

#include "track.h"
#include "part.h"
#include "globals.h"
#include "event.h"
#include "audio.h"
#include "midiport.h"
#include "song.h"
#include "app.h"
#include "StretchDialog.h"

int Part::snGen;

//---------------------------------------------------------
//   unchainClone
//---------------------------------------------------------

void unchainClone(Part* p)/*{{{*/
{
    chainCheckErr(p);

    // Unchain the part.
    p->prevClone()->setNextClone(p->nextClone());
    p->nextClone()->setPrevClone(p->prevClone());

    // Isolate the part.
    p->setPrevClone(p);
    p->setNextClone(p);
}/*}}}*/

//---------------------------------------------------------
//   chainClone
//   The quick way - if part to chain to is known...
//---------------------------------------------------------

void chainClone(Part* p1, Part* p2)/*{{{*/
{
    chainCheckErr(p1);

    // Make sure the part to be chained is unchained first.
    p2->prevClone()->setNextClone(p2->nextClone());
    p2->nextClone()->setPrevClone(p2->prevClone());

    // Link the part to be chained.
    p2->setPrevClone(p1);
    p2->setNextClone(p1->nextClone());

    // Re-link the existing part.
    p1->nextClone()->setPrevClone(p2);
    p1->setNextClone(p2);
}/*}}}*/

//---------------------------------------------------------
//   chainCloneInternal
//   No error check, so it can be called by replaceClone()
//---------------------------------------------------------

void chainCloneInternal(Part* p)/*{{{*/
{
    Track* t = p->track();
    Part* p1 = 0;

    // Look for a part with the same event list, that we can chain to.
    // It's faster if track type is known...

    {
        MidiTrack* mt = 0;
        MidiTrackList* mtl = song->midis();
        for (ciMidiTrack imt = mtl->begin(); imt != mtl->end(); ++imt)
        {
            mt = *imt;
            const PartList* pl = mt->cparts();
            for (ciPart ip = pl->begin(); ip != pl->end(); ++ip)
            {
                // Added by Tim. p3.3.6
                //printf("chainCloneInternal track %p %s part %p %s evlist %p\n", (*imt), (*imt)->name().toLatin1().constData(), ip->second, ip->second->name().toLatin1().constData(), ip->second->cevents());

                if (ip->second != p && ip->second->cevents() == p->cevents())
                {
                    p1 = ip->second;
                    break;
                }
            }
            // If a suitable part was found on a different track, we're done. We will chain to it.
            // Otherwise keep looking for parts on another track. If no others found, then we
            //  chain to any suitable part which was found on the same given track t.
            if (p1 && mt != t)
                break;
        }
    }

    // No part found with same event list? Done.
    if (!p1)
        return;

    // Make sure the part to be chained is unchained first.
    p->prevClone()->setNextClone(p->nextClone());
    p->nextClone()->setPrevClone(p->prevClone());

    // Link the part to be chained.
    p->setPrevClone(p1);
    p->setNextClone(p1->nextClone());

    // Re-link the existing part.
    p1->nextClone()->setPrevClone(p);
    p1->setNextClone(p);
}/*}}}*/

//---------------------------------------------------------
//   chainClone
//   The slow way - if part to chain to is not known...
//---------------------------------------------------------

void chainClone(Part* p)/*{{{*/
{
    chainCheckErr(p);
    chainCloneInternal(p);
}

//---------------------------------------------------------
//   replaceClone
//---------------------------------------------------------

void replaceClone(Part* p1, Part* p2)
{
    chainCheckErr(p1);

    // Make sure the replacement part is unchained first.
    p2->prevClone()->setNextClone(p2->nextClone());
    p2->nextClone()->setPrevClone(p2->prevClone());

    // If the two parts share the same event list, then this MUST
    //  be a straight forward replacement operation. Continue on.
    // If not, and either part has more than one ref count, then do this...
    if (p1->cevents() != p2->cevents())
    {
        bool ret = false;
        // If the part to be replaced is a single uncloned part,
        //  and the replacement part is not, then this operation
        //  MUST be an undo of a de-cloning of a cloned part.
        if (p2->cevents()->refCount() > 1)
        {
            // Chain the replacement part. We don't know the chain it came from,
            //  so we use the slow method.
            chainCloneInternal(p2);
            ret = true;
        }

        // If the replacement part is a single uncloned part,
        //  and the part to be replaced is not, then this operation
        //  MUST be a de-cloning of a cloned part.
        if (p1->cevents()->refCount() > 1)
        {
            // Unchain the part to be replaced.
            p1->prevClone()->setNextClone(p1->nextClone());
            p1->nextClone()->setPrevClone(p1->prevClone());
            // Isolate the part.
            p1->setPrevClone(p1);
            p1->setNextClone(p1);
            ret = true;
        }

        // Was the operation handled?
        if (ret)
            return;
        // Note that two parts here with different event lists, each with more than one
        //  reference count, would be an error. It's not done anywhere in los. But just
        //  to be sure, four lines above were changed to allow that condition.
        // If each of the two different event lists, has only one ref count, we
        //  handle it like a regular replacement, below...
    }

    // If the part to be replaced is a clone not a single lone part, re-link its neighbours to the replacement part...
    if (p1->prevClone() != p1)
    {
        p1->prevClone()->setNextClone(p2);
        p2->setPrevClone(p1->prevClone());
    }
    else
        p2->setPrevClone(p2);

    if (p1->nextClone() != p1)
    {
        p1->nextClone()->setPrevClone(p2);
        p2->setNextClone(p1->nextClone());
    }
    else
        p2->setNextClone(p2);

    // Isolate the replaced part.
    p1->setNextClone(p1);
    p1->setPrevClone(p1);
    //printf("replaceClone p1: %s %p arefs:%d p2: %s %p arefs:%d\n", p1->name().toLatin1().constData(), p1, );

}/*}}}*/

//---------------------------------------------------------
//   unchainTrackParts
//---------------------------------------------------------

void unchainTrackParts(MidiTrack* t, bool decRefCount)/*{{{*/
{
    PartList* pl = t->parts();
    for (iPart ip = pl->begin(); ip != pl->end(); ++ip)
    {
        Part* p = ip->second;
        chainCheckErr(p);

        // Do we want to decrease the reference count?
        if (decRefCount)
            p->events()->incARef(-1);

        // Unchain the part.
        p->prevClone()->setNextClone(p->nextClone());
        p->nextClone()->setPrevClone(p->prevClone());

        // Isolate the part.
        p->setPrevClone(p);
        p->setNextClone(p);
    }
}/*}}}*/

//---------------------------------------------------------
//   chainTrackParts
//---------------------------------------------------------

void chainTrackParts(MidiTrack* t, bool incRefCount)/*{{{*/
{
    PartList* pl = t->parts();
    for (iPart ip = pl->begin(); ip != pl->end(); ++ip)
    {
        Part* p = ip->second;
        chainCheckErr(p);

        // Do we want to increase the reference count?
        if (incRefCount)
            p->events()->incARef(1);

        // Added by Tim. p3.3.6
        //printf("chainTrackParts track %p %s part %p %s evlist %p\n", t, t->name().toLatin1().constData(), p, p->name().toLatin1().constData(), p->cevents());

        Part* p1 = 0;

        // Look for a part with the same event list, that we can chain to.
        // It's faster if track type is known...

        {
            MidiTrack* mt = 0;
            MidiTrackList* mtl = song->midis();
            for (ciMidiTrack imt = mtl->begin(); imt != mtl->end(); ++imt)
            {
                mt = *imt;
                const PartList* pl = mt->cparts();
                for (ciPart ip = pl->begin(); ip != pl->end(); ++ip)
                {
                    // Added by Tim. p3.3.6
                    //printf("chainTrackParts track %p %s part %p %s evlist %p\n", mt, mt->name().toLatin1().constData(), ip->second, ip->second->name().toLatin1().constData(), ip->second->cevents());

                    if (ip->second != p && ip->second->cevents() == p->cevents())
                    {
                        p1 = ip->second;
                        break;
                    }
                }
                // If a suitable part was found on a different track, we're done. We will chain to it.
                // Otherwise keep looking for parts on another track. If no others found, then we
                //  chain to any suitable part which was found on the same given track t.
                if (p1 && mt != t)
                    break;
            }
        }

        // No part found with same event list? Done.
        if (!p1)
            continue;

        // Make sure the part to be chained is unchained first.
        p->prevClone()->setNextClone(p->nextClone());
        p->nextClone()->setPrevClone(p->prevClone());

        // Link the part to be chained.
        p->setPrevClone(p1);
        p->setNextClone(p1->nextClone());

        // Re-link the existing part.
        p1->nextClone()->setPrevClone(p);
        p1->setNextClone(p);
    }
}/*}}}*/

//---------------------------------------------------------
//   chainCheckErr
//---------------------------------------------------------

void chainCheckErr(Part* p)/*{{{*/
{
    // At all times these must be true...
    if (p->nextClone()->prevClone() != p)
        printf("chainCheckErr: Next clone:%s %p prev clone:%s %p != %s %p\n", p->nextClone()->name().toLatin1().constData(), p->nextClone(), p->nextClone()->prevClone()->name().toLatin1().constData(), p->nextClone()->prevClone(), p->name().toLatin1().constData(), p);
    if (p->prevClone()->nextClone() != p)
        printf("chainCheckErr: Prev clone:%s %p next clone:%s %p != %s %p\n", p->prevClone()->name().toLatin1().constData(), p->prevClone(), p->prevClone()->nextClone()->name().toLatin1().constData(), p->prevClone()->nextClone(), p->name().toLatin1().constData(), p);
}/*}}}*/

//---------------------------------------------------------
//   addPortCtrlEvents
//---------------------------------------------------------

void addPortCtrlEvents(Event& event, Part* part, bool doClones)/*{{{*/
{
    // Traverse and process the clone chain ring until we arrive at the same part again.
    // The loop is a safety net.
    // Update: Due to the varying calls, and order of, incARefcount, (msg)ChangePart, replaceClone, and remove/addPortCtrlEvents,
    //  we can not rely on the reference count as a safety net in these routines. We will just have to trust the clone chain.
    Part* p = part;
    //int j = doClones ? p->cevents()->arefCount() : 1;
    //if(j > 0)
    {
        //for(int i = 0; i < j; ++i)
        while (1)
        {
            // Added by Tim. p3.3.6
            //printf("addPortCtrlEvents i:%d %s %p events %p refs:%d arefs:%d\n", i, p->name().toLatin1().constData(), p, part->cevents(), part->cevents()->refCount(), j);

            Track* t = p->track();
            if (t)
            {
                MidiTrack* mt = (MidiTrack*) t;
                int port = mt->outPort();
                //const EventList* el = p->cevents();
                unsigned len = p->lenTick();
                //for(ciEvent ie = el->begin(); ie != el->end(); ++ie)
                //{
                //const Event& ev = ie->second;
                // Added by Tim. p3.3.6
                //printf("addPortCtrlEvents %s len:%d end:%d etick:%d\n", p->name().toLatin1().constData(), p->lenTick(), p->endTick(), event.tick());

                // Do not add events which are past the end of the part.
                if (event.tick() >= len)
                    break;

                if (event.type() == Controller)
                {
                    int ch = mt->outChannel();
                    int tck = event.tick() + p->tick();
                    int cntrl = event.dataA();
                    int val = event.dataB();
                    MidiPort* mp = &midiPorts[port];

                    mp->setControllerVal(ch, tck, cntrl, val, p);
                }
                //}
            }

            if (!doClones)
                break;
            // Get the next clone in the chain ring.
            p = p->nextClone();
            // Same as original part? Finished.
            if (p == part)
                break;
        }
    }
}/*}}}*/

//---------------------------------------------------------
//   addPortCtrlEvents
//---------------------------------------------------------

void addPortCtrlEvents(Part* part, bool doClones)/*{{{*/
{
    // Traverse and process the clone chain ring until we arrive at the same part again.
    // The loop is a safety net.
    // Update: Due to the varying calls, and order of, incARefcount, (msg)ChangePart, replaceClone, and remove/addPortCtrlEvents,
    //  we can not rely on the reference count as a safety net in these routines. We will just have to trust the clone chain.
    Part* p = part;
    //int j = doClones ? p->cevents()->arefCount() : 1;
    //if(j > 0)
    {
        //for(int i = 0; i < j; ++i)
        while (1)
        {
            // Added by Tim. p3.3.6
            //printf("addPortCtrlEvents i:%d %s %p events %p refs:%d arefs:%d\n", i, p->name().toLatin1().constData(), p, part->cevents(), part->cevents()->refCount(), j);

            MidiTrack* t = p->track();
            if (t)
            {
                MidiTrack* mt = (MidiTrack*) t;
                int port = mt->outPort();
                const EventList* el = p->cevents();
                unsigned len = p->lenTick();
                for (ciEvent ie = el->begin(); ie != el->end(); ++ie)
                {
                    const Event& ev = ie->second;
                    // Added by T356. Do not add events which are past the end of the part.
                    if (ev.tick() >= len)
                        break;

                    if (ev.type() == Controller)
                    {
                        int ch = mt->outChannel();
                        int tck = ev.tick() + p->tick();
                        int cntrl = ev.dataA();
                        int val = ev.dataB();
                        MidiPort* mp = &midiPorts[port];

                        mp->setControllerVal(ch, tck, cntrl, val, p);
                    }
                }
            }
            if (!doClones)
                break;
            // Get the next clone in the chain ring.
            p = p->nextClone();
            // Same as original part? Finished.
            if (p == part)
                break;
        }
    }
}/*}}}*/

//---------------------------------------------------------
//   removePortCtrlEvents
//---------------------------------------------------------

void removePortCtrlEvents(Event& event, Part* part, bool doClones)/*{{{*/
{
    // Traverse and process the clone chain ring until we arrive at the same part again.
    // The loop is a safety net.
    // Update: Due to the varying calls, and order of, incARefcount, (msg)ChangePart, replaceClone, and remove/addPortCtrlEvents,
    //  we can not rely on the reference count as a safety net in these routines. We will just have to trust the clone chain.
    Part* p = part;
    //int j = doClones ? p->cevents()->arefCount() : 1;
    //if(j > 0)
    {
        //for(int i = 0; i < j; ++i)
        while (1)
        {
            MidiTrack* t = p->track();
            if (t)
            {
                MidiTrack* mt = (MidiTrack*) t;
                int port = mt->outPort();
                //const EventList* el = p->cevents();
                //unsigned len = p->lenTick();
                //for(ciEvent ie = el->begin(); ie != el->end(); ++ie)
                //{
                //const Event& ev = ie->second;
                // Added by T356. Do not remove events which are past the end of the part.
                // No, actually, do remove ALL of them belonging to the part.
                // Just in case there are stray values left after the part end.
                //if(ev.tick() >= len)
                //  break;

                if (event.type() == Controller)
                {
                    int ch = mt->outChannel();
                    int tck = event.tick() + p->tick();
                    int cntrl = event.dataA();
                    MidiPort* mp = &midiPorts[port];

                    mp->deleteController(ch, tck, cntrl, p);
                }
                //}
            }

            if (!doClones)
                break;
            // Get the next clone in the chain ring.
            p = p->nextClone();
            // Same as original part? Finished.
            if (p == part)
                break;
        }
    }
}/*}}}*/

//---------------------------------------------------------
//   removePortCtrlEvents
//---------------------------------------------------------

void removePortCtrlEvents(Part* part, bool doClones)/*{{{*/
{
    // Traverse and process the clone chain ring until we arrive at the same part again.
    // The loop is a safety net.
    // Update: Due to the varying calls, and order of, incARefcount, (msg)ChangePart, replaceClone, and remove/addPortCtrlEvents,
    //  we can not rely on the reference count as a safety net in these routines. We will just have to trust the clone chain.
    Part* p = part;
    //int j = doClones ? p->cevents()->arefCount() : 1;
    //if(j > 0)
    {
        //for(int i = 0; i < j; ++i)
        while (1)
        {
            MidiTrack* t = p->track();
            if (t)
            {
                MidiTrack* mt = (MidiTrack*) t;
                int port = mt->outPort();
                const EventList* el = p->cevents();
                //unsigned len = p->lenTick();
                for (ciEvent ie = el->begin(); ie != el->end(); ++ie)
                {
                    const Event& ev = ie->second;
                    // Added by T356. Do not remove events which are past the end of the part.
                    // No, actually, do remove ALL of them belonging to the part.
                    // Just in case there are stray values left after the part end.
                    //if(ev.tick() >= len)
                    //  break;

                    if (ev.type() == Controller)
                    {
                        int ch = mt->outChannel();
                        int tck = ev.tick() + p->tick();
                        int cntrl = ev.dataA();
                        MidiPort* mp = &midiPorts[port];

                        mp->deleteController(ch, tck, cntrl, p);
                    }
                }
            }

            if (!doClones)
                break;
            // Get the next clone in the chain ring.
            p = p->nextClone();
            // Same as original part? Finished.
            if (p == part)
                break;
        }
    }
}/*}}}*/

//---------------------------------------------------------
//   addEvent
//---------------------------------------------------------

iEvent Part::addEvent(Event& p)
{
    return _events->add(p);
}

//---------------------------------------------------------
//   index
//---------------------------------------------------------

int PartList::index(Part* part)
{
    int index = 0;
    for (iPart i = begin(); i != end(); ++i, ++index)
        if (i->second == part)
        {
            return index;
        }
    if (debugMsg)
        printf("PartList::index(): not found!\n");
    //return 0;
    return -1;
}

//---------------------------------------------------------
//   find
//---------------------------------------------------------

Part* PartList::find(int idx)
{
    int index = 0;
    for (iPart i = begin(); i != end(); ++i, ++index)
        if (index == idx)
            return i->second;
    return 0;
}

//---------------------------------------------------------
//   Part
//---------------------------------------------------------

Part::Part(MidiTrack* t)
{
    _prevClone = this;
    _nextClone = this;
    setSn(newSn());
    _track = t;
    _selected = false;
    _mute = false;
    m_zIndex = 0;
    _colorIndex = 1;
    if(t)
        _colorIndex = t->getDefaultPartColor();
    _events = new EventList;
    _events->incRef(1);
    _events->incARef(1);
}

//---------------------------------------------------------
//   Part
//---------------------------------------------------------

Part::Part(MidiTrack* t, EventList* ev)
{
    _prevClone = this;
    _nextClone = this;
    setSn(newSn());
    _track = t;
    _selected = false;
    _mute = false;
    m_zIndex = 0;
    _colorIndex = 1;
    if(t)
        _colorIndex = t->getDefaultPartColor();
    _events = ev;
    _events->incRef(1);
    _events->incARef(1);
}

void Part::setZIndex(int i)
{
    m_zIndex = i;
    if(_track)
    {
        _track->setMaxZIndex(i);
    }
}

bool Part::smallerZValue(Part* first, Part* second)
{
    return first->getZIndex() < second->getZIndex();
}

//---------------------------------------------------------
//   MidiPart
//   copy constructor
//---------------------------------------------------------

MidiPart::MidiPart(const MidiPart& p) : Part(p)
{
    _prevClone = this;
    _nextClone = this;
    m_zIndex = p.m_zIndex;
}

//---------------------------------------------------------
//   Part
//---------------------------------------------------------

Part::~Part()
{
    _events->incRef(-1);
    if (_events->refCount() <= 0)
        delete _events;
}

//---------------------------------------------------------
//   findPart
//---------------------------------------------------------

iPart PartList::findPart(unsigned tick)
{
    iPart i;
    for (i = begin(); i != end(); ++i)
        if (i->second->tick() == tick)
            break;
    return i;
}

Part* PartList::find(unsigned tick, int sn)/*{{{*/
{
    Part* rv = 0;
    for (iPart i = begin(); i != end(); ++i)
    {
        if (i->second->tick() == tick && i->second->sn() == sn)
        {
            rv = i->second;
            break;
        }
    }
    return rv;
}/*}}}*/

Part* PartList::findAtTick(unsigned tick)/*{{{*/
{
    Part* rv = 0;
    for (iPart i = begin(); i != end(); ++i)
    {
        if (tick > i->second->tick() && tick < (i->second->tick()+i->second->lenTick()))
        {
            rv = i->second;
            break;
        }
    }
    return rv;
}/*}}}*/

PartMap PartList::partMap(MidiTrack* track)
{
    PartMap pmap;
    PartList* list = new PartList;
    for (iPart i = begin(); i != end(); ++i)
    {
        if (i->second->track() == track)
        {
            list->add(i->second);
        }
    }
    pmap.track = track;
    pmap.parts = list;
    return pmap;
}

QList<MidiTrack*> PartList::tracks()
{
    QList<MidiTrack*> list;
    for (iPart i = begin(); i != end(); ++i)
    {
        if(list.isEmpty() || !list.contains(i->second->track()))
            list.append(i->second->track());
    }
    return list;
}

//---------------------------------------------------------
//   add
//---------------------------------------------------------

iPart PartList::add(Part* part)
{
    // Added by T356. A part list containing wave parts should be sorted by
    //  frames. WaveTrack::fetchData() relies on the sorting order, and
    //  there was a bug that waveparts were sometimes muted because of
    //  incorrect sorting order (by ticks).
    // Also, when the tempo map is changed, every wavepart would have to be
    //  re-added to the part list so that the proper sorting order (by ticks)
    //  could be achieved.
    // Note that in a los file, the tempo list is loaded AFTER all the tracks.
    // There was a bug that all the wave parts' tick values were not correct,
    // since they were computed BEFORE the tempo map was loaded.
    if (part->type() == Pos::FRAMES)
        return insert(std::pair<const int, Part*> (part->frame(), part));
    else
        return insert(std::pair<const int, Part*> (part->tick(), part));
}

//---------------------------------------------------------
//   remove
//---------------------------------------------------------

void PartList::remove(Part* part)
{
    iPart i;
    for (i = begin(); i != end(); ++i)
    {
        if (i->second == part)
        {
            erase(i);
            break;
        }
    }
    //assert(i != end());
}

//---------------------------------------------------------
//   addPart
//---------------------------------------------------------

void Song::addPart(Part* part)
{
    // adjust song len:
    unsigned epos = part->tick() + part->lenTick();

    if (epos > len())
        _len = epos;
    part->track()->addPart(part);

    //part->addPortCtrlEvents();
    // Indicate do not do clones.
    addPortCtrlEvents(part, false);
}

//---------------------------------------------------------
//   removePart
//---------------------------------------------------------

void Song::removePart(Part* part)
{
    //part->removePortCtrlEvents();
    // Indicate do not do clones.
    //removePortCtrlEvents(part);
    removePortCtrlEvents(part, false);
    Track* track = part->track();
    track->parts()->remove(part);
}

//---------------------------------------------------------
//   cmdResizePart
//---------------------------------------------------------

void Song::cmdResizePart(Track* track, Part* oPart, unsigned int len)/*{{{*/
{
        {
            startUndo();

            MidiPart* nPart = new MidiPart(*(MidiPart*) oPart);
            nPart->setLenTick(len);
            // Indicate no undo, and do port controller values but not clone parts.
            audio->msgChangePart(oPart, nPart, false, true, false);

            // cut Events in nPart
            // Changed by T356. Don't delete events if this is a clone part.
            // The other clones might be longer than this one and need these events.
            if (nPart->cevents()->arefCount() <= 1)
            {
                if (oPart->lenTick() > len)
                {
                    EventList* el = nPart->events();
                    iEvent ie = el->lower_bound(len);
                    for (; ie != el->end();)
                    {
                        iEvent i = ie;
                        ++ie;
                        // Indicate no undo, and do port controller values and clone parts.
                        audio->msgDeleteEvent(i->second, nPart, false, true, true);
                    }
                }
            }

            /*
            // cut Events in nPart
            // Changed by T356. Don't delete events if this is a clone part.
            // The other clones might be longer than this one and need these events.
            if(oPart->cevents()->arefCount() <= 1)
            {
              if (oPart->lenTick() > len) {
                    EventList* el = nPart->events();
                    iEvent ie = el->lower_bound(len);
                    for (; ie != el->end();) {
                          iEvent i = ie;
                          ++ie;
                          // Indicate no undo, and do not do port controller values and clone parts.
                          //audio->msgDeleteEvent(i->second, nPart, false);
                          audio->msgDeleteEvent(i->second, nPart, false, false, false);
                          }
                    }
            }
            // Indicate no undo, and do port controller values but not clone parts.
            //audio->msgChangePart(oPart, nPart, false);
            audio->msgChangePart(oPart, nPart, false, true, false);
             */

            endUndo(SC_PART_MODIFIED);
        }
}/*}}}*/

//---------------------------------------------------------
//   splitPart
//    split part "part" at "tick" position
//    create two new parts p1 and p2
//---------------------------------------------------------

void Track::splitPart(Part* part, int tickpos, Part*& p1, Part*& p2)
{
    int l1 = 0; // len of first new part (ticks or samples)
    int l2 = 0; // len of second new part

    int samplepos = tempomap.tick2frame(tickpos);

    {
            l1 = tickpos - part->tick();
            l2 = part->lenTick() - l1;
    }

    if (l1 <= 0 || l2 <= 0)
        return;

    p1 = newPart(part); // new left part
    p2 = newPart(part); // new right part

    // Added by Tim. p3.3.6
    //printf("Track::splitPart part ev %p sz:%d ref:%d p1 %p sz:%d ref:%d p2 %p sz:%d ref:%d\n", part->events(), part->events()->size(), part->events()->arefCount(), p1->events(), p1->events()->size(), p1->events()->arefCount(), p2->events(), p2->events()->size(), p2->events()->arefCount());

    {
            p1->setLenTick(l1);
            p2->setTick(tickpos);
            p2->setLenTick(l2);
    }

    p2->setSn(p2->newSn());

    EventList* se = part->events();
    EventList* de1 = p1->events();
    EventList* de2 = p2->events();

    p1->setColorIndex(part->colorIndex());
    p2->setColorIndex(part->colorIndex());

    {
        for (iEvent ie = se->begin(); ie != se->end(); ++ie)
        {
            Event event = ie->second.clone();
            int t = event.tick();
            if (t >= l1)
            {
                event.move(-l1);
                de2->add(event);
            }
            else
                de1->add(event);
        }
    }
}

//---------------------------------------------------------
//   cmdSplitPart
//---------------------------------------------------------

void Song::cmdSplitPart(Track* track, Part* part, int tick)
{
    int l1 = tick - part->tick();
    int l2 = part->lenTick() - l1;
    if (l1 <= 0 || l2 <= 0)
        return;
    Part* p1;
    Part* p2;
    track->splitPart(part, tick, p1, p2);

    startUndo();
    // Indicate no undo, and do port controller values but not clone parts.
    //audio->msgChangePart(part, p1, false);
    audio->msgChangePart(part, p1, false, true, false);
    audio->msgAddPart(p2, false);
    endUndo(SC_TRACK_MODIFIED | SC_PART_MODIFIED | SC_PART_INSERTED);
}

//---------------------------------------------------------
//   changePart
//---------------------------------------------------------

void Song::changePart(Part* oPart, Part* nPart)
{
    nPart->setSn(oPart->sn());

    Track* oTrack = oPart->track();
    Track* nTrack = nPart->track();

    oTrack->parts()->remove(oPart);
    nTrack->parts()->add(nPart);
    nPart->setColorIndex(oPart->colorIndex());

    // adjust song len:
    unsigned epos = nPart->tick() + nPart->lenTick();
    if (epos > len())
        _len = epos;
}

//---------------------------------------------------------
//   cmdGluePart
//---------------------------------------------------------

void Song::cmdGluePart(Track* track, Part* oPart)
{
    PartList* pl = track->parts();
    Part* nextPart = 0;

    for (iPart ip = pl->begin(); ip != pl->end(); ++ip)
    {
        if (ip->second == oPart)
        {
            ++ip;
            if (ip == pl->end())
                return;
            nextPart = ip->second;
            break;
        }
    }

    Part* nPart = track->newPart(oPart);
    nPart->setLenTick(nextPart->tick() + nextPart->lenTick() - oPart->tick());

    // populate nPart with Events from oPart and nextPart

    EventList* sl1 = oPart->events();
    EventList* dl = nPart->events();

    for (iEvent ie = sl1->begin(); ie != sl1->end(); ++ie)
        dl->add(ie->second);

    EventList* sl2 = nextPart->events();

    //int frameOffset = nextPart->frame() - oPart->frame();
    //for (iEvent ie = sl2->begin(); ie != sl2->end(); ++ie) {
    //      Event event = ie->second.clone();
    //      event.setFrame(event.frame() + frameOffset);
    //      dl->add(event);
    //      }
    // p3.3.54 Changed.

    {
        int tickOffset = nextPart->tick() - oPart->tick();
        for (iEvent ie = sl2->begin(); ie != sl2->end(); ++ie)
        {
            Event event = ie->second.clone();
            event.setTick(event.tick() + tickOffset);
            dl->add(event);
        }
    }

    startUndo();
    audio->msgRemovePart(nextPart, false);
    // Indicate no undo, and do port controller values but not clone parts.
    //audio->msgChangePart(oPart, nPart, false);
    audio->msgChangePart(oPart, nPart, false, true, false);
    endUndo(SC_PART_MODIFIED | SC_PART_REMOVED);
}

//---------------------------------------------------------
//   dump
//---------------------------------------------------------

void Part::dump(int n) const
{
    for (int i = 0; i < n; ++i)
        putchar(' ');
    printf("Part: <%s> ", _name.toLatin1().constData());
    for (int i = 0; i < n; ++i)
        putchar(' ');
    PosLen::dump();
}

void Part::setSelected(bool f)
{
    _selected = f;
    if(f)
        song->hasSelectedParts = f;
}

void MidiPart::dump(int n) const
{
    Part::dump(n);
    for (int i = 0; i < n; ++i)
        putchar(' ');
    printf("MidiPart\n");
}

//---------------------------------------------------------
//   clone
//---------------------------------------------------------

MidiPart* MidiPart::clone() const
{
    return new MidiPart(*this);
}
