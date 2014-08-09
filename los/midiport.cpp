//=========================================================
//  LOS
//  Libre Octave Studio
//  $Id: midiport.cpp,v 1.21.2.15 2009/12/07 20:11:51 terminator356 Exp $
//
//  (C) Copyright 2000-2004 Werner Schweer (ws@seh.de)
//=========================================================

//#include "config.h"

#include <QMenu>

#include "mididev.h"
#include "midiport.h"
#include "midictrl.h"
#include "midi.h"
#include "minstrument.h"
#include "xml.h"
#include "globals.h"
#include "gconfig.h"
#include "mpevent.h"
#include "app.h"
#include "song.h"
#include "utils.h"

MidiPort midiPorts[kMaxMidiPorts];
QHash<qint64, MidiPort*> losMidiPorts;

//---------------------------------------------------------
//   initMidiPorts
//---------------------------------------------------------

void initMidiPorts()
{
    //TODO: Remove the need for this code
    //We should populate the losMidiPort hash with ports as we create them
    for (int i = 0; i < kMaxMidiPorts; ++i)
    {
        MidiPort* port = &midiPorts[i];
        ///port->setInstrument(genericMidiInstrument);
        port->setInstrument(registerMidiInstrument("GM")); // Changed by Tim.
    }
}

//---------------------------------------------------------
//   MidiPort
//---------------------------------------------------------

MidiPort::MidiPort()
: _state("not configured")
{
    _defaultInChannels = 0;
    _defaultOutChannels = 0;
    _device = 0;
    _instrument = 0;
    _controller = new MidiCtrlValListList();
    _foundInSongFile = false;
    _patchSequences = QList<PatchSequence*>();
    m_portId = create_id();

    //
    // create minimum set of managed controllers
    // to make midi mixer operational
    //
    for (int i = 0; i < kMaxMidiChannels; ++i)
    {
        addManagedController(i, CTRL_PROGRAM);
        addManagedController(i, CTRL_VOLUME);
        addManagedController(i, CTRL_PANPOT);
    }
}

//---------------------------------------------------------
//   MidiPort
//---------------------------------------------------------

MidiPort::~MidiPort()
{
    delete _controller;
}

//---------------------------------------------------------
//   guiVisible
//---------------------------------------------------------

bool MidiPort::guiVisible() const
{
    return _instrument ? _instrument->guiVisible() : false;
}

//---------------------------------------------------------
//   hasGui
//---------------------------------------------------------

bool MidiPort::hasGui() const
{
    return _instrument ? _instrument->hasGui() : false;
}

//---------------------------------------------------------
//   setDevice
//---------------------------------------------------------

void MidiPort::setMidiDevice(MidiDevice* dev)
{
    // close old device
    if (_device)
    {
        _device->setPort(-1);
        _device->close();
    }
    // set-up new device
    if (dev)
    {
        for (int i = 0; i < kMaxMidiPorts; ++i)
        {
            MidiPort* mp = &midiPorts[i];
            if (mp->device() == dev)
            {
                // move device
                _state = mp->state();
                mp->clearDevice();
                break;
            }
        }
        _device = dev;
        _state = _device->open();
        _device->setPort(portno());
    }
    else
        // dev is null, clear this device
        clearDevice();
}

//---------------------------------------------------------
//   clearDevice
//---------------------------------------------------------

void MidiPort::clearDevice()
{
    _device = 0;
    _state = "not configured";
}

//---------------------------------------------------------
//   portno
//---------------------------------------------------------

int MidiPort::portno() const
{
    for (int i = 0; i < kMaxMidiPorts; ++i)
    {
        if (&midiPorts[i] == this)
            return i;
    }
    return -1;
}

//---------------------------------------------------------
//   midiPortsPopup
//---------------------------------------------------------

//QPopupMenu* midiPortsPopup(QWidget* parent)

QMenu* midiPortsPopup(QWidget* parent, int checkPort)
{
    QMenu* p = new QMenu(parent);
    for (int i = 0; i < kMaxMidiPorts; ++i)
    {
        MidiPort* port = &midiPorts[i];
        QString name;
        name.sprintf("%d:%s", port->portno() + 1, port->portname().toLatin1().constData());
        QAction *act = p->addAction(name);
        act->setData(i);

        if (i == checkPort)
            act->setChecked(true);
    }
    return p;
}

