#pragma once

#include "Common.h"

//单例模式
class CentralCache
{
private:
    /* data */
    
    SpanList _spanLists[NFREE_LISTS];
    static CentralCache _sInst;
    CentralCache(){} //构造函数初始化
    CentralCache(const CentralCache&) =delete;


public:

    // 从中心缓存获取一定数量的对象给thread cache
    size_t FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size);

    // 从SpanList或者page cache获取一个非空的span
    Span* GetOneSpan(SpanList& list, size_t size);



    // 将一定数量的对象释放到span跨度
    void ReleaseListToSpans(void* start, size_t size);



    //获取这个单例对象
    static CentralCache* GetInstance()
    {
        return &_sInst;
    }

};
