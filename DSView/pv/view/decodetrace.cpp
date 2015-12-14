/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
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
extern "C" {
#include <libsigrokdecode/libsigrokdecode.h>
}

#include <extdef.h>

#include <boost/foreach.hpp>
#include <boost/functional/hash.hpp>

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QDialog>
#include <QDialogButtonBox>
#include <QScrollArea>

#include "decodetrace.h"

#include "../sigsession.h"
#include "../data/decoderstack.h"
#include "../data/decode/decoder.h"
#include "../data/logic.h"
#include "../data/logicsnapshot.h"
#include "../data/decode/annotation.h"
#include "../view/logicsignal.h"
#include "../view/view.h"
#include "../widgets/decodergroupbox.h"
#include "../widgets/decodermenu.h"
#include "../device/devinst.h"

using boost::dynamic_pointer_cast;
using boost::shared_ptr;
using std::list;
using std::max;
using std::map;
using std::min;
using std::vector;

namespace pv {
namespace view {

const QColor DecodeTrace::DecodeColours[4] = {
	QColor(0xEF, 0x29, 0x29),	// Red
	QColor(0xFC, 0xE9, 0x4F),	// Yellow
	QColor(0x8A, 0xE2, 0x34),	// Green
	QColor(0x72, 0x9F, 0xCF)	// Blue
};

const QColor DecodeTrace::ErrorBgColour = QColor(0xEF, 0x29, 0x29);
const QColor DecodeTrace::NoDecodeColour = QColor(0x88, 0x8A, 0x85);

const int DecodeTrace::ArrowSize = 4;
const double DecodeTrace::EndCapWidth = 5;
const int DecodeTrace::DrawPadding = 100;

const QColor DecodeTrace::Colours[16] = {
	QColor(0xEF, 0x29, 0x29),
	QColor(0xF6, 0x6A, 0x32),
	QColor(0xFC, 0xAE, 0x3E),
	QColor(0xFB, 0xCA, 0x47),
	QColor(0xFC, 0xE9, 0x4F),
	QColor(0xCD, 0xF0, 0x40),
	QColor(0x8A, 0xE2, 0x34),
	QColor(0x4E, 0xDC, 0x44),
	QColor(0x55, 0xD7, 0x95),
	QColor(0x64, 0xD1, 0xD2),
	QColor(0x72, 0x9F, 0xCF),
	QColor(0xD4, 0x76, 0xC4),
	QColor(0x9D, 0x79, 0xB9),
	QColor(0xAD, 0x7F, 0xA8),
	QColor(0xC2, 0x62, 0x9B),
	QColor(0xD7, 0x47, 0x6F)
};

const QColor DecodeTrace::OutlineColours[16] = {
	QColor(0x77, 0x14, 0x14),
	QColor(0x7B, 0x35, 0x19),
	QColor(0x7E, 0x57, 0x1F),
	QColor(0x7D, 0x65, 0x23),
	QColor(0x7E, 0x74, 0x27),
	QColor(0x66, 0x78, 0x20),
	QColor(0x45, 0x71, 0x1A),
	QColor(0x27, 0x6E, 0x22),
	QColor(0x2A, 0x6B, 0x4A),
	QColor(0x32, 0x68, 0x69),
	QColor(0x39, 0x4F, 0x67),
	QColor(0x6A, 0x3B, 0x62),
	QColor(0x4E, 0x3C, 0x5C),
	QColor(0x56, 0x3F, 0x54),
	QColor(0x61, 0x31, 0x4D),
	QColor(0x6B, 0x23, 0x37)
};

DecodeTrace::DecodeTrace(pv::SigSession &session,
	boost::shared_ptr<pv::data::DecoderStack> decoder_stack, int index) :
	Trace(QString::fromUtf8(
        decoder_stack->stack().front()->decoder()->name), index, SR_CHANNEL_DECODER),
	_session(session),
	_decoder_stack(decoder_stack),
    _show_hide_mapper(this),
    _popup_form(NULL),
    _popup()
{
	assert(_decoder_stack);

	_colour = DecodeColours[index % countof(DecodeColours)];

    connect(_decoder_stack.get(), SIGNAL(new_decode_data()),
        this, SLOT(on_new_decode_data()));
    connect(_decoder_stack.get(), SIGNAL(decode_done()),
        this, SLOT(on_decode_done()));
	connect(&_show_hide_mapper, SIGNAL(mapped(int)),
		this, SLOT(on_show_hide_decoder(int)));
}

DecodeTrace::~DecodeTrace()
{
    if (_popup_form)
        delete _popup_form;
    if (_popup)
        delete _popup;
    _cur_row_headings.clear();
    _decoder_forms.clear();
    _probe_selectors.clear();
    _bindings.clear();
}

bool DecodeTrace::enabled() const
{
	return true;
}

const boost::shared_ptr<pv::data::DecoderStack>& DecodeTrace::decoder() const
{
	return _decoder_stack;
}

void DecodeTrace::set_view(pv::view::View *view)
{
	assert(view);
	Trace::set_view(view);
}

void DecodeTrace::paint_back(QPainter &p, int left, int right)
{
    QPen pen(Signal::dsGray);
    pen.setStyle(Qt::DotLine);
    p.setPen(pen);
    const double sigY = get_y() - (_signalHeight - _view->get_signalHeight())*0.5;
    p.drawLine(left, sigY, right, sigY);
}

void DecodeTrace::paint_mid(QPainter &p, int left, int right)
{
	using namespace pv::data::decode;

	const double scale = _view->scale();
	assert(scale > 0);

	double samplerate = _decoder_stack->samplerate();

	_cur_row_headings.clear();

	// Show sample rate as 1Hz when it is unknown
	if (samplerate == 0.0)
		samplerate = 1.0;

	const double pixels_offset = (_view->offset() -
		_decoder_stack->get_start_time()) / scale;
	const double samples_per_pixel = samplerate * scale;

	const uint64_t start_sample = (uint64_t)max((left + pixels_offset) *
		samples_per_pixel, 0.0);
	const uint64_t end_sample = (uint64_t)max((right + pixels_offset) *
		samples_per_pixel, 0.0);

    const int annotation_height = _view->get_signalHeight();

	assert(_decoder_stack);
	const QString err = _decoder_stack->error_message();
	if (!err.isEmpty())
	{
        //draw_unresolved_period(p, _view->get_signalHeight(), left, right,
        //	samples_per_pixel, pixels_offset);
		draw_error(p, err, left, right);
		return;
	}

    // Draw the hatching
    if (draw_unresolved_period(p, _view->get_signalHeight(), left, right))
        return;

	// Iterate through the rows
	assert(_view);
    int y =  get_y() - (_signalHeight - _view->get_signalHeight())*0.5;

	assert(_decoder_stack);

    const std::vector< std::pair<Row, bool> > rows(_decoder_stack->get_visible_rows());
    for (size_t i = 0; i < rows.size(); i++)
    {
        const Row &row = rows[i].first;
        const bool shown = rows[i].second;

        if (!shown && _decoder_stack->has_annotations(row)) {
            draw_unshown_row(p, y, _view->get_signalHeight(), left, right);
            y += _view->get_signalHeight();
            _cur_row_headings.push_back(row.title());
            continue;
        }

		size_t base_colour = 0x13579BDF;
		boost::hash_combine(base_colour, this);
		boost::hash_combine(base_colour, row.decoder());
		boost::hash_combine(base_colour, row.row());
		base_colour >>= 16;

        const uint64_t max_annotation =
                _decoder_stack->get_max_annotation(row);
        const double max_annWidth = max_annotation / samples_per_pixel;
        if (max_annWidth > 5) {
            vector<Annotation> annotations;
            _decoder_stack->get_annotation_subset(annotations, row,
                start_sample, end_sample);
            if (!annotations.empty()) {
                BOOST_FOREACH(const Annotation &a, annotations)
                    draw_annotation(a, p, get_text_colour(),
                        annotation_height, left, right,
                        samples_per_pixel, pixels_offset, y,
                        base_colour);
            }
        } else if (max_annWidth != 0){
            draw_nodetail(p, annotation_height, left, right, y, base_colour);
        }
        if (max_annWidth != 0) {
            y += _view->get_signalHeight();
            _cur_row_headings.push_back(row.title());
        }
	}
}

void DecodeTrace::paint_fore(QPainter &p, int left, int right)
{
	using namespace pv::data::decode;

	(void)right;

    const int row_height = _view->get_signalHeight();

	for (size_t i = 0; i < _cur_row_headings.size(); i++)
	{
        const int y = (i + 0.5) * row_height + get_y() - _signalHeight * 0.5;

		p.setPen(QPen(Qt::NoPen));
		p.setBrush(QApplication::palette().brush(QPalette::WindowText));

		if (i != 0)
		{
			const QPointF points[] = {
				QPointF(left, y - ArrowSize),
				QPointF(left + ArrowSize, y),
				QPointF(left, y + ArrowSize)
			};
			p.drawPolygon(points, countof(points));
		}

		const QRect r(left + ArrowSize * 2, y - row_height / 2,
			right - left, row_height);
		const QString h(_cur_row_headings[i]);
        const int f = Qt::AlignLeft | Qt::AlignBottom |
			Qt::TextDontClip;

		// Draw the outline
        QFont font=p.font();
        font.setPointSize(DefaultFontSize);
        p.setFont(font);
		p.setPen(QApplication::palette().color(QPalette::Base));
		for (int dx = -1; dx <= 1; dx++)
			for (int dy = -1; dy <= 1; dy++)
				if (dx != 0 && dy != 0)
					p.drawText(r.translated(dx, dy), f, h);

		// Draw the text
		p.setPen(QApplication::palette().color(QPalette::WindowText));
		p.drawText(r, f, h);
	}
}

bool DecodeTrace::create_popup()
{
    int ret = false;
    _popup = new QDialog();
    create_popup_form();

    if (QDialog::Accepted == _popup->exec())
    {
        BOOST_FOREACH(shared_ptr<data::decode::Decoder> dec,
            _decoder_stack->stack())
        {
            if (dec->commit()) {
                _decoder_stack->options_changed(true);
                ret = true;
            }
        }
    }

    _popup = NULL;
    _popup_form = NULL;

    return ret;
}

void DecodeTrace::create_popup_form()
{
    // Clear the layout

    // Transfer the layout and the child widgets to a temporary widget
    // which then goes out of scope destroying the layout and all the child
    // widgets.
    if (_popup_form)
        QWidget().setLayout(_popup_form);

    _popup_form = new QFormLayout(_popup);
    _popup->setLayout(_popup_form);
    populate_popup_form(_popup, _popup_form);
}

void DecodeTrace::populate_popup_form(QWidget *parent, QFormLayout *form)
{
	using pv::data::decode::Decoder;

	assert(form);
	assert(parent);
	assert(_decoder_stack);

	// Add the decoder options
	_bindings.clear();
	_probe_selectors.clear();
	_decoder_forms.clear();

	const list< shared_ptr<Decoder> >& stack = _decoder_stack->stack();

	if (stack.empty())
	{
		QLabel *const l = new QLabel(
			tr("<p><i>No decoders in the stack</i></p>"));
		l->setAlignment(Qt::AlignCenter);
		form->addRow(l);
	}
	else
	{
		list< shared_ptr<Decoder> >::const_iterator iter =
			stack.begin();
		for (int i = 0; i < (int)stack.size(); i++, iter++) {
			shared_ptr<Decoder> dec(*iter);
			create_decoder_form(i, dec, parent, form);
		}

		form->addRow(new QLabel(
			tr("<i>* Required channels</i>"), parent));
	}

	// Add stacking button
	pv::widgets::DecoderMenu *const decoder_menu =
		new pv::widgets::DecoderMenu(parent);
	connect(decoder_menu, SIGNAL(decoder_selected(srd_decoder*)),
		this, SLOT(on_stack_decoder(srd_decoder*)));
    //connect(decoder_menu, SIGNAL(selected()),
    //        parent, SLOT(accept()));

	QPushButton *const stack_button =
		new QPushButton(tr("Stack Decoder"), parent);
	stack_button->setMenu(decoder_menu);

	QHBoxLayout *stack_button_box = new QHBoxLayout;
    stack_button_box->addWidget(stack_button, 0, Qt::AlignLeft);
	form->addRow(stack_button_box);

    // Add ButtonBox (OK/Cancel)
    QDialogButtonBox *button_box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                Qt::Horizontal, parent);
    connect(button_box, SIGNAL(accepted()), parent, SLOT(accept()));
    connect(button_box, SIGNAL(rejected()), parent, SLOT(reject()));

