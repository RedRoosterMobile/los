//=========================================================
//  LOS
//  Libre Octave Studio
//    $Id: listedit.cpp,v 1.11.2.11 2009/05/24 21:43:44 terminator356 Exp $
//  (C) Copyright 1999 Werner Schweer (ws@seh.de)
//=========================================================

#include <QAction>
#include <QActionGroup>
#include <QCloseEvent>
#include <QKeyEvent>
#include <QShowEvent>
#include <QMenu>
#include <QMenuBar>
#include <QSignalMapper>
#include <QToolBar>
#include <QTreeWidgetItem>

#include "listedit.h"
#include "mtscale.h"
#include "globals.h"
#include "icons.h"
#include "editevent.h"
#include "xml.h"
#include "pitchedit.h"
#include "song.h"
#include "audio.h"
#include "shortcuts.h"
#include "midi.h"
#include "event.h"
#include "midiport.h"
#include "midictrl.h"

#include "traverso_shared/TConfig.h"

//---------------------------------------------------------
//   EventListItem
//---------------------------------------------------------

class EventListItem : public QTreeWidgetItem
{
public:
    Event event;
    MidiPart* part;

    EventListItem(QTreeWidget* parent, Event ev, MidiPart* p)
    : QTreeWidgetItem(parent)
    {
        event = ev;
        part = p;
    }
    virtual QString text(int col) const;

    virtual bool operator<(const QTreeWidgetItem & other) const
    {
        int col = other.treeWidget()->sortColumn();
        EventListItem* eli = (EventListItem*) & other;
        switch (col)
        {
            case 0:
                return event.tick() < eli->event.tick();
                break;
            case 1:
                return part->tick() + event.tick() < (eli->part->tick() + eli->event.tick());
                break;
            case 2:
                return text(col).localeAwareCompare(other.text(col)) < 0;
                break;
            case 3:
                return part->track()->outChannel() < eli->part->track()->outChannel();
                break;
            case 4:
                return event.dataA() < eli->event.dataA();
                break;
            case 5:
                return event.dataB() < eli->event.dataB();
                break;
            case 6:
                return event.dataC() < eli->event.dataC();
                break;
            case 7:
                return event.lenTick() < eli->event.lenTick();
                break;
            case 8:
                return text(col).localeAwareCompare(other.text(col)) < 0;
                break;
            default:
                break;
        }
        return 0;
    }
};

/*---------------------------------------------------------
 *    midi_meta_name
 *---------------------------------------------------------*/

static QString midiMetaComment(const Event& ev)
{
    int meta = ev.dataA();
    QString s = midiMetaName(meta);

    switch (meta)
    {
        case 0:
        case 0x2f:
        case 0x51:
        case 0x54:
        case 0x58:
        case 0x59:
        case 0x74:
        case 0x7f: return s;

        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
        case 0x0a:
        case 0x0b:
        case 0x0c:
        case 0x0d:
        case 0x0e:
        case 0x0f:
        {
            s += QString(": ");
            const char* txt = (char*) (ev.data());
            int len = ev.dataLen();
            char buffer[len + 1];
            memcpy(buffer, txt, len);
            buffer[len] = 0;

            for (int i = 0; i < len; ++i)
            {
                if (buffer[i] == '\n' || buffer[i] == '\r')
                    buffer[i] = ' ';
            }
            return s + QString(buffer);
        }

        case 0x20:
        case 0x21:
        default:
        {
            s += QString(": ");
            int i;
            int len = ev.lenTick();
            int n = len > 10 ? 10 : len;
            for (i = 0; i < n; ++i)
            {
                if (i >= ev.dataLen())
                    break;
                s += QString(" 0x");
                QString k;
                k.setNum(ev.data()[i] & 0xff, 16);
                s += k;
            }
            if (i == 10)
                s += QString("...");
            return s;
        }
    }
}

//---------------------------------------------------------
//   closeEvent
//---------------------------------------------------------

void ListEdit::closeEvent(QCloseEvent* e)
{
    emit deleted((unsigned long) this);
    e->accept();
}

void ListEdit::showEvent(QShowEvent*)
{
    songChanged(SC_SELECTION);
}

//---------------------------------------------------------
//   songChanged
//---------------------------------------------------------

