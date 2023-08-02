#pragma once
#include "Common.h"
class ThreadCache
{
public:
    //申请和释放内存对象
    void* Allocate(size_t size);
    void Deallocate(void* ptr, size_t size);

    // 从中心缓存获取对象
    void* FetchFromCentralCache(size_t index, size_t size);

        // 释放对象时，链表过长时，回收内存回到中心缓存
    void ListTooLong(FreeList& list, size_t size);

 
private:
    FreeList _freeLists[NFREE_LISTS];
};

//可以让每个线程有自己独立的线程池，数据不共享
//static 可以只在当前文件可见
static __thread ThreadCache*  pTLSThreadCache = nullptr;