/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2013 Bert Vermeulen <bert@biot.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBSIGROK_SIGROK_PROTO_H
#define LIBSIGROK_SIGROK_PROTO_H

/**
 * @file
 *
 * Header file containing API function prototypes.
 */

/*--- backend.c -------------------------------------------------------------*/

SR_API int sr_init(struct sr_context **ctx);
SR_API int sr_exit(struct sr_context *ctx);

/*--- log.c -----------------------------------------------------------------*/

typedef int (*sr_log_callback_t)(void *cb_data, int loglevel,
				 const char *format, va_list args);

SR_API int sr_log_loglevel_set(int loglevel);
SR_API int sr_log_loglevel_get(void);
SR_API int sr_log_callback_set(sr_log_callback_t cb, void *cb_data);
SR_API int sr_log_callback_set_default(void);
SR_API int sr_log_logdomain_set(const char *logdomain);
SR_API char *sr_log_logdomain_get(void);

/*--- device.c --------------------------------------------------------------*/

SR_API int sr_dev_probe_name_set(const struct sr_dev_inst *sdi,
		int probenum, const char *name);
SR_API int sr_dev_probe_enable(const struct sr_dev_inst *sdi, int probenum,
		gboolean state);
SR_API int sr_dev_trigger_set(const struct sr_dev_inst *sdi, uint16_t probenum,
		const char *trigger);
SR_API GSList *sr_dev_list(const struct sr_dev_driver *driver);
SR_API const GSList *sr_dev_mode_list(const struct sr_dev_inst *sdi);
SR_API int sr_dev_clear(const struct sr_dev_driver *driver);
SR_API int sr_dev_open(struct sr_dev_inst *sdi);
SR_API int sr_dev_close(struct sr_dev_inst *sdi);

/*--- hwdriver.c ------------------------------------------------------------*/

SR_API struct sr_dev_driver **sr_driver_list(void);
SR_API int sr_driver_init(struct sr_context *ctx,
		struct sr_dev_driver *driver);
SR_API GSList *sr_driver_scan(struct sr_dev_driver *driver, GSList *options);
SR_API int sr_config_get(const struct sr_dev_driver *driver,
                         const struct sr_dev_inst *sdi,
                         const struct sr_channel *ch,
                         const struct sr_channel_group *cg,
                         int key, GVariant **data);
SR_API int sr_config_set(struct sr_dev_inst *sdi,
                         struct sr_channel *ch,
                         struct sr_channel_group *cg,
                         int key, GVariant *data);
SR_API int sr_config_list(const struct sr_dev_driver *driver,
                          const struct sr_dev_inst *sdi,
                          const struct sr_channel_group *cg,
                          int key, GVariant **data);
SR_API const struct sr_config_info *sr_config_info_get(int key);
SR_API const struct sr_config_info *sr_config_info_name_get(const char *optname);
SR_API int sr_status_get(const struct sr_dev_inst *sdi, struct sr_status *status, gboolean prg);
SR_API struct sr_config *sr_config_new(int key, GVariant *data);
SR_API void sr_config_free(struct sr_config *src);

/*--- session.c -------------------------------------------------------------*/

typedef void (*sr_datafeed_callback_t)(const struct sr_dev_inst *sdi,
		const struct sr_datafeed_packet *packet, void *cb_data);

/* Session setup */
SR_API int sr_session_load(const char *filename);
SR_API struct sr_session *sr_session_new(void);
SR_API int sr_session_destroy(void);
SR_API int sr_session_dev_remove_all(void);
SR_API int sr_session_dev_add(struct sr_dev_inst *sdi);
SR_API int sr_session_dev_list(GSList **devlist);

/* Datafeed setup */
SR_API int sr_session_datafeed_callback_remove_all(void);
SR_API int sr_session_datafeed_callback_add(sr_datafeed_callback_t cb,
		void *cb_data);

/* Session control */
SR_API int sr_session_start(void);
SR_API int sr_session_run(void);
SR_API int sr_session_stop(void);
SR_API int sr_session_save_init(const char *filename, const char *metafile, const char *decfile, const char *sesfile);
SR_API int sr_session_append(const char *filename, const unsigned char *buf,
        uint64_t size, int chunk_num, int index, int type, int version);
SR_API int sr_session_source_add(int fd, int events, int timeout,
		sr_receive_data_callback_t cb, const struct sr_dev_inst *sdi);
SR_API int sr_session_source_add_pollfd(GPollFD *pollfd, int timeout,
		sr_receive_data_callback_t cb, const struct sr_dev_inst *sdi);
