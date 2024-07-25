/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2013 DreamSourceLab <support@dreamsourcelab.com>
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

#include "libsigrok-internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <glib.h>
#include "log.h"

#undef LOG_PREFIX
#define LOG_PREFIX "trigger: "

/**
 * @file
 *
 * init, set DSLogic trigger.
 */

/**
 * @defgroup Trigger handling
 *
 * init, set DSLogic trigger
 *
 * @{
 */

struct ds_trigger *trigger = NULL;

/**
 * recovery trigger to initial status.
 *
 * @return SR_OK upon success.
 */
SR_PRIV int ds_trigger_init(void)
{
    int i, j;

    if (!trigger) {
        if (!(trigger = g_try_malloc0(sizeof(struct ds_trigger)))) {
            sr_err("Trigger malloc failed.");
            return SR_ERR_MALLOC;
        }
        memset(trigger, 0, sizeof(struct ds_trigger));
    }

    trigger->trigger_en = 0;
    trigger->trigger_mode = SIMPLE_TRIGGER;
    trigger->trigger_pos = 0;
    trigger->trigger_stages = 0;

    for (i = 0; i <= TriggerStages; i++) {
        for (j = 0; j < MaxTriggerProbes; j++) {
            trigger->trigger0[i][j] = 'X';
            trigger->trigger1[i][j] = 'X';
        }
        trigger->trigger0_count[i] = 0;
        trigger->trigger1_count[i] = 0;
        trigger->trigger0_inv[i] = 0;
        trigger->trigger1_inv[i] = 0;
        trigger->trigger_logic[i] = 1;
    }


    return SR_OK;
}

SR_PRIV int ds_trigger_destroy(void)
{
    if (trigger)
        g_free(trigger);
    trigger = NULL;
    return SR_OK;
}


/**
 * set trigger based on stage
 *
 * @return SR_OK upon success.
 */
SR_API int ds_trigger_stage_set_value(uint16_t stage, uint16_t probes, char *trigger0, char *trigger1)
{
    assert(stage < TriggerStages);
    assert(probes <= MaxTriggerProbes);

    if (trigger == NULL){
        sr_err("ds_trigger_stage_set_value() error, trigger have'nt be inited.");
        return SR_ERR_CALL_STATUS;
    }

    int j;

    for (j = 0; j< probes; j++) {
        trigger->trigger0[stage][probes - j - 1] = *(trigger0 + j * 2);
        trigger->trigger1[stage][probes - j - 1] = *(trigger1 + j * 2);
    }

    return SR_OK;
}
SR_API int ds_trigger_stage_set_logic(uint16_t stage, uint16_t probes, unsigned char trigger_logic)
{
    (void)probes;
    
    assert(stage < TriggerStages);
    assert(probes <= MaxTriggerProbes);

    if (trigger == NULL){
        sr_err("ds_trigger_stage_set_logic() error, trigger have'nt be inited.");
        return SR_ERR_CALL_STATUS;
    }

    trigger->trigger_logic[stage] = trigger_logic;

    return SR_OK;
}
SR_API int ds_trigger_stage_set_inv(uint16_t stage, uint16_t probes, unsigned char trigger0_inv, unsigned char trigger1_inv)
{
    (void)probes;
    
    assert(stage < TriggerStages);
    assert(probes <= MaxTriggerProbes);

    if (trigger == NULL){
        sr_err("ds_trigger_stage_set_inv() error, trigger have'nt be inited.");
        return SR_ERR_CALL_STATUS;
    }

    trigger->trigger0_inv[stage] = trigger0_inv;
    trigger->trigger1_inv[stage] = trigger1_inv;

    return SR_OK;
}
SR_API int ds_trigger_stage_set_count(uint16_t stage, uint16_t probes, uint32_t trigger0_count, uint32_t trigger1_count)
{
    (void)probes;

    assert(stage < TriggerStages);
    assert(probes <= MaxTriggerProbes);

    if (trigger == NULL){
        sr_err("ds_trigger_stage_set_count() error, trigger have'nt be inited.");
        return SR_ERR_CALL_STATUS;
    }

    trigger->trigger0_count[stage] = trigger0_count;
    trigger->trigger1_count[stage] = trigger1_count;

    return SR_OK;
}

