/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2010-2012 Bert Vermeulen <bert@biot.com>
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

#include "libsigrok-internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <glib.h>
#include "log.h"
#include <assert.h>

/* Message logging helpers with subsystem-specific prefix string. */

#undef LOG_PREFIX
#define LOG_PREFIX "session: "

/* There can only be one session at a time. */
/* 'session' is not static, it's used elsewhere (via 'extern'). */
static struct sr_session *session = NULL;
 
/**
 * @file
 *
 * Creating, using, or destroying libsigrok sessions.
 */

/**
 * @defgroup grp_session Session handling
 *
 * Creating, using, or destroying libsigrok sessions.
 *
 * @{
 */

struct source {
	int timeout;
	sr_receive_data_callback_t cb;
    const void *cb_data;

	/* This is used to keep track of the object (fd, pollfd or channel) which is
	 * being polled and will be used to match the source when removing it again.
	 */
	gintptr poll_object;
};

/**
 * Create a new session.
 *
 * @todo Should it use the file-global "session" variable or take an argument?
 *       The same question applies to all the other session functions.
 *
 * @return A pointer to the newly allocated session, or NULL upon errors.
 */
SR_PRIV struct sr_session *sr_session_new(void)
{
	if (session != NULL){
		sr_detail("Destroy the old session.");
		sr_session_destroy(); // Destory the old.
	}

	session = malloc(sizeof(struct sr_session));
	if (session == NULL) {
		sr_err("%s,ERROR:failed to alloc memory.", __func__);
		return NULL;
	}
	memset(session, 0, sizeof(struct sr_session));

	session->source_timeout = -1;
    session->running = FALSE;
	session->abort_session = FALSE;
    g_mutex_init(&session->stop_mutex);

	return session;
}

/**
 * Destroy the current session.
 *
 * This frees up all memory used by the session.
 *
 * @return SR_OK upon success, SR_ERR_BUG if no session exists.
 */
SR_PRIV int sr_session_destroy(void)
{
	if (session == NULL) {
		//sr_detail("%s: session was NULL", __func__);
		return SR_ERR_BUG;
	} 

    if (session->sources) {
        g_free(session->sources);
        session->sources = NULL;
    }

    if (session->pollfds) {
        g_free(session->pollfds);
        session->pollfds = NULL;
    }

	/* TODO: Error checks needed? */

    g_mutex_clear(&session->stop_mutex);

	g_free(session);
	session = NULL;

	return SR_OK;
}


/**
 * Call every device in the session's callback.
 *
 * For sessions not driven by select loops such as sr_session_run(),
 * but driven by another scheduler, this can be used to poll the devices
 * from within that scheduler.
 *
 * @param block If TRUE, this call will wait for any of the session's
 *              sources to fire an event on the file descriptors, or
 *              any of their timeouts to activate. In other words, this
 *              can be used as a select loop.
 *              If FALSE, all sources have their callback run, regardless
 *              of file descriptor or timeout status.
 *
 * @return SR_OK upon success, SR_ERR on errors.
 */
static int sr_session_iteration(gboolean block)
{
	unsigned int i;
	int ret;

	if (session == NULL){
		sr_err("sr_session_iteration(), session is null.");
		return SR_ERR_CALL_STATUS;
	}

	ret = g_poll(session->pollfds, session->num_sources,
			block ? session->source_timeout : 0);
	for (i = 0; i < session->num_sources; i++) {
		if (session->pollfds[i].revents > 0 || (ret == 0
			&& session->source_timeout == session->sources[i].timeout)) {
			/*
			 * Invoke the source's callback on an event,
			 * or if the poll timed out and this source
			 * asked for that timeout.
			 */
			if (!session->sources[i].cb(session->pollfds[i].fd,
					session->pollfds[i].revents,
					session->sources[i].cb_data))
				sr_session_source_remove(session->sources[i].poll_object);
		}
		/*
		 * We want to take as little time as possible to stop
		 * the session if we have been told to do so. Therefore,
		 * we check the flag after processing every source, not
		 * just once per main event loop.
		 */
        g_mutex_lock(&session->stop_mutex);
		if (session->abort_session) {
			sr_info("Collection task aborted.");
			current_device_acquisition_stop();
			/* But once is enough. */
			session->abort_session = FALSE;
		}
        g_mutex_unlock(&session->stop_mutex);
	}

	return SR_OK;
}

