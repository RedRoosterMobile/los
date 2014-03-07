//=========================================================
//  LOS
//  Libre Octave Studio
//    $Id: comment.cpp,v 1.2 2004/02/08 18:30:00 wschweer Exp $
//  (C) Copyright 2001 Werner Schweer (ws@seh.de)
//=========================================================

#include "comment.h"
#include "song.h"
#include "track.h"

#include <QWidget>

//---------------------------------------------------------
//   Comment
//---------------------------------------------------------

Comment::Comment(QWidget* parent)
: QWidget(parent)
{
    setupUi(this);
}

//---------------------------------------------------------
//   textChanged
//---------------------------------------------------------

void Comment::textChanged()
{
    setText(textentry->toPlainText());
}

//---------------------------------------------------------
//   TrackComment
//---------------------------------------------------------

TrackComment::TrackComment(MidiTrack* t, QWidget* parent)
: Comment(parent)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(tr("LOS: Track Comment"));
    m_track = t;
    connect(song, SIGNAL(songChanged(int)), SLOT(songChanged(int)));
    textentry->setText(m_track->comment());
    textentry->moveCursor(QTextCursor::End);
    connect(textentry, SIGNAL(textChanged()), SLOT(textChanged()));
    label1->setText(tr("Track Comment:"));
    label2->setText(m_track->name());
}

//---------------------------------------------------------
//   songChanged
//---------------------------------------------------------

void TrackComment::songChanged(int flags)
{
    if ((flags & (SC_TRACK_INSERTED | SC_TRACK_REMOVED | SC_TRACK_MODIFIED)) == 0)
        return;

    // check if track still exists:
    MidiTrackList* tl = song->tracks();
    iMidiTrack it;
    for (it = tl->begin(); it != tl->end(); ++it)
    {
        if (m_track == *it)
            break;
    }
    if (it == tl->end())
    {
        close();
        return;
    }
    label2->setText(m_track->name());
    if (m_track->comment() != textentry->toPlainText())
    {
        //disconnect(textentry, SIGNAL(textChanged()), this, SLOT(textChanged()));
        textentry->blockSignals(true);
        textentry->setText(m_track->comment());
        textentry->blockSignals(false);
        textentry->moveCursor(QTextCursor::End);
        //connect(textentry, SIGNAL(textChanged()), this, SLOT(textChanged()));
    }
}

//---------------------------------------------------------
//   setText
//---------------------------------------------------------

void TrackComment::setText(const QString& s)
{
    m_track->setComment(s);
    song->update(SC_TRACK_MODIFIED);
}

