#include "include/ObjectPool.h"
#include "include/ConcurrentAlloc.h"
#include "include/CentralCache.h"
#include "include/PageCache.h"
#include "include/ThreadCache.h"

void* Alloc1(void *arg)
{
    for (size_t i = 0; i < 5; i++)
    {
        /* code */
        void *ptr = ConcurrentAlloc(6);
    }
}

void* Alloc2(void *arg)
{
    for (size_t i = 0; i < 5; i++)
    {
        /* code */
        void *ptr = ConcurrentAlloc(7);
    }
}

void TLSTest()
{
    pthread_t pid1, pid2;
    pthread_create(&pid1, NULL, Alloc1, NULL);
    pthread_create(&pid2, NULL, Alloc2, NULL);

    pthread_join(pid1, NULL);
    pthread_join(pid2, NULL);

}


void TestConcurrentAlloc1()
{
    void* p1 = ConcurrentAlloc(6);
    void* p2 = ConcurrentAlloc(8);
    void* p3 = ConcurrentAlloc(1);
    void* p4 = ConcurrentAlloc(7);
    void* p5 = ConcurrentAlloc(8);
    void* p6 = ConcurrentAlloc(7);
    void* p7 = ConcurrentAlloc(8);


    cout<<p1<<endl;
    cout<<p2<<endl;
    cout<<p3<<endl;
    cout<<p4<<endl;
    cout<<p5<<endl;

    ConcurrentFree(p1,6);
    ConcurrentFree(p2,8);
    ConcurrentFree(p3,1);
    ConcurrentFree(p4,7);
    ConcurrentFree(p5,8);
    ConcurrentFree(p6,7);
    ConcurrentFree(p7,8);
}


void TestConcurrentAlloc2()
{
    for(size_t i=0;i < 1024;i++)
    {
        void* p1 = ConcurrentAlloc(8);
        cout<<p1<<endl;
    }
    void* p1 = ConcurrentAlloc(8);
    cout<<p1<<endl;
}

//这样可以知道是属于哪个页
void TestAddressShift()
{
    PAGE_ID id1 = 2000;
    PAGE_ID id2 = 2001;
    char* p1 = (char*)(id1<<PAGE_SHIFT);
    char* p2 = (char*)(id2<<PAGE_SHIFT); 
    cout<<(void*)p1<<endl;
    cout<<(void*)p2<<endl;

    while(p1 < p2)
    {
        cout<< (void*)p1 <<":"<< ((PAGE_ID)p1>>PAGE_SHIFT) <<endl;
        p1+=8;
    }
}

void* MultiThreadAlloc1(void *arg)
{
    std::vector<void *>v;
    for(size_t i=0; i<7;i++)
    {
        void* ptr = ConcurrentAlloc(6);
        v.push_back(ptr);
    }

    for(auto e:v)
    {
        ConcurrentFree(e,6);
    }
}

void* MultiThreadAlloc2(void *arg)
{
    std::vector<void *>v;
    for(size_t i=0; i<7;i++)
    {
        void* ptr = ConcurrentAlloc(6);
        v.push_back(ptr);
    }

    for(auto e:v)
    {
        ConcurrentFree(e,6);
    }
}

void TestMuitiThread()
{
    pthread_t pid1, pid2;
    pthread_create(&pid1, NULL, MultiThreadAlloc1, NULL);
    


    pthread_create(&pid2, NULL, MultiThreadAlloc2, NULL);
    pthread_join(pid1, NULL);
    pthread_join(pid2, NULL);
}


void BigAlloc()
{
    void* p1 = ConcurrentAlloc(257*1024);
    ConcurrentFree(p1,257*1024);
    cout<<"finsh"<<endl;
    void* p2 = ConcurrentAlloc(129* 4 * 1024);
    ConcurrentFree(p2,129* 4 * 1024);
    cout<<"finsh"<<endl;
}




int main()
{
    // TestObjectPool();
    // TLSTest();
    // cout << sizeof(PAGE_ID)<<endl;
    // TestConcurrentAlloc2();
    // TestAddressShift();
    TestConcurrentAlloc1();
    // TestMuitiThread();
    BigAlloc();

    return 0;
} 