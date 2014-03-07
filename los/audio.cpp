//=========================================================
//  LOS
//  Libre Octave Studio
//  $Id: audio.cpp,v 1.59.2.30 2009/12/20 05:00:35 terminator356 Exp $
//
//  (C) Copyright 2001-2004 Werner Schweer (ws@seh.de)
//=========================================================

#include <cmath>
#include <errno.h>

#include <QSocketNotifier>

#include "app.h"
#include "song.h"
#include "node.h"
#include "driver/audiodev.h"
//#include "driver/audiodev.h"   // p4.0.2
#include "mididev.h"
#include "driver/alsamidi.h"
//#include "driver/alsamidi.h"   // p4.0.2
#include "audioprefetch.h"
#include "audio.h"
#include "wave.h"
#include "midictrl.h"
#include "midiseq.h"
#include "midi.h"
#include "event.h"
#include "gconfig.h"
#include "pos.h"
#include "midiport.h"
#include "globals.h"
#include "minstrument.h"

extern double curTime();
Audio* audio;
AudioDevice* audioDevice; // current audio device in use

const char* seqMsgList[] = {
    "SEQM_ADD_TRACK", "SEQM_REMOVE_TRACK", "SEQM_CHANGE_TRACK", "SEQM_MOVE_TRACK",
    "SEQM_ADD_PART", "SEQM_REMOVE_PART", "SEQM_REMOVE_PART_LIST", "SEQM_CHANGE_PART",
    "SEQM_ADD_EVENT", "SEQM_REMOVE_EVENT", "SEQM_CHANGE_EVENT",
    "SEQM_ADD_TEMPO", "SEQM_SET_TEMPO", "SEQM_REMOVE_TEMPO", "SEQM_REMOVE_TEMPO_RANGE",
    "SEQM_ADD_SIG", "SEQM_REMOVE_SIG",
    "SEQM_SET_GLOBAL_TEMPO",
    "SEQM_UNDO", "SEQM_REDO",
    "SEQM_RESET_DEVICES", "SEQM_INIT_DEVICES", "SEQM_PANIC",
    "SEQM_MIDI_LOCAL_OFF",
    "SEQM_SET_MIDI_DEVICE",
    "SEQM_PLAY_MIDI_EVENT",
    "SEQM_SET_HW_CTRL_STATE",
    "SEQM_SET_HW_CTRL_STATES",
    "SEQM_SET_TRACK_OUT_PORT",
    "SEQM_SET_TRACK_OUT_CHAN",
    "SEQM_SCAN_ALSA_MIDI_PORTS",
    "SEQM_UPDATE_SOLO_STATES",
    "MIDI_SHOW_INSTR_GUI",
    "AUDIO_RECORD",
    "AUDIO_ROUTEADD", "AUDIO_ROUTEREMOVE", "AUDIO_REMOVEROUTES",
    "AUDIO_VOL", "AUDIO_PAN",
    "AUDIO_SET_SEG_SIZE",
    "AUDIO_SET_PREFADER", "AUDIO_SET_CHANNELS",
    "AUDIO_SET_PLUGIN_CTRL_VAL",
    "AUDIO_SWAP_CONTROLLER_IDX",
    "AUDIO_CLEAR_CONTROLLER_EVENTS",
    "AUDIO_SEEK_PREV_AC_EVENT",
    "AUDIO_SEEK_NEXT_AC_EVENT",
    "AUDIO_ERASE_AC_EVENT",
    "AUDIO_ERASE_RANGE_AC_EVENTS",
    "AUDIO_ADD_AC_EVENT",
    "AUDIO_SET_SOLO",
    "MS_PROCESS", "MS_STOP", "MS_SET_RTC", "MS_UPDATE_POLL_FD",
    "SEQM_IDLE", "SEQM_SEEK", "SEQM_PRELOAD_PROGRAM", "SEQM_REMOVE_TRACK_GROUP"
};

const char* audioStates[] = {
    "STOP", "START_PLAY", "PLAY", "LOOP1", "LOOP2", "SYNC", "PRECOUNT"
};


//---------------------------------------------------------
//   Audio
//---------------------------------------------------------

