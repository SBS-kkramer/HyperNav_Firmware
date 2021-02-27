/*
 *  File: dynamic_memory.h
 *  Nitrate Sensor Firmware
 *  Copyright 2010 Satlantic Inc.
 */

#ifndef DYNAMIC_MEMORY_H_
#define DYNAMIC_MEMORY_H_

# error "dynamic_memory.h should not be included"

# if 0

# include <compiler.h>

F64** DYNMEM_F64_MatrixAlloc ( const int N, const int M );
void  DYNMEM_F64_MatrixFree ( F64** matrix );

F64* DYNMEM_F64_VectorAlloc ( const int N );
void DYNMEM_F64_VectorFree ( F64* vector );

F32** DYNMEM_F32_MatrixAlloc ( const int N, const int M );
void  DYNMEM_F32_MatrixFree ( F32** matrix );

F32* DYNMEM_F32_VectorAlloc ( const int N );
void DYNMEM_F32_VectorFree ( F32* vector );

# endif

#endif /* DYNAMIC_MEMORY_H_ */
