//=========================================================
//  LOS
//  Libre Octave Studio
//    $Id: $
//  (C) Copyright 1999 Werner Schweer (ws@seh.de)
//  (C) Copyright 2011 Andrew Williams & Christopher Cherrett
//=========================================================

#ifndef __COMPOSERCANVAS_H__
#define __COMPOSERCANVAS_H__

#include "song.h"
#include "canvas.h"
#include <QHash>
#include <QList>
#include <QIcon>
#include <QPixmap>
#include <QRect>
#include <QPainter>

class QDragMoveEvent;
class QDropEvent;
class QDragLeaveEvent;
class QMouseEvent;
class QKeyEvent;
class QEvent;
class QDragEnterEvent;
class QPoint;

#define beats     4

//---------------------------------------------------------
//   NPart
//    ''visual'' Part
//    wraps Parts with additional information needed
//    for displaying
//---------------------------------------------------------

class NPart : public CItem
{
public:
    NPart(Part* e);

    const QString name() const
    {
        return part()->name();
    }

    void setName(const QString& s)
    {
        part()->setName(s);
    }

    MidiTrack* track() const
    {
        return part()->track();
    }
};

enum ControllerVals { doNothing, movingController, addNewController };

class QLineEdit;
class MidiEditor;
class QMenu;
class Xml;
class CtrlVal;
class CtrlList;
class CurveNodeSelection;

//---------------------------------------------------------
//   ComposerCanvas
//---------------------------------------------------------

class ComposerCanvas : public Canvas
{
    Q_OBJECT

    int* _raster;
    MidiTrackList* tracks;

    Part* resizePart;
    QLineEdit* lineEditor;
    NPart* editPart;
    int curColorIndex;
    int trackOffset;
    bool editMode;
    bool unselectNodes;
    bool show_tip;
    bool build_icons;

    CItemList getSelectedItems();
    virtual void keyPress(QKeyEvent*);
    virtual void mousePress(QMouseEvent*);
    virtual void mouseMove(QMouseEvent* event);
    virtual void mouseRelease(const QPoint&);
    virtual void viewMouseDoubleClickEvent(QMouseEvent*);
    virtual void leaveEvent(QEvent*e);
    virtual void drawItem(QPainter&, const CItem*, const QRect&);
    virtual void drawMoving(QPainter&, const CItem*, const QRect&);
    virtual void updateSelection();
    virtual QPoint raster(const QPoint&) const;
    virtual int y2pitch(int y) const;
    virtual int pitch2y(int p) const;

    virtual void moveCanvasItems(CItemList&, int, int, DragType, int*);
    // Changed by T356.
    //virtual bool moveItem(CItem*, const QPoint&, DragType, int*);
    virtual bool moveItem(CItem*, const QPoint&, DragType);
    virtual CItem* newItem(const QPoint&, int);
    virtual void resizeItem(CItem*, bool);
    virtual void resizeItemLeft(CItem*, QPoint, bool);
    virtual bool deleteItem(CItem*);
    virtual void startUndo(DragType);

    virtual void endUndo(DragType, int);
    virtual void startDrag(CItem*, DragType);
    virtual void dragEnterEvent(QDragEnterEvent*);
    virtual void dragMoveEvent(QDragMoveEvent*);
    virtual void dragLeaveEvent(QDragLeaveEvent*);
    virtual void viewDropEvent(QDropEvent*);

    virtual QMenu* genItemPopup(CItem*);
    virtual void itemPopup(CItem*, int, const QPoint&);

    void glueItem(CItem* item);
    void splitItem(CItem* item, const QPoint&);

    void copy(PartList*);
    void paste(bool clone = false, bool toTrack = true, bool doInsert = false);
    int pasteAt(const QString&, MidiTrack*, unsigned int, bool clone = false, bool toTrack = true);
    void movePartsTotheRight(unsigned int startTick, int length);

    void drawMidiPart(QPainter&, const QRect& rect, EventList* events, MidiTrack *mt, const QRect& r, int pTick, int from, int to, QColor c);
    MidiTrack* y2Track(int) const;
    void drawTopItem(QPainter& p, const QRect& rect);
    void drawTooltipText(QPainter& p, const QRect& rr, int height, double lazySelNodeVal, double lazySelNodePrevVal, int lazySelNodeFrame, bool paintAsDb, CtrlList*);

#if 0
    void checkAutomation(Track * t, const QPoint& pointer, bool addNewCtrl);
    bool checkAutomationForTrack(Track * t, const QPoint &pointer, bool addNewCtrl, Track *rt = 0);
    void selectAutomation(Track * t, const QPoint& pointer);
    bool selectAutomationForTrack(Track * t, const QPoint& pointer, Track* realTrack = 0);
    void processAutomationMovements(QMouseEvent *event);
    void addNewAutomation(QMouseEvent *event);
    double getControlValue(CtrlList*, double);
#endif

protected:
    virtual void drawCanvas(QPainter&, const QRect&);

signals:
    void timeChanged(unsigned);
    void tracklistChanged();
    void dclickPart(MidiTrack*);
    void selectionChanged();
    void dropSongFile(const QString&);
    void dropMidiFile(const QString&);
    void setUsedTool(int);
    void trackChanged(MidiTrack*);
    void selectTrackAbove();
    void selectTrackBelow();
    void renameTrack(MidiTrack*);
    void moveSelectedTracks(int);
    void trackHeightChanged();

    void startEditor(PartList*, int);

//private slots:

public:

    enum
    {
        CMD_CUT_PART, CMD_COPY_PART, CMD_PASTE_PART, CMD_PASTE_CLONE_PART, CMD_PASTE_PART_TO_TRACK, CMD_PASTE_CLONE_PART_TO_TRACK,
    CMD_INSERT_PART, CMD_INSERT_EMPTYMEAS, CMD_REMOVE_SELECTED_AUTOMATION_NODES, CMD_COPY_AUTOMATION_NODES, CMD_PASTE_AUTOMATION_NODES,
    CMD_SELECT_ALL_AUTOMATION
    };

    ComposerCanvas(int* raster, QWidget* parent, int, int);
    void partsChanged();
    void cmd(int);
    int track2Y(MidiTrack*) const;
    bool isEditing() { return editMode; }
    CItem* addPartAtCursor(MidiTrack*);
    virtual void newItem(CItem*, bool);
    Part* currentCanvasPart();

public slots:

    void returnPressed();
    void redirKeypress(QKeyEvent* e)
    {
        keyPress(e);
    }
    void trackViewChanged();
};
#endif
