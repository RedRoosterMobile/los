//=========================================================
//  LOS
//  Libre Octave Studio
//  $Id: event.cpp,v 1.8.2.5 2009/12/20 05:00:35 terminator356 Exp $
//
//  (C) Copyright 2000-2003 Werner Schweer (ws@seh.de)
//=========================================================

#include <stdio.h>

#include "event.h"
#include "eventbase.h"
#include "midievent.h"

//---------------------------------------------------------
//   Event
//---------------------------------------------------------

EventBase::EventBase(EventType t)
{
    _type = t;
    Pos::setType(TICKS);
    refCount = 0;
    _selected = false;
    m_leftclip = 0;
    m_rightclip = 0;
}

EventBase::EventBase(const EventBase& ev)
: PosLen(ev)
{
    refCount = 0;
    _selected = ev._selected;
    _type = ev._type;
    m_leftclip = ev.m_leftclip;
    m_rightclip = ev.m_rightclip;
}

//---------------------------------------------------------
//   move
//---------------------------------------------------------

void EventBase::move(int tickOffset)
{
    setTick(tick() + tickOffset);
}

//---------------------------------------------------------
//   dump
//---------------------------------------------------------

void EventBase::dump(int n) const
{
    for (int i = 0; i < n; ++i)
        putchar(' ');
    printf("Event %p refs:%d ", this, refCount);
    PosLen::dump(n + 2);
}

//---------------------------------------------------------
//   clone
//---------------------------------------------------------

Event Event::clone()
{
    // p3.3.31
    //printf("Event::clone() this:%p\n", this);

    return Event(ev->clone());
}

Event::Event()
{
    ev = 0;
}

Event::Event(EventType t)
{
    ev = new MidiEventBase(t);
    ++(ev->refCount);
}

Event::Event(const Event& e)
{
    ev = e.ev;
    if (ev)
        ++(ev->refCount);
}

Event::Event(EventBase* eb)
{
    ev = eb;
    ++(ev->refCount);
}

Event::~Event()
{
    if (ev && --(ev->refCount) == 0)
    {
        delete ev;
        ev = 0;
    }
}

bool Event::empty() const
{
    return ev == 0;
}

EventType Event::type() const
{
    return ev ? ev->type() : Note;
}

void Event::setType(EventType t)
{
    if (ev && --(ev->refCount) == 0)
    {
        delete ev;
        ev = 0;
    }
    ev = new MidiEventBase(t);
    ++(ev->refCount);
}

Event& Event::operator=(const Event& e)
{
    /*
    if (ev == e.ev)
          return *this;
    if (ev && --(ev->refCount) == 0) {
          delete ev;
          ev = 0;
          }
    ev = e.ev;
    if (ev)
          ++(ev->refCount);
    return *this;
     */

    if (ev != e.ev)
    {
        if (ev && --(ev->refCount) == 0)
        {
            delete ev;
            ev = 0;
        }
        ev = e.ev;
        if (ev)
            ++(ev->refCount);
    }

    return *this;
}

bool Event::operator==(const Event& e) const
{
    return ev == e.ev;
}

int Event::getRefCount() const
{
    return ev->getRefCount();
}

bool Event::selected() const
{
    return ev->_selected;
}

void Event::setSelected(bool val)
{
    ev->_selected = val;
}

void Event::move(int offset)
{
    ev->move(offset);
}

void Event::read(Xml& xml)
{
    ev->read(xml);
}

void Event::write(int a, Xml& xml, const Pos& o, bool forceWavePaths) const
{
    ev->write(a, xml, o, forceWavePaths);
}

void Event::dump(int n) const
{
    ev->dump(n);
}

Event Event::mid(unsigned a, unsigned b)
{
    return Event(ev->mid(a, b));
}

bool Event::isNote() const
{
    return ev->isNote();
}

bool Event::isNoteOff() const
{
    return ev->isNoteOff();
}

bool Event::isNoteOff(const Event& e) const
{
    return ev->isNoteOff(e);
}

int Event::dataA() const
{
    return ev->dataA();
}

int Event::pitch() const
{
    return ev->dataA();
}

void Event::setA(int val)
{
    ev->setA(val);
}

void Event::setPitch(int val)
{
    ev->setA(val);
}

int Event::dataB() const
{
    return ev->dataB();
}

int Event::velo() const
{
    return ev->dataB();
}

void Event::setB(int val)
{
    ev->setB(val);
}

void Event::setVelo(int val)
{
    ev->setB(val);
}

int Event::dataC() const
{
    return ev->dataC();
}

int Event::veloOff() const
{
    return ev->dataC();
}

void Event::setC(int val)
{
    ev->setC(val);
}

void Event::setVeloOff(int val)
{
    ev->setC(val);
}

const unsigned char* Event::data() const
{
    return ev->data();
}

int Event::dataLen() const
{
    return ev->dataLen();
}

void Event::setData(const unsigned char* data, int len)
{
    ev->setData(data, len);
}

const EvData Event::eventData() const
{
    return ev->eventData();
}

const QString Event::name() const
{
    return ev->name();
}

void Event::setName(const QString& s)
{
    ev->setName(s);
}

int Event::spos() const
{
    return ev->spos();
}

void Event::setSpos(int s)
{
    ev->setSpos(s);
}

int Event::rightClip() const
{
    return ev->rightClip();
}
void Event::setRightClip(int clip)
{
    ev->setRightClip(clip);
}
int Event::leftClip() const
{
    return ev->leftClip();
}
void Event::setLeftClip(int clip)
{
    ev->setLeftClip(clip);
}

void Event::setTick(unsigned val)
{
    ev->setTick(val);
}

unsigned Event::tick() const
{
    return ev->tick();
}

unsigned Event::frame() const
{
    return ev->frame();
}

void Event::setFrame(unsigned val)
{
    ev->setFrame(val);
}

void Event::setLenTick(unsigned val)
{
    ev->setLenTick(val);
}

void Event::setLenFrame(unsigned val)
{
    ev->setLenFrame(val);
}

unsigned Event::lenTick() const
{
    return ev->lenTick();
}

unsigned Event::lenFrame() const
{
    return ev->lenFrame();
}

Pos Event::end() const
{
    return ev->end();
}

unsigned Event::endTick() const
{
    return ev->end().tick();
}

unsigned Event::endFrame() const
{
    return ev->end().frame();
}

