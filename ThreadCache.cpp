#include "include/ThreadCache.h"
#include "include/CentralCache.h"

// 从central中获取
void *ThreadCache::FetchFromCentralCache(size_t index, size_t size)
{
    //...慢开始反馈调节算法
    // 1.最开始不会一次向central cache一次批量要太多，因为要了可能用不完
    // 2.如果你不断有size大小的内存需求，那么batchNum就会不断增长，直到上限
    // 3、size越大，一次向central cache要的catchNum就越小
    // 4、size越小，一次向central cache要的batchNum就越大
    size_t batchNum = std::min(_freeLists[index].MaxSize(), SizeClass::NumMoveSize(size));
    if (batchNum == _freeLists[index].MaxSize())
    {
        _freeLists[index].MaxSize() += 1;
    }

    // 获取一系列自由链表
    void *start = nullptr;
    void *end = nullptr;
    size_t actualNum = CentralCache::GetInstance()->FetchRangeObj(start, end, batchNum, size);
    assert(actualNum > 0); // 至少要一个

    // 若果只有获取到一个
    if (actualNum == 1)
    {
        assert(start == end);
        return start;
    } // 如果获取到多个
    else
    {
        _freeLists[index].PushRange(NextObj(start), end,actualNum-1);
        return start;
    }
}

// 申请空间
void *ThreadCache::Allocate(size_t size)
{
    assert(size <= MAX_BYTES);
    size_t alignSize = SizeClass::RoundUp(size);
    size_t index = SizeClass::Index(size);

    if (!_freeLists[index].Empty()) // 如果Thread桶里面有
    {
        return _freeLists[index].Pop();
    }
    else // 如果Thread桶里面没有，就调用CentralCache中的
    {
        return FetchFromCentralCache(index, alignSize);
    }
}

// 还空间
void ThreadCache::Deallocate(void *ptr, size_t size)
{
    assert(ptr);
    assert(size <= MAX_BYTES);
    //找映射的自由链表桶，对象插入进入
    size_t index = SizeClass::Index(size);
    _freeLists[index].Push(ptr);
 
    //当链表长度大于一次批量申请的内存时，就开始还一段一段list给central cache
    if (_freeLists[index].Size() >= _freeLists[index].MaxSize())
    {
        ListTooLong(_freeLists[index], size);
    }
}

// 释放对象时，链表过长时，回收内存回到中心缓存
void ThreadCache::ListTooLong(FreeList &list, size_t size)
{
    void *start = nullptr;
    void *end = nullptr;
    list.PopRange(start,end,list.MaxSize());

    CentralCache::GetInstance()->ReleaseListToSpans(start,size);

}