void ListEdit::songChanged(int type)
{
    if (type == 0)
        return;
    if (type & (SC_PART_REMOVED | SC_PART_MODIFIED
            | SC_PART_INSERTED | SC_EVENT_REMOVED | SC_EVENT_MODIFIED
            | SC_EVENT_INSERTED | SC_SELECTION))
    {
        if (type & (SC_PART_REMOVED | SC_PART_INSERTED))
            genPartlist();
        // close window if editor has no parts anymore
        if (parts()->empty())
        {
            close();
            return;
        }
        liste->setSortingEnabled(false);
        if (type == SC_SELECTION)
        {
            bool update = false;
            QTreeWidgetItem* ci = 0;
            liste->blockSignals(true);
            for (int row = 0; row < liste->topLevelItemCount(); ++row)
            {
                QTreeWidgetItem* i = liste->topLevelItem(row);
                if (i->isSelected() ^ ((EventListItem*) i)->event.selected())
                {
                    i->setSelected(((EventListItem*) i)->event.selected());
                    if (i->isSelected())
                        ci = i;
                    update = true;
                }
            }
            if (update)
            {
                if (ci)
                {
                    liste->setCurrentItem(ci);
                    liste->scrollToItem(ci, QAbstractItemView::EnsureVisible);
                }
                //liste->update();
            }
            liste->blockSignals(false);
        }
        else
        {
            curPart = 0;
            curTrack = 0;
            liste->clear();
            for (iPart p = parts()->begin(); p != parts()->end(); ++p)
            {
                MidiPart* part = (MidiPart*) (p->second);
                if (part->sn() == curPartId)
                    curPart = part;
                EventList* el = part->events();
                for (iEvent i = el->begin(); i != el->end(); ++i)
                {
                    EventListItem* item = new EventListItem(liste, i->second, part);
                    for (int col = 0; col < liste->columnCount(); ++col)
                        item->setText(col, item->text(col));
                    item->setSelected(i->second.selected());
                    if (item->event.tick() == (unsigned) selectedTick)
                    { //prevent compiler warning: comparison of signed/unsigned)
                        liste->setCurrentItem(item);
                        item->setSelected(true);
                        liste->scrollToItem(item, QAbstractItemView::EnsureVisible);
                    }
                }
            }
        }

        // p3.3.34
        //if (curPart == 0)
        //      curPart  = (MidiPart*)(parts()->begin()->second);
        //curTrack = curPart->track();
        if (!curPart)
        {
            if (!parts()->empty())
            {
                curPart = (MidiPart*) (parts()->begin()->second);
                if (curPart)
                    curTrack = curPart->track();
                else
                    curPart = 0;
            }
        }
    }
    liste->setSortingEnabled(true);
}

//---------------------------------------------------------
//   text
//---------------------------------------------------------