Audio::Audio()
{
    _running = false;
    recording = false;
    idle = false;
    _freewheel = false;
    _bounce = false;
    //loopPassed    = false;
    _loopFrame = 0;
    _loopCount = 0;

    _pos.setType(Pos::FRAMES);
    _pos.setFrame(0);
    curTickPos = 0;

    syncTime = 0.0;
    syncFrame = 0;
    frameOffset = 0;

    state = STOP;
    msg = 0;

    // Changed by Tim. p3.3.8
    //startRecordPos.setType(Pos::TICKS);
    //endRecordPos.setType(Pos::TICKS);
    startRecordPos.setType(Pos::FRAMES);
    endRecordPos.setType(Pos::FRAMES);

    //---------------------------------------------------
    //  establish pipes/sockets
    //---------------------------------------------------

    int filedes[2]; // 0 - reading   1 - writing
    if (pipe(filedes) == -1)
    {
        perror("creating pipe0");
        exit(-1);
    }
    fromThreadFdw = filedes[1];
    fromThreadFdr = filedes[0];
    int rv = fcntl(fromThreadFdw, F_SETFL, O_NONBLOCK);
    if (rv == -1)
        perror("set pipe O_NONBLOCK");

    if (pipe(filedes) == -1)
    {
        perror("creating pipe1");
        exit(-1);
    }
    sigFd = filedes[1];
    QSocketNotifier* ss = new QSocketNotifier(filedes[0], QSocketNotifier::Read);
    song->connect(ss, SIGNAL(activated(int)), song, SLOT(seqSignal(int)));
}

//---------------------------------------------------------
//   start
//    start audio processing
//---------------------------------------------------------

extern bool initJackAudio();

bool Audio::start()
{
    //process(segmentSize);   // warm up caches
    state = STOP;
    _loopCount = 0;
    los->setHeartBeat();
    if (!audioDevice)
    {
        if (!initJackAudio())
        {
        }
        else
        {
            printf("Failed to init audio!\n");
            return false;
        }
    }

    if(audioDevice)
    {
        audioDevice->start(realTimePriority);

        _running = true;

        // shall we really stop JACK transport and locate to
        // saved position?
        audioDevice->stopTransport();
        audioDevice->seekTransport(song->cPos());
        return true;
    }
    return false;
}

//---------------------------------------------------------
//   stop
//    stop audio processing
//---------------------------------------------------------

void Audio::stop(bool)
{
    if (audioDevice)
        audioDevice->stop();
    _running = false;
}

//---------------------------------------------------------
//   sync
//    return true if sync is completed
//---------------------------------------------------------

bool Audio::sync(int jackState, unsigned frame)
{

    // Changed by Tim. p3.3.24
    /*
          bool done = true;
          if (state == LOOP1)
                state = LOOP2;
          else {
                if (_pos.frame() != frame) {
                      Pos p(frame, false);
                      seek(p);
                      }
                state = State(jackState);
                if (!_freewheel)
                      //done = audioPrefetch->seekDone;
                      done = audioPrefetch->seekDone();
                }

          return done;
     */
    bool done = true;
    if (state == LOOP1)
        state = LOOP2;
    else
    {
        State s = State(jackState);
        //
        //  STOP -> START_PLAY      start rolling
        //  STOP -> STOP            seek in stop state
        //  PLAY -> START_PLAY  seek in play state

        if (state != START_PLAY)
        {
            //Pos p(frame, AL::FRAMES);
            //    seek(p);
            Pos p(frame, false);
            seek(p);
            if (!_freewheel)
                done = audioPrefetch->seekDone();
            if (s == START_PLAY)
                state = START_PLAY;
        }
        else
        {
            //if (frame != _seqTime.pos.frame()) {
            if (frame != _pos.frame())
            {
                // seek during seek
                //seek(Pos(frame, AL::FRAMES));
                seek(Pos(frame, false));
            }
            done = audioPrefetch->seekDone();
        }
    }
    return done;

}

//---------------------------------------------------------
//   setFreewheel
//---------------------------------------------------------

