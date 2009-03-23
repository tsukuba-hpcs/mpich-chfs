/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 *
 *  (C) 2009 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

/* The MPI Standard (MPI-2.1, sec 5.9.2) defines which predfined reduction
   operators are valid by groups of types:
     C integer
     Fortran integer
     Floating point
     Logical
     Complex
     Byte

   We define an "x-macro" for each type group.  Immediately prior to
   instantiating any of these macros you should define a valid
   MPIR_OP_TYPE_MACRO(mpi_type_,c_type_).  The primary use for this is to expand
   a given group's list into a sequence of case statements.  The macro
   MPIR_OP_TYPE_REDUCE_CASE is available as a convenience to generate a case
   block that performs a reduction with the given operator.  */

#if 0 /* sample usage: */
#undef MPIR_OP_TYPE_MACRO
#define MPIR_OP_TYPE_MACRO(mpi_type_,c_type_) MPIR_OP_TYPE_REDUCE_CASE(mpi_type_,c_type_,MPIR_MAX)
/* or */
#define MPIR_OP_TYPE_MACRO(mpi_type_,c_type_) case (mpi_type_):

MPIR_OP_TYPE_GROUP(C_INTEGER)
MPIR_OP_TYPE_GROUP(FORTRAN_INTEGER)
#undef MPIR_OP_TYPE_MACRO
#endif


/* op_macro_ is a 2-arg macro or function that preforms the reduction
   operation on a single element */
#define MPIR_OP_TYPE_REDUCE_CASE(mpi_type_,c_type_,op_macro_) \
    case (mpi_type_): {                                       \
        c_type_ * restrict a = (c_type_ *)inoutvec;           \
        const c_type_ * restrict b = (c_type_ *)invec;        \
        for ( i=0; i<len; i++ )                               \
            a[i] = op_macro_(a[i],b[i]);                      \
        break;                                                \
    }

/* helps enforce consistent naming */
#define MPIR_OP_TYPE_GROUP(group) MPIR_OP_TYPE_GROUP_##group

/* -------------------------------------------------------------------- */
/* These macros are used to disable non-existent types.  They evaluate to
   nothing if the particular feature test is false, otherwise they evaluate to
   the standard macro to be expanded like any other type. */

/* first define all wrapper macros as empty for possibly non-existent types */
#define MPIR_OP_TYPE_MACRO_HAVE_FORTRAN(mpi_type_,c_type_)
#define MPIR_OP_TYPE_MACRO_HAVE_COMPLEX8(mpi_type_,c_type_)
#define MPIR_OP_TYPE_MACRO_HAVE_COMPLEX16(mpi_type_,c_type_)
#define MPIR_OP_TYPE_MACRO_HAVE_LONG_LONG(mpi_type_,c_type_)
#define MPIR_OP_TYPE_MACRO_HAVE_LONG_DOUBLE(mpi_type_,c_type_)
#define MPIR_OP_TYPE_MACRO_HAVE_INTEGER1_CTYPE(mpi_type_,c_type_)
#define MPIR_OP_TYPE_MACRO_HAVE_INTEGER2_CTYPE(mpi_type_,c_type_)
#define MPIR_OP_TYPE_MACRO_HAVE_INTEGER4_CTYPE(mpi_type_,c_type_)
#define MPIR_OP_TYPE_MACRO_HAVE_INTEGER8_CTYPE(mpi_type_,c_type_)
#define MPIR_OP_TYPE_MACRO_HAVE_INTEGER16_CTYPE(mpi_type_,c_type_)
#define MPIR_OP_TYPE_MACRO_HAVE_REAL4_CTYPE(mpi_type_,c_type_)
#define MPIR_OP_TYPE_MACRO_HAVE_REAL8_CTYPE(mpi_type_,c_type_)
#define MPIR_OP_TYPE_MACRO_HAVE_REAL16_CTYPE(mpi_type_,c_type_)
#define MPIR_OP_TYPE_MACRO_HAVE_CXX_COMPLEX(mpi_type_,c_type_)
#define MPIR_OP_TYPE_MACRO_HAVE_CXX_LONG_DOUBLE_COMPLEX(mpi_type_,c_type_)

/* then redefine them to be valid based on other preprocessor definitions */
#if defined(HAVE_FORTRAN_BINDING)
#  undef MPIR_OP_TYPE_MACRO_HAVE_FORTRAN
#  undef MPIR_OP_TYPE_MACRO_HAVE_COMPLEX8
#  undef MPIR_OP_TYPE_MACRO_HAVE_COMPLEX16
#  define MPIR_OP_TYPE_MACRO_HAVE_FORTRAN(mpi_type_,c_type_) MPIR_OP_TYPE_MACRO(mpi_type_,c_type_)
/* These two shouldn't really be gated on HAVE_FORTRAN_BINDING alone.  There
   should instead be an individual test like HAVE_LONG_DOUBLE, etc. */
