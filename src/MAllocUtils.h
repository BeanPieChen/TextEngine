#ifndef MALLOC_UTILS_H
#define MALLOC_UTILS_H

#include <cstddef>

//Copyright (C) 2025 Hongyi Chen (BeanPieChen)
//Licensed under the MIT License

#define IS_NULL_POINTER(p) (p==nullptr||p==0)

typedef void* MAllocF(size_t size);
typedef void FreeF(void* mem);

#endif