//---------------------------------------------------------
//   portname
//---------------------------------------------------------

const QString& MidiPort::portname() const
{
    //static const QString none("<none>");
    static const QString none(QT_TRANSLATE_NOOP("@default", "<none>"));
    if (_device)
        return _device->name();
    else
        return none;
}

//---------------------------------------------------------
//   tryCtrlInitVal
//---------------------------------------------------------

void MidiPort::tryCtrlInitVal(int chan, int ctl, int val)
{
    //printf("Entering: MidiPort::tryCtrlInitVal\n");
    if (_instrument)
    {
        MidiControllerList* cl = _instrument->controller();
        ciMidiController imc = cl->find(ctl);
        if (imc != cl->end())
        {
            MidiController* mc = imc->second;
            int initval = mc->initVal();

            // Initialize from either the instrument controller's initial value, or the supplied value.
            if (initval != CTRL_VAL_UNKNOWN)
            {
                if (_device)
                {
                    MidiPlayEvent ev(0, portno(), chan, ME_CONTROLLER, ctl, initval + mc->bias());
                    _device->putEvent(ev);
                }
                setHwCtrlStates(chan, ctl, CTRL_VAL_UNKNOWN, initval + mc->bias());

                return;
            }
        }
    }

    if (_device)
    {
        //MidiPlayEvent ev(song->cpos(), portno(), chan, ME_CONTROLLER, ctl, val);
        MidiPlayEvent ev(0, portno(), chan, ME_CONTROLLER, ctl, val);
        _device->putEvent(ev);
    }
    // Set it once so the 'last HW value' is set, and control knobs are positioned at the value...
    //setHwCtrlState(chan, ctl, val);
    // Set it again so that control labels show 'off'...
    //setHwCtrlState(chan, ctl, CTRL_VAL_UNKNOWN);
    setHwCtrlStates(chan, ctl, CTRL_VAL_UNKNOWN, val);
}

//---------------------------------------------------------
//   setInstument
//---------------------------------------------------------

void MidiPort::setInstrument(MidiInstrument* i)
{
    _instrument = i;
}


//---------------------------------------------------------
//   sendGmInitValues
//---------------------------------------------------------

void MidiPort::sendGmInitValues()
{
    for (int i = 0; i < kMaxMidiChannels; ++i)
    {
        // By T356. Initialize from instrument controller if it has an initial value, otherwise use the specified value.
        // Tested: Ultimately, a track's controller stored values take priority by sending any 'zero time' value
        //  AFTER these GM/GS/XG init routines are called via initDevices().
        //tryCtrlInitVal(i, CTRL_PROGRAM, 0);
        tryCtrlInitVal(i, CTRL_PITCH, 0);
        tryCtrlInitVal(i, CTRL_VOLUME, 100);
        tryCtrlInitVal(i, CTRL_PANPOT, 64);
        tryCtrlInitVal(i, CTRL_REVERB_SEND, 40);
        tryCtrlInitVal(i, CTRL_CHORUS_SEND, 0);
    }
}

//---------------------------------------------------------
//   sendGsInitValues
//---------------------------------------------------------

void MidiPort::sendGsInitValues()
{
    sendGmInitValues();
}

//---------------------------------------------------------
//   sendXgInitValues
//---------------------------------------------------------

void MidiPort::sendXgInitValues()
{
    for (int i = 0; i < kMaxMidiChannels; ++i)
    {
        // By T356. Initialize from instrument controller if it has an initial value, otherwise use the specified value.
        tryCtrlInitVal(i, CTRL_PROGRAM, 0);
        tryCtrlInitVal(i, CTRL_MODULATION, 0);
        tryCtrlInitVal(i, CTRL_PORTAMENTO_TIME, 0);
        tryCtrlInitVal(i, CTRL_VOLUME, 0x64);
        tryCtrlInitVal(i, CTRL_PANPOT, 0x40);
        tryCtrlInitVal(i, CTRL_EXPRESSION, 0x7f);
        tryCtrlInitVal(i, CTRL_SUSTAIN, 0x0);
        tryCtrlInitVal(i, CTRL_PORTAMENTO, 0x0);
        tryCtrlInitVal(i, CTRL_SOSTENUTO, 0x0);
        tryCtrlInitVal(i, CTRL_SOFT_PEDAL, 0x0);
        tryCtrlInitVal(i, CTRL_HARMONIC_CONTENT, 0x40);
        tryCtrlInitVal(i, CTRL_RELEASE_TIME, 0x40);
        tryCtrlInitVal(i, CTRL_ATTACK_TIME, 0x40);
        tryCtrlInitVal(i, CTRL_BRIGHTNESS, 0x40);
        tryCtrlInitVal(i, CTRL_REVERB_SEND, 0x28);
        tryCtrlInitVal(i, CTRL_CHORUS_SEND, 0x0);
        tryCtrlInitVal(i, CTRL_VARIATION_SEND, 0x0);
    }
}

