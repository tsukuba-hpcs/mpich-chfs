/*
 * Copyright (C) by Argonne National Laboratory
 *     See COPYRIGHT in top-level directory
 */

#include "ad_chfs.h"
#include "adioi.h"

void ADIOI_CHFS_WriteStridedColl(ADIO_File fd, const void *buf, int count,
                                   MPI_Datatype datatype, int file_ptr_type,
                                   ADIO_Offset offset, ADIO_Status * status, int *error_code)
{
    int myrank, nprocs;

    *error_code = MPI_SUCCESS;

#ifdef DEBUG
    MPI_Comm_size(fd->comm, &nprocs);
    MPI_Comm_rank(fd->comm, &myrank);
    FPRINTF(stdout, "[%d/%d] ADIOI_CHFS_WriteStridedColl called on %s\n",
            myrank, nprocs, fd->filename);
    FPRINTF(stdout, "[%d/%d]    calling ADIOI_GEN_WriteStridedColl\n", myrank, nprocs);
#endif

    ADIOI_GEN_WriteStridedColl(fd, buf, count, datatype, file_ptr_type, offset, status, error_code);
}
