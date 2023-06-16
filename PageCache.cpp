#include "include/PageCache.h"

PageCache PageCache::_sInst;

// 获取一个K页的span
Span *PageCache::NewSpan(size_t k)
{
    assert(k > 0);
    // 大于128页的向堆申请
    if(k>NPAGES-1)
    {
        void* ptr = SystemAlloc(k);
        // Span* span = new Span;
        Span* span = _spanPool.New();
        span->_pageId = (PAGE_ID)ptr>>PAGE_SHIFT;
        span->_n = k;

        _idSpanMap[span->_pageId] = span;
        return span;
    }



    _spanLists[k];

    // 先检查第k个桶里面有没有span
    if (!_spanLists[k].Empty())
    {
        // return _spanLists->PopFront();  //奇怪？ 
        return _spanLists[k].PopFront();
    }

    // 检查一下后面的桶里面有没有span，如果有，可以把他进行切分
    for (size_t i = k + 1; i < NPAGES; i++)
    {
        // 如果后面的桶有一个不为空，那就开始切
        if (!_spanLists[i].Empty())
        {
            Span *nSpan = _spanLists[i].PopFront();
            // Span *kSpan = new Span;
            Span* kSpan = _spanPool.New();

            // 在nSpan的头部切一个k页下来
            // k页的span返回
            // n挂到对应的位置
            kSpan->_pageId = nSpan->_pageId;
            kSpan->_n = k;

            nSpan->_pageId += k;
            nSpan->_n -= k;

            // 把且剩下的挂回去
            _spanLists[nSpan->_n].PushFront(nSpan);

            // 存储nSpan的首位页号和nSpan建立映射，方便page cache回收内存时
            // 进行的合并查找
            _idSpanMap[nSpan->_pageId] = nSpan;
            _idSpanMap[nSpan->_pageId + nSpan->_n - 1] - nSpan;

            // 建立id和span的映射，方便central cache回收小块内存是，查找对应的Span
            for (PAGE_ID i = 0; i < kSpan->_n; ++i)
            {
                _idSpanMap[kSpan->_pageId + i] = kSpan;
            }

            return kSpan;
        }
    }

    // 走到这个位置说明后面没有大页的span
    //  这个时候就去找堆要一个128页的span
    // Span *bigSpan = new Span;
    Span *bigSpan = _spanPool.New();
    void *ptr = SystemAlloc(NPAGES - 1);
    bigSpan->_pageId = (PAGE_ID)ptr >> PAGE_SHIFT;
    bigSpan->_n = NPAGES - 1;
    // 把这个128页的span
    _spanLists[bigSpan->_n].PushFront(bigSpan);

    return NewSpan(k);
}

// 获取从对象到span的映射
Span *PageCache::MapObjectToSpan(void *obj)
{
    PAGE_ID id = ((PAGE_ID)obj >> PAGE_SHIFT);
    auto ret = _idSpanMap.find(id);
    if (ret != _idSpanMap.end())
    {
        return ret->second;
    }
    else
    {
        assert(false);
        return nullptr;
    }
}

void PageCache::ReleaseSpanToPageCache(Span *span)
{
    //大于128age的直接还给堆
    if(span->_n > NPAGES-1)
    {
        void* ptr = (void*)(span->_pageId<<PAGE_SHIFT);
        size_t kpage = span->_n;
        SystemFree(ptr,kpage);
        // delete span;
        _spanPool.Delete(span);

        return;
    }

    // 对Span前后的页，尝试进行合并，缓解内存碎片问题
    while (1)
    {
        PAGE_ID prevId = span->_pageId - 1;
        auto ret = _idSpanMap.find(prevId);

        // 前面的页号没有，不合并了
        if (ret == _idSpanMap.end())
        {
            break;
        }
        // 前面的相邻Span页在使用，不合并了
        Span *prevSpan = ret->second;
        if (prevSpan->_isUse == true)
        {
            break;
        }
        // w合并出超过128页的span，不合并
        if (prevSpan->_n + span->_n > NPAGES - 1)
        {
            break;
        }
        span->_pageId = prevSpan->_pageId;
        span->_n += prevSpan->_n;
        // delete prevSpan;
        _spanPool.Delete(prevSpan);
    }

    // x向后合并
    while (1)
    {
        PAGE_ID nextId = span->_pageId + span->_n;
        auto ret = _idSpanMap.find(nextId);

        // 后面的页号没有，不合并了
        if (ret == _idSpanMap.end())
        {
            break;
        }
        // 后面的相邻Span页在使用，不合并了
        Span *nextSpan = ret->second;
        if (nextSpan->_isUse == true)
        {
            break;
        }
        // 合并出超过128页的span，不合并
        if (nextSpan->_n + span->_n > NPAGES - 1)
        {
            break;
        }
        span->_n += nextSpan->_n;
        _spanLists[nextSpan->_n].Erase(nextSpan);
        // delete nextSpan;
        _spanPool.Delete(nextSpan);
    }

    _spanLists[span->_n].PushFront(span);
    span->_isUse = false;
    _idSpanMap[span ->_pageId] = span;
    _idSpanMap[span->_pageId + span->_n - 1] = span;
}