/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
 * Copyright (C) 2014 DreamSourceLab <support@dreamsourcelab.com>
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

#ifndef DSVIEW_PV_VIEW_DECODETRACE_H
#define DSVIEW_PV_VIEW_DECODETRACE_H

#include "trace.h"

#include <list>
#include <map>

#include <QSignalMapper>
#include <QFormLayout>

#include <boost/shared_ptr.hpp>

#include <pv/prop/binding/decoderoptions.h>
#include "../dialogs/dsdialog.h"

struct srd_channel;
struct srd_decoder;

class QComboBox;

namespace pv {

class SigSession;

namespace data {
class DecoderStack;

namespace decode {
class Annotation;
class Decoder;
class Row;
}
}

namespace widgets {
class DecoderGroupBox;
}

namespace view {

class DecodeTrace : public Trace
{
	Q_OBJECT

private:
	struct ProbeSelector
	{
		const QComboBox *_combo;
		const boost::shared_ptr<pv::data::decode::Decoder> _decoder;
		const srd_channel *_pdch;
	};

    enum DecodeSetRegions{
        NONEREG = -1,
        CHNLREG,
    };

private:
	static const QColor DecodeColours[4];
	static const QColor ErrorBgColour;
	static const QColor NoDecodeColour;

	static const int ArrowSize;
	static const double EndCapWidth;
	static const int DrawPadding;

	static const QColor Colours[16];
	static const QColor OutlineColours[16];

    static const int DefaultFontSize = 10;
    static const int ControlRectWidth = 5;
    static const int MaxAnnType = 100;

    static const QString RegionStart;
    static const QString RegionEnd;

public:
	DecodeTrace(pv::SigSession &session,
		boost::shared_ptr<pv::data::DecoderStack> decoder_stack,
		int index);
    ~DecodeTrace();

	bool enabled() const;

	const boost::shared_ptr<pv::data::DecoderStack>& decoder() const;

	void set_view(pv::view::View *view);

	/**
	 * Paints the background layer of the trace with a QPainter
	 * @param p the QPainter to paint into.
	 * @param left the x-coordinate of the left edge of the signal.
	 * @param right the x-coordinate of the right edge of the signal.
	 **/
	void paint_back(QPainter &p, int left, int right);

	/**
	 * Paints the mid-layer of the trace with a QPainter
	 * @param p the QPainter to paint into.
	 * @param left the x-coordinate of the left edge of the signal
	 * @param right the x-coordinate of the right edge of the signal
	 **/
	void paint_mid(QPainter &p, int left, int right);

	/**
	 * Paints the foreground layer of the trace with a QPainter
	 * @param p the QPainter to paint into.
	 * @param left the x-coordinate of the left edge of the signal
	 * @param right the x-coordinate of the right edge of the signal
	 **/
	void paint_fore(QPainter &p, int left, int right);

    bool create_popup();

    int rows_size();

    QRectF get_rect(DecodeSetRegions type, int y, int right);

    /**
     * decode region
     **/
    void frame_ended();

    int get_progress() const;

protected:
    void paint_type_options(QPainter &p, int right, const QPoint pt);

private:
    void create_popup_form();

	void populate_popup_form(QWidget *parent, QFormLayout *form);

	void draw_annotation(const pv::data::decode::Annotation &a, QPainter &p,
		QColor text_colour, int text_height, int left, int right,
		double samples_per_pixel, double pixels_offset, int y,
        size_t base_colour, double min_annWidth) const;
    void draw_nodetail(QPainter &p,
        int text_height, int left, int right, int y,
        size_t base_colour) const;

	void draw_instant(const pv::data::decode::Annotation &a, QPainter &p,
		QColor fill, QColor outline, QColor text_color, int h, double x,
        int y, double min_annWidth) const;

	void draw_range(const pv::data::decode::Annotation &a, QPainter &p,
		QColor fill, QColor outline, QColor text_color, int h, double start,
		double end, int y) const;

	void draw_error(QPainter &p, const QString &message,
		int left, int right);

    void draw_unshown_row(QPainter &p, int y, int h, int left,
                          int right, QString info);

    void create_decoder_form(boost::shared_ptr<data::DecoderStack> &decoder_stack,
        boost::shared_ptr<pv::data::decode::Decoder> &dec,
        QWidget *parent, QFormLayout *form);

	QComboBox* create_probe_selector(QWidget *parent,
		const boost::shared_ptr<pv::data::decode::Decoder> &dec,
		const srd_channel *const pdch);

	void commit_decoder_probes(
		boost::shared_ptr<data::decode::Decoder> &dec);

	void commit_probes();

signals:
    void decoded_progress(int progress);

private slots:
	void on_new_decode_data();

	void on_delete();

	void on_probe_selected(int);

	void on_stack_decoder(srd_decoder *decoder);
    void on_del_stack(boost::shared_ptr<data::decode::Decoder> &dec);

    void on_decode_done();

    void on_region_set(int index);

private:
	pv::SigSession &_session;
	boost::shared_ptr<pv::data::DecoderStack> _decoder_stack;

	uint64_t _decode_start, _decode_end;
    int _start_index, _end_index;
    int _start_count, _end_count;
    QComboBox *_start_comboBox, *_end_comboBox;
    int _progress;

	std::list< boost::shared_ptr<pv::prop::binding::DecoderOptions> >
		_bindings;

	std::list<ProbeSelector> _probe_selectors;
	std::vector<pv::widgets::DecoderGroupBox*> _decoder_forms;

	std::vector<QString> _cur_row_headings;

    QFormLayout *_popup_form;
    dialogs::DSDialog *_popup;
};

} // namespace view
} // namespace pv

#endif // DSVIEW_PV_VIEW_DECODETRACE_H
