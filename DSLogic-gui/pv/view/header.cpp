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

#include "signal.h"
#include "../sigsession.h"

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

	connect(&_view, SIGNAL(signals_moved()),
            this, SLOT(on_signals_moved()));
}


int Header::get_nameEditWidth()
{
    if (nameEdit->hasFocus())
        return nameEdit->width();
    else
        return 0;
}

boost::shared_ptr<pv::view::Signal> Header::get_mSig(
    int &action,
    const QPoint &pt)
{
    const int w = width();
    const vector< boost::shared_ptr<Signal> > sigs(
        _view.session().get_signals());

    const int v_offset = _view.v_offset();
    BOOST_FOREACH(const boost::shared_ptr<Signal> s, sigs)
    {
        assert(s);

        if ((action = s->pt_in_rect(s->get_v_offset() - v_offset - _view.get_signalHeight() / 2,
            w, pt)))
            return s;
    }

    return boost::shared_ptr<Signal>();
}

void Header::paintEvent(QPaintEvent*)
{
    using pv::view::Signal;

    QStyleOption o;
    o.initFrom(this);
    QPainter painter(this);
    style()->drawPrimitive(QStyle::PE_Widget, &o, &painter, this);

	const int w = width();
    int action;
	const vector< boost::shared_ptr<Signal> > sigs(
		_view.session().get_signals());

    //QPainter painter(this);
    //painter.setRenderHint(QPainter::Antialiasing);

	const int v_offset = _view.v_offset();
	const bool dragging = !_drag_sigs.empty();
	BOOST_FOREACH(const boost::shared_ptr<Signal> s, sigs)
	{
		assert(s);

        const int y = s->get_v_offset() - v_offset - _view.get_signalHeight() / 2;
        const bool highlight = !dragging &&
                               (action = s->pt_in_rect(y, w, _mouse_point));
        s->paint_label(painter, y, w, highlight, action);
        // Paint the Backgroud
        painter.setRenderHint(QPainter::Antialiasing, false);
        painter.setPen(Signal::dsGray);
        if (s->selected() && _moveFlag) {
            if (s->get_type() == Signal::DS_ANALOG) {
                painter.drawLine(0, s->get_old_v_offset() - v_offset - s->get_signalHeight(),
                             w, s->get_old_v_offset() - v_offset - s->get_signalHeight());
                painter.drawLine(0, s->get_old_v_offset() - v_offset,
                             w, s->get_old_v_offset() - v_offset);
            } else if (s->get_type() == Signal::DS_LOGIC){
                painter.drawLine(0, s->get_old_v_offset() - v_offset + 10,
                             w, s->get_old_v_offset() - v_offset + 10);
            }
        } else {
            if (s->get_type() == Signal::DS_ANALOG) {
                painter.drawLine(0, s->get_v_offset() - v_offset,
                             w, s->get_v_offset() - v_offset);
                painter.drawLine(0, s->get_v_offset() - v_offset - s->get_signalHeight(),
                             w, s->get_v_offset() - v_offset - s->get_signalHeight());
            } else if (s->get_type() == Signal::DS_LOGIC) {
                painter.drawLine(0, s->get_v_offset() - v_offset + 10,
                             w, s->get_v_offset() - v_offset + 10);
            }
        }
	}

	painter.end();
}