    QHBoxLayout *confirm_button_box = new QHBoxLayout;
    confirm_button_box->addWidget(button_box, 0, Qt::AlignRight);
    form->addRow(confirm_button_box);
}

void DecodeTrace::draw_annotation(const pv::data::decode::Annotation &a,
	QPainter &p, QColor text_color, int h, int left, int right,
	double samples_per_pixel, double pixels_offset, int y,
	size_t base_colour) const
{
    const double start = max(a.start_sample() / samples_per_pixel -
        pixels_offset, (double)left);
    const double end = min(a.end_sample() / samples_per_pixel -
        pixels_offset, (double)right);

	const size_t colour = (base_colour + a.format()) % countof(Colours);
	const QColor &fill = Colours[colour];
	const QColor &outline = OutlineColours[colour];

	if (start > right + DrawPadding || end < left - DrawPadding)
		return;

	if (a.start_sample() == a.end_sample())
		draw_instant(a, p, fill, outline, text_color, h,
			start, y);
	else
		draw_range(a, p, fill, outline, text_color, h,
			start, end, y);
}

void DecodeTrace::draw_nodetail(QPainter &p,
    int h, int left, int right, int y,
    size_t base_colour) const
{
    const QRectF nodetail_rect(left, y - h/2 + 0.5, right - left, h);
    const size_t colour = base_colour % countof(Colours);
    const QColor &fill = Colours[colour];

    p.setPen(Qt::white);
    p.setBrush(fill);
    p.drawRect(nodetail_rect);
    p.drawText(nodetail_rect, Qt::AlignCenter | Qt::AlignVCenter, "Zoom in for more detials");
}

void DecodeTrace::draw_instant(const pv::data::decode::Annotation &a, QPainter &p,
	QColor fill, QColor outline, QColor text_color, int h, double x, int y) const
{
	const QString text = a.annotations().empty() ?
		QString() : a.annotations().back();
	const double w = min((double)p.boundingRect(QRectF(), 0, text).width(),
		0.0) + h;
	const QRectF rect(x - w / 2, y - h / 2, w, h);

	p.setPen(outline);
	p.setBrush(fill);
	p.drawRoundedRect(rect, h / 2, h / 2);

	p.setPen(text_color);
    QFont font=p.font();
    font.setPointSize(DefaultFontSize);
    p.setFont(font);
	p.drawText(rect, Qt::AlignCenter | Qt::AlignVCenter, text);
}

void DecodeTrace::draw_range(const pv::data::decode::Annotation &a, QPainter &p,
	QColor fill, QColor outline, QColor text_color, int h, double start,
	double end, int y) const
{
	const double top = y + .5 - h / 2;
	const double bottom = y + .5 + h / 2;
	const vector<QString> annotations = a.annotations();

    p.setPen(outline);
    p.setBrush(fill);

    // If the two ends are within 2 pixel, draw a vertical line
    if (start + 2.0 > end)
	{
		p.drawLine(QPointF(start, top), QPointF(start, bottom));
		return;
	}

    double cap_width = min((end - start) / 4, EndCapWidth);

	QPointF pts[] = {
		QPointF(start, y + .5f),
		QPointF(start + cap_width, top),
		QPointF(end - cap_width, top),
		QPointF(end, y + .5f),
		QPointF(end - cap_width, bottom),
		QPointF(start + cap_width, bottom)
	};

    p.setPen(Qt::white);
    p.drawConvexPolygon(pts, countof(pts));

	if (annotations.empty())
		return;

	QRectF rect(start + cap_width, y - h / 2,
		end - start - cap_width * 2, h);
	if (rect.width() <= 4)
		return;

	p.setPen(text_color);

	// Try to find an annotation that will fit
	QString best_annotation;
	int best_width = 0;

	BOOST_FOREACH(const QString &a, annotations) {
		const int w = p.boundingRect(QRectF(), 0, a).width();
		if (w <= rect.width() && w > best_width)
			best_annotation = a, best_width = w;
	}

	if (best_annotation.isEmpty())
		best_annotation = annotations.back();

	// If not ellide the last in the list
    QFont font=p.font();
    font.setPointSize(DefaultFontSize);
    p.setFont(font);
	p.drawText(rect, Qt::AlignCenter, p.fontMetrics().elidedText(
		best_annotation, Qt::ElideRight, rect.width()));
}

void DecodeTrace::draw_error(QPainter &p, const QString &message,
	int left, int right)
{
	const int y = get_y();

	p.setPen(ErrorBgColour.darker());
	p.setBrush(ErrorBgColour);

	const QRectF bounding_rect =
		QRectF(left, INT_MIN / 2 + y, right - left, INT_MAX);
	const QRectF text_rect = p.boundingRect(bounding_rect,
		Qt::AlignCenter, message);
	const float r = text_rect.height() / 4;

	p.drawRoundedRect(text_rect.adjusted(-r, -r, r, r), r, r,
		Qt::AbsoluteSize);

	p.setPen(get_text_colour());
    QFont font=p.font();
    font.setPointSize(DefaultFontSize);
    p.setFont(font);
	p.drawText(text_rect, message);
}

bool DecodeTrace::draw_unresolved_period(QPainter &p, int h, int left,
    int right)
{
	using namespace pv::data;
	using pv::data::decode::Decoder;

	assert(_decoder_stack);	

	shared_ptr<Logic> data;
	shared_ptr<LogicSignal> logic_signal;

    //const int64_t sample_count = _session.get_device()->get_sample_limit();
    const int64_t sample_count = _decoder_stack->sample_count();
	if (sample_count == 0)
        return true;

	const int64_t samples_decoded = _decoder_stack->samples_decoded();
	if (sample_count == samples_decoded)
        return false;

    const int y = get_y();
//	const double start = max(samples_decoded /
//		samples_per_pixel - pixels_offset, left - 1.0);
//	const double end = min(sample_count / samples_per_pixel -
//		pixels_offset, right + 1.0);
    const QRectF no_decode_rect(left, y - h/2 + 0.5, right - left, h);

	p.setPen(QPen(Qt::NoPen));
	p.setBrush(Qt::white);
	p.drawRect(no_decode_rect);

	p.setPen(NoDecodeColour);
    p.setBrush(QBrush(NoDecodeColour, Qt::Dense7Pattern));
	p.drawRect(no_decode_rect);

    const int progress100 = ceil(samples_decoded * 100.0 / sample_count);
    p.setPen(dsLightBlue);
    QFont font=p.font();
    font.setPointSize(_view->get_signalHeight()*2/3);
    font.setBold(true);
    p.setFont(font);
    p.drawText(no_decode_rect, Qt::AlignCenter | Qt::AlignVCenter, QString::number(progress100)+"%");

    return true;
}

void DecodeTrace::draw_unshown_row(QPainter &p, int y, int h, int left,
    int right)
{
    const QRectF unshown_rect(left, y - h/2 + 0.5, right - left, h);

    p.setPen(QPen(Qt::NoPen));
    p.setBrush(QBrush(NoDecodeColour, Qt::Dense7Pattern));
    p.drawRect(unshown_rect);

    p.setPen(dsLightBlue);
    QFont font=p.font();
    font.setPointSize(_view->get_signalHeight()*2/3);
    font.setBold(true);
    p.setFont(font);
    p.drawText(unshown_rect, Qt::AlignCenter | Qt::AlignVCenter, "Unshown");
}

void DecodeTrace::create_decoder_form(int index,
	shared_ptr<data::decode::Decoder> &dec, QWidget *parent,
	QFormLayout *form)
{
	const GSList *l;

	assert(dec);
	const srd_decoder *const decoder = dec->decoder();
	assert(decoder);

	pv::widgets::DecoderGroupBox *const group =
		new pv::widgets::DecoderGroupBox(
			QString::fromUtf8(decoder->name));
	group->set_decoder_visible(dec->shown());

	_show_hide_mapper.setMapping(group, index);
	connect(group, SIGNAL(show_hide_decoder()),
		&_show_hide_mapper, SLOT(map()));

	QFormLayout *const decoder_form = new QFormLayout;
	group->add_layout(decoder_form);

	// Add the mandatory channels
	for(l = decoder->channels; l; l = l->next) {
		const struct srd_channel *const pdch =
			(struct srd_channel *)l->data;
		QComboBox *const combo = create_probe_selector(parent, dec, pdch);
		connect(combo, SIGNAL(currentIndexChanged(int)),
			this, SLOT(on_probe_selected(int)));
		decoder_form->addRow(tr("<b>%1</b> (%2) *")
			.arg(QString::fromUtf8(pdch->name))
			.arg(QString::fromUtf8(pdch->desc)), combo);

		const ProbeSelector s = {combo, dec, pdch};
		_probe_selectors.push_back(s);
	}

	// Add the optional channels
	for(l = decoder->opt_channels; l; l = l->next) {
		const struct srd_channel *const pdch =
			(struct srd_channel *)l->data;
		QComboBox *const combo = create_probe_selector(parent, dec, pdch);
		connect(combo, SIGNAL(currentIndexChanged(int)),
			this, SLOT(on_probe_selected(int)));
		decoder_form->addRow(tr("<b>%1</b> (%2)")
			.arg(QString::fromUtf8(pdch->name))
			.arg(QString::fromUtf8(pdch->desc)), combo);

		const ProbeSelector s = {combo, dec, pdch};
		_probe_selectors.push_back(s);
	}

	// Add the options
	shared_ptr<prop::binding::DecoderOptions> binding(
		new prop::binding::DecoderOptions(_decoder_stack, dec));
    binding->add_properties_to_form(decoder_form, true);

	_bindings.push_back(binding);

	form->addRow(group);
	_decoder_forms.push_back(group);
}

QComboBox* DecodeTrace::create_probe_selector(
	QWidget *parent, const shared_ptr<data::decode::Decoder> &dec,
	const srd_channel *const pdch)
{
	assert(dec);

    const vector< shared_ptr<Signal> > sigs(_session.get_signals());

	assert(_decoder_stack);
	const map<const srd_channel*,
		shared_ptr<LogicSignal> >::const_iterator probe_iter =
		dec->channels().find(pdch);

	QComboBox *selector = new QComboBox(parent);

	selector->addItem("-", qVariantFromValue((void*)NULL));

	if (probe_iter == dec->channels().end())
		selector->setCurrentIndex(0);

	for(size_t i = 0; i < sigs.size(); i++) {
		const shared_ptr<view::Signal> s(sigs[i]);
		assert(s);

		if (dynamic_pointer_cast<LogicSignal>(s) && s->enabled())
		{
			selector->addItem(s->get_name(),
				qVariantFromValue((void*)s.get()));
            if (probe_iter != dec->channels().end()) {
                if ((*probe_iter).second->get_index() == s->get_index())
                    selector->setCurrentIndex(i + 1);
            }
		}
	}

	return selector;
}

void DecodeTrace::commit_decoder_probes(shared_ptr<data::decode::Decoder> &dec)
{
	assert(dec);

	map<const srd_channel*, shared_ptr<LogicSignal> > probe_map;
    const vector< shared_ptr<Signal> > sigs(_session.get_signals());

    _index_list.clear();
	BOOST_FOREACH(const ProbeSelector &s, _probe_selectors)
	{
		if(s._decoder != dec)
			break;

		const LogicSignal *const selection =
			(LogicSignal*)s._combo->itemData(
				s._combo->currentIndex()).value<void*>();

		BOOST_FOREACH(shared_ptr<Signal> sig, sigs)
			if(sig.get() == selection) {
				probe_map[s._pdch] =
					dynamic_pointer_cast<LogicSignal>(sig);
                _index_list.push_back(sig->get_index());
				break;
			}
	}

	dec->set_probes(probe_map);
}

void DecodeTrace::commit_probes()
{
	assert(_decoder_stack);
	BOOST_FOREACH(shared_ptr<data::decode::Decoder> dec,
		_decoder_stack->stack())
		commit_decoder_probes(dec);

    //_decoder_stack->begin_decode();
}

void DecodeTrace::on_new_decode_data()
{
    if (_view && _view->session().get_capture_state() == SigSession::Stopped)
        _view->data_updated();
}

void DecodeTrace::on_decode_done()
{
    if (_view) {
        _view->set_need_update(true);
        _view->signals_changed();
    }
}

void DecodeTrace::on_delete()
{
    _session.remove_decode_signal(this);
}

void DecodeTrace::on_probe_selected(int)
{
	commit_probes();
}

void DecodeTrace::on_stack_decoder(srd_decoder *decoder)
{
	assert(decoder);
	assert(_decoder_stack);
	_decoder_stack->push(shared_ptr<data::decode::Decoder>(
		new data::decode::Decoder(decoder)));
    //_decoder_stack->begin_decode();

    create_popup_form();
}

void DecodeTrace::on_show_hide_decoder(int index)
{
	using pv::data::decode::Decoder;

	const list< shared_ptr<Decoder> > stack(_decoder_stack->stack());

	// Find the decoder in the stack
	list< shared_ptr<Decoder> >::const_iterator iter = stack.begin();
	for(int i = 0; i < index; i++, iter++)
		assert(iter != stack.end());

	shared_ptr<Decoder> dec = *iter;
	assert(dec);

	const bool show = !dec->shown();
	dec->show(show);

	assert(index < (int)_decoder_forms.size());
	_decoder_forms[index]->set_decoder_visible(show);

    //_view->set_need_update(true);
}


int DecodeTrace::rows_size()
{
    return _decoder_stack->cur_rows_size();
}

void DecodeTrace::paint_type_options(QPainter &p, int right, const QPoint pt)
{
    (void)pt;

    int y = get_y();
    const QRectF group_index_rect = get_rect(CHNLREG, y, right);
    QString index_string;
    int last_index;
    p.setPen(Qt::transparent);
    p.setBrush(dsBlue);
    p.drawRect(group_index_rect);
    std::list<int>::iterator i = _index_list.begin();
    last_index = (*i);
    index_string = QString::number(last_index);
    while (++i != _index_list.end()) {
        if ((*i) == last_index + 1 && index_string.indexOf("-") < 3 && index_string.indexOf("-") > 0)
            index_string.replace(QString::number(last_index), QString::number((*i)));
        else if ((*i) == last_index + 1)
            index_string = QString::number((*i)) + "-" + index_string;
        else
            index_string = QString::number((*i)) + "," + index_string;
        last_index = (*i);
    }
    p.setPen(Qt::white);
    p.drawText(group_index_rect, Qt::AlignRight | Qt::AlignVCenter, index_string);
}

QRectF DecodeTrace::get_rect(DecodeSetRegions type, int y, int right)
{
    const QSizeF name_size(right - get_leftWidth() - get_rightWidth(), SquareWidth);

    if (type == CHNLREG)
        return QRectF(
            get_leftWidth() + name_size.width() + Margin,
            y - SquareWidth / 2,
            SquareWidth * SquareNum, SquareWidth);
    else
        return QRectF(0, 0, 0, 0);
}

} // namespace view
} // namespace pv
