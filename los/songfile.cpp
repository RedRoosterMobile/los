//=========================================================
//  LOS
//  Libre Octave Studio
//  $Id: songfile.cpp,v 1.25.2.12 2009/11/04 15:06:07 spamatica Exp $
//
//  (C) Copyright 1999/2000 Werner Schweer (ws@seh.de)
//=========================================================

#include <assert.h>
#include <uuid/uuid.h>

#include "app.h"
#include "song.h"
#include "Composer.h"
#include "cobject.h"
#include "Performer.h"
#include "globals.h"
#include "gconfig.h"
#include "xml.h"
#include "event.h"
#include "marker/marker.h"
#include "midiport.h"
#include "audio.h"
#include "midiplugins/mitplugin.h"
#include "midictrl.h"
#include "conf.h"
#include "driver/jackmidi.h"
#include "trackview.h"
#include "instruments/minstrument.h"

//---------------------------------------------------------
//   ClonePart
//---------------------------------------------------------

ClonePart::ClonePart(const Part* p, int i)
{
    cp = p;
    id = i;
    uuid_generate(uuid);
}

CloneList cloneList;

//---------------------------------------------------------
//   readXmlPart
//---------------------------------------------------------

Part* readXmlPart(Xml& xml, MidiTrack* track, bool doClone, bool toTrack)/*{{{*/
{
    int id = -1;
    Part* npart = 0;
    uuid_t uuid;
    uuid_clear(uuid);
    bool uuidvalid = false;
    bool clone = true;
    bool isclone = false;

    for (;;)
    {
        Xml::Token token = xml.parse();
        const QString& tag = xml.s1();
        switch (token)
        {
            case Xml::Error:
            case Xml::End:
                return npart;
            case Xml::TagStart:
                // If the part has not been created yet...
                if (!npart)
                {
                    // If an id was found...
                    if (id != -1)
                    {
                        for (iClone i = cloneList.begin(); i != cloneList.end(); ++i)
                        {
                            // Is a matching part found in the clone list?
                            if (i->id == id)
                            {
                                // This makes a clone, chains the part, and increases ref counts.
                                npart = track->newPart((Part*) i->cp, true);
                                break;
                            }
                        }
                    }
                    else
                        // If a uuid was found...
                        if (uuidvalid)
                    {
                        for (iClone i = cloneList.begin(); i != cloneList.end(); ++i)
                        {
                            // Is a matching part found in the clone list?
                            if (uuid_compare(uuid, i->uuid) == 0)
                            {
                                MidiTrack* cpt = i->cp->track();
                                // If we want to paste to the given track...
                                if (toTrack)
                                {
                                    // If the given track type is not the same as the part's
                                    //  original track type, we can't continue. Just return.
                                    if (!track)
                                    {
                                        xml.skip("part");
                                        return 0;
                                    }
                                }
                                else
                                    // ...else we want to paste to the part's original track.
                                {
                                    // Make sure the track exists (has not been deleted).
                                    if (song->midis()->find(cpt) != song->midis()->end())
                                        track = cpt;
                                    else
                                        // Track was not found. Try pasting to the given track, as above...
                                    {
                                        if (!track)
                                        {
                                            // No luck. Just return.
                                            xml.skip("part");
                                            return 0;
                                        }
                                    }
                                }

                                // If it's a regular paste (not paste clone), and the original part is
                                //  not a clone, defer so that a new copy is created in TagStart above.
                                if (!doClone && !isclone)
                                    break;

                                // This makes a clone, chains the part, and increases ref counts.
                                npart = track->newPart((Part*) i->cp, true);
                                break;
                            }
                        }
                    }

                    // If the part still has not been created yet...
                    if (!npart)
                    {
                        // A clone was not created from any matching part. Create a non-clone part now.
                        if (!track)
                        {
                            xml.skip("part");
                            return 0;
                        }

                        npart = new MidiPart(track);

                        // Signify a new non-clone part was created.
                        // Even if the original part was itself a clone, clear this because the
                        //  attribute section did not create a clone from any matching part.
                        clone = false;

                        // If an id or uuid was found, add the part to the clone list
                        //  so that subsequent parts can look it up and clone from it...
                        if (id != -1)
                        {
                            ClonePart ncp(npart, id);
                            cloneList.push_back(ncp);
                        }
                        else
                            if (uuidvalid)
                        {
                            ClonePart ncp(npart);
                            // New ClonePart creates its own uuid, but we need to replace it.
                            uuid_copy(ncp.uuid, uuid);
                            cloneList.push_back(ncp);
                        }
                    }
                }

                if (tag == "name")
                    npart->setName(xml.parse1());
                else if (tag == "poslen")
                {
                    ((PosLen*) npart)->read(xml, "poslen");
                }
                else if (tag == "pos")
                {
                    Pos pos;
                    pos.read(xml, "pos"); // obsolete
                    npart->setTick(pos.tick());
                }
                else if (tag == "len")
                {
                    Pos len;
                    len.read(xml, "len"); // obsolete
                    npart->setLenTick(len.tick());
                }
                else if (tag == "selected")
                    npart->setSelected(xml.parseInt());
                else if (tag == "color")
                    npart->setColorIndex(xml.parseInt());
                else if (tag == "mute")
                    npart->setMute(xml.parseInt());
                else if(tag == "zIndex")
                    npart->setZIndex(xml.parseInt());
                else if (tag == "event")
                {
                    // If a new non-clone part was created, accept the events...
                    if (!clone)
                    {
                        EventType type = Note;
                        Event e(type);
                        e.read(xml);
                        // stored tickpos for event has absolute value. However internally
                        // tickpos is relative to start of part, we substract tick().
                        e.move(-npart->tick());
                        int tick = e.tick();

                        // Do not discard events belonging to clone parts,
                        //  at least not yet. A later clone might have a longer,
                        //  fully accommodating part length!
                        // No way to tell at the moment whether there will be clones referencing this...
                        // No choice but to accept all events past 0.
                        if (tick < 0)
                        {
                            //printf("readClone: warning: event not in part: %d - %d -%d, discarded\n",
                            printf("readClone: warning: event at tick:%d not in part:%s, discarded\n",
                                    tick, npart->name().toLatin1().constData());
                        }
                        else
                        {
                            npart->events()->add(e);
                        }
                    }
                    else
                        // ...Otherwise a clone was created, so we don't need the events.
                        xml.skip(tag);
                }
                else
                    xml.unknown("readXmlPart");
                break;
            case Xml::Attribut:
                if (tag == "cloneId")
                {
                    id = xml.s2().toInt();
                }
                else if (tag == "uuid")
                {
                    uuid_parse(xml.s2().toLatin1().constData(), uuid);
                    if (!uuid_is_null(uuid))
                    {
                        uuidvalid = true;
                    }
                }
                else if (tag == "isclone")
                    isclone = xml.s2().toInt();
                break;
            case Xml::TagEnd:
                if (tag == "part")
                    return npart;
            default:
                break;
        }
    }
    return npart;
}/*}}}*/

