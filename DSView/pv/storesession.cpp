/*
 * This file is part of the PulseView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2014 Joel Holdsworth <joel@airwebreathe.org.uk>
 * Copyright (C) 2016 DreamSourceLab <support@dreamsourcelab.com>
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

/* __STDC_FORMAT_MACROS is required for PRIu64 and friends (in C++). */
#define __STDC_FORMAT_MACROS

#include "storesession.h"
#include "sigsession.h"

#include "data/logic.h"
#include "data/logicsnapshot.h"
#include "data/dsosnapshot.h"
#include "data/analogsnapshot.h"
#include "data/decoderstack.h"
#include "data/decode/decoder.h"
#include "data/decode/row.h"
#include "view/trace.h"
#include "view/signal.h"
#include "view/logicsignal.h"
#include "view/dsosignal.h"
#include "view/decodetrace.h"
#include "device/devinst.h"
#include "dock/protocoldock.h" 
 
#include <QFileDialog>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <math.h>
#include <QTextStream>
#include <list>

#ifdef _WIN32
#include <QTextCodec>
#endif
 
#include "libsigrokdecode.h"
#include "config/appconfig.h"
#include "dsvdef.h"
#include "utility/encoding.h"
#include "utility/path.h"

 
namespace pv { 

StoreSession::StoreSession(SigSession *session) :
	_session(session),
    _outModule(NULL),
	_units_stored(0),
    _unit_count(0),
    _has_error(false),
    _canceled(false)
{ 
    _sessionDataGetter = NULL;
}

StoreSession::~StoreSession()
{
	wait();
}

SigSession* StoreSession::session()
{
    return _session;
}

std::pair<uint64_t, uint64_t> StoreSession::progress()
{ 
    return std::make_pair(_units_stored, _unit_count);
}

const QString& StoreSession::error()
{ 
	return _error;
}

void StoreSession::wait()
{
    if (_thread.joinable())
        _thread.join();
}

void StoreSession::cancel()
{ 
    _canceled = true; 
}

QList<QString> StoreSession::getSuportedExportFormats(){
    const struct sr_output_module** supportedModules = sr_output_list();
    QList<QString> list;
    while(*supportedModules){
        if(*supportedModules == NULL)
            break;
        if (_session->get_device()->dev_inst()->mode != LOGIC &&
            strcmp((*supportedModules)->id, "csv"))
            break;
        QString format((*supportedModules)->desc);
        format.append(" (*.");
        format.append((*supportedModules)->id);
        format.append(")");
        list.append(format);
        supportedModules++;
    }
    return list;
}

bool StoreSession::save_start()
{ 
    assert(_sessionDataGetter);

    std::set<int> type_set;
    for(auto &sig : _session->get_signals()) {
        assert(sig);
        type_set.insert(sig->get_type());
    }

    if (type_set.size() > 1) {
        _error = tr("DSView does not currently support"
                    "file saving for multiple data types.");
        return false;

    } else if (type_set.size() == 0) {
        _error = tr("No data to save.");
        return false;
    }

    if (_file_name == ""){
        _error = tr("No file name.");
        return false;
    }

    const auto snapshot = _session->get_snapshot(*type_set.begin());
	assert(snapshot);
    // Check we have data
    if (snapshot->empty()) {
        _error = tr("No data to save.");
        return false;
    }

    std::string meta_data;
    std::string decoder_data;
    std::string session_data;
    
    meta_gen(snapshot, meta_data);
    decoders_gen(decoder_data);
    _sessionDataGetter->genSessionData(session_data);

    if (meta_data.empty()) {
        _error = tr("Generate temp file data failed.");
        QFile::remove(_file_name);
        return false;
    }
    if (decoder_data.empty()){
        _error = tr("Generate decoder file data failed.");
        QFile::remove(_file_name);
        return false;
    }
    if (session_data.empty()){
        _error = tr("Generate session file data failed.");
        QFile::remove(_file_name);
        return false;
    }
   
    auto _filename = path::ConvertPath(_file_name);
    
    if (m_zipDoc.CreateNew(_filename.c_str(), false))
    {    
        if ( !m_zipDoc.AddFromBuffer("header", meta_data.c_str(), meta_data.size())
            || !m_zipDoc.AddFromBuffer("decoders", decoder_data.c_str(), decoder_data.size())
            || !m_zipDoc.AddFromBuffer("session", session_data.c_str(), session_data.size())
        )
        {
            _has_error = true;
            _error = m_zipDoc.GetError();
        }
        else
        {
            if (_thread.joinable()) _thread.join();
            _thread = std::thread(&StoreSession::save_proc, this, snapshot);
            return !_has_error;
        }
    }
    else{
         _error = tr("Generate zip file failed.");
    }

    QFile::remove(_file_name);
    return false;
}

void StoreSession::save_proc(data::Snapshot *snapshot)
{
	assert(snapshot);

    char chunk_name[20] = {0};

    int ret = SR_ERR;
    int num = 0;
    data::LogicSnapshot *logic_snapshot = NULL;
    //data::AnalogSnapshot *analog_snapshot = NULL;
    //data::DsoSnapshot *dso_snapshot = NULL;

    if ((logic_snapshot = dynamic_cast<data::LogicSnapshot*>(snapshot))) {
        uint16_t to_save_probes = 0;
        for(auto &s : _session->get_signals()) {
            if (s->enabled() && logic_snapshot->has_data(s->get_index()))
                to_save_probes++;
        }
        _unit_count = logic_snapshot->get_sample_count() / 8 * to_save_probes;
        num = logic_snapshot->get_block_num();
        bool sample;

        for(auto &s : _session->get_signals()) {
            int ch_type = s->get_type();
            if (ch_type == SR_CHANNEL_LOGIC) {
                int ch_index = s->get_index();
                if (!s->enabled() || !logic_snapshot->has_data(ch_index))
                    continue;
                for (int i = 0; !_canceled && i < num; i++) {
                    uint8_t *buf = logic_snapshot->get_block_buf(i, ch_index, sample);
                    uint64_t size = logic_snapshot->get_block_size(i);
                    bool need_malloc = (buf == NULL);
                    if (need_malloc) {
                        buf = (uint8_t *)malloc(size);
                        if (buf == NULL) {
                            _has_error = true;
                            _error = tr("Failed to create zip file. Malloc error.");
                        } else {
                            memset(buf, sample ? 0xff : 0x0, size);
                        }
                    }
                    
                    MakeChunkName(chunk_name, i, ch_index, ch_type, File_Version);
                    ret = m_zipDoc.AddFromBuffer(chunk_name, (const char*)buf, size) ? SR_OK : -1;

                    if (ret != SR_OK) {
                        if (!_has_error) {
                            _has_error = true;
                            _error = tr("Failed to create zip file. Please check write permission of this path.");
                        }
                        progress_updated();
                        if (_has_error)
                            QFile::remove(_file_name);
                        return;
                    }
                    _units_stored += size;
                    if (need_malloc)
                        free(buf);
                    progress_updated();
                }
            }
        }
    } else {
        int ch_type = -1;
        for(auto &s : _session->get_signals()) {
            ch_type = s->get_type();
            break;
        }
        if (ch_type != -1) {
            num = snapshot->get_block_num();
            _unit_count = snapshot->get_sample_count() *
                          snapshot->get_unit_bytes() *
                          snapshot->get_channel_num();
            uint8_t *buf = (uint8_t *)snapshot->get_data() +
                           (snapshot->get_ring_start() * snapshot->get_unit_bytes() * snapshot->get_channel_num());
            const uint8_t *buf_start = (uint8_t *)snapshot->get_data();
            const uint8_t *buf_end = buf_start + _unit_count;

            for (int i = 0; !_canceled && i < num; i++) {
                const uint64_t size = snapshot->get_block_size(i);
                if ((buf + size) > buf_end) {
                    uint8_t *tmp = (uint8_t *)malloc(size);
                    if (tmp == NULL) {
                        _has_error = true;
                        _error = tr("Failed to create zip file. Malloc error.");
                    } else {
                        memcpy(tmp, buf, buf_end-buf);
                        memcpy(tmp+(buf_end-buf), buf_start, buf+size-buf_end);
                    } 

                    MakeChunkName(chunk_name, i, 0, ch_type, File_Version);
                    ret = m_zipDoc.AddFromBuffer(chunk_name, (const char*)tmp, size) ? SR_OK : -1;

                    buf += (size - _unit_count);
                    if (tmp)
                        free(tmp);
                } else { 

                    MakeChunkName(chunk_name, i, 0, ch_type, File_Version);
                    ret = m_zipDoc.AddFromBuffer(chunk_name, (const char*)buf, size) ? SR_OK : -1;

                    buf += size;
                }
                if (ret != SR_OK) {
                    if (!_has_error) {
                        _has_error = true;
                        _error = tr("Failed to create zip file. Please check write permission of this path.");
                    }
                    progress_updated();
                    if (_has_error)
                        QFile::remove(_file_name);
                    return;
                }
                _units_stored += size;
                progress_updated();
            }
        }
    }
	progress_updated();

    if (_canceled || num == 0)
        QFile::remove(_file_name);
    else {
        bool bret = m_zipDoc.Close();
        m_zipDoc.Release();

        if (!bret){
            _has_error = true;
            _error = m_zipDoc.GetError();
        }
    } 
}

bool StoreSession::meta_gen(data::Snapshot *snapshot, std::string &str)
{
    GSList *l;
    GVariant *gvar; 
    struct sr_channel *probe;
    int probecnt;
    char *s;
    struct sr_status status;
    const sr_dev_inst *sdi = NULL;
    char meta[300] = {0};
 
    sdi = _session->get_device()->dev_inst();
  
    sprintf(meta, "%s", "[version]\n"); str += meta;
    sprintf(meta, "version = %d\n", File_Version); str += meta;
    sprintf(meta, "%s", "[header]\n"); str += meta;

    if (sdi->driver) {
        sprintf(meta, "driver = %s\n", sdi->driver->name); str += meta;
        sprintf(meta, "device mode = %d\n", sdi->mode); str += meta;
    }

    /* metadata */
    sprintf(meta, "capturefile = data\n"); str += meta;
    sprintf(meta, "total samples = %" PRIu64 "\n", snapshot->get_sample_count()); str += meta;

    if (sdi->mode != LOGIC) {
        sprintf(meta, "total probes = %d\n", snapshot->get_channel_num()); str += meta;
        sprintf(meta, "total blocks = %d\n", snapshot->get_block_num()); str += meta;
    }

    data::LogicSnapshot *logic_snapshot = NULL;
    if ((logic_snapshot = dynamic_cast<data::LogicSnapshot*>(snapshot))) {
        uint16_t to_save_probes = 0;
        for (l = sdi->channels; l; l = l->next) {
            probe = (struct sr_channel *)l->data;
            if (probe->enabled && logic_snapshot->has_data(probe->index))
                to_save_probes++;
        }
        sprintf(meta, "total probes = %d\n", to_save_probes); str += meta;
        sprintf(meta, "total blocks = %d\n", logic_snapshot->get_block_num()); str += meta;
    }

    s = sr_samplerate_string(_session->cur_snap_samplerate());

    sprintf(meta, "samplerate = %s\n", s); str += meta;

    if (sdi->mode == DSO) {
        gvar = _session->get_device()->get_config(NULL, NULL, SR_CONF_TIMEBASE);
        if (gvar != NULL) {
            uint64_t tmp_u64 = g_variant_get_uint64(gvar);
            sprintf(meta, "hDiv = %" PRIu64 "\n", tmp_u64); str += meta;
            g_variant_unref(gvar);
        }
        gvar = _session->get_device()->get_config(NULL, NULL, SR_CONF_MAX_TIMEBASE);
        if (gvar != NULL) {
            uint64_t tmp_u64 = g_variant_get_uint64(gvar);
            sprintf(meta, "hDiv max = %" PRIu64 "\n", tmp_u64); str += meta;
            g_variant_unref(gvar);
        }
        gvar = _session->get_device()->get_config(NULL, NULL, SR_CONF_MIN_TIMEBASE);
        if (gvar != NULL) {
            uint64_t tmp_u64 = g_variant_get_uint64(gvar);
            sprintf(meta, "hDiv min = %" PRIu64 "\n", tmp_u64); str += meta;
            g_variant_unref(gvar);
        }
        gvar = _session->get_device()->get_config(NULL, NULL, SR_CONF_UNIT_BITS);
        if (gvar != NULL) {
            uint8_t tmp_u8 = g_variant_get_byte(gvar);
            sprintf(meta, "bits = %d\n", tmp_u8); str += meta;
            g_variant_unref(gvar);
        }
        gvar = _session->get_device()->get_config(NULL, NULL, SR_CONF_REF_MIN);
        if (gvar != NULL) {
            uint32_t tmp_u32 = g_variant_get_uint32(gvar);
            sprintf(meta, "ref min = %d\n", tmp_u32); str += meta;
            g_variant_unref(gvar);
        }
        gvar = _session->get_device()->get_config(NULL, NULL, SR_CONF_REF_MAX);
        if (gvar != NULL) {
            uint32_t tmp_u32 = g_variant_get_uint32(gvar);
            sprintf(meta, "ref max = %d\n", tmp_u32); str += meta;
            g_variant_unref(gvar);
        }
    } else if (sdi->mode == LOGIC) {
        sprintf(meta, "trigger time = %lld\n", _session->get_session_time().toMSecsSinceEpoch()); str += meta;
    } else if (sdi->mode == ANALOG) {
        data::AnalogSnapshot *analog_snapshot = NULL;
        if ((analog_snapshot = dynamic_cast<data::AnalogSnapshot*>(snapshot))) {
            uint8_t tmp_u8 = analog_snapshot->get_unit_bytes();
            sprintf(meta, "bits = %d\n", tmp_u8*8); str += meta;
        }
        gvar = _session->get_device()->get_config(NULL, NULL, SR_CONF_REF_MIN);
        if (gvar != NULL) {
            uint32_t tmp_u32 = g_variant_get_uint32(gvar);
            sprintf(meta, "ref min = %d\n", tmp_u32); str += meta;
            g_variant_unref(gvar);
        }
        gvar = _session->get_device()->get_config(NULL, NULL, SR_CONF_REF_MAX);
        if (gvar != NULL) {
            uint32_t tmp_u32 = g_variant_get_uint32(gvar);
            sprintf(meta, "ref max = %d\n", tmp_u32); str += meta;
            g_variant_unref(gvar);
        }
    }
    sprintf(meta, "trigger pos = %" PRIu64 "\n", _session->get_trigger_pos()); str += meta;

    probecnt = 0; 

    for (l = sdi->channels; l; l = l->next) {
        
        probe = (struct sr_channel *)l->data;
        if (!snapshot->has_data(probe->index))
            continue;
        if (sdi->mode == LOGIC && !probe->enabled)
            continue;

        if (probe->name)
        {
            int sigdex = (sdi->mode == LOGIC) ? probe->index : probecnt;
            sprintf(meta, "probe%d = %s\n", sigdex, probe->name);
            str += meta;
        }

        if (probe->trigger){
            sprintf(meta, " trigger%d = %s\n", probecnt, probe->trigger); 
            str += meta;
        }

        if (sdi->mode == DSO)
        {
            sprintf(meta, " enable%d = %d\n", probecnt, probe->enabled);
            str += meta;
            sprintf(meta, " coupling%d = %d\n", probecnt, probe->coupling);
            str += meta;
            sprintf(meta, " vDiv%d = %" PRIu64 "\n", probecnt, probe->vdiv);
            str += meta;
            sprintf(meta, " vFactor%d = %" PRIu64 "\n", probecnt, probe->vfactor);
            str += meta;
            sprintf(meta, " vOffset%d = %d\n", probecnt, probe->hw_offset);
            str += meta;
            sprintf(meta, " vTrig%d = %d\n", probecnt, probe->trig_value);
            str += meta;

            if (sr_status_get(sdi, &status, false) == SR_OK)
            {
                if (probe->index == 0)
                {
                    sprintf(meta, " period%d = %" PRIu32 "\n", probecnt, status.ch0_cyc_tlen);
                    str += meta;
                    sprintf(meta, " pcnt%d = %" PRIu32 "\n", probecnt, status.ch0_cyc_cnt);
                    str += meta;
                    sprintf(meta, " max%d = %d\n", probecnt, status.ch0_max);
                    str += meta;
                    sprintf(meta, " min%d = %d\n", probecnt, status.ch0_min);
                    str += meta;
                    sprintf(meta, " plen%d = %" PRIu32 "\n", probecnt, status.ch0_cyc_plen);
                    str += meta;
                    sprintf(meta, " llen%d = %" PRIu32 "\n", probecnt, status.ch0_cyc_llen);
                    str += meta;
                    sprintf(meta, " level%d = %d\n", probecnt, status.ch0_level_valid);
                    str += meta;
                    sprintf(meta, " plevel%d = %d\n", probecnt, status.ch0_plevel);
                    str += meta;
                    sprintf(meta, " low%d = %" PRIu32 "\n", probecnt, status.ch0_low_level);
                    str += meta;
                    sprintf(meta, " high%d = %" PRIu32 "\n", probecnt, status.ch0_high_level);
                    str += meta;
                    sprintf(meta, " rlen%d = %" PRIu32 "\n", probecnt, status.ch0_cyc_rlen);
                    str += meta;
                    sprintf(meta, " flen%d = %" PRIu32 "\n", probecnt, status.ch0_cyc_flen);
                    str += meta;
                    sprintf(meta, " rms%d = %" PRIu64 "\n", probecnt, status.ch0_acc_square);
                    str += meta;
                    sprintf(meta, " mean%d = %" PRIu32 "\n", probecnt, status.ch0_acc_mean);
                    str += meta;
                }
                else
                {
                    sprintf(meta, " period%d = %" PRIu32 "\n", probecnt, status.ch1_cyc_tlen);
                    str += meta;
                    sprintf(meta, " pcnt%d = %" PRIu32 "\n", probecnt, status.ch1_cyc_cnt);
                    str += meta;
                    sprintf(meta, " max%d = %d\n", probecnt, status.ch1_max);
                    str += meta;
                    sprintf(meta, " min%d = %d\n", probecnt, status.ch1_min);
                    str += meta;
                    sprintf(meta, " plen%d = %" PRIu32 "\n", probecnt, status.ch1_cyc_plen);
                    str += meta;
                    sprintf(meta, " llen%d = %" PRIu32 "\n", probecnt, status.ch1_cyc_llen);
                    str += meta;
                    sprintf(meta, " level%d = %d\n", probecnt, status.ch1_level_valid);
                    str += meta;
                    sprintf(meta, " plevel%d = %d\n", probecnt, status.ch1_plevel);
                    str += meta;
                    sprintf(meta, " low%d = %" PRIu32 "\n", probecnt, status.ch1_low_level);
                    str += meta;
                    sprintf(meta, " high%d = %" PRIu32 "\n", probecnt, status.ch1_high_level);
                    str += meta;
                    sprintf(meta, " rlen%d = %" PRIu32 "\n", probecnt, status.ch1_cyc_rlen);
                    str += meta;
                    sprintf(meta, " flen%d = %" PRIu32 "\n", probecnt, status.ch1_cyc_flen);
                    str += meta;
                    sprintf(meta, " rms%d = %" PRIu64 "\n", probecnt, status.ch1_acc_square);
                    str += meta;
                    sprintf(meta, " mean%d = %" PRIu32 "\n", probecnt, status.ch1_acc_mean);
                    str += meta;
                }
            }
        }
        else if (sdi->mode == ANALOG)
        {
            sprintf(meta, " enable%d = %d\n", probecnt, probe->enabled);
            str += meta;
            sprintf(meta, " coupling%d = %d\n", probecnt, probe->coupling);
            str += meta;
            sprintf(meta, " vDiv%d = %" PRIu64 "\n", probecnt, probe->vdiv);
            str += meta;
            sprintf(meta, " vOffset%d = %d\n", probecnt, probe->hw_offset);
            str += meta;
            sprintf(meta, " mapUnit%d = %s\n", probecnt, probe->map_unit);
            str += meta;
            sprintf(meta, " mapMax%d = %lf\n", probecnt, probe->map_max);
            str += meta;
            sprintf(meta, " mapMin%d = %lf\n", probecnt, probe->map_min);
            str += meta;
        }
        probecnt++;
    } 

    return true;
}

//export as csv file
bool StoreSession::export_start()
{
    std::set<int> type_set;
    for(auto &sig : _session->get_signals()) {
        assert(sig);
        int _tp = sig->get_type();
        type_set.insert(_tp);
    }

    if (type_set.size() > 1) {
        _error = tr("DSView does not currently support"
                    "file export for multiple data types.");
        return false;
    } else if (type_set.size() == 0) {
        _error = tr("No data to save.");
        return false;
    }

    const auto snapshot = _session->get_snapshot(*type_set.begin());
    assert(snapshot);
    // Check we have data
    if (snapshot->empty()) {
        _error = tr("No data to save.");
        return false;
    }

    if (_file_name == ""){
        _error = tr("No set file name.");
        return false;
    }

    const struct sr_output_module **supportedModules = sr_output_list();
    while (*supportedModules)
    {
        if (*supportedModules == NULL)
            break;
        if (!strcmp((*supportedModules)->id, _suffix.toUtf8().data()))
        {
            _outModule = *supportedModules;
            break;
        }
        supportedModules++;
    }

    if (_outModule == NULL)
    {
        _error = tr("Invalid export format.");
    }
    else
    {
        if (_thread.joinable()) _thread.join();
        _thread = std::thread(&StoreSession::export_proc, this, snapshot);
        return !_has_error;
    }

    _error.clear();
    return false;
}

void StoreSession::export_proc(data::Snapshot *snapshot)
{
    assert(snapshot);

        //set export all data flag
    AppConfig &app = AppConfig::Instance();
    int origin_flag = app._appOptions.originalData ? 1 : 0;

    data::LogicSnapshot *logic_snapshot = NULL;
    data::AnalogSnapshot *analog_snapshot = NULL;
    data::DsoSnapshot *dso_snapshot = NULL;
    int channel_type;

    if ((logic_snapshot = dynamic_cast<data::LogicSnapshot*>(snapshot))) {
        channel_type = SR_CHANNEL_LOGIC;
    } else if ((dso_snapshot = dynamic_cast<data::DsoSnapshot*>(snapshot))) {
        channel_type = SR_CHANNEL_DSO;
    } else if ((analog_snapshot = dynamic_cast<data::AnalogSnapshot*>(snapshot))) {
        channel_type = SR_CHANNEL_ANALOG;
    } else {
        _has_error = true;
        _error = tr("data type don't support.");
        return;
    }

    GHashTable *params = g_hash_table_new(g_str_hash, g_str_equal);
    GVariant* filenameGVariant = g_variant_new_bytestring(_file_name.toUtf8().data());
    g_hash_table_insert(params, (char*)"filename", filenameGVariant);
    GVariant* typeGVariant = g_variant_new_int16(channel_type);
    g_hash_table_insert(params, (char*)"type", typeGVariant);

    struct sr_output output;
    output.module = (sr_output_module*) _outModule;
    output.sdi = _session->get_device()->dev_inst();
    output.param = NULL;
    if(_outModule->init)
        _outModule->init(&output, params);

    QFile file(_file_name);
    file.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream out(&file); 
    encoding::set_utf8(out);
    //out.setGenerateByteOrderMark(true);  // UTF-8 without BOM

    // Meta
    GString *data_out;
    struct sr_datafeed_packet p;
    struct sr_datafeed_meta meta;
    struct sr_config *src;

    src = sr_config_new(SR_CONF_SAMPLERATE,
            g_variant_new_uint64(_session->cur_snap_samplerate()));
    meta.config = g_slist_append(NULL, src);

    src = sr_config_new(SR_CONF_LIMIT_SAMPLES,
            g_variant_new_uint64(snapshot->get_sample_count()));
    meta.config = g_slist_append(meta.config, src);

    GVariant *gvar;
    uint8_t bits=0;
    gvar = _session->get_device()->get_config(NULL, NULL, SR_CONF_UNIT_BITS);
    if (gvar != NULL) {
        bits = g_variant_get_byte(gvar);
        g_variant_unref(gvar);
    }
    gvar = _session->get_device()->get_config(NULL, NULL, SR_CONF_REF_MIN);
    if (gvar != NULL) {
        src = sr_config_new(SR_CONF_REF_MIN, gvar);
        g_variant_unref(gvar);
    } else {
        src = sr_config_new(SR_CONF_REF_MIN, g_variant_new_uint32(1));
    }
    meta.config = g_slist_append(meta.config, src);
    gvar = _session->get_device()->get_config(NULL, NULL, SR_CONF_REF_MAX);
    if (gvar != NULL) {
        src = sr_config_new(SR_CONF_REF_MAX, gvar);
        g_variant_unref(gvar);
    } else {
        src = sr_config_new(SR_CONF_REF_MAX, g_variant_new_uint32((1 << bits) - 1));
    }
    meta.config = g_slist_append(meta.config, src);

    p.type = SR_DF_META;
    p.status = SR_PKT_OK;
    p.payload = &meta;
    p.bExportOriginalData = 0;
    _outModule->receive(&output, &p, &data_out);
    if(data_out){
        out << QString::fromUtf8((char*) data_out->str);
        g_string_free(data_out,TRUE);
    }
    for (GSList *l = meta.config; l; l = l->next) {
        src = (struct sr_config *)l->data;
        sr_config_free(src);
    }
    g_slist_free(meta.config);

    if (channel_type == SR_CHANNEL_LOGIC) {
        _unit_count = logic_snapshot->get_sample_count();
        int blk_num = logic_snapshot->get_block_num();
        bool sample;
        std::vector<uint8_t *> buf_vec;
        std::vector<bool> buf_sample;

        for (int blk = 0; !_canceled  &&  blk < blk_num; blk++) {
            uint64_t buf_sample_num = logic_snapshot->get_block_size(blk) * 8;
            buf_vec.clear();
            buf_sample.clear();

            for(auto &s : _session->get_signals()) {
                int ch_type = s->get_type();
                if (ch_type == SR_CHANNEL_LOGIC) {
                    int ch_index = s->get_index();
                    if (!logic_snapshot->has_data(ch_index))
                        continue;
                    uint8_t *buf = logic_snapshot->get_block_buf(blk, ch_index, sample);
                    buf_vec.push_back(buf);
                    buf_sample.push_back(sample);
                }
            }

            uint16_t unitsize = ceil(buf_vec.size() / 8.0);
            unsigned int usize = 8192;
            unsigned int size = usize;
            struct sr_datafeed_logic lp;

            for(uint64_t i = 0; !_canceled && i < buf_sample_num; i+=usize){
                if(buf_sample_num - i < usize)
                    size = buf_sample_num - i;
                uint8_t *xbuf = (uint8_t *)malloc(size * unitsize);
                if (xbuf == NULL) {
                    _has_error = true;
                    _error = tr("xbuffer malloc failed.");
                    return;
                }                
                memset(xbuf, 0, size * unitsize);

                for (uint64_t j = 0; j < size; j++) {
                    for (unsigned int k = 0; k < buf_vec.size(); k++) {
                        if (buf_vec[k] == NULL && buf_sample[k])
                            xbuf[j*unitsize+k/8] +=  1 << k%8;
                        else if (buf_vec[k] && (buf_vec[k][(i+j)/8] & (1 << j%8)))
                            xbuf[j*unitsize+k/8] +=  1 << k%8;
                    }
                }

                lp.data = xbuf;
                lp.length = size * unitsize;
                lp.unitsize = unitsize;
                p.type = SR_DF_LOGIC;
                p.status = SR_PKT_OK;
                p.payload = &lp;
                p.bExportOriginalData = origin_flag;
                _outModule->receive(&output, &p, &data_out);

                if(data_out){
                    out << QString::fromUtf8((char*) data_out->str);
                    g_string_free(data_out,TRUE);
                }

                _units_stored += size;
                if (xbuf)
                    free(xbuf);
                progress_updated();
            }
        }

    } else if (channel_type == SR_CHANNEL_DSO) {
        _unit_count = snapshot->get_sample_count();
        unsigned char* datat = (unsigned char*)snapshot->get_data();
        unsigned int usize = 8192;
        unsigned int size = usize;
        struct sr_datafeed_dso dp; 

        for(uint64_t i = 0; !_canceled && i < _unit_count; i+=usize){
            if(_unit_count - i < usize)
                size = _unit_count - i;

            dp.data = &datat[i*snapshot->get_channel_num()];
            dp.num_samples = size;
            p.type = SR_DF_DSO;
            p.status = SR_PKT_OK;
            p.payload = &dp;
            p.bExportOriginalData = 0;
            _outModule->receive(&output, &p, &data_out);

            if(data_out){
                out << (char*) data_out->str;
                g_string_free(data_out,TRUE);
            }

            _units_stored += size;
            progress_updated();
        }

    } else if (channel_type == SR_CHANNEL_ANALOG) {
        _unit_count = snapshot->get_sample_count();
        unsigned char* datat = (unsigned char*)snapshot->get_data();
        unsigned int usize = 8192;
        unsigned int size = usize;
        struct sr_datafeed_analog ap;

        for(uint64_t i = 0; !_canceled && i < _unit_count; i+=usize){
            if(_unit_count - i < usize)
                size = _unit_count - i;
            ap.data = &datat[i*snapshot->get_channel_num()];
            ap.num_samples = size;
            p.type = SR_DF_ANALOG;
            p.status = SR_PKT_OK;
            p.payload = &ap;
            p.bExportOriginalData = 0;
            _outModule->receive(&output, &p, &data_out);
            if(data_out){
                out << (char*) data_out->str;
                g_string_free(data_out,TRUE);
            }

            _units_stored += size;
            progress_updated();
        }
    }

    // optional, as QFile destructor will already do it:
    file.close();
    _outModule->cleanup(&output);
    g_hash_table_destroy(params);
    if (filenameGVariant != NULL)
        g_variant_unref(filenameGVariant);

    progress_updated();
}

 
bool StoreSession::decoders_gen(std::string &str)
{  
    QJsonArray dec_array;
    if (!json_decoders(dec_array))
        return false;
    QJsonDocument sessionDoc(dec_array);
    QString data = QString::fromUtf8(sessionDoc.toJson());
    str.append(data.toLatin1().data());
    return true;
}

bool StoreSession::json_decoders(QJsonArray &array)
{  
    for(auto &t : _session->get_decode_signals()) {
        QJsonObject dec_obj;
        QJsonArray stack_array;
        QJsonObject show_obj;
        const auto &stack = t->decoder();
        const auto &decoder = stack->stack();

        for(auto &dec : decoder) {
            QJsonArray ch_array;
            const srd_decoder *const d = dec->decoder();;
            const bool have_probes = (d->channels || d->opt_channels) != 0;

            if (have_probes) {
                for(auto i = dec->channels().begin();
                    i != dec->channels().end(); i++) {
                    QJsonObject ch_obj;
                    ch_obj[(*i).first->id] = QJsonValue::fromVariant((*i).second);
                    ch_array.push_back(ch_obj);
                }
            }

            QJsonObject options_obj;
            auto dec_binding = new prop::binding::DecoderOptions(stack, dec);

            for (GSList *l = d->options; l; l = l->next)
            {
                const srd_decoder_option *const opt =
                    (srd_decoder_option*)l->data;

                if (g_variant_is_of_type(opt->def, G_VARIANT_TYPE("d"))) {
                    GVariant *const var = dec_binding->getter(opt->id);
                    if (var != NULL) {
                        options_obj[opt->id] = QJsonValue::fromVariant(g_variant_get_double(var));
                        g_variant_unref(var);
                    }
                } else if (g_variant_is_of_type(opt->def, G_VARIANT_TYPE("x"))) {
                    GVariant *const var = dec_binding->getter(opt->id);
                    if (var != NULL) {
                        options_obj[opt->id] = QJsonValue::fromVariant(get_integer(var));
                        g_variant_unref(var);
                    }
                } else if (g_variant_is_of_type(opt->def, G_VARIANT_TYPE("s"))) {
                    GVariant *const var = dec_binding->getter(opt->id);
                    if (var != NULL) {
                        options_obj[opt->id] = QJsonValue::fromVariant(g_variant_get_string(var, NULL));
                        g_variant_unref(var);
                    }
                }else {
                    continue;
                }
            }

            if (have_probes) {
                dec_obj["id"] = QJsonValue::fromVariant(d->id);
                dec_obj["channel"] = ch_array;
                dec_obj["options"] = options_obj;
            } else {
                QJsonObject stack_obj;
                stack_obj["id"] = QJsonValue::fromVariant(d->id);
                stack_obj["options"] = options_obj;
                stack_array.push_back(stack_obj);
            }
            show_obj[d->id] = QJsonValue::fromVariant(dec->shown());
        }
        dec_obj["stacked decoders"] = stack_array;


        auto rows = stack->get_rows_gshow();
        for (auto i = rows.begin(); i != rows.end(); i++) {
            pv::data::decode::Row _row = (*i).first;
            QString kn = _row.title();
            show_obj[kn] = QJsonValue::fromVariant((*i).second);
        }
        dec_obj["show"] = show_obj;

        array.push_back(dec_obj);
    }

    return true;
}

bool StoreSession::load_decoders(dock::ProtocolDock *widget, QJsonArray dec_array)
{
    if (_session->get_device()->dev_inst()->mode != LOGIC || dec_array.empty())
    {
        return false;
    }
    
    for (const QJsonValue &dec_value : dec_array)
    {
        QJsonObject dec_obj = dec_value.toObject(); 
        std::vector<view::DecodeTrace*> &pre_dsigs = _session->get_decode_signals();
        std::list<pv::data::decode::Decoder*> sub_decoders;

        //get sub decoders
        if (dec_obj.contains("stacked decoders")) {
                for(const QJsonValue &value : dec_obj["stacked decoders"].toArray()) {
                    QJsonObject stacked_obj = value.toObject();

                    GSList *dl = g_slist_copy((GSList*)srd_decoder_list());
                    for(; dl; dl = dl->next) {
                        const srd_decoder *const d = (srd_decoder*)dl->data;
                        assert(d);

                        if (QString::fromUtf8(d->id) == stacked_obj["id"].toString()) {
                            sub_decoders.push_back(new data::decode::Decoder(d));
                            break;
                        }
                    }
                    g_slist_free(dl);
                }
        }

        //create protocol
        bool ret = widget->add_protocol_by_id(dec_obj["id"].toString(), true, sub_decoders);
        if (!ret)
        {
            for(auto sub : sub_decoders){
                delete sub;
            }
            sub_decoders.clear();

            continue; //protocol is not exists;
        }        

        std::vector<view::DecodeTrace*> &aft_dsigs = _session->get_decode_signals();

        if (aft_dsigs.size() >= pre_dsigs.size()) {
            const GSList *l;
            
            auto new_dsig = aft_dsigs.back();
            auto stack = new_dsig->decoder();
 
            auto &decoder_list = stack->stack();

            for(auto dec : decoder_list) {
                const srd_decoder *const d = dec->decoder();
                QJsonObject options_obj;

                if (dec == decoder_list.front()) {
                    std::map<const srd_channel*, int> probe_map;
                    // Load the mandatory channels
                    for(l = d->channels; l; l = l->next) {
                        const struct srd_channel *const pdch = (struct srd_channel *)l->data;

                        for (const QJsonValue &value : dec_obj["channel"].toArray()) {
                            QJsonObject ch_obj = value.toObject();
                            if (ch_obj.contains(pdch->id)) {
                                probe_map[pdch] = ch_obj[pdch->id].toInt();
                                break;
                            }
                        }
                    }

                    // Load the optional channels
                    for(l = d->opt_channels; l; l = l->next) {
                        const struct srd_channel *const pdch = (struct srd_channel *)l->data;

                        for (const QJsonValue &value : dec_obj["channel"].toArray()) {
                            QJsonObject ch_obj = value.toObject();
                            if (ch_obj.contains(pdch->id)) {
                                probe_map[pdch] = ch_obj[pdch->id].toInt();
                                break;
                            }
                        }
                    }
                    dec->set_probes(probe_map);
                    options_obj = dec_obj["options"].toObject();
                }
                else {
                    for(const QJsonValue &value : dec_obj["stacked decoders"].toArray()) {
                        QJsonObject stacked_obj = value.toObject();
                        if (QString::fromUtf8(d->id) == stacked_obj["id"].toString()) {
                            options_obj = stacked_obj["options"].toObject();
                            break;
                        }
                    }
                }

                for (l = d->options; l; l = l->next) {
                    const srd_decoder_option *const opt = (srd_decoder_option*)l->data;

                    if (options_obj.contains(opt->id)) 
                    {
                        GVariant *new_value = NULL;
                        // When the numberic value is a string, it got zero always,
                        // so must convert from string.
                        QString vs = options_obj[opt->id].toString();

                        if (g_variant_is_of_type(opt->def, G_VARIANT_TYPE("d"))) 
                        {
                            double vi = options_obj[opt->id].toDouble();
                            if (vs != "") vi = vs.toDouble();
                            new_value = g_variant_new_double(vi);
                        }
                        else if (g_variant_is_of_type(opt->def, G_VARIANT_TYPE("x"))) {
                            const GVariantType *const type = g_variant_get_type(opt->def);

                            if (g_variant_type_equal(type, G_VARIANT_TYPE_BYTE)){
                                int vi = options_obj[opt->id].toInt();                               
                                if (vs != "") vi = vs.toInt();
                                new_value = g_variant_new_byte(vi);
                            }
                            else if (g_variant_type_equal(type, G_VARIANT_TYPE_INT16)){
                                int vi = options_obj[opt->id].toInt();                               
                                if (vs != "") vi = vs.toInt();
                                new_value = g_variant_new_int16(vi);
                            }
                            else if (g_variant_type_equal(type, G_VARIANT_TYPE_UINT16)){
                                int vi = options_obj[opt->id].toInt();                               
                                if (vs != "") vi = vs.toInt();
                                new_value = g_variant_new_uint16(vi);
                            }
                            else if (g_variant_type_equal(type, G_VARIANT_TYPE_INT32)){
                                int vi = options_obj[opt->id].toInt();                               
                                if (vs != "") vi = vs.toInt();
                                new_value = g_variant_new_int32(vi);
                            }
                            else if (g_variant_type_equal(type, G_VARIANT_TYPE_UINT32)){
                                int vi = options_obj[opt->id].toInt();                               
                                if (vs != "") vi = vs.toInt();
                                new_value = g_variant_new_uint32(vi);
                            }
                            else if (g_variant_type_equal(type, G_VARIANT_TYPE_INT64)){
                                int vi = options_obj[opt->id].toInt();                               
                                if (vs != "") vi = vs.toInt();
                                new_value = g_variant_new_int64(vi);
                            }
                            else if (g_variant_type_equal(type, G_VARIANT_TYPE_UINT64)){
                                int vi = options_obj[opt->id].toInt();                               
                                if (vs != "") vi = vs.toInt();
                                new_value = g_variant_new_uint64(vi);
                            }
                        }
                        else if (g_variant_is_of_type(opt->def, G_VARIANT_TYPE("s"))) {
                            new_value = g_variant_new_string(vs.toUtf8().data());
                        }

                        if (new_value != NULL){
                            dec->set_option(opt->id, new_value);
                        }
                    }
                }
                dec->commit();

                if (dec_obj.contains("show")) {
                    QJsonObject show_obj = dec_obj["show"].toObject();
                    if (show_obj.contains(d->id)) {
                        dec->show(show_obj[d->id].toBool());
                    }
                }
            }

            if (dec_obj.contains("show")) {
                QJsonObject show_obj = dec_obj["show"].toObject();
                std::map<const pv::data::decode::Row, bool> rows = stack->get_rows_gshow();

                for (auto i = rows.begin();i != rows.end(); i++) {
                        QString key = (*i).first.title();
                    if (show_obj.contains(key)) {
                        bool bShow = show_obj[key].toBool();
                        const pv::data::decode::Row r = (*i).first;
                        stack->set_rows_gshow(r, bShow);
                    }
                }
            }
        }
    }

    return true;
}
 

double StoreSession::get_integer(GVariant *var)
{
    double val = 0;
    const GVariantType *const type = g_variant_get_type(var);
    assert(type);

    if (g_variant_type_equal(type, G_VARIANT_TYPE_BYTE))
        val = g_variant_get_byte(var);
    else if (g_variant_type_equal(type, G_VARIANT_TYPE_INT16))
        val = g_variant_get_int16(var);
    else if (g_variant_type_equal(type, G_VARIANT_TYPE_UINT16))
        val = g_variant_get_uint16(var);
    else if (g_variant_type_equal(type, G_VARIANT_TYPE_INT32))
        val = g_variant_get_int32(var);
    else if (g_variant_type_equal(type, G_VARIANT_TYPE_UINT32))
        val = g_variant_get_uint32(var);
    else if (g_variant_type_equal(type, G_VARIANT_TYPE_INT64))
        val = g_variant_get_int64(var);
    else if (g_variant_type_equal(type, G_VARIANT_TYPE_UINT64))
        val = g_variant_get_uint64(var);
    else
        assert(0);

    return val;
}

QString StoreSession::MakeSaveFile(bool bDlg)
{
    QString default_name;

    AppConfig &app = AppConfig::Instance(); 
    if (app._userHistory.saveDir != "")
    {
        default_name = app._userHistory.saveDir + "/"  + _session->get_device()->name() + "-";
    } 
    else{
        QDir _dir;
        QString _root = _dir.home().path();                
        default_name =  _root + "/" + _session->get_device()->name() + "-";
    } 

    for (const GSList *l = _session->get_device()->get_dev_mode_list();
         l; l = l->next) {
        const sr_dev_mode *mode = (const sr_dev_mode *)l->data;
        if (_session->get_device()->dev_inst()->mode == mode->mode) {
            default_name += mode->acronym;
            break;
        }
    }

    default_name += _session->get_session_time().toString("-yyMMdd-hhmmss");

    // Show the dialog
    if (bDlg)
    {
        default_name = QFileDialog::getSaveFileName(
            NULL,
            tr("Save File"),
            default_name,
            tr("DSView Data (*.dsl)"));

        if (default_name.isEmpty())
        {
            return ""; //no select file
        }

        QString _dir_path = path::GetDirectoryName(default_name);

        if (_dir_path != app._userHistory.saveDir)
        {
            app._userHistory.saveDir = _dir_path;
            app.SaveHistory();
        }
    }

    QFileInfo f(default_name);
    if (f.suffix().compare("dsl"))
    {
        default_name.append(tr(".dsl"));
    }
    _file_name = default_name;
    return default_name;     
}

QString StoreSession::MakeExportFile(bool bDlg)
{
    QString default_name;
    AppConfig &app = AppConfig::Instance();  
    
    if (app._userHistory.exportDir != "")
    {
        default_name = app._userHistory.exportDir  + "/"  + _session->get_device()->name() + "-";
    } 
    else{
        QDir _dir;
        QString _root = _dir.home().path();    
        default_name =  _root + "/" + _session->get_device()->name() + "-";
    }  

    for (const GSList *l = _session->get_device()->get_dev_mode_list();
         l; l = l->next) {
        const sr_dev_mode *mode = (const sr_dev_mode *)l->data;
        if (_session->get_device()->dev_inst()->mode == mode->mode) {
            default_name += mode->acronym;
            break;
        }
    }
    default_name += _session->get_session_time().toString("-yyMMdd-hhmmss");

    //ext name
    QList<QString> supportedFormats = getSuportedExportFormats();
    QString filter;
    for(int i = 0; i < supportedFormats.count();i++){
        filter.append(supportedFormats[i]);
        if(i < supportedFormats.count() - 1)
            filter.append(";;");
    }

    QString selfilter;
    if (app._userHistory.exportFormat != ""){
        selfilter.append(app._userHistory.exportFormat);
    }

    if (bDlg)
    {
        default_name = QFileDialog::getSaveFileName(
            NULL,
            tr("Export Data"),
            default_name,
            filter,
            &selfilter);

        if (default_name == "")
        {
            return "";
        }

        bool bChange = false;
        QString _dir_path = path::GetDirectoryName(default_name);
        if (_dir_path != app._userHistory.exportDir)
        {
            app._userHistory.exportDir = _dir_path;
            bChange = true;
        }
        if (selfilter != app._userHistory.exportFormat){
            app._userHistory.exportFormat = selfilter;
             bChange = true;            
        }

        if (bChange){
            app.SaveHistory();            
        }
    }

    QString extName = selfilter;
    if (extName == ""){
        extName = filter;
    }

    QStringList list = extName.split('.').last().split(')');
    _suffix = list.first();

    QFileInfo f(default_name);
    if(f.suffix().compare(_suffix)){
         default_name += tr(".") + _suffix;
    }           

    _file_name = default_name;
    return default_name;    
}

bool StoreSession::IsLogicDataType()
{
    std::set<int> type_set;
    for(auto &sig : _session->get_signals()) {
        assert(sig);
        type_set.insert(sig->get_type());
    }

    if (type_set.size()){
        int type = *(type_set.begin());
        return type == SR_CHANNEL_LOGIC;
    }

    return false;
}

void StoreSession::MakeChunkName(char *chunk_name, int chunk_num, int index, int type, int version)
{ 
    chunk_name[0] = 0;

    if (version == 2)
    {
        const char *type_name = NULL;
        type_name = (type == SR_CHANNEL_LOGIC) ? "L" : (type == SR_CHANNEL_DSO)  ? "O"
                                                   : (type == SR_CHANNEL_ANALOG) ? "A"
                                                                                 : "U";
        snprintf(chunk_name, 15, "%s-%d/%d", type_name, index, chunk_num);
    }
    else
    {
        snprintf(chunk_name, 15, "data");
    }
}

} // pv