void Audio::setFreewheel(bool val)
{
    // printf("JACK: freewheel callback %d\n", val);
    _freewheel = val;
}

//---------------------------------------------------------
//   shutdown
//---------------------------------------------------------

void Audio::shutdown()
{
    _running = false;
    printf("Audio::shutdown()\n");
    write(sigFd, "S", 1);
}

//---------------------------------------------------------
//   process
//    process one audio buffer at position "_pos "
//    of size "frames"
//---------------------------------------------------------

void Audio::process(unsigned frames)
{
    if (!checkAudioDevice()) return;
    if (msg)
    {
        processMsg(msg);
        int sn = msg->serialNo;
        msg = 0; // dont process again
        int rv = write(fromThreadFdw, &sn, sizeof (int));
        if (rv != sizeof (int))
        {
            fprintf(stderr, "audio: write(%d) pipe failed: %s\n",
                    fromThreadFdw, strerror(errno));
        }
    }

    if (idle)
    {
        return;
    }

    int jackState = audioDevice->getState();

    //if(debugMsg)
    //  printf("Audio::process Current state:%s jackState:%s\n", audioStates[state], audioStates[jackState]);

    if (state == START_PLAY && jackState == PLAY)
    {
        _loopCount = 0;
        startRolling();
        if (_bounce)
            write(sigFd, "f", 1);
    }
    else if (state == LOOP2 && jackState == PLAY)
    {
        ++_loopCount; // Number of times we have looped so far
        Pos newPos(_loopFrame, false);
        seek(newPos);
        startRolling();
    }
    else if (isPlaying() && jackState == STOP)
    {
        // p3.3.43 Make sure to stop bounce and freewheel mode, for example if user presses stop
        //  in QJackCtl before right-hand marker is reached (which is handled below).
        //printf("Audio::process isPlaying() && jackState == STOP\n");
        if (freewheel())
        {
        //printf("  stopping bounce...\n");
          _bounce = false;
          write(sigFd, "F", 1);
        }

        stopRolling();
    }
    else if (state == START_PLAY && jackState == STOP)
    {
        state = STOP;
        if (_bounce)
        {
            audioDevice->startTransport();
        }
        else
            write(sigFd, "3", 1); // abort rolling
    }
    else if (state == STOP && jackState == PLAY)
    {
        _loopCount = 0;
        startRolling();
    }
    else if (state == LOOP1 && jackState == PLAY)
        ; // treat as play
    else if (state == LOOP2 && jackState == START_PLAY)
    {
        ; // sync cycle
    }
    else if (state != jackState)
        printf("JACK: state transition %s -> %s ?\n",
            audioStates[state], audioStates[jackState]);

    // printf("p %s %s %d\n", audioStates[jackState], audioStates[state], _pos.frame());

    int samplePos = _pos.frame();
    int offset = 0; // buffer offset in audio buffers

    if (isPlaying())
    {
        if (!freewheel())
            audioPrefetch->msgTick();

        if (_bounce && _pos >= song->rPos())
        {
            _bounce = false;
            write(sigFd, "F", 1);
            return;
        }

        //
        //  check for end of song
        //
        if ((curTickPos >= song->len())
                && !(song->record()
                || _bounce
                || song->loop()))
        {
            //if(debugMsg)
            //  printf("Audio::process curTickPos >= song->len\n");

            audioDevice->stopTransport();
            return;
        }

        //
        //  check for loop end
        //
        if (state == PLAY && song->loop() && !_bounce)
        {
            const Pos& loop = song->rPos();
            unsigned n = loop.frame() - samplePos - (3 * frames);
            if (n < frames)
            {
                // loop end in current cycle
                unsigned lpos = song->lPos().frame();
                // adjust loop start so we get exact loop len
                if (n > lpos)
                    n = 0;
                state = LOOP1;
                _loopFrame = lpos - n;

                // clear sustain
                for (int i = 0; i < MIDI_PORTS; ++i)
                {
                    MidiPort* mp = &midiPorts[i];
                    for (int ch = 0; ch < MIDI_CHANNELS; ++ch)
                    {
                        if (mp->hwCtrlState(ch, CTRL_SUSTAIN) == 127)
                        {
                            if (mp->device() != NULL)
                            {
                                //printf("send clear sustain!!!!!!!! port %d ch %d\n", i,ch);
                                MidiPlayEvent ev(0, i, ch, ME_CONTROLLER, CTRL_SUSTAIN, 0);
                                // may cause problems, called from audio thread
                                mp->device()->putEvent(ev);
                            }
                        }
                    }
                }

                //audioDevice->seekTransport(_loopFrame);
                Pos lp(_loopFrame, false);
                audioDevice->seekTransport(lp);


                // printf("  process: seek to %d, end %d\n", _loopFrame, loop.frame());
            }
        }


        // p3.3.25
        {
            Pos ppp(_pos);
            ppp += frames;
            nextTickPos = ppp.tick();
        }
    }
    //
    // resync with audio interface
    //
    syncFrame = audioDevice->framePos();
    syncTime = curTime();
    frameOffset = syncFrame - samplePos;

    //printf("Audio::process calling process1:\n");

    process1(samplePos, offset, frames);

    if (isPlaying())
    {
        _pos += frames;
        curTickPos = nextTickPos;
    }
}

