/*
 * This file is part of the DSLogic-gui project.
 * DSLogic-gui is based on PulseView.
 *
 * Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
 * Copyright (C) 2013 DreamSourceLab <dreamsourcelab@dreamsourcelab.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */


#include "header.h"
#include "view.h"

#include "trace.h"
#include "dsosignal.h"
#include "logicsignal.h"
#include "analogsignal.h"
#include "groupsignal.h"
#include "decodetrace.h"
#include "../sigsession.h"
#include "../device/devinst.h"

#include <assert.h>

#include <boost/foreach.hpp>

#include <QApplication>
#include <QColorDialog>
#include <QInputDialog>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QRect>
#include <QStyleOption>
#include <QMessageBox>

using namespace boost;
using namespace std;

namespace pv {
namespace view {

Header::Header(View &parent) :
	QWidget(&parent),
    _view(parent),
    _action_add_group(new QAction(tr("Add Group"), this)),
    _action_del_group(new QAction(tr("Del Group"), this))
{
    _moveFlag = false;
    _colorFlag = false;
    _nameFlag = false;
    nameEdit = new QLineEdit(this);
    nameEdit->setFixedWidth(100);
    nameEdit->hide();

	setMouseTracking(true);

    connect(_action_del_group, SIGNAL(triggered()),
        this, SLOT(on_action_del_group_triggered()));
    connect(_action_add_group, SIGNAL(triggered()),
        this, SLOT(on_action_add_group_triggered()));

    connect(nameEdit, SIGNAL(editingFinished()),
            this, SLOT(on_action_set_name_triggered()));

    connect(&_view, SIGNAL(traces_moved()),
            this, SLOT(on_traces_moved()));
}


int Header::get_nameEditWidth()
{
    if (nameEdit->hasFocus())
        return nameEdit->width();
    else
        return 0;
}

boost::shared_ptr<pv::view::Trace> Header::get_mTrace(
    int &action,
    const QPoint &pt)
{
    const int w = width();
    const vector< boost::shared_ptr<Trace> > traces(
        _view.get_traces());

    BOOST_FOREACH(const boost::shared_ptr<Trace> t, traces)
    {
        assert(t);

        if ((action = t->pt_in_rect(t->get_y(), w, pt)))
            return t;
    }

    return boost::shared_ptr<Trace>();
}

void Header::paintEvent(QPaintEvent*)
{
    using pv::view::Trace;

    QStyleOption o;
    o.initFrom(this);
    QPainter painter(this);
    style()->drawPrimitive(QStyle::PE_Widget, &o, &painter, this);

	const int w = width();
    int action = 0;
    const vector< boost::shared_ptr<Trace> > traces(
        _view.get_traces());

    //QPainter painter(this);
    //painter.setRenderHint(QPainter::Antialiasing);

    const bool dragging = !_drag_traces.empty();
    BOOST_FOREACH(const boost::shared_ptr<Trace> t, traces)
	{
        assert(t);

        const int y = t->get_y();
        const bool highlight = !dragging &&
                               (action = t->pt_in_rect(y, w, _mouse_point));
        t->paint_label(painter, w, highlight, action);
	}

	painter.end();
}

void Header::mousePressEvent(QMouseEvent *event)
{
	assert(event);

    const vector< boost::shared_ptr<Trace> > traces(
        _view.get_traces());
    int action;

	if (event->button() & Qt::LeftButton) {
		_mouse_down_point = event->pos();

        // Save the offsets of any Traces which will be dragged
        BOOST_FOREACH(const boost::shared_ptr<Trace> t, traces)
            if (t->selected())
                _drag_traces.push_back(
                    make_pair(t, t->get_v_offset()));

        // Select the Trace if it has been clicked
        const boost::shared_ptr<Trace> mTrace =
            get_mTrace(action, event->pos());
        if (action == Trace::COLOR && mTrace) {
            _colorFlag = true;
        } else if (action == Trace::NAME && mTrace) {
            _nameFlag = true;
        } else if (action == Trace::LABEL && mTrace) {
            if (mTrace->selected())
                mTrace->select(false);
            else {
                if (mTrace->get_type() != Trace::DS_DSO)
                    mTrace->select(true);

                if (~QApplication::keyboardModifiers() &
                    Qt::ControlModifier)
                    _drag_traces.clear();

                // Add the Trace to the drag list
                if (event->button() & Qt::LeftButton) {
                    _drag_traces.push_back(make_pair(mTrace, mTrace->get_zeroPos()));
                }
            }
            mTrace->set_old_v_offset(mTrace->get_v_offset());
        } else if (action == Trace::POSTRIG && mTrace) {
            if (mTrace->get_trig() == Trace::POSTRIG)
                mTrace->set_trig(0);
            else
                mTrace->set_trig(Trace::POSTRIG);
        } else if (action == Trace::HIGTRIG && mTrace) {
            if (mTrace->get_trig() == Trace::HIGTRIG)
                mTrace->set_trig(0);
            else
                mTrace->set_trig(Trace::HIGTRIG);
        } else if (action == Trace::NEGTRIG && mTrace) {
            if (mTrace->get_trig() == Trace::NEGTRIG)
                mTrace->set_trig(0);
            else
                mTrace->set_trig(Trace::NEGTRIG);
        } else if (action == Trace::LOWTRIG && mTrace) {
            if (mTrace->get_trig() == Trace::LOWTRIG)
                mTrace->set_trig(0);
            else
                mTrace->set_trig(Trace::LOWTRIG);
        } else if (action == Trace::EDGETRIG && mTrace) {
            if (mTrace->get_trig() == Trace::EDGETRIG)
                mTrace->set_trig(0);
            else
                mTrace->set_trig(Trace::EDGETRIG);
        } else if (action == Trace::VDIAL && mTrace) {
            shared_ptr<view::DsoSignal> dsoSig;
            BOOST_FOREACH(const shared_ptr<Trace> t, traces) {
                if (dsoSig = dynamic_pointer_cast<view::DsoSignal>(t)) {
                    dsoSig->set_hDialActive(false);
                    if (t != mTrace) {
                        dsoSig->set_vDialActive(false);
                    }
                }
            }
             if (dsoSig = dynamic_pointer_cast<view::DsoSignal>(mTrace))
                dsoSig->set_vDialActive(!dsoSig->get_vDialActive());
        } else if (action == Trace::HDIAL && mTrace) {
            shared_ptr<view::DsoSignal> dsoSig;
            if (dsoSig = dynamic_pointer_cast<view::DsoSignal>(mTrace)) {
                if (dsoSig->get_hDialActive()) {
                    BOOST_FOREACH(const shared_ptr<Trace> t, traces) {
                        if(dsoSig = dynamic_pointer_cast<view::DsoSignal>(t)) {
                            dsoSig->set_vDialActive(false);
                            dsoSig->set_hDialActive(false);
                        }
                    }
                } else {
                    BOOST_FOREACH(const shared_ptr<Trace> t, traces) {
                        if(dsoSig = dynamic_pointer_cast<view::DsoSignal>(t)) {
                            dsoSig->set_vDialActive(false);
                            dsoSig->set_hDialActive(true);
                        }
                    }
                }
            }
        } else if (action == Trace::CHEN && mTrace) {
            shared_ptr<view::DsoSignal> dsoSig;
            if (dsoSig = dynamic_pointer_cast<view::DsoSignal>(mTrace)) {
                dsoSig->set_enable(!dsoSig->enabled());
            }
        } else if (action == Trace::ACDC && mTrace) {
            shared_ptr<view::DsoSignal> dsoSig;
            if (dsoSig = dynamic_pointer_cast<view::DsoSignal>(mTrace))
                dsoSig->set_acCoupling(!dsoSig->get_acCoupling());
        }

        if (~QApplication::keyboardModifiers() & Qt::ControlModifier) {
            // Unselect all other Traces because the Ctrl is not
            // pressed
            BOOST_FOREACH(const shared_ptr<Trace> t, traces)
                if (t != mTrace)
                    t->select(false);
        }
        update();
    }
}

void Header::mouseReleaseEvent(QMouseEvent *event)
{
	assert(event);

    // judge for color / name / trigger / move
    int action;
    const boost::shared_ptr<Trace> mTrace =
        get_mTrace(action, event->pos());
    if (mTrace){
        if (action == Trace::COLOR && _colorFlag) {
            _context_trace = mTrace;
            changeColor(event);
            _view.set_need_update(true);
        } else if (action == Trace::NAME && _nameFlag) {
            _context_trace = mTrace;
            changeName(event);
        }
    }
    if (_moveFlag) {
        //move(event);
        _view.signals_changed();
        _view.set_need_update(true);
    }
    _colorFlag = false;
    _nameFlag = false;
    _moveFlag = false;
    _drag_traces.clear();
    _view.normalize_layout();
}

void Header::wheelEvent(QWheelEvent *event)
{
    assert(event);

    if (event->orientation() == Qt::Vertical) {
        const vector< shared_ptr<Trace> > traces(
            _view.get_traces());
        // Vertical scrolling
        double shift = event->delta() / 20.0;
        BOOST_FOREACH(const shared_ptr<Trace> t, traces) {
            shared_ptr<view::DsoSignal> dsoSig;
            if (dsoSig = dynamic_pointer_cast<view::DsoSignal>(t)) {
                if (dsoSig->get_vDialActive()) {
                    if (shift > 1.0)
                        dsoSig->go_vDialNext();
                    else if (shift < -1.0)
                        dsoSig->go_vDialPre();
                    break;
                } else if (dsoSig->get_hDialActive()) {
                    if (shift > 1.0)
                        dsoSig->go_hDialNext();
                    else if (shift < -1.0)
                        dsoSig->go_hDialPre();
                }
            }
        }
        update();
    }
}

void Header::changeName(QMouseEvent *event)
{
    if ((event->button() == Qt::LeftButton)) {
        header_resize();
        nameEdit->setText(_context_trace->get_name());
        nameEdit->selectAll();
        nameEdit->setFocus();
        nameEdit->show();
        header_updated();
    }
}

void Header::changeColor(QMouseEvent *event)
{
    if ((event->button() == Qt::LeftButton)) {
        const QColor new_color = QColorDialog::getColor(_context_trace->get_colour(), this, tr("Set Channel Colour"));
        if (new_color.isValid())
            _context_trace->set_colour(new_color);
    }
}

void Header::mouseMoveEvent(QMouseEvent *event)
{
	assert(event);
	_mouse_point = event->pos();

    // Move the Traces if we are dragging
    if (!_drag_traces.empty()) {
		const int delta = event->pos().y() - _mouse_down_point.y();

        for (std::list<std::pair<boost::weak_ptr<Trace>,
            int> >::iterator i = _drag_traces.begin();
            i != _drag_traces.end(); i++) {
            const boost::shared_ptr<Trace> sig((*i).first);
			if (sig) {
                int y = (*i).second + delta;
                if (sig->get_type() != Trace::DS_DSO) {
                    const int y_snap =
                        ((y + View::SignalSnapGridSize / 2) /
                            View::SignalSnapGridSize) *
                            View::SignalSnapGridSize;
                    if (y_snap != sig->get_v_offset()) {
                        _moveFlag = true;
                        sig->set_v_offset(y_snap);
                    }
                    // Ensure the Trace is selected
                    sig->select(true);
                } else {
                    shared_ptr<DsoSignal> dsoSig;
                    if (dsoSig = dynamic_pointer_cast<DsoSignal>(sig)) {
                        dsoSig->set_zeroPos(y);
                        dsoSig->select(false);
                        traces_moved();
                    }
                }
			}
		}
        //traces_moved();
	}
	update();
}

void Header::leaveEvent(QEvent*)
{
	_mouse_point = QPoint(-1, -1);
	update();
}

void Header::contextMenuEvent(QContextMenuEvent *event)
{
    int action;

    const boost::shared_ptr<Trace> t = get_mTrace(action, _mouse_point);

    if (!t || !t->selected() || action != Trace::LABEL)
        return;

    QMenu menu(this);
    if (t->get_type() == Trace::DS_LOGIC)
        menu.addAction(_action_add_group);
    else if (t->get_type() == Trace::DS_GROUP)
        menu.addAction(_action_del_group);

    _context_trace = t;
    menu.exec(event->globalPos());
    _context_trace.reset();
}

void Header::on_action_set_name_triggered()
{
    boost::shared_ptr<view::Trace> context_Trace = _context_trace;
    if (!context_Trace)
		return;

    if (nameEdit->isModified()) {
        context_Trace->set_name(nameEdit->text());
        if (context_Trace->get_type() == Trace::DS_LOGIC ||
                context_Trace->get_type() == Trace::DS_ANALOG)
            sr_dev_probe_name_set(_view.session().get_device()->dev_inst(), context_Trace->get_index(), nameEdit->text().toUtf8().constData());
    }

    nameEdit->hide();
    header_updated();
}

void Header::on_action_add_group_triggered()
{
    _view.session().add_group();
}

void Header::on_action_del_group_triggered()
{
    _view.session().del_group();
}

void Header::on_traces_moved()
{
	update();
}

void Header::header_resize()
{
    //if (nameEdit->isVisible()) {
    if (_context_trace) {
        const int y = _context_trace->get_y();
        nameEdit->move(QPoint(_context_trace->get_leftWidth(), y - nameEdit->height() / 2));
    }
}


} // namespace view
} // namespace pv