SR_API int sr_session_source_add_channel(GIOChannel *channel, int events,
		int timeout, sr_receive_data_callback_t cb, const struct sr_dev_inst *sdi);
SR_API int sr_session_source_remove(int fd);
SR_API int sr_session_source_remove_pollfd(GPollFD *pollfd);
SR_API int sr_session_source_remove_channel(GIOChannel *channel);

/*--- input/input.c ---------------------------------------------------------*/

SR_API struct sr_input_format **sr_input_list(void);

/*--- output/output.c -------------------------------------------------------*/

SR_API const struct sr_output_module **sr_output_list(void);

/*--- strutil.c -------------------------------------------------------------*/

SR_API char *sr_si_string_u64(uint64_t x, const char *unit);
SR_API char *sr_iec_string_u64(uint64_t x, const char *unit);
SR_API char *sr_samplerate_string(uint64_t samplerate);
SR_API char *sr_samplecount_string(uint64_t samplecount);
SR_API char *sr_period_string(uint64_t frequency);
SR_API char *sr_time_string(uint64_t time);
SR_API char *sr_voltage_string(uint64_t v_p, uint64_t v_q);
SR_API int sr_parse_sizestring(const char *sizestring, uint64_t *size);
SR_API uint64_t sr_parse_timestring(const char *timestring);
SR_API gboolean sr_parse_boolstring(const char *boolstring);
SR_API int sr_parse_period(const char *periodstr, uint64_t *p, uint64_t *q);
SR_API int sr_parse_voltage(const char *voltstr, uint64_t *p, uint64_t *q);

/*--- version.c -------------------------------------------------------------*/

SR_API int sr_package_version_major_get(void);
SR_API int sr_package_version_minor_get(void);
SR_API int sr_package_version_micro_get(void);
SR_API const char *sr_package_version_string_get(void);

SR_API int sr_lib_version_current_get(void);
SR_API int sr_lib_version_revision_get(void);
SR_API int sr_lib_version_age_get(void);
SR_API const char *sr_lib_version_string_get(void);

/*--- error.c ---------------------------------------------------------------*/

SR_API const char *sr_strerror(int error_code);
SR_API const char *sr_strerror_name(int error_code);

/*--- trigger.c ------------------------------------------------------------*/
SR_API int ds_trigger_init(void);
SR_API int ds_trigger_destroy(void);
SR_API struct ds_trigger *ds_trigger_get(void);
SR_API int ds_trigger_stage_set_value(uint16_t stage, uint16_t probes, char *trigger0, char *trigger1);
SR_API int ds_trigger_stage_set_logic(uint16_t stage, uint16_t probes, unsigned char trigger_logic);
SR_API int ds_trigger_stage_set_inv(uint16_t stage, uint16_t probes, unsigned char trigger0_inv, unsigned char trigger1_inv);
SR_API int ds_trigger_stage_set_count(uint16_t stage, uint16_t probes, uint32_t trigger0_count, uint32_t trigger1_count);
SR_API int ds_trigger_probe_set(uint16_t probe, unsigned char trigger0, unsigned char trigger1);
SR_API int ds_trigger_set_stage(uint16_t stages);
SR_API int ds_trigger_set_pos(uint16_t position);
SR_API uint16_t ds_trigger_get_pos();
SR_API int ds_trigger_set_en(uint16_t enable);
SR_API uint16_t ds_trigger_get_en();
SR_API int ds_trigger_set_mode(uint16_t mode);

SR_PRIV uint16_t ds_trigger_get_mask0(uint16_t stage, uint16_t msc, uint16_t lsc, gboolean qutr_mode, gboolean half_mode);
SR_PRIV uint16_t ds_trigger_get_value0(uint16_t stage, uint16_t msc, uint16_t lsc, gboolean qutr_mode, gboolean half_mode);
SR_PRIV uint16_t ds_trigger_get_edge0(uint16_t stage, uint16_t msc, uint16_t lsc, gboolean qutr_mode, gboolean half_mode);
SR_PRIV uint16_t ds_trigger_get_mask1(uint16_t stage, uint16_t msc, uint16_t lsc, gboolean qutr_mode, gboolean half_mode);
SR_PRIV uint16_t ds_trigger_get_value1(uint16_t stage, uint16_t msc, uint16_t lsc, gboolean qutr_mode, gboolean half_mode);
SR_PRIV uint16_t ds_trigger_get_edge1(uint16_t stage, uint16_t msc, uint16_t lsc, gboolean qutr_mode, gboolean half_mode);
#endif
