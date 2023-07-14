/*
 * This file is part of the PulseView project.
 * DSView is based on PulseView.
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


#include <list>
#include <map>
#include <QSignalMapper>
#include <QFormLayout>
#include <QWidget>
#include <QString>

#include "trace.h"
#include "../prop/binding/decoderoptions.h"
#include "../dialogs/dsdialog.h"

struct srd_channel;
struct srd_decoder;
 
class DsComboBox;

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

namespace view {

//create by  SigSession
class DecodeTrace : public Trace
{
	Q_OBJECT

private:
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

    static const int ControlRectWidth = 5;
    static const int MaxAnnType = 100;

    static const QString RegionStart;
    static const QString RegionEnd;

public:
	DecodeTrace(pv::SigSession *session,
		pv::data::DecoderStack *decoder_stack,
		int index);

public:
    ~DecodeTrace();

	bool enabled();

	inline pv::data::DecoderStack* decoder()
	{
		return _decoder_stack;
	}

	void set_view(pv::view::View *view);

	/**
	 * Paints the background layer of the trace with a QPainter
	 * @param p the QPainter to paint into.
	 * @param left the x-coordinate of the left edge of the signal.
	 * @param right the x-coordinate of the right edge of the signal.
	 **/
    void paint_back(QPainter &p, int left, int right, QColor fore, QColor back);

	/**
	 * Paints the mid-layer of the trace with a QPainter
	 * @param p the QPainter to paint into.
	 * @param left the x-coordinate of the left edge of the signal
	 * @param right the x-coordinate of the right edge of the signal
	 **/
    void paint_mid(QPainter &p, int left, int right, QColor fore, QColor back);

	/**
	 * Paints the foreground layer of the trace with a QPainter
	 * @param p the QPainter to paint into.
	 * @param left the x-coordinate of the left edge of the signal
	 * @param right the x-coordinate of the right edge of the signal
	 **/
    void paint_fore(QPainter &p, int left, int right, QColor fore, QColor back);
 
    int rows_size();

    QRectF get_rect(DecodeSetRegions type, int y, int right);

    /**
     * decode region
     **/
    void frame_ended();

    int get_progress();

	void* get_key_handel();

	bool create_popup(bool isnew);

protected:
    void paint_type_options(QPainter &p, int right, const QPoint pt, QColor fore);

private: 
 
    void draw_annotation(const pv::data::decode::Annotation &a, QPainter &p,
        QColor text_colour, int text_height, int left, int right,
        double samples_per_pixel, double pixels_offset, int y,
        size_t base_colour, double min_annWidth, QColor fore, QColor back, double &last_x);

    void draw_nodetail(QPainter &p,
        int text_height, int left, int right, int y,
        size_t base_colour, QColor fore, QColor back);

	void draw_instant(const pv::data::decode::Annotation &a, QPainter &p,
		QColor fill, QColor outline, QColor text_color, int h, double x,
        int y, double min_annWidth);

    void draw_range(const pv::data::decode::Annotation &a, QPainter &p,
        QColor fill, QColor outline, QColor text_color, int h, double start,
        double end, int y, QColor fore, QColor back);

	void draw_error(QPainter &p, const QString &message,
		int left, int right);

    void draw_unshown_row(QPainter &p, int y, int h, int left,
                          int right, QString info, QColor fore, QColor back);
 

signals:
    void decoded_progress(int progress);

private slots:
	void on_new_decode_data();   

    void on_decode_done(); 

public:
	volatile bool _delete_flag; //destroy it when deocde task end

private:
	pv::SigSession 			*_session;
	pv::data::DecoderStack 	*_decoder_stack;

	uint64_t 		_decode_start;
	uint64_t	 	_decode_end;
	
	uint64_t		_decode_cursor1; // the cursor key, sample start index 
	uint64_t		_decode_cursor2;	 

	std::vector<QString> 	_cur_row_headings; 
 
};

} // namespace view
} // namespace pv

#endif // DSVIEW_PV_VIEW_DECODETRACE_H
