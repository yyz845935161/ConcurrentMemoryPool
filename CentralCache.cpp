#include "include/CentralCache.h"
#include "include/PageCache.h"
// 单例模式的初始化
CentralCache CentralCache::_sInst;

// 从SpanList或者page cache获取一个非空的span
Span *CentralCache::GetOneSpan(SpanList &list, size_t size)
{
    // 查看当前spanlist中是否有未分配对象的span
    Span *it = list.Begin();
    // 遍历CentralCache某个桶中的Span
    // 如果这个Span中有空间就返回
    while (it != list.End())
    {
        if (it->_freeList != nullptr)
        {
            return it;
        }

        it = it->_next; //这个sapn中没有空间就一直找一遍
    }

    // 先把central cache的桶锁解掉，这样如果其他线程释放内存对象回来，不会阻塞
    list._mtx.unlock();

    // 说明桶中Span已经没有剩余空间，只能找pagecatche要
    PageCache::GetInstance()->_pageMtx.lock();
    Span *span = PageCache::GetInstance()->NewSpan(SizeClass::NumMovePage(size));
    span->_isUse = true;
    PageCache::GetInstance()->_pageMtx.unlock();

    // 对获取的span进行切分，不需要加锁，因为其他线程拿不到这个span

    // 计算span的大块内存的起始地址和大块内存的大小（子节数） （通过页号）
    char *start = (char *)(span->_pageId << PAGE_SHIFT);
    size_t bytes = span->_n << PAGE_SHIFT;
    char *end = start + bytes;

    // 把大块内存切成自由链表
    // 1、先切一块下来做头,方便尾插，尾插是为了让物理空间连续
    span->_freeList = start;
    start += size;
    void *tail = span->_freeList;
    int i = 1;
    while (start < end)
    {
        ++i;
        if(tail == nullptr)
        {
            // assert(tail);
            int x = 0;
        }
        NextObj(tail) = start;
        tail = start;
        start += size;
    }

    // 切好span以后，需要把span挂到桶的时候，再加锁
    list._mtx.lock();
    list.PushFront(span);

    return span;
}

// 从中心缓存获取一定数量的对象给thread cache
size_t CentralCache::FetchRangeObj(void *&start, void *&end, size_t batchNum, size_t size)
{

    size_t index = SizeClass::Index(size);
    _spanLists[index]._mtx.lock();
    Span *span = GetOneSpan(_spanLists[index], size);
    assert(span);
    assert(span->_freeList);

    // 从span中获取batchNum个对象
    // 如果不过batchNum，有多少拿多少
    start = span->_freeList;
    end = start;
    size_t i = 0;
    size_t actualNum = 1; //_freeList中一定有一个了
    while (i < batchNum - 1 && NextObj(end) != nullptr)
    {
        end = NextObj(end);
        ++i;
        ++actualNum;  
    }
    span->_freeList = NextObj(end);
    NextObj(end) = nullptr;
    span->_useCount += actualNum;

    //测试验证+条件断点
    int ii=0;
    void* cur = start;
    while (cur)
    {
        cur = NextObj(cur);
        ++ii;
    }
    if(actualNum!=ii)
    {
        int x=0;
        assert(actualNum==ii);
    }


    _spanLists[index]._mtx.unlock();

    return actualNum;
}

// 将一定数量的对象释放到span跨度
void CentralCache::ReleaseListToSpans(void *start, size_t size)
{
    size_t index = SizeClass::Index(size);
    _spanLists[index]._mtx.lock();
    while (start)
    {
        void* next = NextObj(start);
        Span* span = PageCache::GetInstance()->MapObjectToSpan(start);
        NextObj(start) = span->_freeList;
        span->_freeList = start;
        start = next;
        span->_useCount--;

        //说明span的切分出去的所有小块内存都回来了
        //这个span就是可以再回收给page cache， pagecache 可以再尝试做前后页的合并
        if(span->_useCount == 0)
        {
            _spanLists[index].Erase(span);
            span->_freeList = nullptr;
            span->_next = nullptr;
            span->_prev = nullptr;

            //w释放span给page cache时，使用page cache的锁就可以了
            //把锁解掉
            _spanLists[index]._mtx.unlock();
            PageCache::GetInstance()->_pageMtx.lock();
            PageCache::GetInstance()->ReleaseSpanToPageCache(span);
            PageCache::GetInstance()->_pageMtx.unlock();
            _spanLists[index]._mtx.lock();
        }
    }
    



    _spanLists[index]._mtx.unlock();
}