QString EventListItem::text(int col) const
{
    QString s;
    QString commentLabel;
    switch (col)
    {
        case 0:
            s.setNum(event.tick());
            break;
        case 1:
        {
            int t = event.tick() + part->tick();
            int bar, beat;
            unsigned tick;
            sigmap.tickValues(t, &bar, &beat, &tick);
            s.sprintf("%04d.%02d.%03d", bar + 1, beat + 1, tick);
        }
            break;
        case 2:
            switch (event.type())
            {
                case Note:
                    s = QString("Note");
                    break;
                case Controller:
                {
                    const char* cs;
                    switch (midiControllerType(event.dataA()))
                    {
                        case MidiController::Controller7: cs = "Ctrl7";
                            break;
                        case MidiController::Controller14: cs = "Ctrl14";
                            break;
                        case MidiController::RPN: cs = "RPN";
                            break;
                        case MidiController::NRPN: cs = "NRPN";
                            break;
                        case MidiController::Pitch: cs = "Pitch";
                            break;
                        case MidiController::Program: cs = "Program";
                            break;
                        case MidiController::RPN14: cs = "RPN14";
                            break;
                        case MidiController::NRPN14: cs = "NRPN14";
                            break;
                        default: cs = "Ctrl?";
                            break;
                    }
                    s = QString(cs);
                }
                    break;
                case Sysex:
                {
                    commentLabel = QString("len ");
                    QString k;
                    k.setNum(event.dataLen());
                    commentLabel += k;
                    commentLabel += QString(" ");

                    commentLabel += nameSysex(event.dataLen(), event.data());
                    int i;
                    for (i = 0; i < 10; ++i)
                    {
                        if (i >= event.dataLen())
                            break;
                        commentLabel += QString(" 0x");
                        QString k;
                        k.setNum(event.data()[i] & 0xff, 16);
                        commentLabel += k;
                    }
                    if (i == 10)
                        commentLabel += QString("...");
                }
                    s = QString("SysEx");
                    break;
                case PAfter:
                    s = QString("PoAT");
                    break;
                case CAfter:
                    s = QString("ChAT");
                    break;
                case Meta:
                    commentLabel = midiMetaComment(event);
                    s = QString("Meta");
                    break;
                default:
                    printf("unknown event type %d\n", event.type());
            }
            break;
        case 3:
            s.setNum(part->track()->outChannel() + 1);
            break;
        case 4:
            if (event.isNote() || event.type() == PAfter)
                s = pitch2string(event.dataA());
            else if (event.type() == Controller)
                s.setNum(event.dataA() & 0xffff); // mask off type bits
            else
                s.setNum(event.dataA());
            break;
        case 5:
            if (event.type() == Controller &&
                    midiControllerType(event.dataA()) == MidiController::Program)
            {
                int val = event.dataB();
                int hb = ((val >> 16) & 0xff) + 1;
                if (hb == 0x100)
                    hb = 0;
                int lb = ((val >> 8) & 0xff) + 1;
                if (lb == 0x100)
                    lb = 0;
                int pr = (val & 0xff) + 1;
                if (pr == 0x100)
                    pr = 0;
                s.sprintf("%d-%d-%d", hb, lb, pr);
            }
            else
                s.setNum(event.dataB());
            break;
        case 6:
            s.setNum(event.dataC());
            break;
        case 7:
            s.setNum(event.lenTick());
            break;
        case 8:
            switch (event.type())
            {
                case Controller:
                {
                    MidiPort* mp = &midiPorts[part->track()->outPort()];
                    MidiController* mc = mp->midiController(event.dataA());
                    s = mc->name();
                }
                    break;
                case Sysex:
                {
                    s = QString("len ");
                    QString k;
                    k.setNum(event.dataLen());
                    s += k;
                    s += QString(" ");

                    commentLabel += nameSysex(event.dataLen(), event.data());
                    int i;
                    for (i = 0; i < 10; ++i)
                    {
                        if (i >= event.dataLen())
                            break;
                        s += QString(" 0x");
                        QString k;
                        k.setNum(event.data()[i] & 0xff, 16);
                        s += k;
                    }
                    if (i == 10)
                        s += QString("...");
                }
                    break;
                case Meta:
                    s = midiMetaComment(event);
                    break;
                default:
                    break;
            }
            break;

    }
    return s;
}

//---------------------------------------------------------
//   ListEdit
//---------------------------------------------------------

