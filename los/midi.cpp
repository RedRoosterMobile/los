//=========================================================
//  LOS
//  Libre Octave Studio
//  $Id: midi.cpp,v 1.43.2.22 2009/11/09 20:28:28 terminator356 Exp $
//
//  (C) Copyright 1999/2004 Werner Schweer (ws@seh.de)
//=========================================================

#include <cmath>
#include <errno.h>
#include <values.h>
#include <assert.h>

#include "song.h"
#include "midi.h"
#include "event.h"
#include "globals.h"
#include "midictrl.h"
#include "marker/marker.h"
#include "midiport.h"
#include "midictrl.h"
#include "audio.h"
#include "mididev.h"
#include "driver/alsamidi.h"
#include "driver/jackmidi.h"
#include "wave.h"
#include "midiseq.h"
#include "gconfig.h"
#include "minstrument.h"

extern void dump(const unsigned char* p, int n);

const unsigned char gmOnMsg[] = {0x7e, 0x7f, 0x09, 0x01};
const unsigned char gsOnMsg[] = {0x41, 0x10, 0x42, 0x12, 0x40, 0x00, 0x7f, 0x00, 0x41};
const unsigned char gsOnMsg2[] = {0x41, 0x10, 0x42, 0x12, 0x40, 0x01, 0x33, 0x50, 0x3c};
const unsigned char gsOnMsg3[] = {0x41, 0x10, 0x42, 0x12, 0x40, 0x01, 0x34, 0x50, 0x3b};
const unsigned char xgOnMsg[] = {0x43, 0x10, 0x4c, 0x00, 0x00, 0x7e, 0x00};
const unsigned int gmOnMsgLen = sizeof (gmOnMsg);
const unsigned int gsOnMsgLen = sizeof (gsOnMsg);
const unsigned int gsOnMsg2Len = sizeof (gsOnMsg2);
const unsigned int gsOnMsg3Len = sizeof (gsOnMsg3);
const unsigned int xgOnMsgLen = sizeof (xgOnMsg);

#define CALC_TICK(the_tick) lrintf((float(the_tick) * float(config.division) + float(div/2)) / float(div));

/*---------------------------------------------------------
 *    midi_meta_name
 *---------------------------------------------------------*/

QString midiMetaName(int meta)
{
    const char* s = "";
    switch (meta)
    {
        case 0: s = "Sequence Number";
            break;
        case 1: s = "Text Event";
            break;
        case 2: s = "Copyright";
            break;
        case 3: s = "Sequence/Track Name";
            break;
        case 4: s = "Instrument Name";
            break;
        case 5: s = "Lyric";
            break;
        case 6: s = "Marker";
            break;
        case 7: s = "Cue Point";
            break;
        case 8:
        case 9:
        case 0x0a:
        case 0x0b:
        case 0x0c:
        case 0x0d:
        case 0x0e:
        case 0x0f: s = "Text";
            break;
        case 0x20: s = "Channel Prefix";
            break;
        case 0x21: s = "Port Change";
            break;
        case 0x2f: s = "End of Track";
            break;
        case 0x51: s = "Set Tempo";
            break;
        case 0x54: s = "SMPTE Offset";
            break;
        case 0x58: s = "Time Signature";
            break;
        case 0x59: s = "Key Signature";
            break;
        case 0x74: s = "Sequencer-Specific1";
            break;
        case 0x7f: s = "Sequencer-Specific2";
            break;
        default:
            break;
    }
    return QString(s);
}

//---------------------------------------------------------
//   QString nameSysex
//---------------------------------------------------------

