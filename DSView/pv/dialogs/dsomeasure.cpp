/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2015 DreamSourceLab <support@dreamsourcelab.com>
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

#include "dsomeasure.h"
#include "../sigsession.h"
#include "../view/view.h"

#include <QCheckBox>
#include <QVariant>
#include <QLabel>
#include <QTabBar>
#include <QBitmap>
 
#include "../dsvdef.h"

#include "../ui/langresource.h"

using namespace std;
using namespace pv::view;

namespace pv {
namespace dialogs {

DsoMeasure::DsoMeasure(SigSession *session, View &parent,
                       unsigned int position, int last_sig_index) :
    DSDialog((QWidget *)&parent),
    _session(session),
    _view(parent),
    _position(position),
    _button_box(QDialogButtonBox::Reset | QDialogButtonBox::Cancel,
        Qt::Horizontal, this)
{
    _measure_tab = NULL;

    setMinimumSize(500, 400);

    _measure_tab = new QTabWidget(this);
    _measure_tab->setTabPosition(QTabWidget::West);
    _measure_tab->setUsesScrollButtons(false);

    for(auto s : _session->get_signals()) {
        if (s->signal_type() == SR_CHANNEL_DSO && s->enabled()) {
            view::DsoSignal *dsoSig = (view::DsoSignal*)s;
            QWidget *measure_widget = new QWidget(this);
            this->add_measure(measure_widget, dsoSig);
            _measure_tab->addTab(measure_widget, QString::number(dsoSig->get_index()));
            _measure_tab->tabBar()->setMinimumHeight(30);
            _measure_tab->tabBar()->setPalette(QPalette(Qt::red));
            measure_widget->setProperty("index", dsoSig->get_index());
            if (dsoSig->get_index() == last_sig_index)
                _measure_tab->setCurrentIndex(last_sig_index);
        }
    }

    _layout.addWidget(_measure_tab);
    _layout.addWidget(&_button_box, Qt::AlignHCenter | Qt::AlignBottom);

    layout()->addLayout(&_layout);
    setTitle(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_MEASUREMENTS), "Measurements"));

    connect(_button_box.button(QDialogButtonBox::Cancel), SIGNAL(clicked()), this, SLOT(reject()));
    connect(_button_box.button(QDialogButtonBox::Reset), SIGNAL(clicked()), this, SLOT(reset()));
    connect(_session->device_event_object(), SIGNAL(device_updated()), this, SLOT(reject()));
}

DsoMeasure::~DsoMeasure(){
    DESTROY_QT_OBJECT(_measure_tab);
}

void DsoMeasure::add_measure(QWidget *widget, const view::DsoSignal *dsoSig)
{
    const int Column = 5;
    const int IconSizeForText = 5;
    QGridLayout *layout = new QGridLayout(widget);
    layout->setSpacing(0);

    pv::view::DsoSignal *psig = const_cast<pv::view::DsoSignal*>(dsoSig);
    
    for (int i=DSO_MS_BEGIN+1; i<DSO_MS_END; i++) {
        QToolButton *button = new QToolButton(this);
        button->setProperty("id", QVariant(i));
        button->setIconSize(QSize(48, 48));
        QPixmap msPix(get_ms_icon(i));
        QBitmap msMask = msPix.createMaskFromColor(QColor("black"), Qt::MaskOutColor);
        msPix.fill(psig->get_colour());
        msPix.setMask(msMask);
        button->setIcon(QIcon(msPix));
        layout->addWidget(button,
                          ((i-1)/Column)*IconSizeForText, (i-1)%Column,
                          IconSizeForText-1, 1,
                          Qt::AlignCenter);
        layout->addWidget(new QLabel(get_ms_text(i), this),
                          ((i-1)/Column)*IconSizeForText+4, (i-1)%Column,
                          1, 1,
                          Qt::AlignCenter);
        layout->setColumnMinimumWidth((i-1)%Column, this->width()/Column);

        connect(button, SIGNAL(clicked()), this, SLOT(accept()));
    }
}

void DsoMeasure::set_measure(bool en)
{
    (void)en;
    QCheckBox* sc=dynamic_cast<QCheckBox*>(sender());
    if(sc != NULL) {
        QVariant id = sc->property("id");
    }
}

QString DsoMeasure::get_ms_icon(int ms_type)
{
    assert(ms_type >= DSO_MS_BEGIN);
    assert(ms_type < DSO_MS_END);
    const QString icon_name[DSO_MS_END-DSO_MS_BEGIN] = {"blank.png",
                                                        "mFreq.png", "mPeriod.png", "mPduty.png", "mNduty.png", "mPcount.png",
                                                        "mRise.png", "mFall.png", "mPwidth.png", "mNwidth.png", "mBurst.png",
                                                        "mAmplitude.png", "mHigh.png", "mLow.png", "mRms.png", "mMean.png",
                                                        "mVpp.png", "mMax.png", "mMin.png", "mPover.png", "mNover.png"};
    return ":/icons/"+icon_name[ms_type];
}

QString DsoMeasure::get_ms_text(int ms_type)
{
    assert(ms_type >= DSO_MS_BEGIN);
    assert(ms_type < DSO_MS_END);
    //tr
    const QString label_name[DSO_MS_END-DSO_MS_BEGIN] = {tr("NULL"),
                                                         tr("Freq"), tr("Period"), tr("+Duty"), tr("-Duty"), tr("+Count"),
                                                         tr("Rise"), tr("Fall"), tr("+Width"), tr("-Width"), tr("BrstW"),
                                                         tr("Ampl"), tr("High"), tr("Low"), tr("RMS"), tr("Mean"),
                                                         tr("PK-PK"), tr("Max"), tr("Min"), tr("+Over"), tr("-Over")};
    return label_name[ms_type];
}

void DsoMeasure::accept()
{
	using namespace Qt;

    QToolButton* sc = dynamic_cast<QToolButton*>(sender());
    if(sc != NULL) {
        QVariant id = sc->property("id");
        enum DSO_MEASURE_TYPE ms_type = DSO_MEASURE_TYPE(id.toInt());
        
        for(auto s : _session->get_signals()) { 
            if (s->signal_type() == SR_CHANNEL_DSO) {
                view::DsoSignal *dsoSig = (view::DsoSignal*)s;
                if (_measure_tab->currentWidget()->property("index").toInt() == dsoSig->get_index()) {
                    _view.get_viewstatus()->set_measure(_position, false, dsoSig->get_index(), ms_type);
                    break;
                }
            }
        }
    }
    QDialog::accept();
}

void DsoMeasure::reject()
{
    using namespace Qt;

    _view.get_viewstatus()->set_measure(_position, true, -1, DSO_MS_BEGIN);
    QDialog::reject();
}

void DsoMeasure::reset()
{
    using namespace Qt;

    _view.get_viewstatus()->set_measure(_position, false, -1, DSO_MS_BEGIN);
    QDialog::reject();
}

} // namespace dialogs
} // namespace pv
