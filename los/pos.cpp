//=========================================================
//  LOS
//  Libre Octave Studio
//  $Id: pos.cpp,v 1.11.2.1 2006/09/19 19:07:08 spamatica Exp $
//
//  (C) Copyright 2000 Werner Schweer (ws@seh.de)
//=========================================================

#include <cmath>

#include "pos.h"
#include "xml.h"
#include "tempo.h"
#include "globals.h"

#include "sig.h"

//---------------------------------------------------------
//   Pos
//---------------------------------------------------------

Pos::Pos()
{
    _type = TICKS;
    _tick = 0;
    _frame = 0;
    sn = -1;
}

Pos::Pos(const Pos& p)
{
    _type = p._type;
    sn = p.sn;
    _tick = p._tick;
    _frame = p._frame;
}

Pos::Pos(unsigned t, bool ticks)
{
    if (ticks)
    {
        _type = TICKS;
        _tick = t;
        _frame = 0;
    }
    else
    {
        _type = FRAMES;
        _frame = t;
        _tick = 0;
    }
    sn = -1;
}

Pos::Pos(const QString& s)
{
    int m, b, t;
    sscanf(s.toLatin1(), "%04d.%02d.%03d", &m, &b, &t);
    _tick = sigmap.bar2tick(m, b, t);
    _type = TICKS;
    _frame = 0;
    sn = -1;
}

Pos::Pos(int measure, int beat, int tick)
{
    _tick = sigmap.bar2tick(measure, beat, tick);
    _type = TICKS;
    _frame = 0;
    sn = -1;
}

//---------------------------------------------------------
//   setType
//---------------------------------------------------------

void Pos::setType(TType t)
{
    if (t == _type)
        return;

    if (_type == TICKS)
    {
        // convert from ticks to frames
        _frame = tempomap.tick2frame(_tick, _frame, &sn);
    }
    else
    {
        // convert from frames to ticks
        _tick = tempomap.frame2tick(_frame, _tick, &sn);
    }
    _type = t;
}

//---------------------------------------------------------
//   tick
//---------------------------------------------------------

unsigned Pos::tick() const
{
    if (_type == FRAMES)
        _tick = tempomap.frame2tick(_frame, _tick, &sn);
    return _tick;
}

//---------------------------------------------------------
//   frame
//---------------------------------------------------------

unsigned Pos::frame() const
{
    if (_type == TICKS)
        _frame = tempomap.tick2frame(_tick, _frame, &sn);
    return _frame;
}

//---------------------------------------------------------
//   setTick
//---------------------------------------------------------

void Pos::setTick(unsigned pos)
{
    _tick = pos;
    sn = -1;
    if (_type == FRAMES)
        _frame = tempomap.tick2frame(pos, &sn);
}

//---------------------------------------------------------
//   setFrame
//---------------------------------------------------------

void Pos::setFrame(unsigned pos)
{
    _frame = pos;
    sn = -1;
    if (_type == TICKS)
        _tick = tempomap.frame2tick(pos, &sn);
}

//---------------------------------------------------------
//   write
//---------------------------------------------------------

void Pos::write(int level, Xml& xml, const char* name) const
{
    xml.nput(level++, "<%s ", name);

    switch (_type)
    {
        case TICKS:
            xml.nput("tick=\"%d\"", _tick);
            break;
        case FRAMES:
            xml.nput("frame=\"%d\"", _frame);
            break;
    }
    xml.put(" />", name);
}

//---------------------------------------------------------
//   read
//---------------------------------------------------------

void Pos::read(Xml& xml, const char* name)
{
    sn = -1;
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
                xml.unknown(name);
                break;

            case Xml::Attribut:
                if (tag == "tick")
                {
                    _tick = xml.s2().toInt();
                    _type = TICKS;
                }
                else if (tag == "frame")
                {
                    _frame = xml.s2().toInt();
                    _type = FRAMES;
                }
                else if (tag == "sample")
                { // obsolete
                    _frame = xml.s2().toInt();
                    _type = FRAMES;
                }
                else
                    xml.unknown(name);
                break;

            case Xml::TagEnd:
                if (tag == name)
                    return;
            default:
                break;
        }
    }
}

//---------------------------------------------------------
//   mbt
//---------------------------------------------------------

void Pos::mbt(int* bar, int* beat, int* tk) const
{
    sigmap.tickValues(tick(), bar, beat, (unsigned*) tk);
}

//---------------------------------------------------------
//   dump
//---------------------------------------------------------

void Pos::dump(int /*n*/) const
{
    printf("Pos(%s, sn=%d, ", type() == FRAMES ? "Frames" : "Ticks", sn);
    switch (type())
    {
        case FRAMES:
            printf("samples=%d)", _frame);
            break;
        case TICKS:
            printf("ticks=%d)", _tick);
            break;
    }
}

//---------------------------------------------------------
//   operators
//---------------------------------------------------------

Pos& Pos::operator+=(Pos a)
{
    switch (_type)
    {
        case FRAMES:
            _frame += a.frame();
            break;
        case TICKS:
            _tick += a.tick();
            break;
    }
    sn = -1; // invalidate cached values
    return *this;
}