QString nameSysex(unsigned int len, const unsigned char* buf)
{
    QString s;
    if (len == 0)
        return s;
    switch (buf[0])
    {
        case 0x00:
            if (len < 3)
                return s;
            if (buf[1] == 0 && buf[2] == 0x41)
                s = "Microsoft";
            break;
        case 0x01: s = "Sequential Circuits: ";
            break;
        case 0x02: s = "Big Briar: ";
            break;
        case 0x03: s = "Octave / Plateau: ";
            break;
        case 0x04: s = "Moog: ";
            break;
        case 0x05: s = "Passport Designs: ";
            break;
        case 0x06: s = "Lexicon: ";
            break;
        case 0x07: s = "Kurzweil";
            break;
        case 0x08: s = "Fender";
            break;
        case 0x09: s = "Gulbransen";
            break;
        case 0x0a: s = "Delta Labas";
            break;
        case 0x0b: s = "Sound Comp.";
            break;
        case 0x0c: s = "General Electro";
            break;
        case 0x0d: s = "Techmar";
            break;
        case 0x0e: s = "Matthews Research";
            break;
        case 0x10: s = "Oberheim";
            break;
        case 0x11: s = "PAIA: ";
            break;
        case 0x12: s = "Simmons: ";
            break;
        case 0x13: s = "DigiDesign";
            break;
        case 0x14: s = "Fairlight: ";
            break;
        case 0x15: s = "JL Cooper";
            break;
        case 0x16: s = "Lowery";
            break;
        case 0x17: s = "Lin";
            break;
        case 0x18: s = "Emu";
            break;
        case 0x1b: s = "Peavy";
            break;
        case 0x20: s = "Bon Tempi: ";
            break;
        case 0x21: s = "S.I.E.L: ";
            break;
        case 0x23: s = "SyntheAxe: ";
            break;
        case 0x24: s = "Hohner";
            break;
        case 0x25: s = "Crumar";
            break;
        case 0x26: s = "Solton";
            break;
        case 0x27: s = "Jellinghaus Ms";
            break;
        case 0x28: s = "CTS";
            break;
        case 0x29: s = "PPG";
            break;
        case 0x2f: s = "Elka";
            break;
        case 0x36: s = "Cheetah";
            break;
        case 0x3e: s = "Waldorf";
            break;
        case 0x40: s = "Kawai: ";
            break;
        case 0x41: s = "Roland: ";
            break;
        case 0x42: s = "Korg: ";
            break;
        case 0x43: s = "Yamaha: ";
            break;
        case 0x44: s = "Casio";
            break;
        case 0x45: s = "Akai";
            break;
        case 0x7c: s = "LOS Soft Synth";
            break;
        case 0x7d: s = "Educational Use";
            break;
        case 0x7e: s = "Universal: Non Real Time";
            break;
        case 0x7f: s = "Universal: Real Time";
            break;
        default: s = "??: ";
            break;
    }
    //
    // following messages should not show up in event list
    // they are filtered while importing midi files
    //
    if ((len == gmOnMsgLen) && memcmp(buf, gmOnMsg, gmOnMsgLen) == 0)
        s += "GM-ON";
    else if ((len == gsOnMsgLen) && memcmp(buf, gsOnMsg, gsOnMsgLen) == 0)
        s += "GS-ON";
    else if ((len == xgOnMsgLen) && memcmp(buf, xgOnMsg, xgOnMsgLen) == 0)
        s += "XG-ON";
    return s;
}

//---------------------------------------------------------
//   buildMidiEventList
//    TODO:
//      parse data increment/decrement controller
//      NRPN/RPN  fine/course data 7/14 Bit
//          must we set datah/datal to zero after change
//          of NRPN/RPN register?
//      generally: how to handle incomplete messages
//---------------------------------------------------------