ListEdit::ListEdit(PartList* pl)
: AbstractMidiEditor(0, 0, pl)
{
    setWindowTitle("LOS: List Edit");
    setWindowIcon(*losIcon);

    insertItems = new QActionGroup(this);
    insertItems->setExclusive(false);
    insertNote = new QAction(QIcon(*note1Icon), tr("insert Note"), insertItems);
    insertSysEx = new QAction(QIcon(*sysexIcon), tr("insert SysEx"), insertItems);
    insertCtrl = new QAction(QIcon(*ctrlIcon), tr("insert Ctrl"), insertItems);
    insertMeta = new QAction(QIcon(*metaIcon), tr("insert Meta"), insertItems);
    insertCAfter = new QAction(QIcon(*cafterIcon), tr("insert Channel Aftertouch"), insertItems);
    insertPAfter = new QAction(QIcon(*pafterIcon), tr("insert Poly Aftertouch"), insertItems);

    connect(insertNote, SIGNAL(triggered()), SLOT(editInsertNote()));
    connect(insertSysEx, SIGNAL(triggered()), SLOT(editInsertSysEx()));
    connect(insertCtrl, SIGNAL(triggered()), SLOT(editInsertCtrl()));
    connect(insertMeta, SIGNAL(triggered()), SLOT(editInsertMeta()));
    connect(insertCAfter, SIGNAL(triggered()), SLOT(editInsertCAfter()));
    connect(insertPAfter, SIGNAL(triggered()), SLOT(editInsertPAfter()));

    //---------Pulldown Menu----------------------------

    QSignalMapper *editSignalMapper = new QSignalMapper(this);

    menuEdit = menuBar()->addMenu(tr("&Edit"));
    menuEdit->addActions(undoRedo->actions());

    menuEdit->addSeparator();
#if 0
    QAction *cutAction = menuEdit->addAction(QIcon(*editcutIconSet), tr("Cut"));
    connect(cutAction, SIGNAL(triggered()), editSignalMapper, SLOT(map()));
    editSignalMapper->setMapping(cutAction, EList::CMD_CUT);
    cutAction->setShortcut(Qt::CTRL + Qt::Key_X);
    QAction *copyAction = menuEdit->addAction(QIcon(*editcopyIconSet), tr("Copy"));
    connect(copyAction, SIGNAL(triggered()), editSignalMapper, SLOT(map()));
    editSignalMapper->setMapping(cutAction, EList::CMD_COPY);
    copyAction->setShortcut(Qt::CTRL + Qt::Key_C);
    QAction *pasteAction = menuEdit->addAction(QIcon(*editpasteIconSet), tr("Paste"));
    connect(pasteAction, SIGNAL(triggered()), editSignalMapper, SLOT(map()));
    editSignalMapper->setMapping(cutAction, EList::CMD_PASTE);
    pasteAction->setShortcut(Qt::CTRL + Qt::Key_V);
    menuEdit->insertSeparator();
#endif
    QAction *deleteAction = menuEdit->addAction(tr("Delete Events"));
    connect(deleteAction, SIGNAL(triggered()), editSignalMapper, SLOT(map()));
    editSignalMapper->setMapping(deleteAction, CMD_DELETE);
    deleteAction->setShortcut(Qt::Key_Delete);
    menuEdit->addSeparator();

    menuEdit->addActions(insertItems->actions());

    menuEdit->addSeparator();

    _editEventValueAction = menuEdit->addAction(tr("Edit Value"));
    editSignalMapper->setMapping(_editEventValueAction, CMD_EDIT_VALUE);

    connect(_editEventValueAction, SIGNAL(triggered()), editSignalMapper, SLOT(map()));

    connect(editSignalMapper, SIGNAL(mapped(int)), SLOT(cmd(int)));

    //---------ToolBar----------------------------------

    listTools = addToolBar(tr("List tools"));
    listTools->addActions(undoRedo->actions());

    QToolBar* insertTools = addToolBar(tr("Insert tools"));
    insertTools->addActions(insertItems->actions());

    //
    //---------------------------------------------------
    //    liste
    //---------------------------------------------------
    //

    liste = new QTreeWidget(mainw);
    liste->setObjectName("EventListTree");
    QFontMetrics fm(liste->font());
    int n = fm.width('9');
    int b = 24;
    int c = fm.width(QString("Val B"));
    int sortIndW = n * 3;
    liste->setAllColumnsShowFocus(true);
    liste->sortByColumn(0, Qt::AscendingOrder);

    liste->setSelectionMode(QAbstractItemView::ExtendedSelection);

    QStringList columnnames;
    columnnames << tr("Tick")
            << tr("Bar")
            << tr("Type")
            << tr("Ch")
            << tr("Val A")
            << tr("Val B")
            << tr("Val C")
            << tr("Len")
            << tr("Comment");

    liste->setHeaderLabels(columnnames);

    liste->setColumnWidth(0, n * 6 + b);
    liste->setColumnWidth(1, fm.width(QString("9999.99.999")) + b);
    liste->setColumnWidth(2, fm.width(QString("Program")) + b);
    liste->setColumnWidth(3, n * 2 + b + sortIndW);
    liste->setColumnWidth(4, c + b + sortIndW);
    liste->setColumnWidth(5, c + b + sortIndW);
    liste->setColumnWidth(6, c + b + sortIndW);
    liste->setColumnWidth(7, n * 4 + b + sortIndW);
    liste->setColumnWidth(8, fm.width(QString("MainVolume")) + 70);

    connect(liste, SIGNAL(itemSelectionChanged()), SLOT(selectionChanged()));
    connect(liste, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), SLOT(doubleClicked(QTreeWidgetItem*)));

    //---------------------------------------------------
    //    Rest
    //---------------------------------------------------

    mainGrid->setRowStretch(1, 100);
    mainGrid->setColumnStretch(0, 100);
    mainGrid->addWidget(liste, 1, 0, 2, 1);
    //setLayout(mainGrid);
    connect(song, SIGNAL(songChanged(int)), SLOT(songChanged(int)));
    songChanged(-1);

    // p3.3.34
    // Was crashing because of -1 stored, because there was an invalid
    //  part pointer stored.
    //curPart   = (MidiPart*)(pl->begin()->second);
    if (pl->empty())
    {
        curPart = 0;
        curPartId = -1;
    }
    else
    {
        curPart = (MidiPart*) pl->begin()->second;
        if (curPart)
            curPartId = curPart->sn();
        else
        {
            curPart = 0;
            curPartId = -1;
        }
    }

    initShortcuts();

        // Set size stored in global config, or use defaults.
        int w = tconfig().get_property("ListEdit", "widgetwidth", 700).toInt();
        int h = tconfig().get_property("ListEdit", "widgetheigth", 650).toInt();
        resize(w, h);
}