#  define MPIR_OP_TYPE_MACRO_HAVE_COMPLEX8(mpi_type_,c_type_) MPIR_OP_TYPE_MACRO(mpi_type_,c_type_)
#  define MPIR_OP_TYPE_MACRO_HAVE_COMPLEX16(mpi_type_,c_type_) MPIR_OP_TYPE_MACRO(mpi_type_,c_type_)
#endif

#if defined(HAVE_LONG_LONG_INT)
#  undef MPIR_OP_TYPE_MACRO_HAVE_LONG_LONG
#  define MPIR_OP_TYPE_MACRO_HAVE_LONG_LONG(mpi_type_,c_type_) MPIR_OP_TYPE_MACRO(mpi_type_,c_type_)
#endif

#if defined(HAVE_LONG_DOUBLE)
#  undef MPIR_OP_TYPE_MACRO_HAVE_LONG_DOUBLE
#  define MPIR_OP_TYPE_MACRO_HAVE_LONG_DOUBLE(mpi_type_,c_type_) MPIR_OP_TYPE_MACRO(mpi_type_,c_type_)
#endif

/* Fortran fixed width integer type support */
#if defined(MPIR_INTEGER1_CTYPE)
#  undef MPIR_OP_TYPE_MACRO_HAVE_INTEGER1_CTYPE
#  define MPIR_OP_TYPE_MACRO_HAVE_INTEGER1_CTYPE(mpi_type_,c_type_) MPIR_OP_TYPE_MACRO(mpi_type_,c_type_)
#endif
#if defined(MPIR_INTEGER2_CTYPE)
#  undef MPIR_OP_TYPE_MACRO_HAVE_INTEGER2_CTYPE
#  define MPIR_OP_TYPE_MACRO_HAVE_INTEGER2_CTYPE(mpi_type_,c_type_) MPIR_OP_TYPE_MACRO(mpi_type_,c_type_)
#endif
#if defined(MPIR_INTEGER4_CTYPE)
#  undef MPIR_OP_TYPE_MACRO_HAVE_INTEGER4_CTYPE
#  define MPIR_OP_TYPE_MACRO_HAVE_INTEGER4_CTYPE(mpi_type_,c_type_) MPIR_OP_TYPE_MACRO(mpi_type_,c_type_)
#endif
#if defined(MPIR_INTEGER8_CTYPE)
#  undef MPIR_OP_TYPE_MACRO_HAVE_INTEGER8_CTYPE
#  define MPIR_OP_TYPE_MACRO_HAVE_INTEGER8_CTYPE(mpi_type_,c_type_) MPIR_OP_TYPE_MACRO(mpi_type_,c_type_)
#endif
#if defined(MPIR_INTEGER16_CTYPE)
#  undef MPIR_OP_TYPE_MACRO_HAVE_INTEGER16_CTYPE
#  define MPIR_OP_TYPE_MACRO_HAVE_INTEGER16_CTYPE(mpi_type_,c_type_) MPIR_OP_TYPE_MACRO(mpi_type_,c_type_)
#endif

/* Fortran fixed width floating point type support */
#if defined(MPIR_REAL4_CTYPE)
#  undef MPIR_OP_TYPE_MACRO_HAVE_REAL4_CTYPE
#  define MPIR_OP_TYPE_MACRO_HAVE_REAL4_CTYPE(mpi_type_,c_type_) MPIR_OP_TYPE_MACRO(mpi_type_,c_type_)
#endif
#if defined(MPIR_REAL8_CTYPE)
#  undef MPIR_OP_TYPE_MACRO_HAVE_REAL8_CTYPE
#  define MPIR_OP_TYPE_MACRO_HAVE_REAL8_CTYPE(mpi_type_,c_type_) MPIR_OP_TYPE_MACRO(mpi_type_,c_type_)
#endif
#if defined(MPIR_REAL16_CTYPE)
#  undef MPIR_OP_TYPE_MACRO_HAVE_REAL16_CTYPE
#  define MPIR_OP_TYPE_MACRO_HAVE_REAL16_CTYPE(mpi_type_,c_type_) MPIR_OP_TYPE_MACRO(mpi_type_,c_type_)
#endif

