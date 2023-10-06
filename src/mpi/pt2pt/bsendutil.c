/*
 * Copyright (C) by Argonne National Laboratory
 *     See COPYRIGHT in top-level directory
 */

#include "mpiimpl.h"

/*
 * Miscellaneous comments
 * By storing total_size along with "size available for messages", we
 * avoid any complexities associated with alignment, since we must
 * ensure that each KPIR_Bsend_data_t structure is properly aligned
 * (i.e., we can't simply do (sizeof(MPII_Bsend_data_t) + size) to get
 * total_size).
 *
 * Function Summary
 *   MPIR_Bsend_attach - Performs the work of MPI_Buffer_attach
 *   MPIR_Bsend_detach - Performs the work of MPI_Buffer_detach
 *   MPIR_Bsend_isend  - Essentially performs an MPI_Ibsend.  Returns
 *                an MPIR_Request that is also stored internally in the
 *                corresponding MPII_Bsend_data_t entry
 *   MPIR_Bsend_free_segment - Free a buffer that is no longer needed,
 *                merging with adjacent segments
 *   MPIR_Bsend_check_active - Check for completion of any active sends
 *                for bsends (all bsends, both MPI_Ibsend and MPI_Bsend,
 *                are internally converted into Isends on the data
 *                in the Bsend buffer)
 *   MPIR_Bsend_find_buffer - Find a buffer in the bsend buffer large
 *                enough for the message.  However, does not acquire that
 *                buffer (see MPIR_Bsend_take_buffer)
 *   MPIR_Bsend_take_buffer - Find and acquire a buffer for a message
 *   MPIR_Bsend_finalize - Finalize bsendbuffer
 *   MPIR_Bsend_dump - Debugging routine to print the contents of the control
 *                information in the bsend buffer (the MPII_Bsend_data_t entries)
 */

#ifdef MPL_USE_DBG_LOGGING
static void MPIR_Bsend_dump(MPII_BsendBuffer * bsendbuffer);
#endif

#define BSENDDATA_HEADER_TRUE_SIZE (sizeof(MPII_Bsend_data_t) - sizeof(double))

/* Forward references */
static int MPIR_Bsend_check_active(MPII_BsendBuffer * bsendbuffer);
static MPII_Bsend_data_t *MPIR_Bsend_find_buffer(MPII_BsendBuffer * bsendbuffer, size_t size);
static void MPIR_Bsend_take_buffer(MPII_BsendBuffer * bsendbuffer,
                                   MPII_Bsend_data_t * p, size_t size);
static void MPIR_Bsend_free_segment(MPII_BsendBuffer * bsendbuffer, MPII_Bsend_data_t * p);

/*
 * Attach a buffer.  This checks for the error conditions and then
 * initialized the avail buffer.
 */
