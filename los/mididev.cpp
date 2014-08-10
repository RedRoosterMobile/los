//=========================================================
//  LOS
//  Libre Octave Studio
//  $Id: mididev.cpp,v 1.10.2.6 2009/11/05 03:14:35 terminator356 Exp $
//
//  (C) Copyright 1999-2004 Werner Schweer (ws@seh.de)
//=========================================================

#include <config.h>

#include <QMessageBox>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include "midictrl.h"
#include "song.h"
#include "midi.h"
#include "midiport.h"
#include "mididev.h"
#include "config.h"
#include "globals.h"
#include "audio.h"
#include "midiseq.h"
#include "midiplugins/midiitransform.h"
#include "midimonitor.h"

#ifdef MIDI_DRIVER_MIDI_SERIAL
extern void initMidiSerial();
#endif
extern bool initMidiAlsa();
extern bool initMidiJack();

MidiDeviceList midiDevices;
extern void processMidiInputTransformPlugins(MEvent&);

//---------------------------------------------------------
//   initMidiDevices
//---------------------------------------------------------

void initMidiDevices()
{
#ifdef MIDI_DRIVER_MIDI_SERIAL
    initMidiSerial();
#endif
    if (initMidiAlsa())
    {
        QMessageBox::critical(NULL, "LOS fatal error.", "LOS failed to initialize the\n"
                "Alsa midi subsystem, check\n"
                "your configuration.");
        exit(-1);
    }

    if (initMidiJack())
    {
        QMessageBox::critical(NULL, "LOS fatal error.", "LOS failed to initialize the\n"
                "Jack midi subsystem, check\n"
                "your configuration.");
        exit(-1);
    }
}

//---------------------------------------------------------
//   init
//---------------------------------------------------------

void MidiDevice::init()
{
    _readEnable = false;
    _writeEnable = false;
    _rwFlags = 3;
    _openFlags = 3;
    _port = -1;
    m_nrpnCache.clear();
    for (unsigned int i = 0; i <= kMaxMidiChannels; ++i)
    {
        NRPNCache* c = new NRPNCache;
        c->msb = -1;
        c->lsb = -1;
        c->data_msb = -1;
        c->data_lsb = -1; //Currently unused till further notice
        m_nrpnCache.insert(i, c);
    }
}

//---------------------------------------------------------
//   MidiDevice
//---------------------------------------------------------

MidiDevice::MidiDevice()
: m_cachenrpn(false)
{
    for (unsigned int i = 0; i < kMaxMidiChannels + 1; ++i)
        _tmpRecordCount[i] = 0;

    _sysexFIFOProcessed = false;
    //_sysexWritingChunks = false;
    _sysexReadingChunks = false;

    init();
}

MidiDevice::MidiDevice(const QString& n)
: _name(n),m_cachenrpn(false)
{
    for (unsigned int i = 0; i < kMaxMidiChannels + 1; ++i)
        _tmpRecordCount[i] = 0;

    _sysexFIFOProcessed = false;
    //_sysexWritingChunks = false;
    _sysexReadingChunks = false;

    init();
}

//Send incomming events to the midiMonitor
void MidiDevice::monitorEvent(const MidiRecordEvent& event)/*{{{*/
{
    int type = event.type();
    int chan = event.channel();
    if(type == ME_CONTROLLER && midiMonitor->isManagedInputPort(_port))
    {
        if(cacheNRPN())
        {
            if(event.dataA() == CTRL_HNRPN) //NRPN MSB
            {
                //if(m_nrpnCache.msb != event.dataB())
                //	resetNRPNCache();
                m_nrpnCache.value(chan)->msb = event.dataB();
                //printf("Caching nrpn MSB with value: %d\n", event.dataB());
            }
            else if(event.dataA() == CTRL_LNRPN) //NRPN LSB
            {
                //if(m_nrpnCache.lsb != event.dataB())
                //	resetNRPNCache();
                m_nrpnCache.value(chan)->lsb = event.dataB();
                //printf("Caching nrpn LSB with value: %d\n", event.dataB());
            }
            else if(event.dataA() == CTRL_HDATA && hasNRPNIndex(chan)) //NRPN Data MSB
            {
                //TODO: Create the event and send it to the monitor
                MidiRecordEvent ev(event);
                ev.setPort(_port);
                int val = CTRL_NRPN_OFFSET | (m_nrpnCache.value(chan)->msb << 8) | m_nrpnCache.value(chan)->lsb;
                ev.setA(val);
                midiMonitor->msgSendMidiInputEvent(ev);
                //printf("Sending NRPN event with value: %d\n", val);
            }
            /*else if(event.dataA() == CTRL_LDATA && hasNRPNIndex()) //NRPN Data LSB
            {
                //TODO: Create the event and send it to the monitor
                MidiRecordEvent ev(event);
                ev.setPort(_port);
                ev.setA((m_nrpnCache.msb << 8 | m_nrpnCache.lsb) | CTRL_NRPN14_OFFSET);
                midiMonitor->msgSendMidiInputEvent(ev);
            }*/
            else //Normal event
            {
                //TODO: Reset the cache events
                resetNRPNCache(chan);
                MidiRecordEvent ev(event);
                ev.setPort(_port);
                midiMonitor->msgSendMidiInputEvent(ev);
            }
        }
        else
        {
            //TODO: Reset the cache events
            resetNRPNCache(chan);
            MidiRecordEvent ev(event);
            ev.setPort(_port);
            //printf("Event data part:%d channel:%d value:%d\n", ev.port(), chan, ev.dataB());
            midiMonitor->msgSendMidiInputEvent(ev);
        }
    }
}/*}}}*/