void Header::mousePressEvent(QMouseEvent *event)
{
	assert(event);

	const vector< boost::shared_ptr<Signal> > sigs(
		_view.session().get_signals());
    int action;

	if (event->button() & Qt::LeftButton) {
		_mouse_down_point = event->pos();

		// Save the offsets of any signals which will be dragged
		BOOST_FOREACH(const boost::shared_ptr<Signal> s, sigs)
			if (s->selected())
				_drag_sigs.push_back(
					make_pair(s, s->get_v_offset()));

        // Select the signal if it has been clicked
        const boost::shared_ptr<Signal> mSig =
            get_mSig(action, event->pos());
        if (action == Signal::COLOR && mSig) {
            _colorFlag = true;
        } else if (action == Signal::NAME && mSig) {
            _nameFlag = true;
        } else if (action == Signal::LABEL && mSig) {
            if (mSig->selected())
                mSig->select(false);
            else {
                if (mSig->get_type() != Signal::DS_DSO)
                    mSig->select(true);

                if (~QApplication::keyboardModifiers() &
                    Qt::ControlModifier)
                    _drag_sigs.clear();

                // Add the signal to the drag list
                if (event->button() & Qt::LeftButton)
                    _drag_sigs.push_back(
                        make_pair(mSig,
                                  (mSig->get_type() == Signal::DS_DSO) ? mSig->get_zeroPos() : mSig->get_v_offset()));
            }
            mSig->set_old_v_offset(mSig->get_v_offset());
        } else if (action == Signal::POSTRIG && mSig) {
            if (mSig->get_trig() == Signal::POSTRIG)
                mSig->set_trig(0);
            else
                mSig->set_trig(Signal::POSTRIG);
        } else if (action == Signal::HIGTRIG && mSig) {
            if (mSig->get_trig() == Signal::HIGTRIG)
                mSig->set_trig(0);
            else
                mSig->set_trig(Signal::HIGTRIG);
        } else if (action == Signal::NEGTRIG && mSig) {
            if (mSig->get_trig() == Signal::NEGTRIG)
                mSig->set_trig(0);
            else
                mSig->set_trig(Signal::NEGTRIG);
        } else if (action == Signal::LOWTRIG && mSig) {
            if (mSig->get_trig() == Signal::LOWTRIG)
                mSig->set_trig(0);
            else
                mSig->set_trig(Signal::LOWTRIG);
        } else if (action == Signal::EDGETRIG && mSig) {
            if (mSig->get_trig() == Signal::EDGETRIG)
                mSig->set_trig(0);
            else
                mSig->set_trig(Signal::EDGETRIG);
        } else if (action == Signal::VDIAL && mSig) {
            BOOST_FOREACH(const shared_ptr<Signal> s, sigs) {
                s->set_hDialActive(false);
                if (s != mSig) {
                    s->set_vDialActive(false);
                }
            }
            mSig->set_vDialActive(!mSig->get_vDialActive());
        } else if (action == Signal::HDIAL && mSig) {
            if (mSig->get_hDialActive()) {
                BOOST_FOREACH(const shared_ptr<Signal> s, sigs) {
                    s->set_vDialActive(false);
                    s->set_hDialActive(false);
                }
            } else {
                BOOST_FOREACH(const shared_ptr<Signal> s, sigs) {
                    s->set_vDialActive(false);
                    s->set_hDialActive(true);
                }
            }
        } else if (action == Signal::CHEN && mSig) {
            int channel;
            if (mSig->get_index() == 0) {
                bool last = 1;
                channel = 0;
                BOOST_FOREACH(const shared_ptr<Signal> s, sigs) {
                    if (s->get_index() != 0 && s->get_active()) {
                        QMessageBox msg(this);
                        msg.setText("Tips");
                        msg.setInformativeText("If only one channel want, Channel0 has a higher maximum sample rate!");
                        msg.setStandardButtons(QMessageBox::Ok);
                        msg.setIcon(QMessageBox::Information);
                        msg.exec();
                        s->set_active(!s->get_active());
                        last = 0;
                        channel = s->get_index();
                        break;
                    }
                }
                if (last)
                    mSig->set_active(!mSig->get_active());
            } else {
                mSig->set_active(!mSig->get_active());
                channel = mSig->get_index();
            }
            ch_changed(channel);
        } else if (action == Signal::ACDC && mSig) {
            mSig->set_acCoupling(!mSig->get_acCoupling());
            acdc_changed(mSig->get_index());
        }

        if (~QApplication::keyboardModifiers() & Qt::ControlModifier) {
            // Unselect all other signals because the Ctrl is not
            // pressed
            BOOST_FOREACH(const boost::shared_ptr<Signal> s, sigs)
                if (s != mSig)
                    s->select(false);
        }
        update();
    }
}

void Header::mouseReleaseEvent(QMouseEvent *event)
{
	assert(event);

    // judge for color / name / trigger / move
    int action;
    const boost::shared_ptr<Signal> mSig =
        get_mSig(action, event->pos());
    if (mSig){
        if (action == Signal::COLOR && _colorFlag) {
            _context_signal = mSig;
            changeColor(event);
            _view.set_need_update(true);
        } else if (action == Signal::NAME && _nameFlag) {
            _context_signal = mSig;
            changeName(event);
        }
    }
    if (_moveFlag) {
        move(event);
        _view.set_need_update(true);
    }
    _colorFlag = false;
    _nameFlag = false;
    _moveFlag = false;
    _drag_sigs.clear();
    _view.normalize_layout();
}