static int MPIR_Bsend_attach(MPII_BsendBuffer ** bsendbuffer_p, void *buffer, MPI_Aint buffer_size)
{
    MPII_Bsend_data_t *p;
    size_t offset, align_sz;
    MPII_BsendBuffer *bsendbuffer;

    if (*bsendbuffer_p == NULL) {
        *bsendbuffer_p = MPL_calloc(1, sizeof(MPII_BsendBuffer), MPL_MEM_OTHER);
        MPIR_Assert(*bsendbuffer_p);
    }
    bsendbuffer = *bsendbuffer_p;

#ifdef HAVE_ERROR_CHECKING
    {
        MPID_BEGIN_ERROR_CHECKS;
        {
            if (bsendbuffer->buffer) {
                return MPIR_Err_create_code(MPI_SUCCESS, MPIR_ERR_RECOVERABLE,
                                            "MPIR_Bsend_attach", __LINE__, MPI_ERR_BUFFER,
                                            "**bufexists", 0);
            }
            if (buffer_size < MPI_BSEND_OVERHEAD) {
                /* MPI_ERR_OTHER is another valid choice for this error,
                 * but the Intel test wants MPI_ERR_BUFFER, and it seems
                 * to violate the principle of least surprise to not use
                 * MPI_ERR_BUFFER for errors with the Buffer */
                return MPIR_Err_create_code(MPI_SUCCESS, MPIR_ERR_RECOVERABLE,
                                            "MPIR_Bsend_attach", __LINE__, MPI_ERR_BUFFER,
                                            "**bsendbufsmall",
                                            "**bsendbufsmall %d %d", buffer_size,
                                            MPI_BSEND_OVERHEAD);
            }
        }
        MPID_END_ERROR_CHECKS;
    }
#endif /* HAVE_ERROR_CHECKING */

    MPID_THREAD_CS_ENTER(VCI, MPIR_THREAD_VCI_BSEND_MUTEX);

    bsendbuffer->origbuffer = buffer;
    bsendbuffer->origbuffer_size = buffer_size;
    bsendbuffer->buffer = buffer;
    bsendbuffer->buffer_size = buffer_size;

    /* Make sure that the buffer that we use is aligned to align_sz.  Some other
     * code assumes pointer-alignment, and some code assumes double alignment.
     * Further, GCC 4.5.1 generates bad code on 32-bit platforms when this is
     * only 4-byte aligned (see #1149). */
    align_sz = MPL_MAX(sizeof(void *), sizeof(double));
    offset = ((size_t) buffer) % align_sz;
    if (offset) {
        offset = align_sz - offset;
        buffer = (char *) buffer + offset;
        bsendbuffer->buffer = buffer;
        bsendbuffer->buffer_size -= offset;
    }
    bsendbuffer->avail = buffer;
    bsendbuffer->active = 0;

    /* Set the first block */
    p = (MPII_Bsend_data_t *) buffer;
    p->size = buffer_size - BSENDDATA_HEADER_TRUE_SIZE;
    p->total_size = buffer_size;
    p->next = p->prev = NULL;
    p->msg.msgbuf = (char *) p + BSENDDATA_HEADER_TRUE_SIZE;

    MPID_THREAD_CS_EXIT(VCI, MPIR_THREAD_VCI_BSEND_MUTEX);
    return MPI_SUCCESS;
}

/*
 * Detach a buffer.  This routine must wait until any active bsends
 * are complete.  Note that MPI specifies the type of the returned "size"
 * argument as an "int" (the definition predates that of ssize_t as a
 * standard type).
 */
static int MPIR_Bsend_detach(MPII_BsendBuffer ** bsendbuffer_p, void *bufferp, MPI_Aint * size)
{
    int mpi_errno = MPI_SUCCESS;

    MPID_THREAD_CS_ENTER(VCI, MPIR_THREAD_VCI_BSEND_MUTEX);

    if (*bsendbuffer_p == NULL) {
        *(void **) bufferp = NULL;
        *size = 0;
        goto fn_exit;
    }

    MPII_BsendBuffer *bsendbuffer;
    bsendbuffer = *bsendbuffer_p;

    if (bsendbuffer->active) {
        /* Loop through each active element and wait on it */
        MPII_Bsend_data_t *p = bsendbuffer->active;

        while (p) {
            MPIR_Request *r = p->request;
            mpi_errno = MPID_Wait(r, MPI_STATUS_IGNORE);
            MPIR_ERR_CHECK(mpi_errno);
            MPIR_Request_free(r);
            p = p->next;
        }
    }

    /* Note that this works even when the buffer does not exist */
    *(void **) bufferp = bsendbuffer->origbuffer;
    *size = (MPI_Aint) bsendbuffer->origbuffer_size;
    bsendbuffer->origbuffer = NULL;
    bsendbuffer->origbuffer_size = 0;
    bsendbuffer->buffer = 0;
    bsendbuffer->buffer_size = 0;
    bsendbuffer->avail = 0;
    bsendbuffer->active = 0;

  fn_exit:
    MPID_THREAD_CS_EXIT(VCI, MPIR_THREAD_VCI_BSEND_MUTEX);
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}

/*
 * Initiate an ibsend.  We'll used this for Bsend as well.
 */