//---------------------------------------------------------
//   Part::write
//   If isCopy is true, write the xml differently so that
//    we can have 'Paste Clone' feature.
//---------------------------------------------------------


void Part::write(int level, Xml& xml, bool isCopy, bool forceWavePaths) const
{
    const EventList* el = cevents();
    int id = -1;
    uuid_t uuid;
    uuid_clear(uuid);
    bool dumpEvents = true;

    if (isCopy)
    {
        for (iClone i = cloneList.begin(); i != cloneList.end(); ++i)
        {
            if (i->cp->cevents() == el)
            {
                uuid_copy(uuid, i->uuid);
                dumpEvents = false;
                break;
            }
        }
        if (uuid_is_null(uuid))
        {
            ClonePart cp(this);
            uuid_copy(uuid, cp.uuid);
            cloneList.push_back(cp);
        }
    }
    else
    {
        if (el->arefCount() > 1)
        {
            for (iClone i = cloneList.begin(); i != cloneList.end(); ++i)
            {
                if (i->cp->cevents() == el)
                {
                    id = i->id;
                    dumpEvents = false;
                    break;
                }
            }
            if (id == -1)
            {
                id = cloneList.size();
                ClonePart cp(this, id);
                cloneList.push_back(cp);
            }
        }
    }

    // Special markers if this is a copy operation and the
    //  part is a clone.
    if (isCopy)
    {
        char sid[40]; // uuid string is 36 chars. Try 40 for good luck.
        sid[0] = 0;
        uuid_unparse_lower(uuid, sid);

        xml.nput(level, "<part uuid=\"%s\"", sid);

        if (el->arefCount() > 1)
            xml.nput(" isclone=\"1\"");
        xml.put(">");
        level++;
    }
    else
        if (id != -1)
    {
        xml.tag(level++, "part cloneId=\"%d\"", id);
    }
    else
        xml.tag(level++, "part");

    xml.strTag(level, "name", _name);

    PosLen::write(level, xml, "poslen");
    xml.intTag(level, "selected", _selected);
    xml.intTag(level, "color", _colorIndex);
    xml.intTag(level, "zIndex", m_zIndex);

    if (_mute)
        xml.intTag(level, "mute", _mute);
    if (dumpEvents)
    {
        for (ciEvent e = el->begin(); e != el->end(); ++e)
            e->second.write(level, xml, *this, forceWavePaths);
    }
    xml.etag(--level, "part");
}