void MidiDevice::monitorOutputEvent(const MidiPlayEvent& event)/*{{{*/
{
    int type = event.type();
    //int chan = event.channel();
    Track* track = event.track();
    if(type == ME_CONTROLLER && !midiMonitor->isManagedInputPort(_port) && track && event.eventSource() != MonitorSource)
    {
        //printf("MidiDevice::monitorOutputEvent Track: %p\n", track);
        midiMonitor->msgSendMidiOutputEvent(event);
    }
}/*}}}*/

//---------------------------------------------------------
//   filterEvent
//    return true if event filtered
//---------------------------------------------------------

bool filterEvent(const MEvent& event, int type, bool thru)
{
    switch (event.type())
    {
        case ME_NOTEON:
        case ME_NOTEOFF:
            if (type & MIDI_FILTER_NOTEON)
                return true;
            break;
        case ME_POLYAFTER:
            if (type & MIDI_FILTER_POLYP)
                return true;
            break;
        case ME_CONTROLLER:
            if (type & MIDI_FILTER_CTRL)
                return true;
            if (!thru && (midiFilterCtrl1 == event.dataA()
                    || midiFilterCtrl2 == event.dataA()
                    || midiFilterCtrl3 == event.dataA()
                    || midiFilterCtrl4 == event.dataA()))
            {
                return true;
            }
            break;
        case ME_PROGRAM:
            if (type & MIDI_FILTER_PROGRAM)
                return true;
            break;
        case ME_AFTERTOUCH:
            if (type & MIDI_FILTER_AT)
                return true;
            break;
        case ME_PITCHBEND:
            if (type & MIDI_FILTER_PITCH)
                return true;
            break;
        case ME_SYSEX:
            if (type & MIDI_FILTER_SYSEX)
                return true;
            break;
        default:
            printf("Unhandled MIDI event type: %d - type: %d\n",event.type(), type);
            break;
    }
    return false;
}

//---------------------------------------------------------
//   afterProcess
//    clear all recorded events after a process cycle
//---------------------------------------------------------

void MidiDevice::afterProcess()
{
    for (unsigned int i = 0; i < kMaxMidiChannels + 1; ++i)
    {
        while (_tmpRecordCount[i]--)
            _recordFifo[i].remove();
    }
}

//---------------------------------------------------------
//   beforeProcess
//    "freeze" fifo for this process cycle
//---------------------------------------------------------

void MidiDevice::beforeProcess()
{
    for (unsigned int i = 0; i < kMaxMidiChannels + 1; ++i)
        _tmpRecordCount[i] = _recordFifo[i].getSize();

    // Reset this.
    _sysexFIFOProcessed = false;
}

//---------------------------------------------------------
//   recordEvent
//---------------------------------------------------------

void MidiDevice::recordEvent(MidiRecordEvent& event)
{
    // p3.3.35
    // TODO: Tested, but record resolution not so good. Switch to wall clock based separate list in MidiDevice. And revert this line.
    //event.setTime(audio->timestamp());
    event.setTime(audio->timestamp());

    //printf("MidiDevice::recordEvent event time:%d\n", event.time());

    // Added by Tim. p3.3.8

    // By T356. Set the loop number which the event came in at.
    //if(audio->isRecording())
    if (audio->isPlaying())
        event.setLoopNum(audio->loopCount());

    if (midiInputTrace)
    {
        printf("MidiInput: ");
        event.dump();
    }

    int typ = event.type();

    if (_port != -1)
    {
        //call our midimonitor with the data, it can then decide what to do
        monitorEvent(event);

    }

    //
    //  process midi event input filtering and
    //    transformation
    //

    processMidiInputTransformPlugins(event);

    if (filterEvent(event, midiRecordType, false))
        return;

    if (!applyMidiInputTransformation(event))
    {
        if (midiInputTrace)
            printf("   midi input transformation: event filtered\n");
        return;
    }

    //
    // transfer noteOn events to gui for step recording and keyboard
    // remote control
    //
    if (typ == ME_NOTEON)
    {
        int pv = ((event.dataA() & 0xff) << 8) + (event.dataB() & 0xff);
        song->putEvent(pv);
    }
    //This was not sending noteoff events at all sometimes causing strange endless note in step rec
    else if(typ == ME_NOTEOFF)
    {
        int pv = ((event.dataA() & 0xff) << 8) + (0x00);
        song->putEvent(pv);
    }

    // Do not bother recording if it is NOT actually being used by a port.
    // Because from this point on, process handles things, by selected port.
    if (_port == -1)
        return;

    // Split the events up into channel fifos. Special 'channel' number 17 for sysex events.
    unsigned int ch = (typ == ME_SYSEX) ? kMaxMidiChannels : event.channel();
    if (_recordFifo[ch].put(MidiPlayEvent(event)))
        printf("MidiDevice::recordEvent: fifo channel %d overflow\n", ch);
}

