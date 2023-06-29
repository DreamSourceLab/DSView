/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2022 DreamSourceLab <support@dreamsourcelab.com>
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

#ifndef DECODER_OPTIONS_DLG_H
#define DECODER_OPTIONS_DLG_H

#include <QObject>
#include <QWidget>
#include <vector>
#include <QString>

class QGridLayout;
class DsComboBox;
class QFormLayout;

struct srd_channel;

#include "dsdialog.h"

namespace pv {
    namespace data{
        class DecoderStack;

        namespace decode{
            class Decoder;
        }
    }
    namespace prop{   
        namespace binding{
            class DecoderOptions;
        }
    }
    namespace view{
        class View;
        class Cursor;
        class DecodeTrace;
    }

namespace dialogs {
 

class DecoderOptionsDlg: public DSDialog
{
    Q_OBJECT

private:
	struct ProbeSelector
	{
		const DsComboBox *_combo;
        const pv::data::decode::Decoder *_decoder;
		const srd_channel *_pdch;
	};

public:
    DecoderOptionsDlg(QWidget *parent);
    ~DecoderOptionsDlg(); 

    inline void set_cursor_range(uint64_t cursor1, uint64_t cursor2)
    {
        _cursor1 = cursor1;
        _cursor2 = cursor2;
    }

    inline void get_cursor_range(uint64_t &cursor1, uint64_t &cursor2)
    {
        cursor1 = _cursor1;
        cursor2 = _cursor2;
    }

    void load_options(view::DecodeTrace *trace);

    inline bool is_reload_form(){
        return _is_reload_form;
    }

private:
    void load_options_view();

    void load_decoder_forms(QWidget *container);  

    DsComboBox* create_probe_selector(QWidget *parent, const data::decode::Decoder *dec,
            const srd_channel *const pdch);
 
    void create_decoder_form(pv::data::decode::Decoder *dec,
            QWidget *parent, QFormLayout *form);

    void commit_probes();    
    void commit_decoder_probes(data::decode::Decoder *dec);
    void update_decode_range(); 
 
private slots:
    void on_probe_selected(int);
    void on_region_set(int index);
    void on_accept();
    void on_trans_pramas();

private: 
    std::vector<prop::binding::DecoderOptions*> _bindings;
    DsComboBox 		*_start_comboBox;
	DsComboBox 		*_end_comboBox;
    view::DecodeTrace   *_trace;
    uint64_t     _cursor1; //cursor key
    uint64_t     _cursor2;
    int          _contentHeight;
    
    std::vector<ProbeSelector> _probe_selectors;
    bool        _is_reload_form;
    int         _content_width;
};

}//dialogs
}//pv

#endif