//---------------------------------------------------------
//   writeFont
//---------------------------------------------------------

void Song::writeFont(int level, Xml& xml, const char* name,
        const QFont& font) const
{
    xml.nput(level, "<%s family=\"%s\" size=\"%d\"",
            name, Xml::xmlString(font.family()).toLatin1().constData(), font.pointSize());
    if (font.weight() != QFont::Normal)
        xml.nput(" weight=\"%d\"", font.weight());
    if (font.italic())
        xml.nput(" italic=\"1\"");
    xml.nput(" />\n");
}

//---------------------------------------------------------
//   readFont
//---------------------------------------------------------

QFont Song::readFont(Xml& xml, const char* name)
{
    QFont f;
    for (;;)
    {
        Xml::Token token = xml.parse();
        switch (token)
        {
            case Xml::Error:
            case Xml::End:
                return f;
            case Xml::TagStart:
                xml.unknown("readFont");
                break;
            case Xml::Attribut:
                if (xml.s1() == "family")
                    f.setFamily(xml.s2());
                else if (xml.s1() == "size")
                    f.setPointSize(xml.s2().toInt());
                else if (xml.s1() == "weight")
                    f.setWeight(xml.s2().toInt());
                else if (xml.s1() == "italic")
                    f.setItalic(xml.s2().toInt());
                break;
            case Xml::TagEnd:
                if (xml.s1() == name)
                    return f;
            default:
                break;
        }
    }
    return f;
}

//---------------------------------------------------------
//   readPart
//---------------------------------------------------------

Part* LOS::readPart(Xml& xml)
{
    Part* part = 0;
    for (;;)
    {
        Xml::Token token = xml.parse();
        const QString& tag = xml.s1();
        switch (token)
        {
            case Xml::Error:
            case Xml::End:
                return part;
            case Xml::Text:
            {
                int trackIdx, partIdx;
                sscanf(tag.toLatin1().constData(), "%d:%d", &trackIdx, &partIdx);
                Track* track = song->artracks()->index(trackIdx);
                if (track)
                    part = track->parts()->find(partIdx);
            }
                break;
            case Xml::TagStart:
                xml.unknown("readPart");
                break;
            case Xml::TagEnd:
                if (tag == "part")
                    return part;
            default:
                break;
        }
    }
}

//---------------------------------------------------------
//   readMarker
//---------------------------------------------------------

void Song::readMarker(Xml& xml)
{
    Marker m;
    m.read(xml);
    _markerList->add(m);
}

//---------------------------------------------------------
//   read
//---------------------------------------------------------