//---------------------------------------------------------
//   find
//---------------------------------------------------------

MidiDevice* MidiDeviceList::find(const QString& s, int typeHint)
{
    for (iMidiDevice i = begin(); i != end(); ++i)
        if ((typeHint == -1 || typeHint == (*i)->deviceType()) && ((*i)->name() == s))
            return *i;
    return 0;
}

iMidiDevice MidiDeviceList::find(const MidiDevice* dev)
{
    for (iMidiDevice i = begin(); i != end(); ++i)
        if (*i == dev)
            return i;
    return end();
}

//---------------------------------------------------------
//   add
//---------------------------------------------------------

void MidiDeviceList::add(MidiDevice* dev)
{
    bool gotUniqueName = false;
    int increment = 0;
    QString origname = dev->name();
    while (!gotUniqueName)
    {
        gotUniqueName = true;
        // check if the name's been taken
        for (iMidiDevice i = begin(); i != end(); ++i)
        {
            const QString s = (*i)->name();
            if (s == dev->name())
            {
                char incstr[4];
                sprintf(incstr, "_%d", ++increment);
                dev->setName(origname + QString(incstr));
                gotUniqueName = false;
            }
        }
    }

    push_back(dev);
}

//---------------------------------------------------------
//   remove
//---------------------------------------------------------

void MidiDeviceList::remove(MidiDevice* dev)
{
    for (iMidiDevice i = begin(); i != end(); ++i)
    {
        if (*i == dev)
        {
            erase(i);
            break;
        }
    }
}

//---------------------------------------------------------
//   sendNullRPNParams
//---------------------------------------------------------

bool MidiDevice::sendNullRPNParams(int chn, bool nrpn)
{
    if (_port == -1)
        return false;

    int nv = midiPorts[_port].nullSendValue();
    if (nv == -1)
        return false;

    int nvh = (nv >> 8) & 0xff;
    int nvl = nv & 0xff;
    if (nvh != 0xff)
    {
        if (nrpn)
            putMidiEvent(MidiPlayEvent(0, 0, chn, ME_CONTROLLER, CTRL_HNRPN, nvh & 0x7f));
        else
            putMidiEvent(MidiPlayEvent(0, 0, chn, ME_CONTROLLER, CTRL_HRPN, nvh & 0x7f));
    }
    if (nvl != 0xff)
    {
        if (nrpn)
            putMidiEvent(MidiPlayEvent(0, 0, chn, ME_CONTROLLER, CTRL_LNRPN, nvl & 0x7f));
        else
            putMidiEvent(MidiPlayEvent(0, 0, chn, ME_CONTROLLER, CTRL_LRPN, nvl & 0x7f));
    }
    return true;
}

//---------------------------------------------------------
//   putEvent
//    return true if event cannot be delivered
//    TODO: retry on controller putMidiEvent
//---------------------------------------------------------