int MPIR_Bsend_isend(const void *buf, int count, MPI_Datatype dtype,
                     int dest, int tag, MPIR_Comm * comm_ptr, MPIR_Request ** request)
{
    int mpi_errno = MPI_SUCCESS;
    MPII_Bsend_data_t *p = NULL;
    MPII_Bsend_msg_t *msg;
    int pass;

    MPID_THREAD_CS_ENTER(VCI, MPIR_THREAD_VCI_BSEND_MUTEX);

    MPI_Aint packsize = 0;
    if (dtype != MPI_PACKED)
        MPIR_Pack_size(count, dtype, &packsize);
    else
        packsize = count;

    MPII_BsendBuffer *bsendbuffer;
    if (comm_ptr->bsendbuffer) {
        bsendbuffer = comm_ptr->bsendbuffer;
    } else if (comm_ptr->session_ptr && comm_ptr->session_ptr->bsendbuffer) {
        bsendbuffer = comm_ptr->session_ptr->bsendbuffer;
    } else {
        bsendbuffer = MPIR_Process.bsendbuffer;
    }
    MPIR_ERR_CHKANDJUMP2(!bsendbuffer, mpi_errno, MPI_ERR_BUFFER, "**bufbsend",
                         "**bufbsend %d %d", packsize, 0);

    /*
     * We may want to decide here whether we need to pack at all
     * or if we can just use (a MPIR_Memcpy) of the buffer.
     */


    /* We check the active buffer first.  This helps avoid storage
     * fragmentation */
    mpi_errno = MPIR_Bsend_check_active(bsendbuffer);
    MPIR_ERR_CHECK(mpi_errno);

    MPL_DBG_MSG_D(MPIR_DBG_BSEND, TYPICAL, "looking for buffer of size " MPI_AINT_FMT_DEC_SPEC,
                  packsize);
    /*
     * Use two passes.  Each pass is the same; between the two passes,
     * attempt to complete any active requests.
     *  If the message can be initiated in the first pass,
     * do not perform the second pass.
     */
    for (pass = 0; pass < 2; pass++) {

        p = MPIR_Bsend_find_buffer(bsendbuffer, packsize);
        if (p) {
            MPL_DBG_MSG_FMT(MPIR_DBG_BSEND, TYPICAL, (MPL_DBG_FDEST,
                                                      "found buffer of size " MPI_AINT_FMT_DEC_SPEC
                                                      " with address %p", packsize, p));
            /* Found a segment */

            msg = &p->msg;

            /* Pack the data into the buffer */
            /* We may want to optimize for the special case of
             * either primitive or contiguous types, and just
             * use MPIR_Memcpy and the provided datatype */
            msg->count = 0;
            if (dtype != MPI_PACKED) {
                MPI_Aint actual_pack_bytes;
                void *pbuf = (void *) ((char *) p->msg.msgbuf + p->msg.count);
                mpi_errno =
                    MPIR_Typerep_pack(buf, count, dtype, 0, pbuf, packsize, &actual_pack_bytes,
                                      MPIR_TYPEREP_FLAG_NONE);
                MPIR_ERR_CHECK(mpi_errno);
                p->msg.count += actual_pack_bytes;
            } else {
                MPIR_Memcpy(p->msg.msgbuf, buf, count);
                p->msg.count = count;
            }
            /* Try to send the message.  We must use MPID_Isend
             * because this call must not block */
            mpi_errno = MPID_Isend(msg->msgbuf, msg->count, MPI_PACKED,
                                   dest, tag, comm_ptr, 0, &p->request);
            MPIR_ERR_CHKINTERNAL(mpi_errno, mpi_errno, "Bsend internal error: isend returned err");

            if (p->request) {
                MPL_DBG_MSG_FMT(MPIR_DBG_BSEND, TYPICAL,
                                (MPL_DBG_FDEST, "saving request %p in %p", p->request, p));
                /* An optimization is to check to see if the
                 * data has already been sent.  The original code
                 * to do this was commented out and probably did not match
                 * the current request internals */
                MPIR_Bsend_take_buffer(bsendbuffer, p, p->msg.count);
                if (request) {
                    /* Add 1 ref_count for MPI_Wait/Test */
                    MPIR_Request_add_ref(p->request);
                    *request = p->request;
                }
            }
            break;
        }
        /* If we found a buffer or we're in the second pass, then break.
         * Note that the test on phere is redundant, as the code breaks
         * out of the loop in the test above if a block p is found. */
        if (p || pass == 1)
            break;
        MPL_DBG_MSG(MPIR_DBG_BSEND, TYPICAL, "Could not find storage, checking active");
        /* Try to complete some pending bsends */
        MPIR_Bsend_check_active(bsendbuffer);
    }

    if (!p) {
        /* Return error for no buffer space found */
        /* Generate a traceback of the allocated space, explaining why
         * packsize could not be found */
        MPL_DBG_MSG(MPIR_DBG_BSEND, TYPICAL, "Could not find space; dumping arena");
        MPL_DBG_STMT(MPIR_DBG_BSEND, TYPICAL, MPIR_Bsend_dump(bsendbuffer));
        MPIR_ERR_SETANDJUMP2(mpi_errno, MPI_ERR_BUFFER, "**bufbsend", "**bufbsend %d %d", packsize,
                             bsendbuffer->buffer_size);
    }

  fn_exit:
    MPID_THREAD_CS_EXIT(VCI, MPIR_THREAD_VCI_BSEND_MUTEX);
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}

