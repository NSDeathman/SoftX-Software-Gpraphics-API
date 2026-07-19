/////////////////////////////////////////////////////////////////
// SoftX - Software Graphics API
// Copyright (c) 2026 NSDeathman
// Licensed under the MIT License.
/////////////////////////////////////////////////////////////////
#pragma once
/////////////////////////////////////////////////////////////////
#include "ThreadPoolManager.h"
/////////////////////////////////////////////////////////////////
SOFTX_BEGIN

template <typename Index, typename Func>
void ParallelFor(Index start, Index end, Index step, Func&& func)
{
    if (end <= start) return;

    auto& pool = ThreadPoolManager::Get();
    const uint32_t numThreads = pool.threadCount();
    const Index total = (end - start + step - 1) / step;

    if (numThreads <= 1 || total < 64)
    {
        for (Index i = start; i < end; i += step)
            func(i);
        return;
    }

    Index chunkSize = ((total + numThreads - 1) / numThreads) * step;
    for (uint32_t t = 0; t < numThreads; ++t)
    {
        Index chunkStart = start + t * chunkSize;
        Index chunkEnd = std::min(chunkStart + chunkSize, end);
        if (chunkStart >= end) break;

        pool.enqueue([chunkStart, chunkEnd, step, &func]()
            {
                for (Index i = chunkStart; i < chunkEnd; i += step)
                    func(i);
            });
    }
    pool.wait();
}

SOFTX_END
/////////////////////////////////////////////////////////////////