bool MidiDevice::putEvent(const MidiPlayEvent& ev)
{
    if (!_writeEnable)
        //return true;
        return false;

    if (ev.type() == ME_CONTROLLER)
    {
        int a = ev.dataA();
        int b = ev.dataB();
        int chn = ev.channel();
        if (a == CTRL_PITCH)
        {
            return putMidiEvent(MidiPlayEvent(0, 0, chn, ME_PITCHBEND, b, 0));
        }
        if (a == CTRL_PROGRAM)
        {
            // don't output program changes for GM drum channel
            if (!(song->midiType() == MIDI_TYPE_GM && chn == 9))
            {
                int hb = (b >> 16) & 0xff;
                int lb = (b >> 8) & 0xff;
                int pr = b & 0x7f;
                if (hb != 0xff)
                    putMidiEvent(MidiPlayEvent(0, 0, chn, ME_CONTROLLER, CTRL_HBANK, hb));
                if (lb != 0xff)
                    putMidiEvent(MidiPlayEvent(0, 0, chn, ME_CONTROLLER, CTRL_LBANK, lb));
                return putMidiEvent(MidiPlayEvent(0, 0, chn, ME_PROGRAM, pr, 0));
            }
        }
        if (a < CTRL_14_OFFSET)
        { // 7 Bit Controller
            //putMidiEvent(MidiPlayEvent(0, 0, chn, ME_CONTROLLER, a, b));
            putMidiEvent(ev);
            //printf("a < CTRL_14_OFFSET\n");
        }
        else if (a < CTRL_RPN_OFFSET)
        { // 14 bit high resolution controller
            int ctrlH = (a >> 8) & 0x7f;
            int ctrlL = a & 0x7f;
            int dataH = (b >> 7) & 0x7f;
            int dataL = b & 0x7f;
            putMidiEvent(MidiPlayEvent(0, 0, chn, ME_CONTROLLER, ctrlH, dataH));
            putMidiEvent(MidiPlayEvent(0, 0, chn, ME_CONTROLLER, ctrlL, dataL));
            //printf("a < CTRL_RPN_OFFSET\n");
        }
        else if (a < CTRL_NRPN_OFFSET)
        { // RPN 7-Bit Controller
            int ctrlH = (a >> 8) & 0x7f;
            int ctrlL = a & 0x7f;
            putMidiEvent(MidiPlayEvent(0, 0, chn, ME_CONTROLLER, CTRL_HRPN, ctrlH));
            putMidiEvent(MidiPlayEvent(0, 0, chn, ME_CONTROLLER, CTRL_LRPN, ctrlL));
            putMidiEvent(MidiPlayEvent(0, 0, chn, ME_CONTROLLER, CTRL_HDATA, b));

            // Added by T356. Select null parameters so that subsequent data controller
            //  events do not upset the last *RPN controller.
            sendNullRPNParams(chn, false);
            //printf("a < CTRL_NRPN_OFFSET\n");
        }
        else if (a < CTRL_INTERNAL_OFFSET)
        { // NRPN 7-Bit Controller
            int ctrlH = (a >> 8) & 0x7f;
            int ctrlL = a & 0x7f;
            putMidiEvent(MidiPlayEvent(0, 0, chn, ME_CONTROLLER, CTRL_HNRPN, ctrlH));
            putMidiEvent(MidiPlayEvent(0, 0, chn, ME_CONTROLLER, CTRL_LNRPN, ctrlL));
            putMidiEvent(MidiPlayEvent(0, 0, chn, ME_CONTROLLER, CTRL_HDATA, b));

            sendNullRPNParams(chn, true);
            //printf("a < CTRL_INTERNAL_OFFSET\n");
        }
        else if (a < CTRL_NRPN14_OFFSET)
        { // RPN14 Controller
            int ctrlH = (a >> 8) & 0x7f;
            int ctrlL = a & 0x7f;
            int dataH = (b >> 7) & 0x7f;
            int dataL = b & 0x7f;
            putMidiEvent(MidiPlayEvent(0, 0, chn, ME_CONTROLLER, CTRL_HRPN, ctrlH));
            putMidiEvent(MidiPlayEvent(0, 0, chn, ME_CONTROLLER, CTRL_LRPN, ctrlL));
            putMidiEvent(MidiPlayEvent(0, 0, chn, ME_CONTROLLER, CTRL_HDATA, dataH));
            putMidiEvent(MidiPlayEvent(0, 0, chn, ME_CONTROLLER, CTRL_LDATA, dataL));

            sendNullRPNParams(chn, false);
            //printf("a < CTRL_NRPN14_OFFSET\n");
        }
        else if (a < CTRL_NONE_OFFSET)
        { // NRPN14 Controller
            int ctrlH = (a >> 8) & 0x7f;
            int ctrlL = a & 0x7f;
            int dataH = (b >> 7) & 0x7f;
            int dataL = b & 0x7f;
            putMidiEvent(MidiPlayEvent(0, 0, chn, ME_CONTROLLER, CTRL_HNRPN, ctrlH));
            putMidiEvent(MidiPlayEvent(0, 0, chn, ME_CONTROLLER, CTRL_LNRPN, ctrlL));
            putMidiEvent(MidiPlayEvent(0, 0, chn, ME_CONTROLLER, CTRL_HDATA, dataH));
            putMidiEvent(MidiPlayEvent(0, 0, chn, ME_CONTROLLER, CTRL_LDATA, dataL));

            sendNullRPNParams(chn, true);
            //printf("a < CTRL_NONE_OFFSET\n");
        }
        else
        {
            printf("putEvent: unknown controller type 0x%x\n", a);
        }
        return false;
    }
    return putMidiEvent(ev);
}
