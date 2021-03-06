//=========================================================
//  LOS
//  Libre Octave Studio
//    $Id: vscale.cpp,v 1.1.1.1 2003/10/27 18:54:41 wschweer Exp $
//  (C) Copyright 1999 Werner Schweer (ws@seh.de)
//=========================================================

#include "vscale.h"

#include <QPainter>
#include <QPaintEvent>

//---------------------------------------------------------
//   paintEvent
//---------------------------------------------------------

void VScale::paintEvent(QPaintEvent*)
{
	int h = height();
	int w = width();
	QPainter p;
	p.begin(this);
	p.drawLine(w / 2, h / 4, w, h / 4);
	p.drawLine(0, h / 2, w, h / 2);
	p.drawLine(w / 2, (3 * h) / 4, w, (3 * h) / 4);
	p.end();
}

