#pragma once

#include <iostream>
#include <vector>
#include <time.h>
#include <assert.h>
#include <pthread.h>
#include <unistd.h>
#include <mutex>
#include <algorithm>
#include <unordered_map>
#include<string.h>
// #include<memory>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

using std::cout;
using std::endl;

static const size_t MAX_BYTES = 256 * 1024;
static const size_t NFREE_LISTS = 208;
static const size_t NPAGES = 129;
static const size_t PAGE_SHIFT = 12;
static const size_t ONE_PAGE = 4;

#ifdef __x86_64__ // 如果是64位
typedef unsigned long long PAGE_ID;

#elif __i386__ // 如果是linux32位
typedef size_t PAGE_ID;

#elif _WIN64
typedef unsigned long long PAGE_ID;

#elif _WIN32
typedef size_t PAGE_ID;

#endif

// 直接去堆上按页申请空间
inline static void *SystemAlloc(size_t kpage)
{
#ifdef _WIN32

    // MEM_COMMIT 交给物理地址
    // PAGE_READWRITE 保护属性，读和写
    void *ptr = VirtualAlloc(0, kpage * (1 << 12), MEM_COMMIT | MEM_RESERVE,
                             PAGE_READWRITE);
#else
    // linux下brk mmap等
    void *ptr;
    // void* mmap(void* start,size_t length,int prot,int flags,int fd,off_t offset);
    //  start:0时表示由系统决定映射区的起始地址。
    //  length：映射区的长度。度单位是 以字节为单位，不足一内存页按一内存页处理
    //  prot：期望的内存保护标志 PROT_READ PROT_WRITE 可读写
    //  flags:映射选项和映射页是否可以共享 MAP_PRIVATE:私有映射 MAP_ANONYMOUS //匿名映射，映射区不与任何文件关联。
    //  fd:有效的文件描述词。一般是由open()函数返回，其值也可以设置为-1
    //  offset：被映射对象内容的起点。
    //  ptr = mmap(NULL, kpage*4*1024, PROT_READ | PROT_WRITE,
    //                  MAP_PRIVATE | MAP_ANONYMOUS | 0x40000 /*MAP_HUGETLB*/, -1, 0);
    ptr = mmap(NULL, kpage * ONE_PAGE * 1024, PROT_READ | PROT_WRITE,
               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED)
    {
        throw std::bad_alloc();
    }
    return ptr;

#endif
    if (ptr == nullptr)
        throw std::bad_alloc();
    return ptr;
}

inline static void SystemFree(void *ptr,size_t kpage=0)
{
#ifdef _WIN32
    VirtualFree(ptr, 0, MEM_RELEASE);
#else
// sbrk unmmap等
    int i = munmap(ptr,kpage* ONE_PAGE * 1024);
    if(i!=0)
    {
        std::cerr<<"释放空间错误"<<endl;
        exit(-1);
    }

#endif
}

static void *&NextObj(void *obj)
{
    return *(void **)obj;
}

// 管理切分好的小对象的自由链表
class FreeList
{
public:
    void Push(void *obj)
    {
        assert(obj);

        // 头插
        NextObj(obj) = _freeList;
        _freeList = obj;
        ++_size;
    }

    // 插入多个链表
    void PushRange(void *start, void *end, size_t n)
    {
        NextObj(end) = _freeList;
        _freeList = start;

        // 测试验证+条件断点
        // int i=0;
        // void* cur = start;
        // while (cur)
        // {
        //     cur = NextObj(cur);
        //     ++i;
        // }
        // if(n!=i)
        // {
        //     int x=0;
        //     assert(n==i);
        // }
        
        _size += n;

    }

    // 回收多个链表
    void PopRange(void *&start, void *&end, size_t n)
    {
        assert(n <= _size);
        start = _freeList;
        end = start;
        for (size_t i = 0; i < n - 1; i++)
        {
            end = NextObj(end);
        }
        _freeList = NextObj(end);
        NextObj(end) = nullptr;

        if(_size - n > 1000)
        {
            int x= 0;
        }


        _size -= n;
    }

    size_t Size()
    {
        return _size;
    }

    void *Pop()
    {
        assert(_freeList);
        // 头删
        void *obj = _freeList;
        _freeList = NextObj(obj);
        --_size;
        return obj;
    }

    bool Empty()
    {
        return _freeList == nullptr;
    }

    size_t &MaxSize()
    {
        return _maxSize;
    }

private:
    void *_freeList = nullptr;
    size_t _maxSize = 1;
    size_t _size = 0;
};

// 计算对象大小的对齐映射规则
class SizeClass
{
public:
    // 控制在10%左右的内碎片浪费
    // [1,128] 8byte对齐 freelist[0,16)
    // [129,1024] 16byte对齐 freelist[16,72)
    // [1025,8*1024] 128byte对齐 freelist[72,128)
    // [8*1024+1,64*1024] 1024byte对齐 freelist[128,184)
    // [64*1024+1,256*1024] 8*1024byte对齐 freelist[184,208)
    // 总共208个桶