void Song::read(Xml& xml)/*{{{*/
{
    cloneList.clear();
    associatedRoute = "";
    bool click = false;
    for (;;)
    {
        Xml::Token token;
        token = xml.parse();
        const QString& tag = xml.s1();
        switch (token)
        {
            case Xml::Error:
            case Xml::End:
                return;
            case Xml::TagStart:
                if (tag == "master")
                    setMasterFlag(xml.parseInt());
                else if (tag == "info")
                    songInfoStr = xml.parse1();
                else if (tag == "associatedRoute")
                    associatedRoute = xml.parse1();
                else if (tag == "loop")
                    setLoop(xml.parseInt());
                else if (tag == "punchin")
                    setPunchin(xml.parseInt());
                else if (tag == "punchout")
                    setPunchout(xml.parseInt());
                else if (tag == "record")
                    setRecord(xml.parseInt());
                else if (tag == "solo")
                    soloFlag = xml.parseInt();
                else if (tag == "type")
                    _mtype = MidiType(xml.parseInt());
                else if (tag == "recmode")
                    _recMode = xml.parseInt();
                else if (tag == "cycle")
                    _cycleMode = xml.parseInt();
                else if (tag == "click")
                    click = xml.parseInt();
                else if (tag == "quantize")
                    _quantize = xml.parseInt();
                else if (tag == "len")
                    _len = xml.parseInt();
                else if (tag == "follow")
                    _follow = FollowMode(xml.parseInt());
                else if (tag == "tempolist")
                {
                    tempomap.read(xml);
                }
                else if (tag == "siglist")
                    sigmap.read(xml);
                else if (tag == "miditrack")
                {
                    MidiTrack* track = new MidiTrack();
                    track->read(xml);
                    insertTrack(track, -1);
                }
                else if (tag == "Route")
                {
                    readRoute(xml);
                }
                else if (tag == "marker")
                    readMarker(xml);
                else if (tag == "globalPitchShift")
                    _globalPitchShift = xml.parseInt();
                else if (tag == "cpos")
                {
                    int pos = xml.parseInt();
                    Pos p(pos, true);
                    setPos(Song::CPOS, p, false, false, false);
                }
                else if (tag == "lpos")
                {
                    int pos = xml.parseInt();
                    Pos p(pos, true);
                    setPos(Song::LPOS, p, false, false, false);
                }
                else if (tag == "rpos")
                {
                    int pos = xml.parseInt();
                    Pos p(pos, true);
                    setPos(Song::RPOS, p, false, false, false);
                }
                else if (tag == "trackview")
                {//Read in our trackviews
                    //printf("Song::read() found track view\n");
                    TrackView* tv = new TrackView();
                    tv->read(xml);
                    if(tv->selected())
                    {
                        QList<qint64> *tlist = tv->trackIndexList();
                        for(int i = 0;  i < tlist->size(); ++i)
                        {
                            TrackView::TrackViewTrack *tvt = tv->tracks()->value(tlist->at(i));
                            if(tvt && !tvt->is_virtual)
                            {
                                MidiTrack *it = findTrackById(tvt->id);
                                if(it)
                                {
                                    _viewtracks.push_back(it);
                                    m_viewTracks[it->id()] = it;
                                    m_trackIndex.append(it->id());
                                }
                            }
                        }
                    }
                    insertTrackView(tv, -1);
                }
                else
                    xml.skip(tag);
                break;
            case Xml::Attribut:
                break;
            case Xml::TagEnd:
                if (tag == "song")
                {
                    //Call song->updateTrackViews() to update the canvas and headers
                    updateTrackViews();
                    //Call to update the track view menu
                    update(SC_VIEW_CHANGED);
                    return;
                }
            default:
                break;
        }
    }
    dirty = false;

    // Since cloneList is also used for copy/paste operations,
    //  clear the copy clone list again.
    cloneList.clear();
}/*}}}*/

//---------------------------------------------------------
//   read
//    read song
//---------------------------------------------------------