//---------------------------------------------------------
//   process1
//---------------------------------------------------------

void Audio::process1(unsigned samplePos, unsigned offset, unsigned frames)
{
    if (midiSeqRunning)
    {
        processMidi();
    }
    //midiSeq->msgProcess();
}

//---------------------------------------------------------
//   processMsg
//---------------------------------------------------------

void Audio::processMsg(AudioMsg* msg)
{
    switch (msg->id)
    {
        case AUDIO_RECORD:
            //msg->snode->setRecordFlag2(msg->ival);
            //TODO: hook this and send midi cc to bcf2000
            break;
        case AUDIO_ROUTEADD:
            addRoute(msg->sroute, msg->droute);
            break;
        case AUDIO_ROUTEREMOVE:
            removeRoute(msg->sroute, msg->droute);
            break;
        case AUDIO_REMOVEROUTES: // p3.3.55
            removeAllRoutes(msg->sroute, msg->droute);
            break;
        case AUDIO_VOL:
            //msg->snode->setVolume(msg->dval);
            //TODO: hook this and send midi cc to bcf2000
            break;
        case AUDIO_PAN:
            //msg->snode->setPan(msg->dval);
            //TODO: hook this and send midi cc to bcf2000
            break;
        case AUDIO_SET_PREFADER:
            //msg->snode->setPrefader(msg->ival);
            break;
        case AUDIO_SET_CHANNELS:
            //msg->snode->setChannels(msg->ival);
            break;
        case AUDIO_SWAP_CONTROLLER_IDX:
            //msg->snode->swapControllerIDX(msg->a, msg->b);
            break;
        case AUDIO_CLEAR_CONTROLLER_EVENTS:
            //msg->snode->clearControllerEvents(msg->ival);
            break;
        case AUDIO_SEEK_PREV_AC_EVENT:
            //msg->snode->seekPrevACEvent(msg->ival);
            break;
        case AUDIO_SEEK_NEXT_AC_EVENT:
            //msg->snode->seekNextACEvent(msg->ival);
            break;
        case AUDIO_ERASE_AC_EVENT:
            //msg->snode->eraseACEvent(msg->ival, msg->a);
            break;
        case AUDIO_ERASE_RANGE_AC_EVENTS:
            //msg->snode->eraseRangeACEvents(msg->ival, msg->a, msg->b);
            break;
        case AUDIO_ADD_AC_EVENT:
            //msg->snode->addACEvent(msg->ival, msg->a, msg->dval);
            break;
        case AUDIO_SET_SOLO:
            msg->track->setSolo((bool)msg->ival);
            //TODO: hook this and send midi cc to bcf2000
            break;

        case AUDIO_SET_SEG_SIZE:
            segmentSize = msg->ival;
            sampleRate = msg->iival;
#if 0 //TODO
            audioOutput.segmentSizeChanged();
            for (int i = 0; i < mixerGroups; ++i)
                audioGroups[i].segmentSizeChanged();
            for (iSynthI ii = synthiInstances.begin(); ii != synthiInstances.end(); ++ii)
                (*ii)->segmentSizeChanged();
#endif
            break;
        case SEQM_RESET_DEVICES:
            for (int i = 0; i < MIDI_PORTS; ++i)
            {
                if(midiPorts[i].device())
                    midiPorts[i].instrument()->reset(i, song->mtype());
            }
            break;
        case SEQM_INIT_DEVICES:
            initDevices();
            break;
        case SEQM_MIDI_LOCAL_OFF:
            sendLocalOff();
            break;
        case SEQM_PANIC:
            panic();
            break;
        case SEQM_PLAY_MIDI_EVENT:
        {
            MidiPlayEvent* ev = (MidiPlayEvent*) (msg->p1);
            midiPorts[ev->port()].sendEvent(*ev);
            // Record??
        }
            break;
        case SEQM_SET_HW_CTRL_STATE:
        {
            MidiPort* port = (MidiPort*) (msg->p1);
            port->setHwCtrlState(msg->a, msg->b, msg->c);
        }
            break;
        case SEQM_SET_HW_CTRL_STATES:
        {
            MidiPort* port = (MidiPort*) (msg->p1);
            port->setHwCtrlStates(msg->a, msg->b, msg->c, msg->ival);
        }
            break;
        case SEQM_SCAN_ALSA_MIDI_PORTS:
            alsaScanMidiPorts();
            break;
        case SEQM_ADD_TEMPO:
        case SEQM_REMOVE_TEMPO:
        case SEQM_REMOVE_TEMPO_RANGE:
        case SEQM_SET_GLOBAL_TEMPO:
        case SEQM_SET_TEMPO:
            song->processMsg(msg);
            if (isPlaying())
            {
                if (!checkAudioDevice()) return;
                _pos.setTick(curTickPos);
                int samplePos = _pos.frame();
                syncFrame = audioDevice->framePos();
                syncTime = curTime();
                frameOffset = syncFrame - samplePos;
            }
            break;
        case SEQM_ADD_TRACK:
        case SEQM_REMOVE_TRACK:
        case SEQM_REMOVE_TRACK_GROUP:
        case SEQM_CHANGE_TRACK:
        case SEQM_ADD_PART:
        case SEQM_REMOVE_PART:
        case SEQM_REMOVE_PART_LIST:
        case SEQM_CHANGE_PART:
        case SEQM_SET_TRACK_OUT_CHAN:
        case SEQM_SET_TRACK_OUT_PORT:
        case SEQM_PRELOAD_PROGRAM:
            midiSeq->sendMsg(msg);
            break;

        case SEQM_IDLE:
            idle = msg->a;
            midiSeq->sendMsg(msg);
            break;

        default:
            song->processMsg(msg);
            break;
    }
}