/* C++ complex types */
#if defined(HAVE_CXX_COMPLEX)
#  undef MPIR_OP_TYPE_MACRO_HAVE_CXX_COMPLEX
#  define MPIR_OP_TYPE_MACRO_HAVE_CXX_COMPLEX(mpi_type_,c_type_) MPIR_OP_TYPE_MACRO(mpi_type_,c_type_)
#endif
/* also test against MPI_DATATYPE_NULL for extra safety, 0x0c000000 is the uncasted value. */
#if defined(HAVE_CXX_COMPLEX) && (MPIR_CXX_LONG_DOUBLE_COMPLEX_VALUE != 0x0c000000)
#  undef MPIR_OP_TYPE_MACRO_HAVE_CXX_LONG_DOUBLE_COMPLEX
#  define MPIR_OP_TYPE_MACRO_HAVE_CXX_LONG_DOUBLE_COMPLEX(mpi_type_,c_type_) MPIR_OP_TYPE_MACRO(mpi_type_,c_type_)
#endif

/* C types needed to support some of the complex types.

   FIXME These are a hack in most cases, but they seem to work in practice
   and it's what we were doing prior to the oputil.h refactoring. */
typedef struct {
    float re;
    float im;
} s_complex;

typedef struct {
    double re;
    double im;
} d_complex;

#if defined(HAVE_LONG_DOUBLE)
typedef struct {
    long double re;
    long double im;
} ld_complex;
#endif

/* -------------------------------------------------------------------- */
/* type group macros

   Implementation note: it is important that no MPI type show up more than once
   among all the lists.  Otherwise it will be easy to end up with two case
   statements with the same value, which is erroneous in C.  Duplicate C types
   in this list are not a problem. */

/* c integer group */
#define MPIR_OP_TYPE_GROUP_C_INTEGER                       \
    MPIR_OP_TYPE_MACRO(MPI_INT, int)                       \
    MPIR_OP_TYPE_MACRO(MPI_LONG, long)                     \
    MPIR_OP_TYPE_MACRO(MPI_SHORT, short)                   \
    MPIR_OP_TYPE_MACRO(MPI_UNSIGNED_SHORT, unsigned short) \
    MPIR_OP_TYPE_MACRO(MPI_UNSIGNED, unsigned)             \
    MPIR_OP_TYPE_MACRO(MPI_UNSIGNED_LONG, unsigned long)   \
    MPIR_OP_TYPE_MACRO_HAVE_LONG_LONG(MPI_LONG_LONG, long long) \
    MPIR_OP_TYPE_MACRO_HAVE_LONG_LONG(MPI_UNSIGNED_LONG_LONG, unsigned long long) \
    MPIR_OP_TYPE_MACRO(MPI_SIGNED_CHAR, signed char)       \
    MPIR_OP_TYPE_MACRO(MPI_UNSIGNED_CHAR, unsigned char)
/* The MPI Standard doesn't include these types in the C integer group for
   predefined operations but MPICH2 supports them when possible. */
#define MPIR_OP_TYPE_GROUP_C_INTEGER_EXTRA                  \
    MPIR_OP_TYPE_MACRO(MPI_CHAR, char)

/* fortran integer group */
#define MPIR_OP_TYPE_GROUP_FORTRAN_INTEGER                 \
    MPIR_OP_TYPE_MACRO_HAVE_FORTRAN(MPI_INTEGER, MPI_Fint)
/* The MPI Standard doesn't include these types in the Fortran integer group for
   predefined operations but MPICH2 supports them when possible. */
#define MPIR_OP_TYPE_GROUP_FORTRAN_INTEGER_EXTRA                                 \
    MPIR_OP_TYPE_MACRO_HAVE_FORTRAN(MPI_CHARACTER, char)                         \
    MPIR_OP_TYPE_MACRO_HAVE_INTEGER1_CTYPE(MPI_INTEGER1, MPIR_INTEGER1_CTYPE)    \
    MPIR_OP_TYPE_MACRO_HAVE_INTEGER2_CTYPE(MPI_INTEGER2, MPIR_INTEGER2_CTYPE)    \
    MPIR_OP_TYPE_MACRO_HAVE_INTEGER4_CTYPE(MPI_INTEGER4, MPIR_INTEGER4_CTYPE)    \
    MPIR_OP_TYPE_MACRO_HAVE_INTEGER8_CTYPE(MPI_INTEGER8, MPIR_INTEGER8_CTYPE)    \
    MPIR_OP_TYPE_MACRO_HAVE_INTEGER16_CTYPE(MPI_INTEGER16, MPIR_INTEGER16_CTYPE)

