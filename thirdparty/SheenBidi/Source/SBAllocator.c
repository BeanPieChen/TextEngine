/*
 * Copyright (C) 2025 Muhammad Tayyab Akram
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stddef.h>
#include <stdlib.h>

#include <SheenBidi/SBConfig.h>

#include "Object.h"
#include "SBBase.h"
#include "SBAllocator.h"

typedef SBAllocator* SBMutableAllocatorRef;

static SBAllocatorRef DefaultAllocator = NULL;

#ifndef SB_CONFIG_DISABLE_SCRATCH_MEMORY

#if defined(HAS_ATOMIC_POINTER_SUPPORT) && defined(HAS_TLS_SUPPORT) && defined(HAS_ONCE_SUPPORT)
#define USE_SCRATCH_MEMORY
#else
#error "Scratch memory functionality requires atomic operations, thread-local, and once support. \
To proceed without scratch memory, manually define `SB_CONFIG_DISABLE_SCRATCH_MEMORY`."
#endif

#endif


#define NativeAllocateScratch   NULL
#define NativeResetScratch      NULL


static void *NativeAllocateBlock(SBUInteger size, void *info)
{
    return malloc(size);
}

static void *NativeReallocateBlock(void *pointer, SBUInteger size, void *info)
{
    return realloc(pointer, size);
}

static void NativeDeallocateBlock(void *pointer, void *info)
{
    free(pointer);
}

static SBAllocator NativeAllocator = SBAllocatorMake(
    NativeAllocateBlock, NativeReallocateBlock, NativeDeallocateBlock, NULL);

static void FinalizeAllocator(ObjectRef object)
{
    SBAllocatorRef allocator = object;

    if (allocator->_protocol.finalize) {
        allocator->_protocol.finalize(allocator->_info);
    }
}

static SBAllocatorRef SBAllocatorGetCurrent(void)
{
    SBAllocatorRef allocator = DefaultAllocator;

    if (!allocator) {
        allocator = &NativeAllocator;
    }

    return allocator;
}

SB_INTERNAL void *SBAllocatorAllocateBlock(SBAllocatorRef allocator, SBUInteger size)
{
    if (!allocator) {
        allocator = SBAllocatorGetCurrent();
    }

    return allocator->_protocol.allocateBlock(size, allocator->_info);
}

SB_INTERNAL void *SBAllocatorReallocateBlock(SBAllocatorRef allocator, void *pointer, SBUInteger newSize)
{
    if (!allocator) {
        allocator = SBAllocatorGetCurrent();
    }

    return allocator->_protocol.reallocateBlock(pointer, newSize, allocator->_info);
}

SB_INTERNAL void SBAllocatorDeallocateBlock(SBAllocatorRef allocator, void *pointer)
{
    if (!allocator) {
        allocator = SBAllocatorGetCurrent();
    }

    allocator->_protocol.deallocateBlock(pointer, allocator->_info);
}

SBAllocatorRef SBAllocatorGetDefault(void)
{
    return DefaultAllocator;
}

void SBAllocatorSetDefault(SBAllocatorRef allocator)
{
    DefaultAllocator = (SBAllocator *)allocator;
}

SBAllocatorRef SBAllocatorCreate(const SBAllocatorProtocol *protocol, void *info)
{
    const SBUInteger size = sizeof(SBAllocator);
    void *pointer = NULL;
    SBMutableAllocatorRef allocator = NULL;

    allocator = ObjectCreate(&size, 1, &pointer, FinalizeAllocator);

    if (allocator) {
        allocator->_info = info;
        allocator->_protocol = *protocol;
    }

    return allocator;
}

SBAllocatorRef SBAllocatorRetain(SBAllocatorRef allocator)
{
    return ObjectRetain((ObjectRef)allocator);
}

void SBAllocatorRelease(SBAllocatorRef allocator)
{
    ObjectRelease((ObjectRef)allocator);
}