void Header::wheelEvent(QWheelEvent *event)
{
    assert(event);

    if (event->orientation() == Qt::Vertical) {
        const vector< shared_ptr<Signal> > sigs(
            _view.session().get_signals());
        // Vertical scrolling
        double shift = event->delta() / 20.0;
        if (shift > 1.0) {
            BOOST_FOREACH(const shared_ptr<Signal> s, sigs) {
                if (s->get_vDialActive()) {
                    if(s->go_vDialNext())
                        vDial_changed(s->get_index());
                    break;
                } else if (s->get_hDialActive()) {
                    if(s->go_hDialNext())
                        hDial_changed(0);
                }
            }
        } else if (shift < -1.0) {
            BOOST_FOREACH(const shared_ptr<Signal> s, sigs) {
                if (s->get_vDialActive()) {
                    if(s->go_vDialPre())
                        vDial_changed(s->get_index());
                    break;
                } else if (s->get_hDialActive()) {
                    if(s->go_hDialPre())
                        hDial_changed(0);
                }
            }
        }
        update();
    }
}

void Header::move(QMouseEvent *event)
{
    bool _moveValid = false;
    bool _moveUp = false;
    bool firstCheck = true;
    const vector< boost::shared_ptr<Signal> > sigs(
        _view.session().get_signals());
    boost::shared_ptr<Signal> minDragSig;
    boost::shared_ptr<Signal> maxDragSig;
    int minOffset;
    int minOldOffset;
    int maxOffset;
    int maxOldOffset;
    int targetOffset;
    std::list<std::pair<boost::weak_ptr<Signal>,
            int> >::iterator minJ;
    std::list<std::pair<boost::weak_ptr<Signal>,
            int> >::iterator maxJ;
    int targetOrder;

    // reCalculate _v_offset of all signals after dragging release
    if ((event->button() == Qt::LeftButton)) {
        while (!_drag_sigs.empty()) {
            minOffset = INT_MAX;
            maxOffset = 0;
            for (std::list<std::pair<boost::weak_ptr<Signal>,
                int> >::iterator i = _drag_sigs.begin();
                i != _drag_sigs.end(); i++) {
                const boost::shared_ptr<Signal> sig((*i).first);
                if (sig) {
                    if (sig->get_v_offset() < minOffset) {
                        minDragSig = sig;
                        minOldOffset = (*i).second;
                        minOffset = sig->get_v_offset();
                        minJ = i;
                    }
                    if (sig->get_v_offset() > maxOffset) {
                        maxDragSig = sig;
                        maxOldOffset = (*i).second;
                        maxOffset = sig->get_v_offset();
                        maxJ = i;
                    }
                }
            }
            if (minOffset > minOldOffset) {
                _moveUp = false;
                _drag_sigs.erase(maxJ);
            } else {
                _moveUp = true;
                _drag_sigs.erase(minJ);
            }
            if (!_moveValid && firstCheck){
                firstCheck = false;
                BOOST_FOREACH(const boost::shared_ptr<Signal> s, sigs) {
                    if (_moveUp) {
                        if (s->selected()) {
                            if ((minOffset <= s->get_old_v_offset()) && (minOffset > (s->get_old_v_offset() - _view.get_spanY()))) {
                                _moveValid = true;
                                targetOffset = s->get_old_v_offset();
                                targetOrder = s->get_order();
                                break;
                            }
                        } else {
                            if ((minOffset <= s->get_v_offset()) && (minOffset > (s->get_v_offset() - _view.get_spanY()))) {
                                _moveValid = true;
                                targetOffset = s->get_v_offset();
                                targetOrder = s->get_order();
                                break;
                            }
                        }
                    } else {
                        if (s->selected()) {
                            if ((maxOffset >= s->get_old_v_offset()) && (maxOffset < (s->get_old_v_offset() + _view.get_spanY()))) {
                                _moveValid = true;
                                targetOffset = s->get_old_v_offset();
                                targetOrder = s->get_order();
                                break;
                            }
                        } else {
                            if ((maxOffset >= s->get_v_offset()) && (maxOffset < (s->get_v_offset() + _view.get_spanY()))) {
                                _moveValid = true;
                                targetOffset = s->get_v_offset();
                                targetOrder = s->get_order();
                                break;
                            }
                        }
                    }
                }
            }
            if (_moveValid) {
                BOOST_FOREACH(const boost::shared_ptr<Signal> s, sigs) {
                    if (_moveUp) {
                        if (s->selected() && s == minDragSig) {
                            s->set_v_offset(targetOffset);
                            s->set_order(targetOrder);
                            s->select(false);
                        } else if (!s->selected() && s->get_v_offset() >= targetOffset && s->get_v_offset() < minOldOffset) {
                            s->set_v_offset(s->get_v_offset() + _view.get_spanY());
                            s->set_order(s->get_order() + 1);
                        }
                    } else {
                        if (s->selected() && s == maxDragSig) {
                            s->set_v_offset(targetOffset);
                            s->set_order(targetOrder);
                            s->select(false);
                        } else if (!s->selected() && s->get_v_offset() <= targetOffset && s->get_v_offset() > maxOldOffset) {
                            s->set_v_offset(s->get_v_offset() - _view.get_spanY());
                            s->set_order(s->get_order() - 1);
                        }
                    }
                }
                if (_moveUp) {
                    targetOffset += _view.get_spanY();
                    targetOrder++;
                } else {
                    targetOffset -= _view.get_spanY();
                    targetOrder--;
                }
            }
        }
        if (_moveValid) {
            signals_moved();
        } else {
            BOOST_FOREACH(const boost::shared_ptr<Signal> s, sigs) {
                if (s->selected()) {
                    s->set_v_offset(s->get_old_v_offset());
                    s->select(false);
                }
            }
        }
    }
    _moveValid = false;
}