void buildMidiEventList(EventList* del, const MPEventList* el, MidiTrack* track,
        int div, bool addSysexMeta, bool doLoops, bool matchPort)
{
    int hbank = 0xff;
    int lbank = 0xff;
    int rpnh = -1;
    int rpnl = -1;
    int datah = 0;
    int datal = 0;
    int dataType = 0; // 0 : disabled, 0x20000 : rpn, 0x30000 : nrpn

    EventList mel;

    for (iMPEvent i = el->begin(); i != el->end(); ++i)
    {
        MidiPlayEvent ev = *i;
        if (!addSysexMeta && (ev.type() == ME_SYSEX || ev.type() == ME_META))
            continue;
        if (!(ev.type() == ME_SYSEX || ev.type() == ME_META || ((ev.channel() == track->outChannel()))))
            continue;
        if((matchPort && (ev.port() != track->outPort())))
            continue;
        unsigned tick = ev.time();

        if (doLoops)
        {
            if (tick >= song->lPos().tick() && tick < song->rPos().tick())
            {
                int loopn = ev.loopNum();
                int loopc = audio->loopCount();
                int cmode = song->cycleMode(); // CYCLE_NORMAL, CYCLE_MIX, CYCLE_REPLACE
                // If we want REPLACE and the event was recorded in a previous loop,
                //  just ignore it. This will effectively ignore ALL previous loop events inside
                //  the left and right markers, regardless of where recording was started or stopped.
                // We want to keep any loop 0 note-offs from notes which crossed over the left marker.
                // To avoid more searching here, just keep ALL note-offs from loop 0, and let code below
                //  sort out and keep which ones had note-ons.
                if (!(ev.isNoteOff() && loopn == 0))
                {
                    if (cmode == Song::CYCLE_REPLACE && loopn < loopc)
                    {
                        //printf("buildMidiEventList: CYCLE_REPLACE t:%d type:%d A:%d B:%d ln:%d lc:%d\n", tick, ev.type(), ev.dataA(), ev.dataB(), loopn, loopc);
                        continue;
                    }
                    // If we want NORMAL, same as REPLACE except keep all events from the previous loop
                    //  from rec stop position to right marker (and beyond).
                    if (cmode == Song::CYCLE_NORMAL)
                    {
                        // Not sure of accuracy here. Adjust? Adjusted when used elsewhere?
                        unsigned endRec = audio->getEndRecordPos().tick();
                        if ((tick < endRec && loopn < loopc) || (tick >= endRec && loopn < (loopc - 1)))
                        {
                            //printf("buildMidiEventList: CYCLE_NORMAL t:%d type:%d A:%d B:%d ln:%d lc:%d\n", tick, ev.type(), ev.dataA(), ev.dataB(), loopn, loopc);
                            continue;
                        }
                    }
                }
            }
        }

        Event e;
        switch (ev.type())
        {
            case ME_NOTEON:
                e.setType(Note);
                {
                    e.setPitch(ev.dataA());
                }

                e.setVelo(ev.dataB());
                e.setLenTick(0);
                break;
            case ME_NOTEOFF:
                e.setType(Note);
                e.setPitch(ev.dataA());
                e.setVelo(0);
                e.setVeloOff(ev.dataB());
                e.setLenTick(0);
                break;
            case ME_POLYAFTER:
                e.setType(PAfter);
                e.setA(ev.dataA());
                e.setB(ev.dataB());
                break;
            case ME_CONTROLLER:
            {
                int val = ev.dataB();
                switch (ev.dataA())
                {
                    case CTRL_HBANK:
                        hbank = val;
                        break;

                    case CTRL_LBANK:
                        lbank = val;
                        break;

                    case CTRL_HDATA:
                        datah = val;
                        // check if a CTRL_LDATA follows
                        // e.g. wie have a 14 bit controller:
                    {
                        iMPEvent ii = i;
                        ++ii;
                        bool found = false;
                        for (; ii != el->end(); ++ii)
                        {
                            MidiPlayEvent ev = *ii;
                            if (ev.type() == ME_CONTROLLER)
                            {
                                if (ev.dataA() == CTRL_LDATA)
                                {
                                    // handle later
                                    found = true;
                                }
                                break;
                            }
                        }
                        if (!found)
                        {
                            if (rpnh == -1 || rpnl == -1)
                            {
                                printf("parameter number not defined, data 0x%x\n", datah);
                            }
                            else
                            {
                                int ctrl = dataType | (rpnh << 8) | rpnl;
                                e.setType(Controller);
                                e.setA(ctrl);
                                e.setB(datah);
                            }
                        }
                    }
                        break;

                    case CTRL_LDATA:
                        datal = val;

                        if (rpnh == -1 || rpnl == -1)
                        {
                            printf("parameter number not defined, data 0x%x 0x%x, tick %d, channel %d\n",
                                    datah, datal, tick, track->outChannel());
                            break;
                        }
                        // assume that the sequence is always
                        //    CTRL_HDATA - CTRL_LDATA
                        // eg. that LDATA is always send last

                        e.setType(Controller);
                        // 14 Bit RPN/NRPN
                        e.setA((dataType + 0x30000) | (rpnh << 8) | rpnl);
                        e.setB((datah << 7) | datal);
                        break;

                    case CTRL_HNRPN:
                        rpnh = val;
                        dataType = 0x30000;
                        break;

                    case CTRL_LNRPN:
                        rpnl = val;
                        dataType = 0x30000;
                        break;

                    case CTRL_HRPN:
                        rpnh = val;
                        dataType = 0x20000;
                        break;

                    case CTRL_LRPN:
                        rpnl = val;
                        dataType = 0x20000;
                        break;

                    default:
                        e.setType(Controller);
                        int ctl = ev.dataA();
                        e.setA(ctl);
                        e.setB(val);
                        break;
                }
            }
                break;

            case ME_PROGRAM:
                e.setType(Controller);
                e.setA(CTRL_PROGRAM);
                e.setB((hbank << 16) | (lbank << 8) | ev.dataA());
                break;

            case ME_AFTERTOUCH:
                e.setType(CAfter);
                e.setA(ev.dataA());
                break;

            case ME_PITCHBEND:
                e.setType(Controller);
                e.setA(CTRL_PITCH);
                e.setB(ev.dataA());
                break;

            case ME_SYSEX:
                e.setType(Sysex);
                e.setData(ev.data(), ev.len());
                break;

            case ME_META:
            {
                const unsigned char* data = ev.data();
                switch (ev.dataA())
                {
                    case 0x01: // Text
                        if (track->comment().isEmpty())
                            track->setComment(QString((const char*) data));
                        else
                            track->setComment(track->comment() + "\n" + QString((const char*) data));
                        break;
                    case 0x03: // Sequence-/TrackName
                        track->setName(QString((char*) data));
                        break;
                    case 0x6: // Marker
                    {
                        unsigned ltick = CALC_TICK(tick); //(tick * config.division + div/2) / div;
                        song->addMarker(QString((const char*) (data)), ltick, false);
                    }
                        break;
                    //TODO implement text part to deal with this, would be a nice feature
                    case 0x5: // Lyrics
                    case 0x8: // text
                    case 0x9:
                    case 0xa:
                        break;
                    case 0x0f: // Track Comment
                        track->setComment(QString((char*) data));
                        break;
                    case 0x51: // Tempo
                    {
                        unsigned tempo = data[2] + (data[1] << 8) + (data[0] << 16);
                        unsigned ltick = CALC_TICK(tick); // (unsigned(tick) * unsigned(config.division) + unsigned(div/2)) / unsigned(div);
                        // After ca 10 mins 32 bits will not be enough... This expression has to be changed/factorized or so in some "sane" way...
                        tempomap.addTempo(ltick, tempo);
                    }
                        break;
                    case 0x58: // Time Signature
                    {
                        int timesig_z = data[0];
                        int n = data[1];
                        int timesig_n = 1;
                        for (int i = 0; i < n; i++)
                            timesig_n *= 2;
                        int ltick = CALC_TICK(tick); //(tick * config.division + div/2) / div;
                        ///sigmap.add(ltick, timesig_z, timesig_n);
                        AL::sigmap.add(ltick, AL::TimeSignature(timesig_z, timesig_n));
                    }
                        break;
                    case 0x59: // Key Signature
                        // track->scale.set(data[0]);
                        // track->scale.setMajorMinor(data[1]);
                        break;
                    default:
                        printf("unknown Meta 0x%x %d\n", ev.dataA(), ev.dataA());
                }
            }
                break;
        } // switch(ev.type()
        if (!e.empty())
        {
            e.setTick(tick);
            //printf("buildMidiEventList: mel adding t:%d type:%d A:%d B:%d C:%d\n", tick, e.type(), e.dataA(), e.dataB(), e.dataC());

            mel.add(e);
        }
    }

    // Added by Tim. p3.3.8

    // The loop is a safe way to delete while iterating.
    bool loop;
    do
    {
        loop = false;

        for (iEvent i = mel.begin(); i != mel.end(); ++i)
        {
            Event ev = i->second;
            if (ev.isNote())
            {
                if (ev.isNoteOff())
                {
                    iEvent k;
                    bool found = false;
                    for (k = i; k != mel.end(); ++k)
                    {
                        Event event = k->second;
                        if (event.tick() > ev.tick())
                            break;
                        if (event.isNoteOff(ev))
                        {
                            ev.setLenTick(1);
                            ev.setVelo(event.velo());
                            ev.setVeloOff(0);
                            // Added by Tim. p3.3.8
                            //printf("buildMidiEventList: found note off: event t:%d len:%d type:%d A:%d B:%d C:%d  ev t:%d len:%d type:%d A:%d B:%d C:%d\n", event.tick(), event.lenTick(), event.type(), event.dataA(), event.dataB(), event.dataC(), ev.tick(), ev.lenTick(), ev.type(), ev.dataA(), ev.dataB(), ev.dataC());

                            found = true;
                            break;
                        }
                    }
                    if (!found)
                    {
                        printf("NOTE OFF without Note ON tick %d type %d  %d %d\n",
                                ev.tick(), ev.type(), ev.pitch(), ev.velo());
                    }
                    else
                    {
                        mel.erase(k);

                        // Changed by Tim. p3.3.8
                        //continue;
                        loop = true;
                        break;

                    }
                }
                // Added by Tim. p3.3.8

                // If the event length is not zero, it means the event and its
                //  note on/off have already been taken care of. So ignore it.
                if (ev.lenTick() != 0)
                {
                    continue;
                }

                iEvent k;
                for (k = mel.lower_bound(ev.tick()); k != mel.end(); ++k)
                {
                    Event event = k->second;
                    if (ev.isNoteOff(event))
                    {
                        int t = k->first - i->first;
                        if (t <= 0)
                        {
                            if (debugMsg)
                            {
                                printf("Note len is (%d-%d)=%d, set to 1\n",
                                        k->first, i->first, k->first - i->first);
                                ev.dump();
                                event.dump();
                            }
                            t = 1;
                        }
                        ev.setLenTick(t);
                        ev.setVeloOff(event.veloOff());
                        // Added by Tim. p3.3.8
                        //printf("buildMidiEventList: set len and velOff: event t:%d len:%d type:%d A:%d B:%d C:%d  ev t:%d len:%d type:%d A:%d B:%d C:%d\n", event.tick(), event.lenTick(), event.type(), event.dataA(), event.dataB(), event.dataC(), ev.tick(), ev.lenTick(), ev.type(), ev.dataA(), ev.dataB(), ev.dataC());

                        break;
                    }
                }
                if (k == mel.end())
                {
                    printf("-no note-off! %d pitch %d velo %d\n",
                            ev.tick(), ev.pitch(), ev.velo());
                    //
                    // switch off at end of measure
                    //
                    int endTick = song->roundUpBar(ev.tick() + 1);
                    ev.setLenTick(endTick - ev.tick());
                }
                else
                {
                    mel.erase(k);
                    // Added by Tim. p3.3.8
                    loop = true;
                    break;

                }
            }
        }
    } while (loop);

    for (iEvent i = mel.begin(); i != mel.end(); ++i)
    {
        Event ev = i->second;
        if (ev.isNoteOff())
        {
            printf("+extra note-off! %d pitch %d velo %d\n",i->first, ev.pitch(), ev.velo());
            continue;
        }
        int tick = CALC_TICK(ev.tick());
        if (ev.isNote())
        {
            int lenTick = CALC_TICK(ev.lenTick());
            ev.setLenTick(lenTick);
        }
        ev.setTick(tick);
        del->add(ev);
    }
}

