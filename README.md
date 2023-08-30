# ConcurrentMemoryPool
c++实现高并发内存是linux版

## 内存池结构
三层缓存结构
![image](https://github.com/yyz845935161/ConcurrentMemoryPool/assets/51375977/b105df2e-fe1b-4e79-959b-988ecbeb8062)


### 一、ThreadCache
1. ThreadCache为每一个线程独有，不存在竞争资源问题，不需要加锁
2. 当内存申请size<=256KB时在thread cache中申请内存，计算size在自由链表中的位置，如果自由链表中有内存对象时，直接从FistList[i]中Pop一下对象，时间复杂度是O(1)。
3. 当FreeList[i]中没有对象时，则批量从central cache中获取一定数量的对象，插入到自由链表并返回一个对象。
![image](https://github.com/yyz845935161/ConcurrentMemoryPool/assets/51375977/9d432b93-466b-450d-95ba-42977f9ca84f)

内存对齐规则
![image](https://github.com/yyz845935161/ConcurrentMemoryPool/assets/51375977/3e0b0185-44a3-4418-bcbe-5a4087e243e2)


### 二、CentralCache
1.当thread cache中没有内存时，就会 "批量" 向central cache申请一些内存对象，central cache也有一个哈希映射的freelist(与ThreadCache哈希键值一一对应)，freelist中挂着span，从span中取出对象给thread cache，这个过程是需要加锁的（**桶锁**）。
2.central cache中没有非空的span时，则将空的span链在一起，向page cache申请一个span对象，span对象中是一些以页为单位的内存，切成需要的内存大小，并链接起来，挂到span中。
![image](https://github.com/yyz845935161/ConcurrentMemoryPool/assets/51375977/716e42e6-46f4-4061-8df1-e1b4844ae930)


### 三、PageCache 
1. 当central cache向page cache申请内存时，page cache先检查对应位置有没有span，如果没有则向更大页寻找一个span，如果找到则分裂成两个。比如：申请的是4page，4page后面没有挂span，则向后面寻找更大的span，假设在10page位置找到一个span，则将10page span分裂为一个4page span和一个6page span。
2. 当释放内存小于64k时将内存释放回thread cache，计算size在自由链表中的位置，将对象Push到FreeList[i].
3. 当链表的长度过长，则回收一部分内存对象到central cache。(这是为了最后解决外碎片的问题)
![image](https://github.com/yyz845935161/ConcurrentMemoryPool/assets/51375977/f62fdc47-e10b-4526-ae70-9b3a0865d1a0)

## Linux版本与Windows版本区别
1. Linux申请和释放空间使用函数不同，参考include/Common.h
2. 本次开发使用的linux系统页面大小为4kb，不是8kb，因此static const size_t PAGE_SHIFT = 12，2^12 = 4kb
3. linux下地址空间为64位
4. linux64为地址空间下，下基数树必须使用三层，参考代码(include/PageMap.hpp)


内存池原理参考：https://blog.csdn.net/RNGWGzZs/article/details/128329729