    // size_t _RoundUp(size_t size, size_t alignNum)
    // {
    //     size_t alingSize;
    //     if(size % 8 !=0)
    //     {
    //         alingSize = (size/alignNum + 1)* alignNum;
    //     }
    //     else
    //     {
    //         alingSize = size;
    //     }
    // }

    static inline size_t _RoundUp(size_t bytes, size_t alignNum)
    {
        return (((bytes) + alignNum - 1) & ~(alignNum - 1));
    }

    static inline size_t RoundUp(size_t size)
    {
        if (size <= 128)
        {
            return _RoundUp(size, 8);
        }
        else if (size <= 1024)
        {
            return _RoundUp(size, 16);
        }
        else if (size <= 8 * 1024)
        {
            return _RoundUp(size, 128);
        }
        else if (size <= 64 * 1024)
        {
            return _RoundUp(size, 1024);
        }
        else if (size <= 256 * 1024)
        {
            return _RoundUp(size, 8 * 1024);
        }
        else
        {
            return _RoundUp(size, 1 << PAGE_SHIFT);
            assert(false);
            return -1;
        }
    }

    // size_t _Index(size_t bytes, size_t alignNum)
    // {
    //     if(bytes % alignNum == 0)
    //     {
    //         return bytes/alignNum -1;
    //     }
    //     else
    //     {
    //         return bytes/alignNum;
    //     }
    // }

    static inline size_t _Index(size_t bytes, size_t align_shift)
    {
        return ((bytes + (1 << align_shift) - 1) >> align_shift) - 1;
    }
    // 计算映射的哪一个自由链表桶
    static inline size_t Index(size_t bytes)
    {
        assert(bytes <= MAX_BYTES);
        // 每个区间有多少个链
        static int group_array[4] = {16, 56, 56, 56};
        if (bytes <= 128)
        {
            return _Index(bytes, 3);
        }
        else if (bytes <= 1024)
        {
            return _Index(bytes - 128, 4) + group_array[0];
        }
        else if (bytes <= 8 * 1024)
        {
            return _Index(bytes - 1024, 7) + group_array[1] + group_array[0];
        }
        else if (bytes <= 64 * 1024)
        {
            return _Index(bytes - 8 * 1024, 10) + group_array[2] + group_array[1] + group_array[0];
        }
        else if (bytes <= 256 * 1024)
        {
            return _Index(bytes - 64 * 1024, 13) + group_array[3] +
                   group_array[2] + group_array[1] + group_array[0];
        }
        else
        {
            assert(false);
        }
        return -1;
    }

    // 一次从中心缓存获取多少个
    static size_t NumMoveSize(size_t size)
    {
        assert(size > 0);
        if (size == 0)
            return 0;
        // [2, 512]，一次批量移动多少个对象的(慢启动)上限值
        // 小对象一次批量上限高
        // 小对象一次批量上限低
        int num = MAX_BYTES / size;
        if (num < 2)
            num = 2;
        if (num > 512)
            num = 512;
        return num;
    }

    // 计算一次向系统获取几个页
    // 单个对象 8byte
    // ...
    // 单个对象 256KB
    static size_t NumMovePage(size_t size)
    {
        size_t num = NumMoveSize(size);
        size_t npage = num * size;
        npage >>= PAGE_SHIFT;
        if (npage == 0)
            npage = 1;
        return npage;
    }
};

// 管理多个连续内存的页号
struct Span
{
    PAGE_ID _pageId = 0;   // 大块内存起始页的页号
    size_t _n = 0;         // 页的数量
    Span *_next = nullptr; // 双向链表结构
    Span *_prev = nullptr;

    size_t _objSize = 0;  // 切好的小对象的大小
    size_t _useCount = 0;      // 切好的内存，被分配给thread cache的数量
    void *_freeList = nullptr; // 切好的小块内存的自由链表
    bool _isUse = false;       // 是非在被使用
};

// 带头双向循环链表
class SpanList
{
public:
    SpanList()
    {
        _head = new Span;
        _head->_next = _head;
        _head->_prev = _head;
    }

    Span *Begin()
    {
        return _head->_next;
    }

    Span *End()
    {
        return _head;
    }

    bool Empty()
    {
        return _head->_next == _head;
    }

    void PushFront(Span *span)
    {
        Insert(Begin(), span);
    }

    Span *PopFront()
    {
        Span *front = _head->_next;
        Erase(front);
        return front;
    }

    void Insert(Span *pos, Span *newSpan)
    {
        assert(pos);
        assert(newSpan);
        Span *prev = pos->_prev;
        prev->_next = newSpan;
        newSpan->_prev = prev;
        newSpan->_next = pos;
        pos->_prev = newSpan;
    }

    void Erase(Span *pos)
    {
        assert(pos);
        assert(pos != _head);
        // 条件断点
        // if(pos == _head)
        // {
        //     int x = 0;
        //     assert(pos != _head);
        // }
        Span *prev = pos->_prev;
        Span *next = pos->_next;
        prev->_next = next;
        next->_prev = prev;
        // delete pos;//不用delete 要还给pagecache
    }

private:
    Span *_head;

public:
    std::mutex _mtx; // 桶锁 208个桶
};