/*
 * The following routines are used to manage the allocation of bsend segments
 * in the user buffer.  These routines handle, for example, merging segments
 * when an active segment that is adjacent to a free segment becomes free.
 *
 */

/* Add block p to the free list. Merge into adjacent blocks.  Used only
   within the check_active */

static void MPIR_Bsend_free_segment(MPII_BsendBuffer * bsendbuffer, MPII_Bsend_data_t * p)
{
    MPII_Bsend_data_t *prev = p->prev, *avail = bsendbuffer->avail, *avail_prev;

    MPL_DBG_MSG_FMT(MPIR_DBG_BSEND, TYPICAL, (MPL_DBG_FDEST,
                                              "Freeing bsend segment at %p of size %llu, next at %p",
                                              p, (unsigned long long) p->size,
                                              ((char *) p) + p->total_size));

    MPL_DBG_MSG_D(MPIR_DBG_BSEND, TYPICAL,
                  "At the beginning of free_segment with size %llu:",
                  (unsigned long long) p->total_size);
    MPL_DBG_STMT(MPIR_DBG_BSEND, TYPICAL, MPIR_Bsend_dump(bsendbuffer));

    /* Remove the segment from the active list */
    if (prev) {
        MPL_DBG_MSG(MPIR_DBG_BSEND, TYPICAL, "free segment is within active list");
        prev->next = p->next;
    } else {
        /* p was at the head of the active list */
        MPL_DBG_MSG(MPIR_DBG_BSEND, TYPICAL, "free segment is head of active list");
        bsendbuffer->active = p->next;
        /* The next test sets the prev pointer to null */
    }
    if (p->next) {
        p->next->prev = prev;
    }

    MPL_DBG_STMT(MPIR_DBG_BSEND, VERBOSE, MPIR_Bsend_dump(bsendbuffer));

    /* Merge into the avail list */
    /* Find avail_prev, avail, such that p is between them.
     * either may be null if p is at either end of the list */
    avail_prev = 0;
    while (avail) {
        if (avail > p) {
            break;
        }
        avail_prev = avail;
        avail = avail->next;
    }

    /* Try to merge p with the next block */
    if (avail) {
        if ((char *) p + p->total_size == (char *) avail) {
            p->total_size += avail->total_size;
            p->size = p->total_size - BSENDDATA_HEADER_TRUE_SIZE;
            p->next = avail->next;
            if (avail->next)
                avail->next->prev = p;
            avail = 0;
        } else {
            p->next = avail;
            avail->prev = p;
        }
    } else {
        p->next = 0;
    }
    /* Try to merge p with the previous block */
    if (avail_prev) {
        if ((char *) avail_prev + avail_prev->total_size == (char *) p) {
            avail_prev->total_size += p->total_size;
            avail_prev->size = avail_prev->total_size - BSENDDATA_HEADER_TRUE_SIZE;
            avail_prev->next = p->next;
            if (p->next)
                p->next->prev = avail_prev;
        } else {
            avail_prev->next = p;
            p->prev = avail_prev;
        }
    } else {
        /* p is the new head of the list */
        bsendbuffer->avail = p;
        p->prev = 0;
    }

    MPL_DBG_MSG(MPIR_DBG_BSEND, TYPICAL, "At the end of free_segment:");
    MPL_DBG_STMT(MPIR_DBG_BSEND, TYPICAL, MPIR_Bsend_dump(bsendbuffer));
}

/*
 * The following routine tests for completion of active sends and
 * frees the related storage
 *
 * To make it easier to identify the source of the request, we keep
 * track of the type of MPI routine (ibsend, bsend, or bsend_init/start)
 * that created the bsend entry.
 */