//---------------------------------------------------------
//   ~ListEdit
//---------------------------------------------------------

ListEdit::~ListEdit()
{
    // undoRedo->removeFrom(listTools);  // p4.0.6 Removed
        tconfig().set_property("ListEdit", "widgetwidth", width());
        tconfig().set_property("ListEdit", "widgetheigth", height());
}


unsigned ListEdit::getSelectedTick()
{
    Q_ASSERT(curPart);

    EventListItem* ev = 0;

    QList<QTreeWidgetItem*> items = liste->selectedItems();
    if (items.size()) {
        ev = (EventListItem*) items.first();

    }

    return ev ? (ev->event.tick() + curPart->tick()) : curPart->tick();
}

//---------------------------------------------------------
//   editInsertNote
//---------------------------------------------------------

void ListEdit::editInsertNote()
{
    // p3.3.34
    if (!curPart)
        return;


    Event event = EditNoteDialog::getEvent(getSelectedTick(), Event(), this);
    if (!event.empty())
    {
        //No events before beginning of part + take Part offset into consideration
        unsigned tick = event.tick();
        if (tick < curPart->tick())
            tick = 0;
        else
            tick -= curPart->tick();
        event.setTick(tick);
        // Indicate do undo, and do not handle port controller values.
        //audio->msgAddEvent(event, curPart);
        audio->msgAddEvent(event, curPart, true, false, false);
    }
}

//---------------------------------------------------------
//   editInsertSysEx
//---------------------------------------------------------

void ListEdit::editInsertSysEx()
{
    // p3.3.34
    if (!curPart)
        return;

    Event event = EditSysexDialog::getEvent(getSelectedTick(), Event(), this);
    if (!event.empty())
    {
        //No events before beginning of part + take Part offset into consideration
        unsigned tick = event.tick();
        if (tick < curPart->tick())
            tick = 0;
        else
            tick -= curPart->tick();
        event.setTick(tick);
        // Indicate do undo, and do not handle port controller values.
        //audio->msgAddEvent(event, curPart);
        audio->msgAddEvent(event, curPart, true, false, false);
    }
}

//---------------------------------------------------------
//   editInsertCtrl
//---------------------------------------------------------

void ListEdit::editInsertCtrl()
{
    // p3.3.34
    if (!curPart)
        return;

    Event event = EditCtrlDialog::getEvent(getSelectedTick(), Event(), curPart, this);
    if (!event.empty())
    {
        //No events before beginning of part + take Part offset into consideration
        unsigned tick = event.tick();
        if (tick < curPart->tick())
            tick = 0;
        else
            tick -= curPart->tick();
        event.setTick(tick);
        // Indicate do undo, and do port controller values and clone parts.
        //audio->msgAddEvent(event, curPart);
        audio->msgAddEvent(event, curPart, true, true, true);
    }
}

//---------------------------------------------------------
//   editInsertMeta
//---------------------------------------------------------

void ListEdit::editInsertMeta()
{
    // p3.3.34
    if (!curPart)
        return;

    Event event = EditMetaDialog::getEvent(getSelectedTick(), Event(), this);
    if (!event.empty())
    {
        //No events before beginning of part + take Part offset into consideration
        unsigned tick = event.tick();
        if (tick < curPart->tick())
            tick = 0;
        else
            tick -= curPart->tick();
        event.setTick(tick);
        // Indicate do undo, and do not handle port controller values.
        //audio->msgAddEvent(event, curPart);
        audio->msgAddEvent(event, curPart, true, false, false);
    }
}

//---------------------------------------------------------
//   editInsertCAfter
//---------------------------------------------------------