void Header::changeName(QMouseEvent *event)
{
    if ((event->button() == Qt::LeftButton)) {
        header_resize();
        nameEdit->setText(_context_signal->get_name());
        nameEdit->selectAll();
        nameEdit->setFocus();
        nameEdit->show();
        header_updated();
    }
}

void Header::changeColor(QMouseEvent *event)
{
    if ((event->button() == Qt::LeftButton)) {
        const QColor new_color = QColorDialog::getColor(_context_signal->get_colour(), this, tr("Set Channel Colour"));
        if (new_color.isValid())
            _context_signal->set_colour(new_color);
    }
}

void Header::mouseMoveEvent(QMouseEvent *event)
{
	assert(event);
	_mouse_point = event->pos();

	// Move the signals if we are dragging
	if (!_drag_sigs.empty()) {
		const int delta = event->pos().y() - _mouse_down_point.y();

		for (std::list<std::pair<boost::weak_ptr<Signal>,
			int> >::iterator i = _drag_sigs.begin();
			i != _drag_sigs.end(); i++) {
			const boost::shared_ptr<Signal> sig((*i).first);
			if (sig) {
                int y = (*i).second + delta;
                if (sig->get_type() != Signal::DS_DSO) {
                    const int y_snap =
                        ((y + View::SignalSnapGridSize / 2) /
                            View::SignalSnapGridSize) *
                            View::SignalSnapGridSize;
                    if (y_snap != sig->get_v_offset()) {
                        _moveFlag = true;
                        sig->set_v_offset(y_snap);
                    }
                    // Ensure the signal is selected
                    sig->select(true);
                } else {
                    if (y < 0)
                        y = 0;
                    else if (y > height())
                        y = height();
                    sig->set_zeroPos(y);
                    sig->select(false);
                    signals_moved();
                }
			}
		}
        //signals_moved();
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

    const boost::shared_ptr<Signal> s = get_mSig(action, _mouse_point);

    if (!s || !s->selected() || action != Signal::LABEL)
        return;

    QMenu menu(this);
    if (s->get_type() == Signal::DS_LOGIC)
        menu.addAction(_action_add_group);
    else if (s->get_type() == Signal::DS_GROUP)
        menu.addAction(_action_del_group);

    _context_signal = s;
    menu.exec(event->globalPos());
    _context_signal.reset();
}

void Header::on_action_set_name_triggered()
{
	boost::shared_ptr<view::Signal> context_signal = _context_signal;
	if (!context_signal)
		return;

    if (nameEdit->isModified()) {
        context_signal->set_name(nameEdit->text());
        if (context_signal->get_type() == Signal::DS_LOGIC ||
                context_signal->get_type() == Signal::DS_ANALOG)
            sr_dev_probe_name_set(_view.session().get_device(), context_signal->get_index(), nameEdit->text().toUtf8().constData());
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

void Header::on_signals_moved()
{
	update();
}

void Header::header_resize()
{
    //if (nameEdit->isVisible()) {
    if (_context_signal) {
        const int y = _context_signal->get_v_offset() - _view.v_offset() - _view.get_signalHeight() / 2;
        nameEdit->move(QPoint(_context_signal->get_leftWidth(), y - nameEdit->height() / 2));
    }
}


} // namespace view
} // namespace pv
