
#include "include/ConcurrentAlloc.h"
#include <atomic>
void *AllocBenchmarkMalloc(void *arg)
{
    size_t *args = (size_t *)arg;
    size_t ntimes = args[0];
    size_t rounds = args[1];
    size_t *malloc_costtime = (size_t *)*(args + 2);
    size_t *free_costtime = (size_t *)*(args + 3);

    std::vector<void *> v;
    v.reserve(ntimes);
    for (size_t j = 0; j < rounds; ++j)
    {
        size_t begin1 = clock();
        for (size_t i = 0; i < ntimes; i++)
        {
            v.push_back(malloc(16));
        }
        size_t end1 = clock();
        size_t begin2 = clock();
        for (size_t i = 0; i < ntimes; i++)
        {
            free(v[i]);
        }
        size_t end2 = clock();
        v.clear();
        *malloc_costtime += end1 - begin1;
        *free_costtime += end2 - begin2;
    }
    return nullptr;
}

void *AllocBenchmarkConcurrentMalloc(void *arg)
{
    size_t *args = (size_t *)arg;
    size_t ntimes = args[0];
    size_t rounds = args[1];
    size_t *malloc_costtime = (size_t *)*(args + 2);
    size_t *free_costtime = (size_t *)*(args + 3);

    std::vector<void *> v;
    v.reserve(ntimes);
    for (size_t j = 0; j < rounds; ++j)
    {
        size_t begin1 = clock();
        for (size_t i = 0; i < ntimes; i++)
        {
            v.push_back(ConcurrentAlloc(16));
            // v.push_back(ConcurrentAlloc((16 + i) % 8192 + 1));
        }
        size_t end1 = clock();
        size_t begin2 = clock();
        for (size_t i = 0; i < ntimes; i++)
        {
            ConcurrentFree(v[i],16);
        }
        size_t end2 = clock();
        v.clear();
        *malloc_costtime += (end1 - begin1);
        *free_costtime += (end2 - begin2);
    }
    return nullptr;
}
void BenchmarkMalloc(size_t ntimes, size_t nworks, size_t rounds)
{
    // ntimes 一轮申请和释放的轮次
    // nworks 伦次
    // nworks 线程次
    std::vector<pthread_t> vthread(nworks);
    std::atomic<size_t> malloc_costtime = {0};
    std::atomic<size_t> free_costtime = {0};
    // size_t malloc_costtime=0;
    // size_t free_costtime=0;

    for (size_t k = 0; k < nworks; ++k)
    {
        size_t args[4];
        args[0] = ntimes;
        args[1] = rounds;
        args[2] = (size_t)(&malloc_costtime);
        args[3] = (size_t)(&free_costtime);
        pthread_create(&vthread[k], NULL, AllocBenchmarkMalloc, (void *)args);
    }

    for (auto &t : vthread)
    {
        pthread_join(t, NULL);
    }
    size_t _malloc_costtime = malloc_costtime;
    size_t _free_costtime = free_costtime;
    printf("%zu个线程并发执行%zu轮次，每轮次malloc %zu次: 花费：%zu ms\n",
           nworks, rounds, ntimes, _malloc_costtime);
    printf("%zu个线程并发执行%zu轮次，每轮次free %zu次: 花费：%zu ms\n",
           nworks, rounds, ntimes, _free_costtime);
    printf("%zu个线程并发malloc&free %zu次，总计花费：%zu ms\n",
           nworks, nworks * rounds * ntimes, malloc_costtime + _free_costtime);
}

// 单轮次申请释放次数 线程数 轮次
void BenchmarkConcurrentMalloc(size_t ntimes, size_t nworks, size_t rounds)
{
    std::vector<pthread_t> vthread(nworks);
    std::atomic<size_t> malloc_costtime = {0};
    std::atomic<size_t> free_costtime = {0};
    // size_t malloc_costtime=0;
    // size_t free_costtime=0;

    for (size_t k = 0; k < nworks; ++k)
    {
        size_t args[4];
        args[0] = ntimes;
        args[1] = rounds;
        args[2] = (size_t)(&malloc_costtime);
        args[3] = (size_t)(&free_costtime);
        pthread_create(&vthread[k], NULL, AllocBenchmarkConcurrentMalloc, (void *)args);
    }

    for (auto &t : vthread)
    {
        pthread_join(t, NULL);
    }
    size_t _malloc_costtime = malloc_costtime;
    size_t _free_costtime = free_costtime;
    printf("%zu个线程并发执行%zu轮次，每轮次malloc %zu次: 花费：%zu ms\n",
           nworks, rounds, ntimes, _malloc_costtime);
    printf("%zu个线程并发执行%zu轮次，每轮次free %zu次: 花费：%zu ms\n",
           nworks, rounds, ntimes, _free_costtime);
    printf("%zu个线程并发malloc&free %zu次，总计花费：%zu ms\n",
           nworks, nworks * rounds * ntimes, malloc_costtime + _free_costtime);
}

int main()
{
    cout << "==========================================================" << endl;
    // BenchmarkMalloc(10000, 4, 10);

    cout << endl
         << endl;
    BenchmarkConcurrentMalloc(10000, 1, 10);

    cout << "==========================================================" << endl;
    return 0;
}
