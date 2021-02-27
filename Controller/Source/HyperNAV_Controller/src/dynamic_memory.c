/*
 *  File: dynamic_memory.c
 *  HyperNav Sensor Firmware
 *  Copyright 2010 Satlantic Inc.
 */

# if 0

# include "dynamic_memory.h"

// Use thread-safe dynamic memory allocation if available
#ifdef FREERTOS_USED
#include "FreeRTOS.h"
#else
#define pvPortMalloc	malloc
#define vPortFree		free
#endif


F64** DYNMEM_F64_MatrixAlloc ( const int N, const int M ) {

   int i;
   F64** matrix = (F64**) pvPortMalloc ( N * sizeof(F64*) );

   if ( !matrix ) {
       return 0;
   }

   matrix[0] = (F64*) pvPortMalloc ( N*M*sizeof(F64) );

   if ( !matrix[0] ) {
       vPortFree ( matrix );
       return 0;
   }

   for ( i=1; i<N; i++ ) {
       matrix[i] = matrix[i-1] + M;
   }

   return matrix;
}

void DYNMEM_F64_MatrixFree ( F64** matrix ) {

   if ( matrix ) {
       vPortFree ( matrix[0] );
       vPortFree ( matrix );
   }

   return;
}

F64* DYNMEM_F64_VectorAlloc ( const int N ) {

	return (F64*) pvPortMalloc ( N * sizeof(F64) );
}

void DYNMEM_F64_VectorFree ( F64* vector ) {

   vPortFree ( vector );
   return;
}


F32** DYNMEM_F32_MatrixAlloc ( const int N, const int M ) {

   int i;
   F32** matrix = (F32**) pvPortMalloc ( N * sizeof(F32*) );

   if ( !matrix ) {
       return 0;
   }

   matrix[0] = (F32*) pvPortMalloc ( N*M*sizeof(F32) );

   if ( !matrix[0] ) {
       vPortFree ( matrix );
       return 0;
   }

   for ( i=1; i<N; i++ ) {
       matrix[i] = matrix[i-1] + M;
   }

   return matrix;
}

void DYNMEM_F32_MatrixFree ( F32** matrix ) {

   if ( matrix ) {
       vPortFree ( matrix[0] );
       vPortFree ( matrix );
   }

   return;
}


F32* DYNMEM_F32_VectorAlloc ( const int N ) {

	return (F32*) pvPortMalloc ( N * sizeof(F32) );
}

void DYNMEM_F32_VectorFree ( F32* vector ) {

   vPortFree ( vector );
   return;
}





# if NEED_NUM_REC_SUPPORT

/*  Allocate / Free of 1-based vectors and arrays  */

#define NR_END   1
#define FREE_ARG char*

/*  Allocate / Free an vector of bool with subscript range v[nl..nh] */
bool *bvector ( short nl, short nh );
bool *bvector ( short nl, short nh )
{
        bool *v;

        v = (bool*) pvPortMalloc ( (size_t) ( (nh-nl+1+NR_END)*sizeof(bool) ) );
        if (!v) return 0;
        else    return v-nl+NR_END;
}

void free_bvector ( bool *v, short nl /*, short nh */ );
void free_bvector ( bool *v, short nl /*, short nh */ )
{
        vPortFree((FREE_ARG) (v+nl-NR_END));
}

/*  Allocate / Free an vector of short with subscript range v[nl..nh] */
short *svector ( short nl, short nh );
short *svector ( short nl, short nh )
{
        short *v;

        v = (short*) pvPortMalloc ( (size_t) ( (nh-nl+1+NR_END)*sizeof(short) ) );
        if (!v) return 0;
        else    return v-nl+NR_END;
}

void free_svector ( short *v, short nl /*, short nh */ );
void free_svector ( short *v, short nl /*, short nh */ )
{
        vPortFree((FREE_ARG) (v+nl-NR_END));
}

/*  Allocate / Free an vector of double with subscript range v[nl..nh] */
double *dvector ( short nl, short nh );
double *dvector ( short nl, short nh )
{
        double *v;

        v = (double*) pvPortMalloc ( (size_t) ( (nh-nl+1+NR_END)*sizeof(double) ) );
        if (!v) return 0;
        else    return v-nl+NR_END;
}

void free_dvector ( double *v, short nl /*, short nh */ );
void free_dvector ( double *v, short nl /*, short nh */ )
{
        vPortFree((FREE_ARG) (v+nl-NR_END));
}


/*  Allocate / Free a matrix of char with subscript range m[nrl..nrh][ncl..nch] */
char **cmatrix ( short nrl, short nrh, short ncl, short nch);
char **cmatrix ( short nrl, short nrh, short ncl, short nch)
{
        short i, nrow=nrh-nrl+1,ncol=nch-ncl+1;
        char **m;

        /* allocate pointers to rows */
        m = (char**) pvPortMalloc ( (size_t) ( (nrow+NR_END)*sizeof(char*) ) );
        if ( !m ) return 0;
        m += NR_END;
        m -= nrl;

        /* allocate rows (as one block) and set pointers to them */
        m[nrl] = (char *) pvPortMalloc((size_t)((nrow*ncol+NR_END)*sizeof(char)));
        if ( !m[nrl] ) {
                vPortFree ( (FREE_ARG) ( m+nrl-NR_END ) );
                return 0;
        }
        m[nrl] += NR_END;
        m[nrl] -= ncl;

        for(i=nrl+1;i<=nrh;i++) m[i]=m[i-1]+ncol;

        /* return pointer to array of pointers to rows */
        return m;
}

void free_cmatrix ( char **m, short nrl, /* short nrh, */ short ncl /*, short nch */ );
void free_cmatrix ( char **m, short nrl, /* short nrh, */ short ncl /*, short nch */ )
{
        vPortFree ( (FREE_ARG) (m[nrl]+ncl-NR_END) );
        vPortFree ( (FREE_ARG) (m+nrl-NR_END) );
}

/*  Allocate / Free a matrix of double with subscript range m[nrl..nrh][ncl..nch] */
double **dmatrix ( short nrl, short nrh, short ncl, short nch);
double **dmatrix ( short nrl, short nrh, short ncl, short nch)
{
        short i, nrow=nrh-nrl+1,ncol=nch-ncl+1;
        double **m;

        /* allocate pointers to rows */
        m = (double**) pvPortMalloc ( (size_t) ( (nrow+NR_END)*sizeof(double*) ) );
        if ( !m ) return 0;
        m += NR_END;
        m -= nrl;

        /* allocate rows (as one block) and set pointers to them */
        m[nrl] = (double *) pvPortMalloc((size_t)((nrow*ncol+NR_END)*sizeof(double)));
        if ( !m[nrl] ) {
                vPortFree ( (FREE_ARG) ( m+nrl-NR_END ) );
                return 0;
        }
        m[nrl] += NR_END;
        m[nrl] -= ncl;

        for(i=nrl+1;i<=nrh;i++) m[i]=m[i-1]+ncol;

        /* return pointer to array of pointers to rows */
        return m;
}

void free_dmatrix ( double **m, short nrl, /* short nrh, */ short ncl /*, short nch */ );
void free_dmatrix ( double **m, short nrl, /* short nrh, */ short ncl /*, short nch */ )
{
        vPortFree ( (FREE_ARG) (m[nrl]+ncl-NR_END) );
        vPortFree ( (FREE_ARG) (m+nrl-NR_END) );
}

# endif

# endif