/**
 * set trigger based on probe
 *
 * @return SR_OK upon success.
 */
SR_API int ds_trigger_probe_set(uint16_t probe, unsigned char trigger0, unsigned char trigger1)
{
    assert(probe < MaxTriggerProbes);

    if (trigger == NULL){
        sr_err("ds_trigger_probe_set() error, trigger have'nt be inited.");
        return SR_ERR_CALL_STATUS;
    }

    trigger->trigger0[TriggerStages][probe] = trigger0;
    trigger->trigger1[TriggerStages][probe] = trigger1;

    return SR_OK;
}

/**
 * set trigger stage
 *
 * @return SR_OK upon success.
 */
SR_API int ds_trigger_set_stage(uint16_t stages)
{
    assert(stages <= TriggerStages);

    if (trigger == NULL){
        sr_err("ds_trigger_set_stage() error, trigger have'nt be inited.");
        return SR_ERR_CALL_STATUS;
    }

    trigger->trigger_stages = stages;

    return SR_OK;
}

/**
 * set trigger position
 *
 * @return SR_OK upon success.
 */
SR_API int ds_trigger_set_pos(uint16_t position)
{
    assert(position <= 100);

    if (trigger == NULL){
        sr_err("ds_trigger_set_pos() error, trigger have'nt be inited.");
        return SR_ERR_CALL_STATUS;
    }

    trigger->trigger_pos = position;

    return SR_OK;
}

/**
 * get trigger position
 *
 * @return uint16_t trigger_pos.
 */
SR_API uint16_t ds_trigger_get_pos()
{
    if (trigger == NULL){
        sr_err("ds_trigger_get_pos() error, trigger have'nt be inited.");
        return 0;
    }

    return trigger->trigger_pos;
}

/**
 * set trigger en
 *
 * @return SR_OK upon success.
 */
SR_API int ds_trigger_set_en(uint16_t enable)
{
    if (trigger == NULL){
        sr_err("ds_trigger_set_en() error, trigger have'nt be inited.");
        return SR_ERR_CALL_STATUS;
    }

    trigger->trigger_en = enable;

    return SR_OK;
}

/**
 * get trigger en
 *
 * @return SR_OK upon success.
 */
SR_API uint16_t ds_trigger_get_en()
{
    if (trigger == NULL)
        return 0;
    else
        return trigger->trigger_en;
}

/**
 * set trigger mode
 *
 * @return SR_OK upon success.
 */
SR_API int ds_trigger_set_mode(uint16_t mode)
{
    if (trigger == NULL){
        sr_err("ds_trigger_set_en() error, trigger have'nt be inited.");
        return SR_ERR_CALL_STATUS;
    }

    trigger->trigger_mode = mode;

    return SR_OK;
}

/*
 *
 */
