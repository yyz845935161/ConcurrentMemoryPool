#pragma once
#include "Common.h"
// #ifdef _WIN32
//     #include<windows.h>
// #else
//     #include<sys/mman.h>
// #endif

// //直接去堆上按页申请空间
// inline static void *SystemAlloc(size_t kpage)
// {
// #ifdef _WIN32

//     //MEM_COMMIT 交给物理地址
//     //PAGE_READWRITE 保护属性，读和写
//     void *ptr = VirtualAlloc(0, kpage * (1 << 12), MEM_COMMIT | MEM_RESERVE,
//                              PAGE_READWRITE);
// #else
//     // linux下brk mmap等
//     void *ptr;
//     //void* mmap(void* start,size_t length,int prot,int flags,int fd,off_t offset);
//     // start:0时表示由系统决定映射区的起始地址。
//     // length：映射区的长度。度单位是 以字节为单位，不足一内存页按一内存页处理
//     // prot：期望的内存保护标志 PROT_READ PROT_WRITE 可读写
//     // flags:映射选项和映射页是否可以共享 MAP_PRIVATE:私有映射 MAP_ANONYMOUS //匿名映射，映射区不与任何文件关联。
//     // fd:有效的文件描述词。一般是由open()函数返回，其值也可以设置为-1
//     // offset：被映射对象内容的起点。
//     // ptr = mmap(NULL, kpage*4*1024, PROT_READ | PROT_WRITE,
//     //                 MAP_PRIVATE | MAP_ANONYMOUS | 0x40000 /*MAP_HUGETLB*/, -1, 0);
//     ptr = mmap(NULL, kpage*4*1024, PROT_READ | PROT_WRITE,
//                     MAP_PRIVATE | MAP_ANONYMOUS , -1, 0);
//     if(ptr == MAP_FAILED)
//     {
//         throw std::bad_alloc();
//     }
//     return ptr;

// #endif
//     if (ptr == nullptr)
//         throw std::bad_alloc();
//     return ptr;
// }

// 定长内存池
template <class T>
class ObjectPool
{
public:
    T *New()
    {
        T *obj = nullptr;
        // 优先去找还回来的内存块对象重复利用
        if (_freeList)
        {
            void *next = *((void **)_freeList);
            obj = (T *)_freeList;
            _freeList = next;
        }
        else
        {
            // 剩余内存不够一个对象大小，则重新开辟大空间
            if (_remainBytes < sizeof(T))
            {
                if(sizeof(T) < 128 * 1024)
                _remainBytes = 128 * 1024;
                else
                {
                    _remainBytes = sizeof(T);
                }
                
                size_t size = _remainBytes;
		        size_t alignSize = SizeClass::_RoundUp(size, 1<<PAGE_SHIFT);

                // _memory = (char *)malloc(128 * 1024);
                _memory = (char *)SystemAlloc(alignSize>>PAGE_SHIFT);//一页是4kb 128kb是32页
                if (_memory == nullptr)
                {
                    throw std::bad_alloc();
                }
            }

            obj = (T *)_memory;
            // 至少得存下一个指针的大小
            size_t objSize = sizeof(T) < sizeof(void *) ? sizeof(void *) : sizeof(T);

            _memory += objSize;
            _remainBytes -= objSize;

            // return obj;
        }

        // 定位new，显示调用构造函数T的构造函数初始化
        new (obj) T;

        return obj;
    }

    void Delete(T *obj)
    {
        // 显示调用析构函数
        obj->~T();
        _freeList = obj;
        //*(int*)obj = nullptr; 64位下出错
        *(void **)obj = nullptr;
    }

private:
    char *_memory = nullptr;   // 指向大块内存的指针
    void *_freeList = nullptr; // 链接还回的内存
    size_t _remainBytes = 0;   // 大块内存切分过程中剩余字节数
};

// struct TreeNode
// {
//     int _val;
//     TreeNode *_left;
//     TreeNode *_right;
//     TreeNode()
//         : _val(0), _left(nullptr), _right(nullptr)
//     {
//     }
// };
// void TestObjectPool()
// {
//     // 申请释放的轮次
//     const size_t Rounds = 3;
//     // 每轮申请释放多少次
//     const size_t N = 1000000;
//     size_t begin1 = clock();
//     std::vector<TreeNode *> v1;
//     v1.reserve(N);
//     for (size_t j = 0; j < Rounds; ++j)
//     {
//         for (int i = 0; i < N; ++i)
//         {
//             v1.push_back(new TreeNode);
//         }
//         for (int i = 0; i < N; ++i)
//         {
//             delete v1[i];
//         }
//         v1.clear();
//     }
//     size_t end1 = clock();

//     //
//     ObjectPool<TreeNode> TNPool;
//     size_t begin2 = clock();
//     std::vector<TreeNode *> v2;
//     v2.reserve(N);
//     for (size_t j = 0; j < Rounds; ++j)
//     {
//         for (int i = 0; i < N; ++i)
//         {
//             v2.push_back(TNPool.New());
//         }
//         for (int i = 0; i < N; ++i)
//         {
//             TNPool.Delete(v2[i]);
//         }
//         v2.clear();
//     }
//     size_t end2 = clock();
//     cout << "new cost time:" << end1 - begin1 << endl;
//     cout << "object pool cost time:" << end2 - begin2 << endl;
// }