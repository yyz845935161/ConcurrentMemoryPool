#pragma once
#include "Common.h"
#include "ThreadCache.h"
#include "PageCache.h"
#include "ObjectPool.h"
static void *ConcurrentAlloc(size_t size)
{
    if (size > MAX_BYTES)
    {

        size_t alignSize = SizeClass::RoundUp(size);
        size_t kpage = alignSize >> PAGE_SHIFT;

        PageCache::GetInstance()->_pageMtx.lock();
        Span *span = PageCache::GetInstance()->NewSpan(kpage);
        PageCache::GetInstance()->_pageMtx.unlock();

        void *ptr = (void *)(span->_pageId << PAGE_SHIFT);
        return ptr;
    }
    else
    {
        // 通过TLS 每个线程无锁的获取自己专属的ThreadCache对象
        if (pTLSThreadCache == nullptr)
        {
            static ObjectPool<ThreadCache> tcPool;
            // pTLSThreadCache = new ThreadCache;
            pTLSThreadCache = tcPool.New();
            cout << "成功new出了ThreadCache对象" << endl;
        }
        // cout << getpid() << ":" << pTLSThreadCache << endl;
        return pTLSThreadCache->Allocate(size);
        // return nullptr;
    }
}

static void *ConcurrentFree(void *ptr, size_t size)
{
    if (size > MAX_BYTES)
    {
        Span* span = PageCache::GetInstance()->MapObjectToSpan(ptr);

        PageCache::GetInstance()->_pageMtx.lock();
        PageCache::GetInstance()->ReleaseSpanToPageCache(span);
        PageCache::GetInstance()->_pageMtx.unlock();
    }
    else
    {
        assert(pTLSThreadCache);
        pTLSThreadCache->Deallocate(ptr, size);
    }
}