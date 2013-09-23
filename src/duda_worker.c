/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*  Duda I/O
 *  --------
 *  Copyright (C) 2012-2013, Eduardo Silva P. <edsiper@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <pthread.h>

#include "duda.h"
#include "duda_api.h"
#include "duda_worker.h"

/* --- Local functions --- */

/*
 * This function is a middle step before to invoke the target function, the
 * goal is to be able to initialize some data in the worker before to release
 * the control of it. Originally created to create Logger instances per customs
 * workers that are not related to the main server workers.
 */
static void *duda_worker_step(void *arg)
{
    void *ret;
    struct duda_worker *wk = (struct duda_worker *) arg;

    /* initialize same data as done for server workers */
    _mkp_core_thctx();

    /* call the target function */
    ret = wk->func(wk->arg);
    mk_warn("User defined worker thread #%lu has ended",
            syscall(__NR_gettid));

    /* We do not handle threads exit, just put the thread to sleep */
    while (1) {
        sleep(60);
    }

    return ret;
}

/* Spawn each registered worker */
int duda_worker_spawn_all(struct mk_list *list)
{
    pthread_t tid;
    pthread_attr_t thread_attr;
    struct mk_list *head;
    struct duda_worker *wk;

    pthread_attr_init(&thread_attr);
    pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);

    /* spawn each worker */
    mk_list_foreach(head, list) {
        wk = mk_list_entry(head, struct duda_worker, _head);

        if (pthread_create(&tid, &thread_attr, duda_worker_step, (void *) wk) < 0) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }
    return tid;
}


/* --- API --- */

/*
 * @OBJ_NAME: worker
 * @OBJ_MENU: Workers
 * @OBJ_DESC: This object export different methods to spawn and manage
 * workers (posix threads).
 */

struct duda_api_worker *duda_worker_object()
{
    struct duda_api_worker *wk;

    wk = mk_api->mem_alloc(sizeof(struct duda_api_worker));
    wk->_spawn = duda_worker_spawn;
    wk->gettid = duda_worker_gettid;

    return wk;
}


/*
 * @METHOD_NAME: spawn
 * @METHOD_DESC: It spawn a new worker thread. This method must be invoked from
 * duda_main().
 * @METHOD_PROTO: int spawn(void *(start_routine) (void *), void *arg)
 * @METHOD_PARAM: start_routine the routine or function that will be triggered under a thread context.
 * @METHOD_PARAM: arg a reference to the argument that will be passed to the function
 * @METHOD_RETURN: Upon successful completion it returns the worker id, on error
 * returns -1.
 */
int duda_worker_spawn(void *(start_routine) (void *), void *arg, struct mk_list *list)
{
    struct duda_worker *wk;

    /* alloc node */
    wk = mk_api->mem_alloc(sizeof(struct duda_worker));
    wk->id   = 0;
    wk->func = start_routine;
    wk->arg  = arg;

    /* link to main list */
    mk_list_add(&wk->_head, list);

    return 0;
}

/*
 * @METHOD_NAME: gettid
 * @METHOD_DESC: It returns the thread ID assigned by the operating system. In a
 * single-threaded process, the thread ID is equal to the process ID (PID). In a
 * multi-threaded process, all threads have the same PID, but each one has a unique TID.
 * When running a Duda I/O service you can try a <code>ps -LA</code> from the command line, compare
 * the PID and LWP columns output.
 * @METHOD_PROTO: pid_t gettid()
 * @METHOD_RETURN: Upon successful completion, it returns the thread ID of the calling
 * process.
 */
pid_t duda_worker_gettid()
{
    /*
     * There is no wrapper for this system call by glibc, we need to perform a direct
     * syscall(2) to obtain the value.
     */
    return syscall(__NR_gettid);
}
