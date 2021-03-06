//=========================================================
//  LOS
//  Libre Octave Studio
//    $Id: view.h,v 1.2.2.1 2008/01/26 07:23:21 terminator356 Exp $
//  (C) Copyright 1999 Werner Schweer (ws@seh.de)
//=========================================================

#ifndef __VIEW_H__
#define __VIEW_H__

#include <QWidget>

class QDropEvent;
class QKeyEvent;
class QMouseEvent;
class QPaintEvent;
class QPainter;
class QPixmap;
class QResizeEvent;

//---------------------------------------------------------
//   View
//    horizontal View with double buffering
//---------------------------------------------------------

class View : public QWidget
{
    Q_OBJECT
    QPixmap pm; // for double buffering
    bool pmValid;
    QPixmap bgPixmap; // background Pixmap
    QBrush brush;
    bool _virt;

protected:
    int xorg;
    int yorg;
    int xpos, ypos;
    float xmag;
    float ymag;

    virtual void keyPressEvent(QKeyEvent* event);
    virtual void mousePressEvent(QMouseEvent* event);
    virtual void mouseDoubleClickEvent(QMouseEvent* event);
    virtual void mouseMoveEvent(QMouseEvent* event);
    virtual void mouseReleaseEvent(QMouseEvent* event);
    virtual void dropEvent(QDropEvent* event);

    virtual void draw(QPainter&, const QRect&)
    {
    }

    virtual void drawOverlay(QPainter&, const QRect&)
    {
    }

    virtual QRect overlayRect() const
    {
        return QRect(0, 0, 0, 0);
    }

    virtual void pdraw(QPainter&, const QRect&);

    virtual void paintEvent(QPaintEvent* ev);
    void redraw(const QRect&);

    void paint(const QRect& r);

    virtual void resizeEvent(QResizeEvent*);
    virtual void viewKeyPressEvent(QKeyEvent*);

    virtual void viewMousePressEvent(QMouseEvent*)
    {
    }

    virtual void viewMouseDoubleClickEvent(QMouseEvent*)
    {
    }

    virtual void viewMouseMoveEvent(QMouseEvent*)
    {
    }

    virtual void viewMouseReleaseEvent(QMouseEvent*)
    {
    }

    virtual void viewDropEvent(QDropEvent*)
    {
    }

    int mapy(int y) const;
    int mapyDev(int y) const;
    int mapxDev(int x) const;
    int rmapy(int y) const;
    int rmapyDev(int y) const;
    //QRect devToVirt(const QRect&);

    void setPainter(QPainter& p);

public slots:
    void setXPos(int);
    void setYPos(int);
    void setXMag(float xs);
    void setYMag(float ys);
    void redraw();

public:
    View(QWidget*, int, int, const char* name = 0);
    void setBg(const QPixmap& pm);

    void setBg(const QColor& color)
    {
        brush.setColor(color);
        redraw();
    }

    QRect map(const QRect&) const;
    QPoint map(const QPoint&) const;
    QRect mapDev(const QRect&) const;
    QPoint mapDev(const QPoint&) const;

    void setXOffset(int v)
    {
        setXPos(mapx(v));
    }

    int xOffset() const
    {
        return mapxDev(xpos) - xorg;
    }

    int xOffsetDev() const
    {
        return xpos - rmapx(xorg);
    }

    int yOffset() const
    {
        return mapyDev(ypos) - yorg;
    }

    void setOrigin(int x, int y);

    void setVirt(bool flag)
    {
        _virt = flag;
    }

    bool virt() const
    {
        return _virt;
    }
    int rmapxDev(int x) const;
    int rmapx(int x) const;
    int mapx(int x) const;

};

#endif

