/////////////////////////////////////////////////////////////////
// SoftX - Software Graphics API
// Copyright (c) 2026 NSDeathman
// Licensed under the MIT License.
/////////////////////////////////////////////////////////////////
#include "../include/SoftX.h"
#include "ThreadPoolManager.h"
/////////////////////////////////////////////////////////////////
SOFTX_BEGIN

void ThreadPoolManager::Initialize(size_t threadCount)
{
    if (instance)
        SOFTX_THROW(InvalidState("ThreadPoolManager already initialized"));
    instance = std::make_unique<ThreadPool>(threadCount);
}

ThreadPool& ThreadPoolManager::Get()
{
    if (!instance)
        SOFTX_THROW(InvalidState("ThreadPoolManager not initialized. Call Device constructor first."));
    return *instance;
}

void ThreadPoolManager::Shutdown() 
{
    instance.reset();
}

SOFTX_END
/////////////////////////////////////////////////////////////////
