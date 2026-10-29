#include <pans/mutex.h>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <latch>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

constexpr int RUNS = 7; // 每次跑7遍
uint64_t g_Counter = 0; // 计数的
std::uint64_t g_target = 0; // 要跑多少次，100w或者1000w

// 然后是spinlock，这里是前面x86的部分，在h头文件中因为当前系统是ubuntu
// 会直接被OS干掉不编译，因此额外写这段方便测试
class Spinlock
{
public:
	using Lock = std::lock_guard<Spinlock>;
	
	Spinlock() noexcept = default;
	~Spinlock() noexcept = default;
	
	Spinlock(const Spinlock&) = delete;
	Spinlock& operator=(const Spinlock&) = delete;
	
	void lock() noexcept
	{
		while (m_mutex.test_and_set(std::memory_order_acquire))
		{
			while (m_mutex.test(std::memory_order_relaxed))
			{
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
	std::atomic_flag m_mutex = ATOMIC_FLAG_INIT;
};

std::uint64_t OperationsForThread(std::size_t thread_index, std::size_t thread_count)
{
	const std::uint64_t base = g_target / thread_count;
	const std::uint64_t remainder = g_target % thread_count;
	return base + (thread_index < remainder ? 1 : 0);
}

template <typename Operation>
std::chrono::nanoseconds BenchmarkThreads(std::size_t thread_count, Operation&& operation)
{
	std::latch ready(static_cast<std::ptrdiff_t>(thread_count));
	std::latch start_gate(1);
	std::latch finished(static_cast<std::ptrdiff_t>(thread_count));
	
	std::vector<std::thread> threads;
	threads.reserve(thread_count);
	for (std::size_t thread_index = 0; thread_index < thread_count; ++thread_index)
	{
		threads.emplace_back([&, thread_index]()
		{
			ready.count_down();
			start_gate.wait();
			operation(thread_index, OperationsForThread(thread_index,thread_count));
			finished.count_down();
		});
	}
	ready.wait();
	const auto begin = std::chrono::steady_clock::now();
	start_gate.count_down();
	finished.wait();
	const auto end = std::chrono::steady_clock::now();
	
	for (std::thread& thread : threads)
	{
		thread.join();
	}
	return std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin);
}

// mutex
template <typename Mutex>
std::chrono::nanoseconds BenchmarkMutex(std::size_t thread_count)
{
	Mutex mutex;
	g_Counter = 0;
	const auto elapsed = BenchmarkThreads(thread_count, [&mutex](std::size_t, uint64_t operations)
	{
		for(uint64_t index = 0; index < operations; ++index)
		{
			std::lock_guard<Mutex> lock(mutex);
			++g_Counter;
		}
	});
	if (g_Counter != g_target)
	{
		throw std::runtime_error("mutex correctness check failed");
	}
	return elapsed;
}

// 用一个函数模板来跑benchmark。
template<typename Benchmark>
double Run(Benchmark&& benchmark)
{
	long double total_nanoseconds = 0.0L;
	for (int run = 0; run < RUNS; ++run)
	{
		// 这里会返回一个chrono对象，可以取count
		total_nanoseconds += static_cast<long double>(benchmark().count());
	}
	return static_cast<double>(total_nanoseconds / RUNS);
}

// 就是线程安全部分的PrintResult。
void PrintResult(std::string_view name, double total_nanoseconds)
{
	const double nanoseconds_per_operation = total_nanoseconds / static_cast<double>(g_target);
	std::cout << std::left << std::setw(30) << name << std::right 
		<< total_nanoseconds << " ns \ttotal, " 
		<< nanoseconds_per_operation << " ns/op\n";
}

int main(int argc, char** argv)
{
	// 这里有两个参数，如果传参数量不对就直接结束
	if (argc != 2)
	{
		std::cerr << "Usage: ./test_mutex count\n";
		return EXIT_FAILURE;
	}
	// 将第二个参数变为目标次数
	g_target = std::stoull(argv[1]);
	
	std::cout << std::fixed << std::setprecision(2);
	std::cout << "operations: " << g_target << ", average of " << RUNS << " runs\n\n";
	
	// 测试1、2、4、8、10个线程
	const std::vector<std::size_t> thread_counts = {1,2,4,8,10};
	// 范围for循环，遍历数组的每个数量的线程
	for (const std::size_t thread_count : thread_counts)
	{
		std::cout << "threads: " << thread_count << '\n';
		// 分别跑linux、x86、mutex的测试。
		PrintResult("pthread_spinlock::Spinlock", Run([&](){ return BenchmarkMutex<pans::Spinlock>(thread_count);}));
		PrintResult("atomic_flag::Spinlock", Run([&](){ return BenchmarkMutex<Spinlock>(thread_count);}));
		PrintResult("std::mutex", Run([&](){ return BenchmarkMutex<std::mutex>(thread_count);}));
		std::cout << '\n';
	}
	return EXIT_SUCCESS;
}