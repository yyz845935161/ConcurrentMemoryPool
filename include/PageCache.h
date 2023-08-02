#pragma once
#include "Common.h"
#include "ObjectPool.h"
class PageCache
{

public:
    // 获取这个单例对象
    static PageCache *GetInstance()
    {
        return &_sInst;
    }

    // 获取从对象到span的映射
    Span *MapObjectToSpan(void *obj);

    // 释放空闲span回到Pagecache，并合并相邻的span
    void ReleaseSpanToPageCache(Span *span);

    // 获取一个K页的span
    Span *NewSpan(size_t k);
    std::mutex _pageMtx; // 只能是全局的锁
private:
    SpanList _spanLists[NPAGES];
    ObjectPool<Span> _spanPool;
    std::unordered_map<PAGE_ID, Span *> _idSpanMap;

    static PageCache _sInst;

    PageCache(){};
    PageCache(const PageCache &) = delete;
};