void LOS::read(Xml& xml, bool skipConfig)
{
    bool skipmode = true;
    for (;;)
    {
        Xml::Token token = xml.parse();
        const QString& tag = xml.s1();
        switch (token)
        {
            case Xml::Error:
            case Xml::End:
                return;
            case Xml::TagStart:
                if (skipmode && tag == "los")
                    skipmode = false;
                else if (skipmode)
                    break;
                else if (tag == "configuration")
                    if (skipConfig)
                        readConfiguration(xml, true /* only read sequencer settings */);
                    else
                        readConfiguration(xml, false);
                else if (tag == "song")
                {
                    song->read(xml);
                    audio->msgUpdateSoloStates();
                }
                else if (tag == "mplugin")
                    readStatusMidiInputTransformPlugin(xml);
                else
                    xml.skip(tag);
                break;
            case Xml::Attribut:
                if (tag == "version")
                {
                    int major = xml.s2().section('.', 0, 0).toInt();
                    int minor = xml.s2().section('.', 1, 1).toInt();
                    xml.setVersion(major, minor);
                }
                break;
            case Xml::TagEnd:
                if (!skipmode && tag == "los")
                    return;
            default:
                break;
        }
    }
}

//---------------------------------------------------------
//   write
//---------------------------------------------------------

void Song::write(int level, Xml& xml) const
{
    xml.tag(level++, "song");
    xml.strTag(level, "info", songInfoStr);
    xml.strTag(level, "associatedRoute", associatedRoute);
    xml.intTag(level, "cpos", song->cpos());
    xml.intTag(level, "rpos", song->rpos());
    xml.intTag(level, "lpos", song->lpos());
    xml.intTag(level, "master", _masterFlag);
    xml.intTag(level, "loop", loopFlag);
    xml.intTag(level, "punchin", punchinFlag);
    xml.intTag(level, "punchout", punchoutFlag);
    xml.intTag(level, "record", recordFlag);
    xml.intTag(level, "solo", soloFlag);
    xml.intTag(level, "type", _mtype);
    xml.intTag(level, "recmode", _recMode);
    xml.intTag(level, "cycle", _cycleMode);
    xml.intTag(level, "quantize", _quantize);
    xml.intTag(level, "len", _len);
    xml.intTag(level, "follow", _follow);
    if (_globalPitchShift)
        xml.intTag(level, "globalPitchShift", _globalPitchShift);

    // Make a backup of the current clone list, to retain any 'copy' items,
    //  so that pasting works properly after.
    CloneList copyCloneList = cloneList;
    cloneList.clear();

    //Write the aux tracks first so they can be loaded first, We need them for available configuring
    //audio tracks later
    QList<qint64> added;
    // XXX investigate

    // then write Composer visible tracks since the track view will maintain order for itself
    for (ciMidiTrack i = _artracks.begin(); i != _artracks.end(); ++i)
    {
        if(!added.contains((*i)->id()))
        {
            (*i)->write(level, xml);
            added.append((*i)->id());
        }
    }

    //Write out any remaining tracks that are not view members
    for (ciMidiTrack i = _tracks.begin(); i != _tracks.end(); ++i)
    {
        if(!added.contains((*i)->id()))
            (*i)->write(level, xml);
    }

    // write track views
    for (ciTrackView i = _tviews.begin(); i != _tviews.end(); ++i)
    {
        (*i)->write(level, xml);
    }

    // write track routing
    for (ciMidiTrack i = _tracks.begin(); i != _tracks.end(); ++i)
    {
        (*i)->writeRouting(level, xml);
    }

    // Write midi device routing.
    for (iMidiDevice i = midiDevices.begin(); i != midiDevices.end(); ++i)
    {
        (*i)->writeRouting(level, xml);
    }

    //Write midi port routing.
    for (int i = 0; i < kMaxMidiPorts; ++i)
    {
        midiPorts[i].writeRouting(level, xml);
    }

    tempomap.write(level, xml);
    sigmap.write(level, xml);
    _markerList->write(level, xml);

    xml.tag(--level, "/song");

    // Restore backup of the clone list, to retain any 'copy' items,
    //  so that pasting works properly after.
    cloneList.clear();
    cloneList = copyCloneList;
}

//---------------------------------------------------------
//   write
//    write song
//---------------------------------------------------------

void LOS::write(Xml& xml) const
{
    xml.header();

    int level = 0;
    xml.tag(level++, "los version=\"2.0\"");
    writeConfiguration(level, xml);

    writeStatusMidiInputTransformPlugins(level, xml);

    song->write(level, xml);

    xml.tag(--level, "/los");
}

