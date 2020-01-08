/*
 * This file is part of the DSLogic project.
 */

#include "libsigrok.h"
#include "libsigrok-internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <glib.h>

/* Message logging helpers with subsystem-specific prefix string. */
#define LOG_PREFIX "session: "
#define sr_log(l, s, args...) sr_log(l, LOG_PREFIX s, ## args)
#define sr_spew(s, args...) sr_spew(LOG_PREFIX s, ## args)
#define sr_dbg(s, args...) sr_dbg(LOG_PREFIX s, ## args)
#define sr_info(s, args...) sr_info(LOG_PREFIX s, ## args)
#define sr_warn(s, args...) sr_warn(LOG_PREFIX s, ## args)
#define sr_err(s, args...) sr_err(LOG_PREFIX s, ## args)

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
SR_API int ds_trigger_init(void)
{
    int i, j;

    if (!trigger) {
        if (!(trigger = g_try_malloc0(sizeof(struct ds_trigger)))) {
            sr_err("Trigger malloc failed.");
            return SR_ERR_MALLOC;
        }
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

SR_API int ds_trigger_destroy(void)
{
    if (trigger)
        g_free(trigger);
    trigger = NULL;
    return SR_OK;
}

/**
 *
 *
 */

SR_API struct ds_trigger *ds_trigger_get(void)
{
    return trigger;
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

    int j;

    for (j = 0; j< probes; j++) {
        trigger->trigger0[stage][probes - j - 1] = *(trigger0 + j * 2);
        trigger->trigger1[stage][probes - j - 1] = *(trigger1 + j * 2);
    }

    return SR_OK;
}
SR_API int ds_trigger_stage_set_logic(uint16_t stage, uint16_t probes, unsigned char trigger_logic)
{
    assert(stage < TriggerStages);
    assert(probes <= MaxTriggerProbes);

    trigger->trigger_logic[stage] = trigger_logic;

    return SR_OK;
}
SR_API int ds_trigger_stage_set_inv(uint16_t stage, uint16_t probes, unsigned char trigger0_inv, unsigned char trigger1_inv)
{
    assert(stage < TriggerStages);
    assert(probes <= MaxTriggerProbes);

    trigger->trigger0_inv[stage] = trigger0_inv;
    trigger->trigger1_inv[stage] = trigger1_inv;

    return SR_OK;
}
SR_API int ds_trigger_stage_set_count(uint16_t stage, uint16_t probes, uint32_t trigger0_count, uint32_t trigger1_count)
{
    assert(stage < TriggerStages);
    assert(probes <= MaxTriggerProbes);

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
    return trigger->trigger_pos;
}

/**
 * set trigger en
 *
 * @return SR_OK upon success.
 */
SR_API int ds_trigger_set_en(uint16_t enable)
{

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