//---------------------------------------------------------
//   midiPortsChanged
//---------------------------------------------------------

void Audio::midiPortsChanged()
{
    write(sigFd, "P", 1);
}

//---------------------------------------------------------
//   sendLocalOff
//---------------------------------------------------------

void Audio::sendLocalOff()
{
    for (int k = 0; k < MIDI_PORTS; ++k)
    {
        for (int i = 0; i < MIDI_CHANNELS; ++i)
            midiPorts[k].sendEvent(MidiPlayEvent(0, k, i, ME_CONTROLLER, CTRL_LOCAL_OFF, 0));
    }
}

//---------------------------------------------------------
//   panic
//---------------------------------------------------------

void Audio::panic()
{
#if 1
    for (iMidiDevice i = midiDevices.begin(); i != midiDevices.end(); ++i)
    {
        MidiDevice* dev = *i;
        MPEventList* el  = 0;//synth->playEvents();
        MPEventList* sel = 0;//synth->stuckNotes();
        {
            el = dev->playEvents();
            sel = dev->stuckNotes();
        }

            // stop all notes
            el->clear();
            //MPEventList* sel = dev->stuckNotes();
            for (iMPEvent i = sel->begin(); i != sel->end(); ++i)
            {
                MidiPlayEvent ev = *i;
                ev.setTime(0);
                ev.setType(ME_NOTEOFF);
                el->add(ev);
            }
            sel->clear();
        //}
    }
#endif

    for (int i = 0; i < MIDI_PORTS; ++i)/*{{{*/
    {
        MidiPort* port = &midiPorts[i];
        if (!port || !port->device())
            continue;
        for (int chan = 0; chan < MIDI_CHANNELS; ++chan)
        {
            if (debugMsg)
                 printf("send all sound of to midi port %d channel %d\n", i, chan);
            port->sendEvent(MidiPlayEvent(0, i, chan, ME_CONTROLLER, CTRL_ALL_SOUNDS_OFF, 0), true);
            port->sendEvent(MidiPlayEvent(0, i, chan, ME_CONTROLLER, CTRL_ALL_NOTES_OFF, 0), true);
            port->sendEvent(MidiPlayEvent(0, i, chan, ME_CONTROLLER, CTRL_RESET_ALL_CTRL, 0), true);
        }
    }/*}}}*/
}