void ListEdit::editInsertCAfter()
{
    // p3.3.34
    if (!curPart)
        return;

    Event event = EditCAfterDialog::getEvent(getSelectedTick(), Event(), this);
    if (!event.empty())
    {
        //No events before beginning of part + take Part offset into consideration
        unsigned tick = event.tick();
        if (tick < curPart->tick())
            tick = 0;
        else
            tick -= curPart->tick();
        event.setTick(tick);
        // Indicate do undo, and do not handle port controller values.
        //audio->msgAddEvent(event, curPart);
        audio->msgAddEvent(event, curPart, true, false, false);
    }
}

//---------------------------------------------------------
//   editInsertPAfter
//---------------------------------------------------------

void ListEdit::editInsertPAfter()
{
    // p3.3.34
    if (!curPart)
        return;

    Event ev;
    Event event = EditPAfterDialog::getEvent(getSelectedTick(), ev, this);
    if (!event.empty())
    {
        //No events before beginning of part + take Part offset into consideration
        unsigned tick = event.tick();
        if (tick < curPart->tick())
            tick = 0;
        else
            tick -= curPart->tick();
        event.setTick(tick);
        // Indicate do undo, and do not handle port controller values.
        //audio->msgAddEvent(event, curPart);
        audio->msgAddEvent(event, curPart, true, false, false);
    }
}

//---------------------------------------------------------
//   editEvent
//---------------------------------------------------------

void ListEdit::editEvent(Event& event, MidiPart* part)
{
    int tick = event.tick() + part->tick();
    Event nevent;
    switch (event.type())
    {
        case Note:
            nevent = EditNoteDialog::getEvent(tick, event, this);
            break;
        case Controller:
            nevent = EditCtrlDialog::getEvent(tick, event, part, this);
            break;
        case Sysex:
            nevent = EditSysexDialog::getEvent(tick, event, this);
            break;
        case PAfter:
            nevent = EditPAfterDialog::getEvent(tick, event, this);
            break;
        case CAfter:
            nevent = EditCAfterDialog::getEvent(tick, event, this);
            break;
        case Meta:
            nevent = EditMetaDialog::getEvent(tick, event, this);
            break;
        default:
            return;
    }
    if (!nevent.empty())
    {
        // TODO: check for event != nevent
        int tick = nevent.tick() - part->tick();
        nevent.setTick(tick);
        if (tick < 0)
            printf("event not in part %d - %d - %d, not changed\n", part->tick(),
                nevent.tick(), part->tick() + part->lenTick());
        else
        {
            if (event.type() == Controller)
                // Indicate do undo, and do port controller values and clone parts.
                //audio->msgChangeEvent(event, nevent, part);
                audio->msgChangeEvent(event, nevent, part, true, true, true);
            else
                // Indicate do undo, and do not do port controller values and clone parts.
                //audio->msgChangeEvent(event, nevent, part);
                audio->msgChangeEvent(event, nevent, part, true, false, false);
        }
    }
}

//---------------------------------------------------------
//   readStatus
//---------------------------------------------------------

void ListEdit::readStatus(Xml& xml)
{
    for (;;)
    {
        Xml::Token token = xml.parse();
        const QString& tag = xml.s1();
        if (token == Xml::Error || token == Xml::End)
            break;
        switch (token)
        {
            case Xml::TagStart:
                if (tag == "midieditor")
                    AbstractMidiEditor::readStatus(xml);
                else
                    xml.unknown("ListEdit");
                break;
            case Xml::TagEnd:
                if (tag == "listeditor")
                    return;
            default:
                break;
        }
    }
}

//---------------------------------------------------------
//   writeStatus
//---------------------------------------------------------

void ListEdit::writeStatus(int level, Xml& xml) const
{
    writePartList(level, xml);
    xml.tag(level++, "listeditor");
    AbstractMidiEditor::writeStatus(level, xml);
    xml.tag(level, "/listeditor");
}

//---------------------------------------------------------
//   selectionChanged
//---------------------------------------------------------

void ListEdit::selectionChanged()
{
    bool update = false;
    for (int row = 0; row < liste->topLevelItemCount(); ++row)
    {
        QTreeWidgetItem* i = liste->topLevelItem(row);
        if (i->isSelected() ^ ((EventListItem*) i)->event.selected())
        {
            ((EventListItem*) i)->event.setSelected(i->isSelected());
            update = true;
        }
    }
    if (update)
        song->update(SC_SELECTION);
}