/**
 * Run the session.
 *
 * @return SR_OK upon success, SR_ERR_BUG upon errors.
 */
SR_PRIV int sr_session_run(void)
{
	if (session == NULL) {
		sr_err("%s: session was NULL; a session must be "
		       "created first, before running it.", __func__);
		return SR_ERR_BUG;
	}
	if (session->running == TRUE){
		sr_err("Session is running.");
		return SR_ERR_CALL_STATUS;
	}
 
    session->running = TRUE;

	sr_dbg("Running...");

	/* Do we have real sources? */
	if (session->num_sources == 1 && session->pollfds[0].fd == -1) {
		/* Dummy source, freewheel over it. */
        while (session->num_sources) {
            if (session->abort_session) {
                session->sources[0].cb(-1, -1, session->sources[0].cb_data);
                break;
            } else {
                session->sources[0].cb(-1, 0, session->sources[0].cb_data);
            }
        }
	} else {
		/* Real sources, use g_poll() main loop. */
        while (session->num_sources){
			sr_session_iteration(TRUE);
		}            
	}

    g_mutex_lock(&session->stop_mutex);
    current_device_acquisition_stop();
    session->abort_session = FALSE;
    session->running = FALSE;
    g_mutex_unlock(&session->stop_mutex);
	return SR_OK;
}

/**
 * Stop the current session.
 *
 * The current session is stopped immediately, with all acquisition sessions
 * being stopped and hardware drivers cleaned up.
 *
 * If the session is run in a separate thread, this function will not block
 * until the session is finished executing. It is the caller's responsibility
 * to wait for the session thread to return before assuming that the session is
 * completely decommissioned.
 *
 * @return SR_OK upon success, SR_ERR_BUG if no session exists.
 */
SR_PRIV int sr_session_stop(void)
{
	if (!session) {
		sr_err("%s: session was NULL", __func__);
		return SR_ERR_BUG;
	}

    g_mutex_lock(&session->stop_mutex);
    if (session->running)
        session->abort_session = TRUE;  
    g_mutex_unlock(&session->stop_mutex);

	return SR_OK;
}

/**
 * Add an event source for a file descriptor.
 *
 * @param pollfd The GPollFD.
 * @param timeout Max time to wait before the callback is called, ignored if 0.
 * @param cb Callback function to add. Must not be NULL.
 * @param cb_data Data for the callback function. Can be NULL.
 * @param poll_object Object responsible for the event source.
 *
 * @return SR_OK upon success, SR_ERR_ARG upon invalid arguments, or
 *         SR_ERR_MALLOC upon memory allocation errors.
 */
static int sr_session_source_add_internal(GPollFD *pollfd, int timeout,
                                          sr_receive_data_callback_t cb, const struct sr_dev_inst *sdi, gintptr poll_object)
{
	struct source *new_sources, *s;
	GPollFD *new_pollfds;

	if (!cb) {
		sr_err("%s: cb was NULL", __func__);
		return SR_ERR_ARG;
	}
	if (session == NULL){
		sr_err("sr_session_source_add_internal(), session is null.");
		return SR_ERR_CALL_STATUS;
	}

	/* Note: cb_data can be NULL, that's not a bug. */

	new_pollfds = g_try_realloc(session->pollfds,
			sizeof(GPollFD) * (session->num_sources + 1));
	if (!new_pollfds) {
		sr_err("%s: new_pollfds malloc failed", __func__);
		return SR_ERR_MALLOC;
	}

	new_sources = g_try_realloc(session->sources, sizeof(struct source) *
			(session->num_sources + 1));
	if (!new_sources) {
		sr_err("%s: new_sources malloc failed", __func__);
		return SR_ERR_MALLOC;
	}

	new_pollfds[session->num_sources] = *pollfd;
	s = &new_sources[session->num_sources++];
	s->timeout = timeout;
	s->cb = cb;
	s->cb_data = sdi;
	s->poll_object = poll_object;
	session->pollfds = new_pollfds;
	session->sources = new_sources;

	if (timeout != session->source_timeout && timeout > 0
	    && (session->source_timeout == -1 || timeout < session->source_timeout))
		session->source_timeout = timeout;

	return SR_OK;
}