//---------------------------------------------------------
//   initDevices
//    - called on seek to position 0
//    - called from Composer pulldown menu
//---------------------------------------------------------

void Audio::initDevices()
{
    //
    // mark all used ports
    //
    bool activePorts[MIDI_PORTS];
    for (int i = 0; i < MIDI_PORTS; ++i)
        activePorts[i] = false;

    MidiTrackList* tracks = song->midis();
    for (iMidiTrack it = tracks->begin(); it != tracks->end(); ++it)
    {
        MidiTrack* track = *it;
        activePorts[track->outPort()] = true;
    }

    //
    // test for explicit instrument initialization
    //

    for (int i = 0; i < MIDI_PORTS; ++i)
    {
        if (!activePorts[i])
            continue;

        MidiPort* port = &midiPorts[i];
        MidiInstrument* instr = port->instrument();
        MidiDevice* md = port->device();

        if (instr && md)
        {
            EventList* events = instr->midiInit();
            if (events->empty())
                continue;
            for (iEvent ie = events->begin(); ie != events->end(); ++ie)
            {
                MidiPlayEvent ev(0, i, 0, ie->second);
                md->putEvent(ev);
            }
            activePorts[i] = false; // no standard initialization
        }
    }
    //
    // damit Midi-Devices, die mehrere Ports besitzen, wie z.B.
    // das Korg NS5R, nicht mehrmals zwischen GM und XG/GS hin und
    // hergeschaltet werden, wird zunÃÂ¯ÃÂ¿ÃÂ½hst auf allen Ports GM
    // initialisiert, und dann erst XG/GS
    //

    // Standard initialization...
    for (int i = 0; i < MIDI_PORTS; ++i)
    {
        if (!activePorts[i])
            continue;
        MidiPort* port = &midiPorts[i];
        switch (song->mtype())
        {
            case MT_GS:
            case MT_UNKNOWN:
                break;
            case MT_GM:
            case MT_XG:
                port->sendGmOn();
                break;
        }
    }
    for (int i = 0; i < MIDI_PORTS; ++i)
    {
        if (!activePorts[i])
            continue;
        MidiPort* port = &midiPorts[i];
        switch (song->mtype())
        {
            case MT_UNKNOWN:
                break;
            case MT_GM:
                port->sendGmInitValues();
                break;
            case MT_GS:
                port->sendGsOn();
                port->sendGsInitValues();
                break;
            case MT_XG:
                port->sendXgOn();
                port->sendXgInitValues();
                break;
        }
    }
}

//---------------------------------------------------------
//   collectEvents
//    collect events for next audio segment
//---------------------------------------------------------

