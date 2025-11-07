# 🧪 实验报告：线程同步与屏障实现
## 实验背景
在并行编程中，线程间的同步是保证正确性的一个重要问题。一个常见的同步机制是**屏障**（Barrier），它要求一组线程在同一个点上等待，直到所有线程都达到该点后再继续执行。在本实验中，我们将实现一个线程同步的屏障，使用条件变量来协调多个线程的执行，确保每个线程都能在同一个点上暂停，直到所有线程都到达该点。通过实现这个屏障机制，我们可以更好地理解线程的同步和并行执行。
## 🧩 实验目的
* 实现一个同步屏障（Barrier），使得多个线程能够在某个点上同步等待。
* 使用 `pthread_cond_wait()` 和 `pthread_cond_broadcast()` 等条件变量机制实现线程间的同步。
* 解决线程间竞态条件，确保在屏障处没有线程提前跳出。
* 测试不同线程数下屏障同步的正确性和性能。
## 🧠 实验原理
### 1. 屏障（Barrier）同步机制
在并行计算中，屏障是一个用于同步多线程的机制。所有线程都会到达屏障并等待，直到所有线程都到达屏障为止。这可以确保在某个点之前的计算完成后，所有线程都开始执行下一步操作。在本实验中，我们通过使用互斥锁和条件变量来实现这个屏障。
### 2. 使用条件变量和互斥锁
在本实验中，使用了 `pthread_mutex_t` 和 `pthread_cond_t` 来实现线程的同步机制。具体步骤如下：
* **互斥锁（`pthread_mutex_t`）**：每个线程在到达屏障时需要先获取锁，确保同步的正确性。
* **条件变量（`pthread_cond_t`）**：线程通过 `pthread_cond_wait()` 进入等待状态，直到其他线程也到达屏障并通过 `pthread_cond_broadcast()` 唤醒所有线程。
### 3. 屏障的实现步骤
* 每个线程都会在某个时刻调用 `barrier()`，当线程到达屏障时，它们会被阻塞，直到所有线程都到达屏障。
* 在每次屏障达到后，`round` 和 `flag` 会更新，以便下一轮的屏障能够继续工作。
* 线程会被唤醒后，继续执行接下来的任务。
## ⚙️ 实验步骤
### 🧩 1. 屏障初始化
首先，在 `barrier_init()` 函数中，我们初始化了互斥锁和条件变量：
```c
static void barrier_init(void) {
  assert(pthread_mutex_init(&bstate.barrier_mutex, NULL) == 0);
  assert(pthread_cond_init(&bstate.barrier_cond, NULL) == 0);
  bstate.nthread = 0;
  bstate.round = 0;
  bstate.flag = 0;
}
```
### 🧩 2. 屏障等待
在 `barrier()` 函数中，我们实现了线程的同步等待：
```c
static void barrier() {
  pthread_mutex_lock(&bstate.barrier_mutex);
  thread_flag = !thread_flag;

  while (thread_flag == bstate.flag) {
    pthread_cond_wait(&bstate.barrier_cond, &bstate.barrier_mutex);
  }

  int arrived = ++bstate.nthread;
  if (arrived == nthread) {
    // I am the last thread in this round
    bstate.round++;
    bstate.flag = !bstate.flag;
    bstate.nthread = 0;
    pthread_cond_broadcast(&bstate.barrier_cond);
  } else {
    pthread_cond_wait(&bstate.barrier_cond, &bstate.barrier_mutex);
  }
  pthread_mutex_unlock(&bstate.barrier_mutex);
}
```