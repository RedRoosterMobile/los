//=========================================================
//  LOS
//  Libre Octave Studio
//  $Id: sig.cpp,v 1.5.2.2 2009/03/09 02:05:17 terminator356 Exp $
//
//  (C) Copyright 2000 Werner Schweer (ws@seh.de)
//=========================================================

#include <stdio.h>
#include "sig.h"
#include "gconfig.h"
#include "globals.h"
#include "xml.h"

SigList sigmap;

//---------------------------------------------------------
//   SigList
//---------------------------------------------------------

SigList::SigList()
{
    insert(std::pair<const unsigned, SigEvent*> (kMaxTick, new SigEvent(4, 4, 0)));
}

//---------------------------------------------------------
//   add
//    signatures are only allowed at the beginning of
//    a bar
//---------------------------------------------------------

void SigList::add(unsigned tick, int z, int n)
{
    if (z == 0 || n == 0)
    {
        if (debugMsg)
            printf("SigList::add illegal signature %d/%d\n", z, n);

        // Added p3.3.43
        return;
    }
    tick = raster1(tick, 0);
    iSigEvent e = upper_bound(tick);
    if (e == end())
    {
        if(debugMsg)
            printf("SigList::add Signal not found tick:%d\n", tick);
        return;
    }

    if (tick == e->second->tick)
    {
        e->second->sig = TimeSignature(z, n);
    }
    else
    {
        SigEvent* ne = e->second;
        SigEvent* ev = new SigEvent(ne->sig, ne->tick);
        ne->sig  = TimeSignature(z, n);
        ne->tick = tick;
        insert(std::pair<const unsigned, SigEvent*> (tick, ev));
    }
    normalize();
}

//---------------------------------------------------------
//   del
//---------------------------------------------------------

void SigList::del(unsigned tick)
{
    // printf("SigList::del(%d)\n", tick);
    iSigEvent e = find(tick);
    if (e == end())
    {
        if(debugMsg)
            printf("SigList::del(%d): not found\n", tick);
        return;
    }
    iSigEvent ne = e;
    ++ne;
    if (ne == end())
    {
        if(debugMsg)
            printf("SigList::del() next event not found!\n");
        return;
    }
    ne->second->sig = e->second->sig;
    ne->second->tick = e->second->tick;
    erase(e);
    normalize();
}

//---------------------------------------------------------
//   SigList::normalize
//---------------------------------------------------------

void SigList::normalize()
{
    TimeSignature sig(0, 0);
    unsigned tick = 0;
    iSigEvent ee;

    for (iSigEvent e = begin(); e != end();)
    {
        if (sig == e->second->sig)
        {
            e->second->tick = tick;
            erase(ee);
        }
        sig = e->second->sig;
        ee = e;
        tick = e->second->tick;
        ++e;
    }

    int bar = 0;
    for (iSigEvent e = begin(); e != end();)
    {
        e->second->bar = bar;
        int delta = e->first - e->second->tick;
        int ticksB = ticks_beat(e->second->sig.n);
        int ticksM = ticksB * e->second->sig.z;
        bar += delta / ticksM;
        if (delta % ticksM) // Teil eines Taktes
            ++bar;
        ++e;
    }
}

//---------------------------------------------------------
//   SigList::dump
//---------------------------------------------------------

void SigList::dump() const
{
    printf("\nSigList:\n");
    for (ciSigEvent i = begin(); i != end(); ++i)
    {
        printf("%6d %06d Bar %3d %02d/%d\n",
               i->first, i->second->tick,
               i->second->bar, i->second->sig.z, i->second->sig.n);
    }
}

void SigList::clear()
{
    for (iSigEvent i = begin(); i != end(); ++i)
        delete i->second;
    SIGLIST::clear();
    insert(std::pair<const unsigned, SigEvent*> (kMaxTick, new SigEvent(4, 4, 0)));
}

//---------------------------------------------------------
//   ticksMeasure
//---------------------------------------------------------

int SigList::ticksMeasure(int Z, int N) const
{
    return ticks_beat(N) * Z;
}