//---------------------------------------------------------
//   sendGmOn
//    send GM-On message to midi device and keep track
//    of device state
//---------------------------------------------------------

void MidiPort::sendGmOn()
{
    sendSysex(gmOnMsg, gmOnMsgLen);
}

//---------------------------------------------------------
//   sendGsOn
//    send Roland GS-On message to midi device and keep track
//    of device state
//---------------------------------------------------------

void MidiPort::sendGsOn()
{
    sendSysex(gsOnMsg2, gsOnMsg2Len);
    sendSysex(gsOnMsg3, gsOnMsg3Len);
}

//---------------------------------------------------------
//   sendXgOn
//    send Yamaha XG-On message to midi device and keep track
//    of device state
//---------------------------------------------------------

void MidiPort::sendXgOn()
{
    sendSysex(xgOnMsg, xgOnMsgLen);
}

//---------------------------------------------------------
//   sendSysex
//    send SYSEX message to midi device
//---------------------------------------------------------

void MidiPort::sendSysex(const unsigned char* p, int n)
{
    if (_device)
    {
        MidiPlayEvent event(0, 0, ME_SYSEX, p, n);
        _device->putEvent(event);
    }
}

//---------------------------------------------------------
//   addManagedController
//---------------------------------------------------------

MidiCtrlValList* MidiPort::addManagedController(int channel, int ctrl)
{
    iMidiCtrlValList cl = _controller->find(channel, ctrl);
    if (cl == _controller->end())
    {
        MidiCtrlValList* pvl = new MidiCtrlValList(ctrl);
        _controller->add(channel, pvl);
        return pvl;
    }
    else
        return cl->second;
}

//---------------------------------------------------------
//   limitValToInstrCtlRange
//---------------------------------------------------------

int MidiPort::limitValToInstrCtlRange(MidiController* mc, int val)
{
    if (!_instrument || !mc || val == CTRL_VAL_UNKNOWN)
        return val;

    int mn = mc->minVal();
    int mx = mc->maxVal();
    int bias = mc->bias();

    // Subtract controller bias from value.
    val -= bias;

    // Limit value to controller range.
    if (val < mn)
        val = mn;
    else
        if (val > mx)
        val = mx;

    // Re-add controller bias to value.
    val += bias;

    return val;
}

int MidiPort::limitValToInstrCtlRange(int ctl, int val)
{
    if (!_instrument || val == CTRL_VAL_UNKNOWN)
        return val;

    MidiControllerList* cl = _instrument->controller();

    // Is it a drum controller?
    MidiController *mc = NULL;

    {
        // It's not a drum controller. Find it as a regular controller instead.
        iMidiController imc = cl->find(ctl);
        if (imc != cl->end())
            mc = imc->second;
    }

    // If it's a valid controller, limit the value to the instrument controller range.
    if (mc)
        return limitValToInstrCtlRange(mc, val);

    return val;
}

//---------------------------------------------------------
//   sendEvent
//    return true, if event cannot be delivered
//---------------------------------------------------------

bool MidiPort::sendEvent(const MidiPlayEvent& ev, bool forceSend)
{
    if (ev.type() == ME_CONTROLLER)
    {
        //TODO: This is where PC are added
        //      printf("current sustain %d %d %d\n", hwCtrlState(ev.channel(),CTRL_SUSTAIN), CTRL_SUSTAIN, ev.dataA());

        // Added by T356.
        int da = ev.dataA();
        int db = ev.dataB();
        db = limitValToInstrCtlRange(da, db);


        // Removed by T356.
        //
        //  optimize controller settings
        //
        if (!setHwCtrlState(ev.channel(), da, db))
        {
            if (debugMsg)
                printf("setHwCtrlState failed\n");
            if (!forceSend)
                return false;
        }
    }
    else if (ev.type() == ME_PITCHBEND)
    {
        int da = limitValToInstrCtlRange(CTRL_PITCH, ev.dataA());

        if (!setHwCtrlState(ev.channel(), CTRL_PITCH, da))
        {
            if(!forceSend)
                return false;
        }
    }
    else if (ev.type() == ME_PROGRAM)
    {
        if (!setHwCtrlState(ev.channel(), CTRL_PROGRAM, ev.dataA()))
        {
            if(!forceSend)
                return false;
        }
    }


    if (!_device)
    {
        if (debugMsg)
            printf("no device for this midi port\n");
        return true;
    }
    //printf("MidiPort::sendEvent\n");
    return _device->putEvent(ev);
}