//---------------------------------------------------------
//   doubleClicked
//---------------------------------------------------------

void ListEdit::doubleClicked(QTreeWidgetItem* item)
{
    EventListItem* ev = (EventListItem*) item;
    selectedTick = ev->event.tick();
    editEvent(ev->event, ev->part);
}

//---------------------------------------------------------
//   cmd
//---------------------------------------------------------

void ListEdit::cmd(int cmd)
{
    switch (cmd)
    {
        case CMD_DELETE:
        {
            bool found = false;
            for (int row = 0; row < liste->topLevelItemCount(); ++row)
            {
                QTreeWidgetItem* i = liste->topLevelItem(row);
                EventListItem *item = (EventListItem *) i;
                if (i->isSelected() || item->event.selected())
                {
                    found = true;
                    break;
                }
            }
            if (!found)
                break;
            song->startUndo();

            EventListItem *deletedEvent = NULL;
            for (int row = 0; row < liste->topLevelItemCount(); ++row)
            {
                QTreeWidgetItem* i = liste->topLevelItem(row);
                EventListItem *item = (EventListItem *) i;

                if (i->isSelected() || item->event.selected())
                {
                    deletedEvent = item;
                    // Indicate no undo, and do port controller values and clone parts.
                    //audio->msgDeleteEvent(item->event, item->part, false);
                    //audio->msgDeleteEvent(item->event, item->part, false, true, true);
                    audio->msgDeleteEvent(item->event, item->part, false, false, false);
                }
            }

            unsigned int nextTick = 0;
            // find biggest tick
            for (int row = 0; row < liste->topLevelItemCount(); ++row)
            {
                QTreeWidgetItem* i = liste->topLevelItem(row);
                EventListItem *item = (EventListItem *) i;
                if (item->event.tick() > nextTick && item != deletedEvent)
                    nextTick = item->event.tick();
            }
            // check if there's a tick that is "just" bigger than the deleted
            for (int row = 0; row < liste->topLevelItemCount(); ++row)
            {
                QTreeWidgetItem* i = liste->topLevelItem(row);
                EventListItem *item = (EventListItem *) i;
                if (item->event.tick() >= deletedEvent->event.tick() &&
                        item->event.tick() < nextTick &&
                        item != deletedEvent)
                {
                    nextTick = item->event.tick();
                }
            }
            selectedTick = nextTick;
            song->endUndo(SC_EVENT_MODIFIED);
            //printf("selected tick = %d\n", selectedTick);
            //emit selectionChanged();
            break;
        }
        case CMD_EDIT_VALUE:
            QList<QTreeWidgetItem*> items = liste->selectedItems();
            if (items.size()) {
                doubleClicked(items.first());

            }
            break;
    }
}

//---------------------------------------------------------
//   configChanged
//---------------------------------------------------------

void ListEdit::configChanged()
{
    initShortcuts();
}

//---------------------------------------------------------
//   initShortcuts
//---------------------------------------------------------

void ListEdit::initShortcuts()
{
    insertNote->setShortcut(shortcuts[SHRT_LE_INS_NOTES].key);
    insertSysEx->setShortcut(shortcuts[SHRT_LE_INS_SYSEX].key);
    insertCtrl->setShortcut(shortcuts[SHRT_LE_INS_CTRL].key);
    insertMeta->setShortcut(shortcuts[SHRT_LE_INS_META].key);
    insertCAfter->setShortcut(shortcuts[SHRT_LE_INS_CHAN_AFTERTOUCH].key);
    insertPAfter->setShortcut(shortcuts[SHRT_LE_INS_POLY_AFTERTOUCH].key);
    _editEventValueAction->setShortcut(shortcuts[SHRT_GLOBAL_EDIT_EVENT_VALUE].key);
}

//---------------------------------------------------------
//   viewKeyPressEvent
//---------------------------------------------------------

void ListEdit::keyPressEvent(QKeyEvent* event)
{
    int key = event->key();
    if (key == Qt::Key_Escape)
    {
        close();
        return;
    }
}

//---------------------------------------------------------
//   resizeEvent
//---------------------------------------------------------

void ListEdit::resizeEvent(QResizeEvent* e)
{
    QSize newSize = e->size();
    liste->resize(newSize.width(), newSize.height()-listTools->height()-6); // 6 = default Qt widget padding
    AbstractMidiEditor::resizeEvent(e);
}