//---------------------------------------------------------
//   seek
//    - called before start play
//    - initiated from gui
//---------------------------------------------------------

void Audio::seek(const Pos& p)
{
    if (_pos == p)
    {
        if (debugMsg)
            printf("Audio::seek already there\n");
        return;
    }

    //printf("Audio::seek frame:%d\n", p.frame());
    _pos = p;
    if (!checkAudioDevice()) return;
    syncFrame = audioDevice->framePos();
    frameOffset = syncFrame - _pos.frame();
    curTickPos = _pos.tick();

    midiSeq->msgSeek(); // handle stuck notes and set
    // controller for new position

    if (state != LOOP2 && !freewheel())
    {
        // We need to force prefetch to update,
        //  to ensure the most recent data. Things can happen to a part
        //  before play is pressed - such as part muting, part moving etc.
        // Without a force, the wrong data was being played.
        audioPrefetch->msgSeek(_pos.frame(), true);
    }

    write(sigFd, "G", 1); // signal seek to gui
}

//---------------------------------------------------------
//   startRolling
//---------------------------------------------------------

void Audio::startRolling()
{
    if (debugMsg)
        printf("startRolling - loopCount=%d, _pos=%d\n", _loopCount, _pos.tick());

    if (_loopCount == 0)
    {
        startRecordPos = _pos;
    }

    if (song->record())
    {
        recording = true;
    }
    state = PLAY;
    write(sigFd, "1", 1); // Play

    // reenable sustain
    for (int i = 0; i < MIDI_PORTS; ++i)
    {
        MidiPort* mp = &midiPorts[i];
        for (int ch = 0; ch < MIDI_CHANNELS; ++ch)
        {
            if (mp->hwCtrlState(ch, CTRL_SUSTAIN) == 127)
            {
                if (mp->device() != NULL)
                {
                    //printf("send enable sustain!!!!!!!! port %d ch %d\n", i,ch);
                    MidiPlayEvent ev(0, i, ch, ME_CONTROLLER, CTRL_SUSTAIN, 127);

                    // may cause problems, called from audio thread
                    mp->device()->playEvents()->add(ev);
                }
            }
        }
    }
}