SR_PRIV uint16_t ds_trigger_get_mask0(uint16_t stage, uint16_t msc, uint16_t lsc, gboolean qutr_mode, gboolean half_mode)
{
    assert(stage <= TriggerStages);
    assert(lsc <= msc);
    assert(msc < MaxTriggerProbes);

    if (trigger == NULL){
        sr_err("ds_trigger_get_mask0() error, trigger have'nt be inited.");
        return SR_ERR_CALL_STATUS;
    }

    uint16_t mask = 0;
    const uint16_t qutr_mask = (0xffff >> (TriggerProbes - TriggerProbes/4));
    const uint16_t half_mask = (0xffff >> (TriggerProbes - TriggerProbes/2));
    int i;

    for (i = msc; i >= lsc ; i--) {
        mask = (mask << 1);
        mask += ((trigger->trigger0[stage][i] == 'X') | (trigger->trigger0[stage][i] == 'C'));
    }

    if (qutr_mode)
        mask = ((mask & qutr_mask) << (TriggerProbes/4*3)) +
               ((mask & qutr_mask) << (TriggerProbes/4*2)) +
               ((mask & qutr_mask) << (TriggerProbes/4*1)) +
               ((mask & qutr_mask) << (TriggerProbes/4*0));
    else if (half_mode)
        mask = ((mask & half_mask) << (TriggerProbes/2*1)) +
               ((mask & half_mask) << (TriggerProbes/2*0));

    return mask;
}
SR_PRIV uint16_t ds_trigger_get_mask1(uint16_t stage, uint16_t msc, uint16_t lsc, gboolean qutr_mode, gboolean half_mode)
{
    assert(stage <= TriggerStages);
    assert(lsc <= msc);
    assert(msc < MaxTriggerProbes);

    if (trigger == NULL){
        sr_err("ds_trigger_get_mask1() error, trigger have'nt be inited.");
        return SR_ERR_CALL_STATUS;
    }

    uint16_t mask = 0;
    const uint16_t qutr_mask = (0xffff >> (TriggerProbes - TriggerProbes/4));
    const uint16_t half_mask = (0xffff >> (TriggerProbes - TriggerProbes/2));
    int i;

    for (i = msc; i >= lsc ; i--) {
        mask = (mask << 1);
        mask += ((trigger->trigger1[stage][i] == 'X') | (trigger->trigger1[stage][i] == 'C'));
    }

    if (qutr_mode)
        mask = ((mask & qutr_mask) << (TriggerProbes/4*3)) +
               ((mask & qutr_mask) << (TriggerProbes/4*2)) +
               ((mask & qutr_mask) << (TriggerProbes/4*1)) +
               ((mask & qutr_mask) << (TriggerProbes/4*0));
    else if (half_mode)
        mask = ((mask & half_mask) << (TriggerProbes/2*1)) +
               ((mask & half_mask) << (TriggerProbes/2*0));

    return mask;
}
SR_PRIV uint16_t ds_trigger_get_value0(uint16_t stage, uint16_t msc, uint16_t lsc, gboolean qutr_mode, gboolean half_mode)
{
    assert(stage <= TriggerStages);
    assert(lsc <= msc);
    assert(msc < MaxTriggerProbes);

    if (trigger == NULL){
        sr_err("ds_trigger_get_value0() error, trigger have'nt be inited.");
        return SR_ERR_CALL_STATUS;
    }

    uint16_t value = 0;
    const uint16_t qutr_mask = (0xffff >> (TriggerProbes - TriggerProbes/4));
    const uint16_t half_mask = (0xffff >> (TriggerProbes - TriggerProbes/2));
    int i;

    for (i = msc; i >= lsc ; i--) {
        value = (value << 1);
        value += ((trigger->trigger0[stage][i] == '1') | (trigger->trigger0[stage][i] == 'R'));
    }

    if (qutr_mode)
        value = ((value & qutr_mask) << (TriggerProbes/4*3)) +
                ((value & qutr_mask) << (TriggerProbes/4*2)) +
                ((value & qutr_mask) << (TriggerProbes/4*1)) +
                ((value & qutr_mask) << (TriggerProbes/4*0));
    else if (half_mode)
        value = ((value & half_mask) << (TriggerProbes/2*1)) +
                ((value & half_mask) << (TriggerProbes/2*0));

    return value;
}
SR_PRIV uint16_t ds_trigger_get_value1(uint16_t stage, uint16_t msc, uint16_t lsc, gboolean qutr_mode, gboolean half_mode)
{
    assert(stage <= TriggerStages);
    assert(lsc <= msc);
    assert(msc < MaxTriggerProbes);

    uint16_t value = 0;
    const uint16_t qutr_mask = (0xffff >> (TriggerProbes - TriggerProbes/4));
    const uint16_t half_mask = (0xffff >> (TriggerProbes - TriggerProbes/2));
    int i;

    if (trigger == NULL){
        sr_err("ds_trigger_get_value1() error, trigger have'nt be inited.");
        return SR_ERR_CALL_STATUS;
    }

    for (i = msc; i >= lsc ; i--) {
        value = (value << 1);
        value += ((trigger->trigger1[stage][i] == '1') | (trigger->trigger1[stage][i] == 'R'));
    }

    if (qutr_mode)
        value = ((value & qutr_mask) << (TriggerProbes/4*3)) +
                ((value & qutr_mask) << (TriggerProbes/4*2)) +
                ((value & qutr_mask) << (TriggerProbes/4*1)) +
                ((value & qutr_mask) << (TriggerProbes/4*0));
    else if (half_mode)
        value = ((value & half_mask) << (TriggerProbes/2*1)) +
                ((value & half_mask) << (TriggerProbes/2*0));

    return value;
}
SR_PRIV uint16_t ds_trigger_get_edge0(uint16_t stage, uint16_t msc, uint16_t lsc, gboolean qutr_mode, gboolean half_mode)
{
    assert(stage <= TriggerStages);
    assert(lsc <= msc);
    assert(msc < MaxTriggerProbes);

    uint16_t edge = 0;
    const uint16_t qutr_mask = (0xffff >> (TriggerProbes - TriggerProbes/4));
    const uint16_t half_mask = (0xffff >> (TriggerProbes - TriggerProbes/2));
    int i;

    if (trigger == NULL){
        sr_err("ds_trigger_get_edge0() error, trigger have'nt be inited.");
        return SR_ERR_CALL_STATUS;
    }

    for (i = msc; i >= lsc ; i--) {
        edge = (edge << 1);
        edge += ((trigger->trigger0[stage][i] == 'R') | (trigger->trigger0[stage][i] == 'F') |
                 (trigger->trigger0[stage][i] == 'C'));
    }

    if (qutr_mode)
        edge = ((edge & qutr_mask) << (TriggerProbes/4*3)) +
               ((edge & qutr_mask) << (TriggerProbes/4*2)) +
               ((edge & qutr_mask) << (TriggerProbes/4*1)) +
               ((edge & qutr_mask) << (TriggerProbes/4*0));
    else if (half_mode)
        edge = ((edge & half_mask) << (TriggerProbes/2*1)) +
               ((edge & half_mask) << (TriggerProbes/2*0));

    return edge;
}
SR_PRIV uint16_t ds_trigger_get_edge1(uint16_t stage, uint16_t msc, uint16_t lsc, gboolean qutr_mode, gboolean half_mode)
{
    assert(stage <= TriggerStages);
    assert(lsc <= msc);
    assert(msc < MaxTriggerProbes);

    uint16_t edge = 0;
    const uint16_t qutr_mask = (0xffff >> (TriggerProbes - TriggerProbes/4));
    const uint16_t half_mask = (0xffff >> (TriggerProbes - TriggerProbes/2));
    int i;

    if (trigger == NULL){
        sr_err("ds_trigger_get_edge1() error, trigger have'nt be inited.");
        return SR_ERR_CALL_STATUS;
    }

    for (i = msc; i >= lsc ; i--) {
        edge = (edge << 1);
        edge += ((trigger->trigger1[stage][i] == 'R') | (trigger->trigger1[stage][i] == 'F') |
                 (trigger->trigger1[stage][i] == 'C'));
    }

    if (qutr_mode)
        edge = ((edge & qutr_mask) << (TriggerProbes/4*3)) +
               ((edge & qutr_mask) << (TriggerProbes/4*2)) +
               ((edge & qutr_mask) << (TriggerProbes/4*1)) +
               ((edge & qutr_mask) << (TriggerProbes/4*0));
    else if (half_mode)
        edge = ((edge & half_mask) << (TriggerProbes/2*1)) +
               ((edge & half_mask) << (TriggerProbes/2*0));

    return edge;
}

/** @} */