//---------------------------------------------------------
//   lastValidHWCtrlState
//---------------------------------------------------------

int MidiPort::lastValidHWCtrlState(int ch, int ctrl) const
{
    ch &= 0xff;
    iMidiCtrlValList cl = _controller->find(ch, ctrl);
    if (cl == _controller->end())
    {
        return CTRL_VAL_UNKNOWN;
    }
    MidiCtrlValList* vl = cl->second;
    return vl->lastValidHWVal();
}

//---------------------------------------------------------
//   hwCtrlState
//---------------------------------------------------------

int MidiPort::hwCtrlState(int ch, int ctrl) const
{
    ch &= 0xff;
    iMidiCtrlValList cl = _controller->find(ch, ctrl);
    if (cl == _controller->end())
    {
        //if (debugMsg)
        //      printf("hwCtrlState: chan %d ctrl 0x%x not found\n", ch, ctrl);
        return CTRL_VAL_UNKNOWN;
    }
    MidiCtrlValList* vl = cl->second;
    return vl->hwVal();
}

//---------------------------------------------------------
//   setHwCtrlState
//   Returns false if value is already equal, true if value is set.
//---------------------------------------------------------

bool MidiPort::setHwCtrlState(int ch, int ctrl, int val)
{
    MidiCtrlValList* vl = addManagedController(ch, ctrl);

    return vl->setHwVal(val);
}

//---------------------------------------------------------
//   setHwCtrlStates
//   Sets current and last HW values.
//   Handy for forcing labels to show 'off' and knobs to show specific values
//    without having to send two messages.
//   Returns false if both values are already set, true if either value is changed.
//---------------------------------------------------------

bool MidiPort::setHwCtrlStates(int ch, int ctrl, int val, int lastval)
{
    // This will create a new value list if necessary, otherwise it returns the existing list.
    MidiCtrlValList* vl = addManagedController(ch, ctrl);

    return vl->setHwVals(val, lastval);
}

//---------------------------------------------------------
//   setControllerVal
//   This function sets a controller value,
//   creating the controller if necessary.
//   Returns true if a value was actually added or replaced.
//---------------------------------------------------------

bool MidiPort::setControllerVal(int ch, int tick, int ctrl, int val, Part* part)
{
    MidiCtrlValList* pvl;
    iMidiCtrlValList cl = _controller->find(ch, ctrl);
    if (cl == _controller->end())
    {
        pvl = new MidiCtrlValList(ctrl);
        _controller->add(ch, pvl);
    }
    else
        pvl = cl->second;

    return pvl->addMCtlVal(tick, val, part);
}

//---------------------------------------------------------
//   getCtrl
//---------------------------------------------------------

int MidiPort::getCtrl(int ch, int tick, int ctrl) const
{
    iMidiCtrlValList cl = _controller->find(ch, ctrl);
    if (cl == _controller->end())
    {
        //if (debugMsg)
        //      printf("getCtrl: controller %d(0x%x) for channel %d not found size %zd\n",
        //         ctrl, ctrl, ch, _controller->size());
        return CTRL_VAL_UNKNOWN;
    }
    return cl->second->value(tick);
}

int MidiPort::getCtrl(int ch, int tick, int ctrl, Part* part) const
{
    iMidiCtrlValList cl = _controller->find(ch, ctrl);
    if (cl == _controller->end())
    {
        //if (debugMsg)
        //      printf("getCtrl: controller %d(0x%x) for channel %d not found size %zd\n",
        //         ctrl, ctrl, ch, _controller->size());
        return CTRL_VAL_UNKNOWN;
    }
    return cl->second->value(tick, part);
}
//---------------------------------------------------------
//   deleteController
//---------------------------------------------------------