void Audio::collectEvents(MidiTrack* track, unsigned int cts, unsigned int nts)
{
    int port = track->outPort();
    int channel = track->outChannel();
    int defaultPort = port;

    MidiDevice* md = midiPorts[port].device();
    MPEventList* playEvents = md->playEvents();
    MPEventList* stuckNotes = md->stuckNotes();

    PartList* pl = track->parts();
    for (iPart p = pl->begin(); p != pl->end(); ++p)
    {
        MidiPart* part = (MidiPart*) (p->second);
        // dont play muted parts
        if (part->mute())
            continue;
        EventList* events = part->events();
        unsigned partTick = part->tick();
        unsigned partLen = part->lenTick();
        int delay = track->delay;

        if (cts > nts)
        {
            printf("processMidi: FATAL: cur > next %d > %d\n",
                    cts, nts);
            return;
        }
        unsigned offset = delay + partTick;
        if (offset > nts)
            continue;
        unsigned stick = (offset > cts) ? 0 : cts - offset;
        unsigned etick = nts - offset;
        // By T356. Do not play events which are past the end of this part.
        if (etick > partLen)
            continue;

        iEvent ie = events->lower_bound(stick);
        iEvent iend = events->lower_bound(etick);

        for (; ie != iend; ++ie)
        {
            Event ev = ie->second;
            port = defaultPort; //Reset each loop
            //
            //  dont play any meta events
            //
            if (ev.type() == Meta)
                continue;

            unsigned tick = ev.tick() + offset;
            unsigned frame = tempomap.tick2frame(tick) + frameOffset;
            switch (ev.type())
            {
                case Note:
                {
                    int len = ev.lenTick();
                    int pitch = ev.pitch();
                    int velo = ev.velo();

                    {
                        //
                        // transpose non drum notes
                        //
                        pitch += /*(track->getTransposition() +*/ song->globalPitchShift();
                    }

                    if (pitch > 127)
                        pitch = 127;
                    if (pitch < 0)
                        pitch = 0;
                    velo += track->velocity;
                    velo = (velo * track->compression) / 100;
                    if (velo > 127)
                        velo = 127;
                    if (velo < 1) // no off event
                        velo = 1;
                    len = (len * track->len) / 100;
                    if (len <= 0) // dont allow zero length
                        len = 1;
                    int veloOff = ev.veloOff();

                    if (port == defaultPort)
                    {
                        //printf("Adding event normally: frame=%d port=%d channel=%d pitch=%d velo=%d\n",frame, port, channel, pitch, velo);

                        // p3.3.25
                        // If syncing to external midi sync, we cannot use the tempo map.
                        // Therefore we cannot get sub-tick resolution. Just use ticks instead of frames.
                        playEvents->add(MidiPlayEvent(frame, port, channel, 0x90, pitch, velo, track));

                        stuckNotes->add(MidiPlayEvent(tick + len, port, channel, veloOff ? 0x80 : 0x90, pitch, veloOff, track));
                    }
                    else
                    { //Handle events to different port than standard.
                        MidiDevice* mdAlt = midiPorts[port].device();
                        if (mdAlt)
                        {
                            mdAlt->playEvents()->add(MidiPlayEvent(frame, port, channel, 0x90, pitch, velo, track));

                            mdAlt->stuckNotes()->add(MidiPlayEvent(tick + len, port, channel, veloOff ? 0x80 : 0x90, pitch, veloOff, track));
                        }
                    }

                    if (velo > track->activity())
                        track->setActivity(velo);
                }
                    break;

                    // Added by T356.
                case Controller:
                {
                    //int len   = ev.lenTick();
                    //int pitch = ev.pitch();

                    // p3.3.25
                    playEvents->add(MidiPlayEvent(frame, port, channel, ev, track));
                }
                    break;


                default:
                    playEvents->add(MidiPlayEvent(frame, port, channel, ev, track));

                    break;
            }
        }
    }
}

//---------------------------------------------------------
//   processMidi
//    - collects midi events for current audio segment and
//       sends them to midi thread
//    - current audio segment position is (curTickPos, nextTickPos)
//    - called from midiseq thread,
//      executed in audio thread
//---------------------------------------------------------

