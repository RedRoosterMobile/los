//=========================================================
//  LOS
//  Libre Octave Studio
//    $Id: markerview.h,v 1.4.2.3 2008/08/18 00:15:25 terminator356 Exp $
//  (C) Copyright 2000 Werner Schweer (ws@seh.de)
//=========================================================

#ifndef __MARKERVIEW_H__
#define __MARKERVIEW_H__

#include "cobject.h"

#include <QTreeWidgetItem>

class QCloseEvent;
class QLineEdit;
class QToolBar;
class QToolButton;
class QTreeWidget;

class Marker;
class Pos;
class PosEdit;

//---------------------------------------------------------
//   MarkerItem
//---------------------------------------------------------

class MarkerItem : public QTreeWidgetItem
{
    Marker* _marker;

public:
    MarkerItem(QTreeWidget* parent, Marker* m);

    Marker* marker() const
    {
        return _marker;
    }
    unsigned tick() const;
    const QString name() const;
    bool lock() const;
    void setName(const QString& s);
    void setTick(unsigned t);
    void setLock(bool lck);
};

//---------------------------------------------------------
//   MarkerView
//---------------------------------------------------------

class MarkerView : public TopWin
{
    Q_OBJECT

    QTreeWidget* table;
    QLineEdit* editName;
    PosEdit* editTick;
    QToolButton* lock;
    QToolBar* tools;

    virtual void closeEvent(QCloseEvent*);

private slots:
    void addMarker();
    void addMarker(int);
    void deleteMarker();
    void markerSelectionChanged();
    void nameChanged(const QString&);
    void tickChanged(const Pos&);
    void lockChanged(bool);
    void markerChanged(int);
    void clicked(QTreeWidgetItem*);
    void updateList();
    void songChanged(int);

signals:
    void deleted(unsigned long);
    void closed();

public:
    MarkerView(QWidget* parent);
    ~MarkerView();
    virtual void readStatus(Xml&);
    virtual void writeStatus(int, Xml&) const;
    void nextMarker();
    void prevMarker();
};

#endif