void MidiPort::deleteController(int ch, int tick, int ctrl, Part* part)
{
    iMidiCtrlValList cl = _controller->find(ch, ctrl);
    if (cl == _controller->end())
    {
        if (debugMsg)
            printf("deleteController: controller %d(0x%x) for channel %d not found size %zd\n",
                ctrl, ctrl, ch, _controller->size());
        return;
    }

    cl->second->delMCtlVal(tick, part);
}

//---------------------------------------------------------
//   midiController
//---------------------------------------------------------

MidiController* MidiPort::midiController(int num) const
{
    {
        MidiControllerList* mcl = _instrument->controller();
        for (iMidiController i = mcl->begin(); i != mcl->end(); ++i)
        {
            int cn = i->second->num();
            if (cn == num)
                return i->second;
            // wildcard?
            if (((cn & 0xff) == 0xff) && ((cn & ~0xff) == (num & ~0xff)))
                return i->second;
        }
    }

    for (iMidiController i = defaultMidiController.begin(); i != defaultMidiController.end(); ++i)
    {
        int cn = i->second->num();
        if (cn == num)
            return i->second;
        // wildcard?
        if (((cn & 0xff) == 0xff) && ((cn & ~0xff) == (num & ~0xff)))
            return i->second;
    }


    QString name = midiCtrlName(num);
    int min = 0;
    int max = 127;

    MidiController::ControllerType t = midiControllerType(num);
    switch (t)
    {
        case MidiController::RPN:
        case MidiController::NRPN:
        case MidiController::Controller7:
            max = 127;
            break;
        case MidiController::Controller14:
        case MidiController::RPN14:
        case MidiController::NRPN14:
            max = 16383;
            break;
        case MidiController::Program:
            max = 0xffffff;
            break;
        case MidiController::Pitch:
            max = 8191;
            min = -8192;
            break;
        case MidiController::Velo: // cannot happen
            break;
    }
    MidiController* c = new MidiController(name, num, min, max, 0);
    defaultMidiController.add(c);
    return c;
}

int MidiPort::nullSendValue()
{
    return _instrument ? _instrument->nullSendValue() : -1;
}

void MidiPort::setNullSendValue(int v)
{
    if (_instrument)
        _instrument->setNullSendValue(v);
}

void MidiPort::addPatchSequence(PatchSequence* p)
{
    if (p)
        _patchSequences.push_front(p);
}

void MidiPort::appendPatchSequence(PatchSequence* p)
{
    if (p)
        _patchSequences.push_back(p);
}

void MidiPort::deletePatchSequence(PatchSequence* p)
{
    if (p && _patchSequences.indexOf(p) != -1)
    {
        PatchSequence* ps = _patchSequences.takeAt(_patchSequences.indexOf(p));
        delete ps; //FIXME: Not sure about this
    }
}

void MidiPort::insertPatchSequence(int p, PatchSequence* ps)
{
    if (p < _patchSequences.size() && ps)
        _patchSequences.insert(p, ps);
    else if (ps)
        appendPatchSequence(ps);
        //addPatchSequence(ps);
}

//---------------------------------------------------------
//   writeRouting    // p3.3.50
//---------------------------------------------------------

void MidiPort::writeRouting(int level, Xml& xml) const
{
    // If this device is not actually in use by the song, do not write any routes.
    // This prevents bogus routes from being saved and propagated in the los file.
    if (!device())
        return;

    QString s;

    for (ciRoute r = _outRoutes.begin(); r != _outRoutes.end(); ++r)
    {
        if (r->type == Route::TRACK_ROUTE && !r->name().isEmpty())
        {
            s = QT_TRANSLATE_NOOP("@default", "Route");
            if (r->channel != -1 && r->channel != 0)
                s += QString(QT_TRANSLATE_NOOP("@default", " channelMask=\"%1\"")).arg(r->channel); // Use new channel mask.
            xml.tag(level++, s.toLatin1().constData());

            xml.tag(level, "source mportId=\"%lld\"/", m_portId);

            s = QT_TRANSLATE_NOOP("@default", "dest");
            s += QString(QT_TRANSLATE_NOOP("@default", " name=\"%1\" trackId=\"%2\"/")).arg(Xml::xmlString(r->name())).arg(r->track->id());
            xml.tag(level, s.toLatin1().constData());

            xml.etag(--level, "Route");
        }
    }
}