//---------------------------------------------------------
//   stopRolling
//---------------------------------------------------------

void Audio::stopRolling()
{
    state = STOP;
    midiSeq->msgStop();

#if 1 //TODO
    //---------------------------------------------------
    //    reset sustain
    //---------------------------------------------------


    // clear sustain
    for (int i = 0; i < MIDI_PORTS; ++i)
    {
        MidiPort* mp = &midiPorts[i];
        for (int ch = 0; ch < MIDI_CHANNELS; ++ch)
        {
            if(!mp->device())
                continue;
            if (mp->hwCtrlState(ch, CTRL_SUSTAIN) == 127)
            {
                //printf("send clear sustain!!!!!!!! port %d ch %d\n", i,ch);
                MidiPlayEvent susEv(0, i, ch, ME_CONTROLLER, CTRL_SUSTAIN, 0);
                // may cause problems, called from audio thread
                mp->device()->putEvent(susEv);
            }
            mp->sendEvent(MidiPlayEvent(0, i, ch, ME_CONTROLLER, CTRL_ALL_SOUNDS_OFF, 0), true);
            mp->sendEvent(MidiPlayEvent(0, i, ch, ME_CONTROLLER, CTRL_ALL_NOTES_OFF, 0), true);
            mp->sendEvent(MidiPlayEvent(0, i, ch, ME_CONTROLLER, CTRL_RESET_ALL_CTRL, 0), true);
        }
    }

#endif

    recording = false;
    endRecordPos = _pos;
    write(sigFd, "0", 1); // STOP
}

//---------------------------------------------------------
//   recordStop
//    execution environment: gui thread
//---------------------------------------------------------

void Audio::recordStop()
{
    if (debugMsg)
        printf("recordStop - startRecordPos=%d\n", startRecordPos.tick());
    audio->msgIdle(true); // gain access to all data structures

    song->startUndo();

    MidiTrackList* ml = song->midis();
    for (iMidiTrack it = ml->begin(); it != ml->end(); ++it)
    {
        MidiTrack* mt = *it;
        MPEventList* mpel = mt->mpevents();
        EventList* el = mt->events();

        //---------------------------------------------------
        //    resolve NoteOff events, Controller etc.
        //---------------------------------------------------

        //buildMidiEventList(el, mpel, mt, config.division, true);
        // Do SysexMeta. Do loops.
        buildMidiEventList(el, mpel, mt, config.division, true, true);
        song->cmdAddRecordedEvents(mt, el, startRecordPos.tick());
        el->clear();
        mpel->clear();
    }

    audio->msgIdle(false);
    song->endUndo(0);
    song->setRecord(false);
}

//---------------------------------------------------------
//   curFrame
//    extrapolates current play frame on syncTime/syncFrame
//---------------------------------------------------------

unsigned int Audio::curFrame() const
{
    return lrint((curTime() - syncTime) * sampleRate) + syncFrame;
}

//---------------------------------------------------------
//   timestamp
//---------------------------------------------------------

int Audio::timestamp() const
{
    int t = curFrame() - frameOffset;
    return t;
}

//---------------------------------------------------------
//   sendMsgToGui
//---------------------------------------------------------

void Audio::sendMsgToGui(char c)
{
    write(sigFd, &c, 1);
}