/* TODO: make it as a progress_hook. The critical section need be made more granular.
 * Or, does it matter? */
static int MPIR_Bsend_progress(MPII_BsendBuffer * bsendbuffer)
{
    int mpi_errno = MPI_SUCCESS;

    MPII_Bsend_data_t *active = bsendbuffer->active;
    while (active) {
        MPII_Bsend_data_t *next_active = active->next;
        MPIR_Request *req = active->request;
        if (MPIR_Request_is_complete(req)) {
            MPIR_Bsend_free_segment(bsendbuffer, active);
            if (!MPIR_Request_is_persistent(req)) {
                MPIR_Request_free(req);
            }
        }
        active = next_active;
    }

    return mpi_errno;
}

static int MPIR_Bsend_check_active(MPII_BsendBuffer * bsendbuffer)
{
    int mpi_errno = MPI_SUCCESS;

    if (bsendbuffer->active) {
        mpi_errno = MPID_Progress_test(NULL);
        MPIR_ERR_CHECK(mpi_errno);
        MPIR_Bsend_progress(bsendbuffer);
    }

  fn_exit:
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}

/*
 * Find a slot in the avail buffer that can hold size bytes.  Does *not*
 * remove the slot from the avail buffer (see MPIR_Bsend_take_buffer)
 */
static MPII_Bsend_data_t *MPIR_Bsend_find_buffer(MPII_BsendBuffer * bsendbuffer, size_t size)
{
    MPII_Bsend_data_t *p = bsendbuffer->avail;

    while (p) {
        if (p->size >= size) {
            return p;
        }
        p = p->next;
    }
    return 0;
}

/* This is the minimum number of bytes that a segment must be able to
   hold. */
#define MIN_BUFFER_BLOCK 8
/*
 * Carve off size bytes from buffer p and leave the remainder
 * on the avail list.  Handle the head/tail cases.
 * If there isn't enough left of p, remove the entire segment from
 * the avail list.
 */
static void MPIR_Bsend_take_buffer(MPII_BsendBuffer * bsendbuffer,
                                   MPII_Bsend_data_t * p, size_t size)
{
    MPII_Bsend_data_t *prev;
    size_t alloc_size;

    /* Compute the remaining size.  This must include any padding
     * that must be added to make the new block properly aligned */
    alloc_size = size;
    if (alloc_size & (MAX_ALIGNMENT - 1)) {
        alloc_size += (MAX_ALIGNMENT - (alloc_size & (MAX_ALIGNMENT - 1)));
    }
    /* alloc_size is the amount of space (out of size) that we will
     * allocate for this buffer. */

    MPL_DBG_MSG_FMT(MPIR_DBG_BSEND, TYPICAL, (MPL_DBG_FDEST,
                                              "Taking %lu bytes from a block with %llu bytes\n",
                                              alloc_size, (unsigned long long) p->total_size));

    /* Is there enough space left to create a new block? */
    if (alloc_size + BSENDDATA_HEADER_TRUE_SIZE + MIN_BUFFER_BLOCK <= p->size) {
        /* Yes, the available space (p->size) is large enough to
         * carve out a new block */
        MPII_Bsend_data_t *newp;

        MPL_DBG_MSG_P(MPIR_DBG_BSEND, TYPICAL, "Breaking block into used and allocated at %p", p);
        newp = (MPII_Bsend_data_t *) ((char *) p + BSENDDATA_HEADER_TRUE_SIZE + alloc_size);
        newp->total_size = p->total_size - alloc_size - BSENDDATA_HEADER_TRUE_SIZE;
        newp->size = newp->total_size - BSENDDATA_HEADER_TRUE_SIZE;
        newp->msg.msgbuf = (char *) newp + BSENDDATA_HEADER_TRUE_SIZE;

        /* Insert this new block after p (we'll remove p from the avail list
         * next) */
        newp->next = p->next;
        newp->prev = p;
        if (p->next) {
            p->next->prev = newp;
        }
        p->next = newp;
        p->total_size = (char *) newp - (char *) p;
        p->size = p->total_size - BSENDDATA_HEADER_TRUE_SIZE;

        MPL_DBG_MSG_FMT(MPIR_DBG_BSEND, TYPICAL, (MPL_DBG_FDEST,
                                                  "broken blocks p (%llu) and new (%llu)\n",
                                                  (unsigned long long) p->total_size,
                                                  (unsigned long long) newp->total_size));
    }

    /* Remove p from the avail list and add it to the active list */
    prev = p->prev;
    if (prev) {
        prev->next = p->next;
    } else {
        bsendbuffer->avail = p->next;
    }

    if (p->next) {
        p->next->prev = p->prev;
    }

    if (bsendbuffer->active) {
        bsendbuffer->active->prev = p;
    }
    p->next = bsendbuffer->active;
    p->prev = 0;
    bsendbuffer->active = p;

    MPL_DBG_MSG_P(MPIR_DBG_BSEND, VERBOSE, "segment %p now head of active", p);
    MPL_DBG_MSG(MPIR_DBG_BSEND, TYPICAL, "At end of take buffer");
    MPL_DBG_STMT(MPIR_DBG_BSEND, TYPICAL, MPIR_Bsend_dump(bsendbuffer));
}

