# 🧪 实验报告：并行编程与线程同步
## 实验背景
在现代计算机中，线程是实现并行计算和高效资源共享的基本单位。多线程编程能够充分利用多核处理器的优势，显著提高程序的执行效率。本实验的目的是通过实现一个并发的哈希表，并通过多个线程并行执行插入（`put`）和查询（`get`）操作来体验多线程编程的挑战，特别是线程同步的问题。
### 线程安全问题
哈希表是一种常见的数据结构，在多线程并发访问时，多个线程同时修改同一个哈希桶中的内容可能导致数据不一致的问题。在多线程环境下，访问同一数据结构时必须进行同步，确保数据一致性。在本实验中，我们通过引入互斥锁来保证每个哈希桶的线程安全。
## 🧩 实验目的
* 理解和实现线程的创建、切换和调度。
* 使用互斥锁实现线程同步，确保哈希表的线程安全。
* 测试并发插入和查询操作的性能，分析并行化带来的性能提升和线程同步的开销。
* 实现不同的线程数目的情况下，观察并行化性能的提升。
## 🧠 实验原理
### 1. 哈希表的线程安全
在多线程环境中，多个线程并发访问同一哈希表时，如果没有线程同步机制，可能会发生数据竞争，导致数据不一致。为了避免这种情况，在哈希表的插入（`put`）和查询（`get`）操作中，我们为每个哈希桶添加了一个互斥锁（`pthread_mutex_t`），确保每次只有一个线程能够修改哈希桶中的数据。
### 2. 线程的创建与调度
线程的创建和调度是并行编程的基础。在本实验中，我们使用了 POSIX 线程库（`pthread`）来创建和管理线程。每个线程通过 `pthread_create()` 创建，并通过 `pthread_join()` 等待线程结束。为了确保每个线程的任务能够正确完成，我们使用了互斥锁来同步对哈希表的访问。
### 3. 线程同步
为了保证哈希表在多线程并发访问时的一致性，我们使用了互斥锁来同步对哈希表的访问。具体而言，我们为每个哈希桶分配一个锁，线程在执行插入和查询操作时需要先获取相应的锁，操作完成后再释放锁。
## ⚙️ 实验步骤
### 🧩 1. 哈希表的实现
我们在 `notxv6/ph.c` 文件中实现了一个简单的哈希表。每个哈希桶使用链表来存储键值对，插入（`put`）和查询（`get`）操作分别用于将键值对插入哈希表和从哈希表中查询键值。
```c
static void put(int key, int value) {
  int i = key % NBUCKET;
  pthread_mutex_lock(&lock[i]);

  struct entry *e = 0;
  for (e = table[i]; e != 0; e = e->next) {
    if (e->key == key)
      break;
  }
  if (e) {
    e->value = value;
  } else {
    insert(key, value, &table[i], table[i]);
  }
  pthread_mutex_unlock(&lock[i]);
}

static struct entry* get(int key) {
  int i = key % NBUCKET;
  struct entry *e = 0;
  for (e = table[i]; e != 0; e = e->next) {
    if (e->key == key) break;
  }
  return e;
}
```
* **插入操作**：在 `put` 函数中，首先计算键的哈希值并获取对应哈希桶的锁。然后，遍历哈希桶中的链表查找是否已经存在相同的键。如果存在，则更新值；如果不存在，则插入新的键值对。
* **查询操作**：在 `get` 函数中，计算键的哈希值并遍历对应的哈希桶，返回找到的条目。
### 🧩 2. 线程的创建与调度
在 `main()` 函数中，我们使用 `pthread_create()` 创建多个线程，这些线程将执行插入操作。我们为每个线程分配不同的工作负载，并使用 `pthread_join()` 等待所有线程完成。
```c
static void *put_thread(void *xa) {
  int n = (int)(long)xa; // thread number
  int b = NKEYS / nthread;
  
  for (int i = 0; i < b; i++) {
    put(keys[b * n + i], n);
  }

  return NULL;
}

static void *get_thread(void *xa) {
  int n = (int)(long)xa; // thread number
  int missing = 0;

  for (int i = 0; i < NKEYS; i++) {
    struct entry *e = get(keys[i]);
    if (e == 0) missing++;
  }
  printf("%d: %d keys missing\n", n, missing);
  return NULL;
}
```
每个线程负责插入一部分键值对，插入操作后，另一个线程会从哈希表中查询这些键，检查是否成功插入。
### 🧩 3. 多线程并发访问
我们通过设置 `nthread` 参数来指定线程数量，利用 `pthread_mutex_t` 来确保每个线程在操作哈希表时的同步。并发执行插入操作时，通过锁保护每个哈希桶，避免不同线程修改同一桶时发生竞争。
```c
pthread_mutex_t lock[NBUCKET];  // 为每个桶添加一个锁
```
## 🧪 测试与验证
### 测试结果
1. **`ph_safe`**：测试通过，确保了在多个线程访问哈希表时不会出现数据丢失。
2. **`ph_fast`**：在两个线程的情况下，性能提高了约 2x，达到了预期的并行加速效果。