/**
 * Add an event source for a file descriptor.
 *
 * @param poll_object Pointer to the polled object.
 * @param events Events to check for.
 * @param timeout Max time to wait before the callback is called, ignored if 0.
 * @param cb Callback function to add. Must not be NULL.
 * @param cb_data Data for the callback function. Can be NULL.
 *
 * @return SR_OK upon success, SR_ERR_ARG upon invalid arguments, or
 *         SR_ERR_MALLOC upon memory allocation errors.
 */
SR_PRIV int sr_session_source_add(gintptr poll_object, int events,
                                  int timeout,
                                  sr_receive_data_callback_t cb,
                                  const struct sr_dev_inst *sdi)
{
	GPollFD p;

#ifdef WIN32
    if(poll_object > 0)
    {
        /* poll_object points to a valid GIOChannel */
        g_io_channel_win32_make_pollfd((GIOChannel *) poll_object,events,&p);
    }
    else
    {
        /* poll_object is not a valid descriptor */
        p.fd = (gint64) poll_object;
        p.events = events;
    }
#else
    p.fd = (gint64) poll_object;
	p.events = events;
#endif

    return sr_session_source_add_internal(&p, timeout, cb, sdi,
                                          poll_object);
}

/**
 * Remove the source belonging to the specified channel.
 *
 * @todo Add more error checks and logging.
 *
 * @param poll_object The object for which the source should be removed.
 *
 * @return SR_OK upon success, SR_ERR_ARG upon invalid arguments, or
 *         SR_ERR_MALLOC upon memory allocation errors, SR_ERR_BUG upon
 *         internal errors.
 */
static int sr_session_source_remove_internal(gintptr poll_object)
{
	struct source *new_sources;
	GPollFD *new_pollfds;
	unsigned int old;

	if (session == NULL){
		sr_err("sr_session_source_remove_internal(), session is null.");
		return SR_ERR_CALL_STATUS;
	}

	if (!session->sources || !session->num_sources) {
		sr_err("%s: sources was NULL", __func__);
		return SR_ERR_BUG;
	}

	for (old = 0; old < session->num_sources; old++) {
		if (session->sources[old].poll_object == poll_object)
			break;
	}

	/* fd not found, nothing to do */
	if (old == session->num_sources)
		return SR_OK;

    session->num_sources -= 1;

    if (session->num_sources == 0) {
        session->source_timeout = -1;
        g_free(session->pollfds);
        g_free(session->sources);
        session->pollfds = NULL;
        session->sources = NULL;
    } 
	else {
        if (old != session->num_sources) {
            memmove(&session->pollfds[old], &session->pollfds[old+1],
                (session->num_sources - old) * sizeof(GPollFD));
            memmove(&session->sources[old], &session->sources[old+1],
                (session->num_sources - old) * sizeof(struct source));
        }

        new_pollfds = g_try_realloc(session->pollfds, sizeof(GPollFD) * session->num_sources);
        if (!new_pollfds && session->num_sources > 0) {
            sr_err("%s: new_pollfds malloc failed", __func__);
            return SR_ERR_MALLOC;
        }

        new_sources = g_try_realloc(session->sources, sizeof(struct source) * session->num_sources);
        if (!new_sources && session->num_sources > 0) {
            sr_err("%s: new_sources malloc failed", __func__);
            return SR_ERR_MALLOC;
        }

        session->pollfds = new_pollfds;
        session->sources = new_sources;
    }

	return SR_OK;
}

/**
 * Remove the source belonging to the specified file descriptor.
 *
 * @param poll_object The object for which the source should be removed.
 *
 * @return SR_OK upon success, SR_ERR_ARG upon invalid arguments, or
 *         SR_ERR_MALLOC upon memory allocation errors, SR_ERR_BUG upon
 *         internal errors.
 */
SR_PRIV int sr_session_source_remove(gintptr poll_object)
{
    return sr_session_source_remove_internal(poll_object);
}

/** @} */
