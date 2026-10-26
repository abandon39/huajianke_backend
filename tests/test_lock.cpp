//首先定义了一大堆头文件
#include <atomic>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <latch>
#include <mutex>
#include <numeric>
#include <shared_mutex>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <vector>

// 定义几个全局变量
constexpr int RUNS = 7;// 跑7次取平均

uint64_t g_target = 0;// 记录跑多少次
uint64_t g_Counter = 0;// 累加计数，模拟短暂的业务
std::atomic<uint64_t> g_atomicCounter{0};// 原子类型变量
std::mutex g_mutex;// 互斥锁
std::shared_mutex g_sharedMutex;// 读写锁

uint64_t OperationsForThread(std::size_t thread_index, std::size_t thread_count)
{
	const uint64_t base = g_target / thread_count;
	const uint64_t remainder = g_target % thread_count;
	return base + (thread_index < remainder ? 1 : 0);
}

// 写了个模板函数声明，用来跑多线程，负责把传进来的operation函数体用thread_count个线程跑。
// 这里的 Operation&& 是C++11引入的转发引用（万能引用），既能接收左值也能接受右值。
// std::size_t 是 <cstddef>（std库）的一种无符号整数，32或64位，刚好用于各种计数，是std底层实现之一。
template <typename Operation>
std::chrono::nanoseconds BenchmarkThreads(std::size_t thread_count, Operation&& operation)
{
	// std::latch 是 C++20 提供的一次性线程同步计数器，构造时给一个初始计数，任何线程都可以count_down() 把计数-1或者 wait() 阻塞，计数到0后就不能再用。
	std::latch ready(static_cast<std::ptrdiff_t>(thread_count)); // ready等所有线程都准备好
	std::latch start_gate(1); // start_gate(1) 主线程的工作？
	std::latch finished(static_cast<std::ptrdiff_t>(thread_count)); // finished 等所有工作线程都执行完
	
	// 这里的逻辑需要仔细缕一缕，现在解决。
	std::vector<std::thread> threads; // 创建向量，存储线程对象
	threads.reserve(thread_count); // 预分配容量
	for(std::size_t thread_index = 0;thread_index < thread_count; ++thread_index)
	{ // 对每个子线程
		threads.emplace_back([&, thread_index]() 
		{ // lambda对象调用工作线程，这种写法是C++标准（C++标准要求线程接受一个可调用对象，这里是lambda函数对象）；也是一种经验规范（但不是标准设计模式）
			ready.count_down(); // 1线程ready
			start_gate.wait(); // 1线程同步，等待主线程发令
			operation(thread_index, OperationsForThread(thread_index, thread_count)); // 3核心的测试代码
			finished.count_down(); // 3线程测试完成
		});
	}
	ready.wait(); // 2主线程等待所有线程ready
	const auto begin = std::chrono::steady_clock::now(); // 2主线程启动计时
	start_gate.count_down(); // 2主线程允许所有线程启动
	finished.wait(); // 4主线程等待测试完成
	const auto end = std::chrono::steady_clock::now(); // 4主线程结束计时
	
	for(std::thread& thread : threads)
	{
		thread.join(); // 结束每个线程
	}
	// 返回时间（纳秒）
	return std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin);
}

//
std::chrono::nanoseconds BenchmarkMutex(std::size_t thread_count)
{
	g_Counter = 0;// 值置为0
	// 这里用了一个[](){},C++的lambda表达式，多看几遍，见 1.2.1。
	const auto elapsed = BenchmarkThreads(thread_count, [](std::size_t, uint64_t operations)
		{
			for(uint64_t index = 0; index < operations; ++index)
			{
				std::lock_guard<std::mutex> lock(g_mutex);
				++g_Counter;
			}
		});
	return elapsed;
}
std::chrono::nanoseconds BenchmarkSharedMutex(std::size_t thread_count)
{
	g_Counter = 0;
	const auto elapsed = BenchmarkThreads(thread_count, [](std::size_t, uint64_t operations) 
		{
			for(uint64_t index = 0; index < operations; ++index)
			{
				// 和Mutex的区别就是std::unique_lock和shared_mutex。
				std::unique_lock<std::shared_mutex> lock(g_sharedMutex);
				++g_Counter;
			}
		});
	return elapsed;
}
std::chrono::nanoseconds BenchmarkAtomic(std::size_t thread_count)
{
	// 没有阅读，需要仔细阅读。
	g_atomicCounter.store(0, std::memory_order_relaxed);
	const auto elapsed = BenchmarkThreads(thread_count, [](std::size_t, uint64_t operations)
		{
			for(uint64_t index = 0; index < operations; ++index)
			{
				g_atomicCounter.fetch_add(1, std::memory_order_relaxed);
			}
		});
	return elapsed;
}
std::chrono::nanoseconds BenchmarkAtomicCas(std::size_t thread_count)
{
	// 没有阅读，需要仔细阅读。
	g_atomicCounter.store(0, std::memory_order_relaxed);
	const auto elapsed = BenchmarkThreads(thread_count, [](std::size_t, uint64_t operations)
		{
			for(uint64_t index = 0; index < operations; ++index)
			{
				uint64_t expected = g_atomicCounter.load(std::memory_order_relaxed);
				while(!g_atomicCounter.compare_exchange_weak(expected, expected + 1, std::memory_order_relaxed, std::memory_order_relaxed))
				{}
			}
		});
	return elapsed;
}

// 模板函数，负责把传入的每个方法运行RUN次，然后返回
template <typename Benchmark>
double Run(Benchmark&& benchmark)
{
	long double total_nanoseconds = 0.0L;
	for(int run = 0; run < RUNS; ++run)
	{
		//
		total_nanoseconds += static_cast<long double>(benchmark().count());
	}
	return static_cast<double>(total_nanoseconds / RUNS);
}

// 打印结果的函数，输入名字和耗时，调用iostream打印出来。
void PrintResult(std::string_view name, double total_nanoseconds)
{
	const double nanoseconds_per_operation = total_nanoseconds / static_cast<double>(g_target);
	std::cout << std::left << std::setw(14) << name <<std::right
		<< total_nanoseconds << " ns \ttotal, "
		<< nanoseconds_per_operation << " ns/op\n";
}

int main(int argc, char** argv)// 一个是参数个数，一个是每个值
{
	// 加了个断言，输入的数量不够就直接截断然后说数量不够。
	if(argc != 2)
	{
		std::cerr << "Usage: ./test_lock count\n";
		return -1;
	}
	g_target = std::atoi(argv[1]);
	
	std::cout << std::fixed << std::setprecision(2);// 保留两位小数点
	std::cout << "operation: " << g_target << ", average of " << RUNS << " runs\n\n";
	const std::vector<std::size_t> thread_counts = {1,2,4,8,10};// 定义线程
	for(const std::size_t thread_count : thread_counts)// 进行测试，打印结果
	{
		std::cout << "threads: " << thread_count << '\n';
		PrintResult("mutex", Run([&]() { return BenchmarkMutex(thread_count); }));
		PrintResult("shared mutex", Run([&]() { return BenchmarkSharedMutex(thread_count); }));
		PrintResult("atomic", Run([&]() { return BenchmarkAtomic(thread_count); }));
		PrintResult("atomic CAS", Run([&]() { return BenchmarkAtomicCas(thread_count); }));
		std::cout << '\n';
	} 
}