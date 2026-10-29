// 头文件，用一个宏变量+ifndef，保证头文件在被多次包含的时候，只有一份实现。
// 这个是C++的标准，而#pragma once不是语言标准。
#ifndef PANS_INCLUDE_PANS_MUTEX_H
#define PANS_INCLUDE_PANS_MUTEX_H

#include <atomic>
#include <mutex>

// 根据不同的系统架构调用不同的库
#if defined(__x86_64__) || defined(__i386__)
#include <immintrin.h>
#elif defined(__aarch64__) || defined(__arm__)
#include <arm_acle.h>
#endif

#if defined(__linux__) && defined(__GLIBC__)
#include <pthread.h>
#endif

//
inline void cpu_relax() noexcept
{
#if defined(__x86_64__) || defined(__i386__)
	_mm_pause(); // 这个是x86平台的一条特殊指令，用于告诉CPU这个进程在忙等而不是在进行普通计算；可能可以改变调度，降低对CPU超线程资源的干扰；降低部分处理器退出循环时的流水线开销。
#elif defined(__aarch64__) || defined(__arm__)
	__yield(); // arm平台的类似上面的语句。
#endif
}

namespace pans{
// 只在Linux以及glibc的环境下使用
#if defined(__linux__) && defined(__GLIBC__) 
class Spinlock{
public:
	// noexcept是承诺不报异常，让编译器不用准备那些捕获异常的代码，以最快速度运行
	Spinlock() noexcept
	{
		pthread_spin_init(&m_mutex, PTHREAD_PROCESS_PRIVATE);
	}
	~Spinlock() noexcept
	{
		pthread_spin_destroy(&m_mutex);
	}
	
	Spinlock(const Spinlock&) = delete;
	Spinlock& operator=(const Spinlock&) = delete;
	
	void lock() noexcept
	{
		pthread_spin_lock(&m_mutex);
	}
	void unlock() noexcept
	{
		pthread_spin_unlock(&m_mutex);
	}
private:
	// 是linux平台，可以用pthread_spinlock_t（linux接口），性能比较优越且不会抛异常
	pthread_spinlock_t m_mutex{};
};
#else
// 不是linux平台，使用标准的atomic_flag实现
class Spinlock
{
public:
	using Lock = std::lock_guard<Spinlock>;
	
	Spinlock() noexcept = default;
	~Spinlock() noexcept = default;
	
	Spinlock(const Spinlock&) = delete;
	Spinlock& operator=(const Spinlock&) = delete;
	
	// 2在上锁的时候，调用test_and_set，会将atomic_flag设置为true
	void lock() noexcept
	{
		// 调用test_and_set，尝试抢锁，会将m_mutex的值设为true，然后返回原来的m_mutex值
		// 第一个线程会占用锁，即返回的是false，说明抢锁成功，但返回false不进入内层循环
		// 后续线程返回的是true，此时说明锁已经被抢了，因此进入内层的忙等循环。
		while (m_mutex.test_and_set(std::memory_order_acquire))
		{
			// 忙等循环。
			// 如果返回的是true，说明锁还在被抢，开始忙等。
			// 如果返回的是false，说明锁被释放了，退出外层循环重新判定。
			while (m_mutex.test(std::memory_order_relaxed))
			{
				// 是x86平台专门为自旋等待而设计的一条指令，忙等但降低CPU占用
				// 是一个很核心很重要的指令，尤其是在手游，自旋锁是手机发烫的罪魁祸首之一。
				cpu_relax();
			}
		}
	}
	[[nodiscard]] bool try_lock() noexcept
	{
		return !m_mutex.test_and_set(std::memory_order_acquire);
	}
	void unlock() noexcept
	{
		m_mutex.clear(std::memory_order_release);
	}
private:
	// 1定义了一个atomic_flag的原子变量
	std::atomic_flag m_mutex = ATOMIC_FLAG_INIT;
};
#endif
}

#endif