int SigList::ticksMeasure(unsigned tick) const
{
    ciSigEvent i = upper_bound(tick);
    if (i == end())
    {
        if(debugMsg)
            printf("ticksMeasure: not found %d\n", tick);
        return 0;
    }
    return ticksMeasure(i->second->sig.z, i->second->sig.n);
}

//---------------------------------------------------------
//   ticksBeat
//---------------------------------------------------------

int SigList::ticksBeat(unsigned tick) const
{
    ciSigEvent i = upper_bound(tick);
    if (i == end())
    {
        if(debugMsg)
            printf("SigList::ticksBeat event not found! tick:%d\n", tick);
        return 0;
    }
    return ticks_beat(i->second->sig.n);
}

int SigList::ticks_beat(int n) const
{
    int m = config.division;
    switch (n)
    {
    case 1: m <<= 2;
        break; // 1536
    case 2: m <<= 1;
        break; // 768
    case 3: m += m >> 1;
        break; // 384+192
    case 4: break; // 384
    case 8: m >>= 1;
        break; // 192
    case 16: m >>= 2;
        break; // 96
    case 32: m >>= 3;
        break; // 48
    case 64: m >>= 4;
        break; // 24
    case 128: m >>= 5;
        break; // 12
    default:
        break;
    }
    return m;
}

//---------------------------------------------------------
//   timesig
//---------------------------------------------------------

void SigList::timesig(unsigned tick, int& z, int& n) const
{
    ciSigEvent i = upper_bound(tick);
    if (i == end())
    {
        if (debugMsg)
            printf("timesig(%d): not found\n", tick);
        z = 4;
        n = 4;
    }
    else
    {
        z = i->second->sig.z;
        n = i->second->sig.n;
    }
}

//---------------------------------------------------------
//   tickValues
//---------------------------------------------------------

void SigList::tickValues(unsigned t, int* bar, int* beat, unsigned* tick) const
{
    ciSigEvent e = upper_bound(t);
    if (e == end())
    {
        if (debugMsg)
            fprintf(stderr, "tickValues(0x%x) not found(%zd)\n", t, size());
        *bar = 0;
        *beat = 0;
        *tick = 0;
        return;
    }

    int delta = t - e->second->tick;
    int ticksB = ticks_beat(e->second->sig.n);
    int ticksM = ticksB * e->second->sig.z;
    *bar = e->second->bar + delta / ticksM;
    int rest = delta % ticksM;
    *beat = rest / ticksB;
    *tick = rest % ticksB;
}

//---------------------------------------------------------
//   bar2tick
//---------------------------------------------------------

unsigned SigList::bar2tick(int bar, int beat, unsigned tick) const
{
    ciSigEvent e;

    if (bar < 0)
        bar = 0;
    for (e = begin(); e != end();)
    {
        ciSigEvent ee = e;
        ++ee;
        if (ee == end())
            break;
        if (bar < ee->second->bar)
            break;
        e = ee;
    }
    int ticksB = ticks_beat(e->second->sig.n);
    int ticksM = ticksB * e->second->sig.z;
    return e->second->tick + (bar - e->second->bar) * ticksM + ticksB * beat + tick;
}

//---------------------------------------------------------
//   raster
//---------------------------------------------------------

unsigned SigList::raster(unsigned t, int raster) const
{
    if (raster == 1)
        return t;
    ciSigEvent e = upper_bound(t);
    if (e == end())
    {
        if (debugMsg)
            printf("SigList::raster(%x,)\n", t);
        return t;
    }
    int delta = t - e->second->tick;
    int ticksM = ticks_beat(e->second->sig.n) * e->second->sig.z;
    if (raster == 0)
        raster = ticksM;
    int rest = delta % ticksM;
    int bb = (delta / ticksM) * ticksM;
    return e->second->tick + bb + ((rest + raster / 2) / raster) * raster;
}

//---------------------------------------------------------
//   raster1
//    round down
//---------------------------------------------------------