Pos& Pos::operator+=(int a)
{
    switch (_type)
    {
        case FRAMES:
            _frame += a;
            break;
        case TICKS:
            _tick += a;
            break;
    }
    sn = -1; // invalidate cached values
    return *this;
}

Pos operator+(Pos a, int b)
{
    Pos c;
    c.setType(a.type());
    return c += b;
}

Pos operator+(Pos a, Pos b)
{
    Pos c = a;
    return c += b;
}

bool Pos::operator>=(const Pos& s) const
{
    if (_type == FRAMES)
        return _frame >= s.frame();
    else
        return _tick >= s.tick();
}

bool Pos::operator>(const Pos& s) const
{
    if (_type == FRAMES)
        return _frame > s.frame();
    else
        return _tick > s.tick();
}

bool Pos::operator<(const Pos& s) const
{
    if (_type == FRAMES)
        return _frame < s.frame();
    else
        return _tick < s.tick();
}

bool Pos::operator<=(const Pos& s) const
{
    if (_type == FRAMES)
        return _frame <= s.frame();
    else
        return _tick <= s.tick();
}

bool Pos::operator==(const Pos& s) const
{
    if (_type == FRAMES)
        return _frame == s.frame();
    else
        return _tick == s.tick();
}

//---------------------------------------------------------
//   PosLen
//---------------------------------------------------------

PosLen::PosLen()
{
    _lenTick = 0;
    _lenFrame = 0;
    sn = -1;
}

PosLen::PosLen(const PosLen& p)
: Pos(p)
{
    _lenTick = p._lenTick;
    _lenFrame = p._lenFrame;
    sn = -1;
}

//---------------------------------------------------------
//   end
//---------------------------------------------------------

Pos PosLen::end() const
{
    Pos pos(*this);
    pos.invalidSn();
    switch (type())
    {
        case FRAMES:
            pos.setFrame(pos.frame() + _lenFrame);
            break;
        case TICKS:
            pos.setTick(pos.tick() + _lenTick);
            break;
    }
    return pos;
}


//---------------------------------------------------------
//   write
//---------------------------------------------------------

void PosLen::write(int level, Xml& xml, const char* name) const
{
    xml.nput(level++, "<%s ", name);

    switch (type())
    {
        case TICKS:
            xml.nput("tick=\"%d\" len=\"%d\"", tick(), _lenTick);
            break;
        case FRAMES:
            xml.nput("sample=\"%d\" len=\"%d\"", frame(), _lenFrame);
            break;
    }
    xml.put(" />", name);
}

//---------------------------------------------------------
//   read
//---------------------------------------------------------

void PosLen::read(Xml& xml, const char* name)
{
    sn = -1;
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
                xml.unknown(name);
                break;

            case Xml::Attribut:
                if (tag == "tick")
                {
                    setType(TICKS);
                    setTick(xml.s2().toInt());
                }
                else if (tag == "sample")
                {
                    setType(FRAMES);
                    setFrame(xml.s2().toInt());
                }
                else if (tag == "len")
                {
                    int n = xml.s2().toInt();
                    switch (type())
                    {
                        case TICKS:
                            setLenTick(n);
                            break;
                        case FRAMES:
                            setLenFrame(n);
                            break;
                    }
                }
                else
                    xml.unknown(name);
                break;

            case Xml::TagEnd:
                if (tag == name)
                    return;
            default:
                break;
        }
    }
}

//---------------------------------------------------------
//   setLenTick
//---------------------------------------------------------

void PosLen::setLenTick(unsigned len)
{
    _lenTick = len;
    sn = -1;
    //      if (type() == FRAMES)
    _lenFrame = tempomap.deltaTick2frame(tick(), tick() + len, &sn);
}

//---------------------------------------------------------
//   setLenFrame
//---------------------------------------------------------

void PosLen::setLenFrame(unsigned len)
{
    _lenFrame = len;
    sn = -1;
    //      if (type() == TICKS)
    _lenTick = tempomap.deltaFrame2tick(frame(), frame() + len, &sn);
}

//---------------------------------------------------------
//   lenTick
//---------------------------------------------------------

unsigned PosLen::lenTick() const
{
    if (type() == FRAMES)
        _lenTick = tempomap.deltaFrame2tick(frame(), frame() + _lenFrame, &sn);
    return _lenTick;
}

//---------------------------------------------------------
//   lenFrame
//---------------------------------------------------------

unsigned PosLen::lenFrame() const
{
    if (type() == TICKS)
        _lenFrame = tempomap.deltaTick2frame(tick(), tick() + _lenTick, &sn);
    return _lenFrame;
}

//---------------------------------------------------------
//   dump
//---------------------------------------------------------

void PosLen::dump(int n) const
{
    Pos::dump(n);
    printf("  Len(");
    switch (type())
    {
        case FRAMES:
            printf("samples=%d)\n", _lenFrame);
            break;
        case TICKS:
            printf("ticks=%d)\n", _lenTick);
            break;
    }
}