static int MPIR_Bsend_finalize(MPII_BsendBuffer ** bsendbuffer_p)
{
    if (*bsendbuffer_p) {
        /* No lock since this is inside MPI_Finalize */
        void *b;
        MPI_Aint s;

        if ((*bsendbuffer_p)->buffer) {
            /* Use detach to complete any communication */
            MPIR_Bsend_detach(bsendbuffer_p, &b, &s);
        }
        MPL_free(*bsendbuffer_p);
        *bsendbuffer_p = NULL;
    }

    return MPI_SUCCESS;
}

/*
 * These routines are defined only if debug logging is enabled
 */
#ifdef MPL_USE_DBG_LOGGING
static void MPIR_Bsend_dump(MPII_BsendBuffer * bsendbuffer)
{
    MPII_Bsend_data_t *a = bsendbuffer->avail;

    MPL_DBG_MSG_D(MPIR_DBG_BSEND, TYPICAL, "Total size is %llu",
                  (unsigned long long) bsendbuffer->buffer_size);
    MPL_DBG_MSG(MPIR_DBG_BSEND, TYPICAL, "Avail list is:");
    while (a) {
        MPL_DBG_MSG_FMT(MPIR_DBG_BSEND, TYPICAL, (MPL_DBG_FDEST, "[%p] totalsize = %llu(%llx)",
                                                  a, (unsigned long long) a->total_size,
                                                  (unsigned long long) a->total_size));
        if (a == a->next) {
            MPL_DBG_MSG(MPIR_DBG_BSEND, TYPICAL, "@@@Corrupt list; avail block points at itself");
            break;
        }
        a = a->next;
    }

    MPL_DBG_MSG(MPIR_DBG_BSEND, TYPICAL, "Active list is:");
    a = bsendbuffer->active;
    while (a) {
        MPL_DBG_MSG_FMT(MPIR_DBG_BSEND, TYPICAL, (MPL_DBG_FDEST, "[%p] totalsize = %llu(%llx)",
                                                  a, (unsigned long long) a->total_size,
                                                  (unsigned long long) a->total_size));
        if (a == a->next) {
            MPL_DBG_MSG(MPIR_DBG_BSEND, TYPICAL, "@@@Corrupt list; active block points at itself");
            break;
        }
        a = a->next;
    }
    MPL_DBG_MSG(MPIR_DBG_BSEND, TYPICAL, "end of list");
}
#endif

int MPIR_Buffer_attach_impl(void *buffer, MPI_Aint size)
{
    return MPIR_Bsend_attach(&(MPIR_Process.bsendbuffer), buffer, size);
}

int MPIR_Buffer_detach_impl(void *buffer_addr, MPI_Aint * size)
{
    return MPIR_Bsend_detach(&(MPIR_Process.bsendbuffer), buffer_addr, size);
}

int MPIR_Process_bsend_finalize(void)
{
    return MPIR_Bsend_finalize(&(MPIR_Process.bsendbuffer));
}

int MPIR_Comm_bsend_finalize(MPIR_Comm * comm_ptr)
{
    return MPIR_Bsend_finalize(&(comm_ptr->bsendbuffer));
}

int MPIR_Session_bsend_finalize(MPIR_Session * session)
{
    return MPIR_Bsend_finalize(&(session->bsendbuffer));
}