unsigned SigList::raster1(unsigned t, int raster) const
{
    if (raster == 1)
        return t;
    ciSigEvent e = upper_bound(t);
    if (e == end())
    {
        if (debugMsg)
            printf("SigList::raster1 event not found tick:%d\n", t);
        return t;
    }

    int delta = t - e->second->tick;
    int ticksM = ticks_beat(e->second->sig.n) * e->second->sig.z;
    if (raster == 0)
        raster = ticksM;
    int rest = delta % ticksM;
    int bb = (delta / ticksM) * ticksM;
    return e->second->tick + bb + (rest / raster) * raster;
}

//---------------------------------------------------------
//   raster2
//    round up
//---------------------------------------------------------

unsigned SigList::raster2(unsigned t, int raster) const
{
    if (raster == 1)
        return t;
    ciSigEvent e = upper_bound(t);
    if (e == end())
    {
        if (debugMsg)
            printf("SigList::raster2 event not found tick:%d\n", t);
        return t;
    }

    int delta = t - e->second->tick;
    int ticksM = ticks_beat(e->second->sig.n) * e->second->sig.z;
    if (raster == 0)
        raster = ticksM;
    int rest = delta % ticksM;
    int bb = (delta / ticksM) * ticksM;
    return e->second->tick + bb + ((rest + raster - 1) / raster) * raster;
}

//---------------------------------------------------------
//   rasterStep
//---------------------------------------------------------

int SigList::rasterStep(unsigned t, int raster) const
{
    if (raster == 0)
    {
        ciSigEvent e = upper_bound(t);
        if (e == end())
        {
            if (debugMsg)
                printf("SigList::rasterStep event not found tick:%d\n", t);
            return raster;
        }
        return ticks_beat(e->second->sig.n) * e->second->sig.z;
    }
    return raster;
}

//---------------------------------------------------------
//   SigList::write
//---------------------------------------------------------

void SigList::write(int level, Xml& xml) const
{
    xml.tag(level++, "siglist");
    for (ciSigEvent i = begin(); i != end(); ++i)
        i->second->write(level, xml, i->first);
    xml.tag(--level, "/siglist");
}

//---------------------------------------------------------
//   SigList::read
//---------------------------------------------------------

void SigList::read(Xml& xml)
{
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
            if (tag == "sig")
            {
                SigEvent* t = new SigEvent();
                unsigned tick = t->read(xml);
                iSigEvent pos = find(tick);
                if (pos != end())
                    erase(pos);
                insert(std::pair<const unsigned, SigEvent*> (tick, t));
            }
            else
                xml.unknown("SigList");
            break;
        case Xml::Attribut:
            break;
        case Xml::TagEnd:
            if (tag == "siglist")
            {
                normalize();
                return;
            }
        default:
            break;
        }
    }
}

//---------------------------------------------------------
//   SigEvent::write
//---------------------------------------------------------

void SigEvent::write(int level, Xml& xml, int at) const
{
    xml.tag(level++, "sig at=\"%d\"", at);
    xml.intTag(level, "tick", tick);
    xml.intTag(level, "nom", sig.z);
    xml.intTag(level, "denom", sig.n);
    xml.tag(--level, "/sig");
}

//---------------------------------------------------------
//   SigEvent::read
//---------------------------------------------------------

int SigEvent::read(Xml& xml)
{
    int at = 0;
    for (;;)
    {
        Xml::Token token = xml.parse();
        const QString& tag = xml.s1();
        switch (token)
        {
        case Xml::Error:
        case Xml::End:
            return 0;
        case Xml::TagStart:
            if (tag == "tick")
                tick = xml.parseInt();
            else if (tag == "nom")
                sig.z = xml.parseInt();
            else if (tag == "denom")
                sig.n = xml.parseInt();
            else
                xml.unknown("SigEvent");
            break;
        case Xml::Attribut:
            if (tag == "at")
                at = xml.s2().toInt();
            break;
        case Xml::TagEnd:
            if (tag == "sig")
                return at;
        default:
            break;
        }
    }
    return 0;
}