void Audio::processMidi()
{
    midiBusy = true;
    //
    // TODO: syntis should directly write into recordEventList
    //
    for (iMidiDevice id = midiDevices.begin(); id != midiDevices.end(); ++id)
    {
        MidiDevice* md = *id;
        md->collectMidiEvents();

        // Take snapshots of the current sizes of the recording fifos,
        //  because they may change while here in process, asynchronously.
        md->beforeProcess();
    }

    for (iMidiTrack t = song->midis()->begin(); t != song->midis()->end(); ++t)
    {
        MidiTrack* track = *t;
        int port = track->outPort();
        MidiDevice* md = midiPorts[port].device();

        MPEventList* playEvents = 0;
        if (md)
        {
            playEvents = md->playEvents();

            // only add track events if the track is unmuted
            if (!track->isMute())
            {
                if (isPlaying() && (curTickPos < nextTickPos))
                    collectEvents(track, curTickPos, nextTickPos);
            }
        }

        //
        //----------midi recording
        //
        if (track->recordFlag())
        {
            MPEventList* rl = track->mpevents();
            MidiPort* tport = &midiPorts[port];

            RouteList* irl = track->inRoutes();
            for (ciRoute r = irl->begin(); r != irl->end(); ++r)
            {
                if (!r->isValid() || (r->type != Route::MIDI_PORT_ROUTE))
                    continue;

                int devport = r->midiPort;
                if (devport == -1)
                    continue;

                MidiDevice* dev = midiPorts[devport].device();
                if (!dev)
                    continue;

                int channelMask = r->channel;
                if (channelMask == -1 || channelMask == 0)
                    continue;
                for (int channel = 0; channel < MIDI_CHANNELS; ++channel)
                {
                    if (!(channelMask & (1 << channel)))
                        continue;

                    if (!dev->sysexFIFOProcessed())
                    {
                        // Set to the sysex fifo at first.
                        MidiRecFifo& rf = dev->recordEvents(MIDI_CHANNELS);
                        // Get the frozen snapshot of the size.
                        int count = dev->tmpRecordCount(MIDI_CHANNELS);

                        for (int i = 0; i < count; ++i)
                        {
                            MidiPlayEvent event(rf.peek(i));

                            event.setPort(port);

                            // dont't echo controller changes back to software
                            // synthesizer:
                            if (md && track->recEcho())
                                playEvents->add(event);

                            // If syncing externally the event time is already in units of ticks, set above.
                            event.setTime(tempomap.frame2tick(event.time())); // set tick time

                            if (recording)
                                rl->add(event);
                        }

                        dev->setSysexFIFOProcessed(true);
                    }

                    {
                        MidiRecFifo& rf = dev->recordEvents(channel);
                        int count = dev->tmpRecordCount(channel);

                        for (int i = 0; i < count; ++i)
                        {
                            MidiPlayEvent event(rf.peek(i));

                            int defaultPort = devport;

                            int drumRecPitch = 0; //prevent compiler warning: variable used without initialization
                            MidiController *mc = 0;
                            int ctl = 0;

                            //Hmmm, hehhh...
                            // TODO: Clean up a bit around here when it comes to separate events for rec & for playback.
                            // But not before 0.7 (ml)

                            int prePitch = 0, preVelo = 0;

                            event.setChannel(track->outChannel());

                            if (event.isNote() || event.isNoteOff())
                            {
                                //
                                // apply track values
                                //

                                { //Track transpose if non-drum
                                    prePitch = event.dataA();
                                    int pitch = prePitch + track->getTransposition();
                                    if (pitch > 127)
                                        pitch = 127;
                                    if (pitch < 0)
                                        pitch = 0;
                                    event.setA(pitch);
                                }

                                if (!event.isNoteOff())
                                {
                                    preVelo = event.dataB();
                                    int velo = preVelo + track->velocity;
                                    velo = (velo * track->compression) / 100;
                                    if (velo > 127)
                                        velo = 127;
                                    if (velo < 1)
                                        velo = 1;
                                    event.setB(velo);
                                }
                            }
                            else if (event.type() == ME_CONTROLLER)
                            {
                            }

                            // dont't echo controller changes back to software
                            // synthesizer:

                            //if (!dev->isSynthPlugin())
                            //{
                                //printf("444444444444444444444444444444444444444444444444444444\n");
                                //Check if we're outputting to another port than default:
                                if (devport == defaultPort)
                                {

                                    //printf("5555555555555555555555555555555555555555555\n");
                                    event.setPort(port);
                                    if (md && track->recEcho())
                                        playEvents->add(event);
                                }
                                else
                                {
                                    //printf("66666666666666666666666666666666666666\n");
                                    // Hmm, this appears to work, but... Will this induce trouble with md->setNextPlayEvent??
                                    MidiDevice* mdAlt = midiPorts[devport].device();
                                    if (mdAlt && track->recEcho())
                                        mdAlt->playEvents()->add(event);
                                }
                                // Shall we activate meters even while rec echo is off? Sure, why not...
                                if (event.isNote() && event.dataB() > track->activity())
                                    track->setActivity(event.dataB());
                            //}

                            // p3.3.25
                            // If syncing externally the event time is already in units of ticks, set above.
                            {
                                //printf("7777777777777777777777777777777777777777777\n");
                                // p3.3.35
                                //time = tempomap.frame2tick(event.time());
                                //event.setTime(time);  // set tick time
                                event.setTime(tempomap.frame2tick(event.time())); // set tick time
                            }

                            // Special handling of events stored in rec-lists. a bit hACKish. TODO: Clean up (after 0.7)! :-/ (ml)
                            if (recording)
                            {
                                //printf("888888888888888888888888888888888888888888888888\n");
                                // In these next steps, it is essential to set the recorded event's port
                                //  to the track port so buildMidiEventList will accept it. Even though
                                //  the port may have no device "<none>".
                                //
                                {
                                    //printf("ccccccccccccccccccccccccccccccccccccccccccccc\n");
                                    // Restore record-pitch to non-transposed value since we don't want the note transposed twice next
                                    MidiPlayEvent recEvent = event;
                                    recEvent.setPort(port);
                                    recEvent.setChannel(track->outChannel());

                                    rl->add(recEvent);
                                    if (prePitch)
                                        recEvent.setA(prePitch);
                                    if (preVelo)
                                        recEvent.setB(preVelo);
                                }
                            }
                        }
                    }
                }//END for loop
            }
        }
    }

    //
    // clear all recorded events in midiDevices
    // process stuck notes
    //
    for (iMidiDevice id = midiDevices.begin(); id != midiDevices.end(); ++id)
    {
        //printf("--------------------------aaaaaaaaaaaaaaaaaaaaaaaaaaaa\n");
        MidiDevice* md = *id;

        // We are done with the 'frozen' recording fifos, remove the events.
        md->afterProcess();

        MPEventList* stuckNotes = md->stuckNotes();
        MPEventList* playEvents = md->playEvents();

        iMPEvent k;
        for (k = stuckNotes->begin(); k != stuckNotes->end(); ++k)
        {
            if (k->time() >= nextTickPos)
                break;
            MidiPlayEvent ev(*k);

            {
                int frame = tempomap.tick2frame(k->time()) + frameOffset;
                ev.setTime(frame);
            }

            playEvents->add(ev);
        }
        stuckNotes->erase(stuckNotes->begin(), k);
    }

    if (state == STOP)
    {
        //---------------------------------------------------
        //    end all notes
        //---------------------------------------------------
        for (iMidiDevice imd = midiDevices.begin(); imd != midiDevices.end(); ++imd)
        {
            MidiDevice* md = *imd;
            MPEventList* playEvents = md->playEvents();
            MPEventList* stuckNotes = md->stuckNotes();
            if(playEvents->size())
            {
                /*if(debugMsg)
                    qDebug("Audio::processMidi: State STOP: device name: %s, playEvents.size(%d), stuckNotes.size(%d)",
                        md->name().toUtf8().constData(),
                        (int)playEvents->size(),
                        (int)stuckNotes->size()
                    );*/
                //playEvents->clear();
            }
            for (iMPEvent k = stuckNotes->begin(); k != stuckNotes->end(); ++k)
            {
                MidiPlayEvent ev(*k);
                //THis is why we are getting those error messages on song stop because it is setting all the
                //event times to the same point in time
                ev.setTime(0); // play now
                playEvents->add(ev);
            }
            stuckNotes->clear();
        }
    }


    // p3.3.36
    //
    // Special for Jack midi devices: Play all Jack midi events up to curFrame.
    //
    for (iMidiDevice id = midiDevices.begin(); id != midiDevices.end(); ++id)
    {
        (*id)->processMidi();
    }

    midiBusy = false;
}