/* floating point group */
#define MPIR_OP_TYPE_GROUP_FLOATING_POINT                             \
    MPIR_OP_TYPE_MACRO(MPI_FLOAT, float)                              \
    MPIR_OP_TYPE_MACRO(MPI_DOUBLE, double)                            \
    MPIR_OP_TYPE_MACRO_HAVE_FORTRAN(MPI_REAL, float)                  \
    MPIR_OP_TYPE_MACRO_HAVE_FORTRAN(MPI_DOUBLE_PRECISION, double)     \
    MPIR_OP_TYPE_MACRO_HAVE_LONG_DOUBLE(MPI_LONG_DOUBLE, long double) \
/* The MPI Standard doesn't include these types in the floating point group for
   predefined operations but MPICH2 supports them when possible. */
#define MPIR_OP_TYPE_GROUP_FLOATING_POINT_EXTRA                         \
    MPIR_OP_TYPE_MACRO_HAVE_REAL4_CTYPE(MPI_REAL4, MPIR_REAL4_CTYPE)    \
    MPIR_OP_TYPE_MACRO_HAVE_REAL8_CTYPE(MPI_REAL8, MPIR_REAL8_CTYPE)    \
    MPIR_OP_TYPE_MACRO_HAVE_REAL16_CTYPE(MPI_REAL16, MPIR_REAL16_CTYPE)

/* logical group */
/* FIXME Is MPI_Fint really OK here? */
#define MPIR_OP_TYPE_GROUP_LOGICAL                         \
    MPIR_OP_TYPE_MACRO_HAVE_FORTRAN(MPI_LOGICAL, MPI_Fint)
#define MPIR_OP_TYPE_GROUP_LOGICAL_EXTRA /* empty, provided for consistency */

/* complex group */
#define MPIR_OP_TYPE_GROUP_COMPLEX                          \
    MPIR_OP_TYPE_MACRO_HAVE_FORTRAN(MPI_COMPLEX, s_complex)
#define MPIR_OP_TYPE_GROUP_COMPLEX_EXTRA                                                            \
    MPIR_OP_TYPE_MACRO_HAVE_FORTRAN(MPI_DOUBLE_COMPLEX, d_complex)                                  \
    MPIR_OP_TYPE_MACRO_HAVE_COMPLEX8(MPI_COMPLEX8, s_complex)                                       \
    MPIR_OP_TYPE_MACRO_HAVE_COMPLEX16(MPI_COMPLEX16, d_complex)                                     \
    MPIR_OP_TYPE_MACRO_HAVE_CXX_COMPLEX(MPIR_CXX_COMPLEX_VALUE, s_complex)                          \
    MPIR_OP_TYPE_MACRO_HAVE_CXX_COMPLEX(MPIR_CXX_DOUBLE_COMPLEX_VALUE, d_complex)                   \
    MPIR_OP_TYPE_MACRO_HAVE_CXX_LONG_DOUBLE_COMPLEX(MPIR_CXX_LONG_DOUBLE_COMPLEX_VALUE, ld_complex)

/* byte group */
#define MPIR_OP_TYPE_GROUP_BYTE                 \
    MPIR_OP_TYPE_MACRO(MPI_BYTE, unsigned char)
#define MPIR_OP_TYPE_GROUP_BYTE_EXTRA /* empty, provided for consistency */

/* convenience macro that just is all non-extra groups concatenated */
#define MPIR_OP_TYPE_GROUP_ALL_BASIC    \
    MPIR_OP_TYPE_GROUP(C_INTEGER)       \
    MPIR_OP_TYPE_GROUP(FORTRAN_INTEGER) \
    MPIR_OP_TYPE_GROUP(FLOATING_POINT)  \
    MPIR_OP_TYPE_GROUP(LOGICAL)         \
    MPIR_OP_TYPE_GROUP(COMPLEX)         \
    MPIR_OP_TYPE_GROUP(BYTE)

/* this macro includes just the extra type groups */
#define MPIR_OP_TYPE_GROUP_ALL_EXTRA          \
    MPIR_OP_TYPE_GROUP(C_INTEGER_EXTRA)       \
    MPIR_OP_TYPE_GROUP(FORTRAN_INTEGER_EXTRA) \
    MPIR_OP_TYPE_GROUP(FLOATING_POINT_EXTRA)  \
    MPIR_OP_TYPE_GROUP(LOGICAL_EXTRA)         \
    MPIR_OP_TYPE_GROUP(COMPLEX_EXTRA)         \
    MPIR_OP_TYPE_GROUP(BYTE_EXTRA)