void Audio::preloadControllers()/*{{{*/
{
    midiBusy = true;

    MidiTrackList* tracks = song->midis();
    for (iMidiTrack it = tracks->begin(); it != tracks->end(); ++it)
    {
        MidiTrack* track = *it;
        //activePorts[track->outPort()] = true;
        QList<ProcessList*> pcevents;

        int port = track->outPort();
        int channel = track->outChannel();
        int defaultPort = port;

        MidiDevice* md = midiPorts[port].device();
        if (!md)
        {
            continue;
        }
        MPEventList* playEvents = md->playEvents();
        playEvents->erase(playEvents->begin(), playEvents->end());

        PartList* pl = track->parts();
        for (iPart p = pl->begin(); p != pl->end(); ++p)
        {
            MidiPart* part = (MidiPart*) (p->second);
            EventList* events = part->events();
            unsigned partTick = part->tick();
            //unsigned partLen = part->lenTick();
            int delay = track->delay;

            unsigned offset = delay + partTick;

            for (iEvent ie = events->begin(); ie != events->end(); ++ie)
            {
                Event ev = ie->second;
                port = defaultPort;
                unsigned tick = ev.tick() + offset;
                //unsigned frame = tempomap.tick2frame(tick) + frameOffset;
                switch (ev.dataA())
                {
                    case CTRL_PROGRAM:
                    {
                        ProcessList *pl = new ProcessList;
                        pl->port = port;
                        pl->channel = channel;
                        pl->dataB = ev.dataB();
                        bool addEvent = true;
                        for(int i = 0; i < pcevents.size(); ++i)
                        {
                            ProcessList* ipl = pcevents.at(i);
                            if(ipl->port == pl->port && ipl->channel == pl->channel && ipl->dataB == pl->dataB)
                            {
                                addEvent = false;
                                break;
                            }
                        }
                        if(addEvent)
                        {
                            printf("Audio::preloadControllers() Loading event @ tick: %d - on channel: %d - on port: %d - dataA: %d - dataB: %d\n",
                                tick, channel, port, ev.dataA(), ev.dataB());
                            pcevents.append(pl);
                            playEvents->add(MidiPlayEvent(tick, port, channel, ev));
                        }
                    }
                        break;
                    default:
                        break;
                }
            }
        }
        //md->setNextPlayEvent(playEvents->begin());
    }
    midiBusy = false;
}/*}}}*/
