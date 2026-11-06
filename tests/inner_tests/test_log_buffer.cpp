#include <array>
#include <atomic>
#include <chrono>
#include <cstddef> // 这是为了用std::size_t
#include <cstdint> // 这是为了用uint64_t等
#include <iomanip> // 这是为了用setw
#include <iostream>
#include <sstream> // 这是为了用std::stringstream
#include <string>
#include <string_view>

#include "logger/buffer.h"
#include "logger/buffer_config.h"

constexpr int RUNS = 7;
std::uint64_t g_target = 0;
std::atomic<std::uint64_t> BENCHMARK_SINK{0}; // 收集测试数据，比如经常去缓冲区瞅一眼，产生一个不可忽略的外部副作用，避免编译器优化掉需要测试的逻辑，因为如果编译器判断这个逻辑没什么用，就会把它优化掉，编译使用的release是o2优化，似乎还是比较智能的

std::uint64_t Observe(std::string_view value) noexcept
{
	return static_cast<std::uint64_t>(value.size()) + 
			static_cast<unsigned char>(value.front()) + 
			static_cast<unsigned char>(value[value.size() / 2]) + 
			static_cast<unsigned char>(value.back());
}
std::uint64_t WriteWithSmallStreamBuffer(std::string_view message)
{
	// 构建一个自定义的inline buffer
	pans::detail::InlineBuffer<pans::detail::LOG_MESSAGE_INLINE_CAPACITY> buffer;
	// 然后再用inline buffer构造一个stream buffer
	pans::detail::SmallStreamBuffer<pans::detail::LOG_MESSAGE_INLINE_CAPACITY> stream_buffer(buffer);
	// 然后通过自定义的缓冲区来构造对象
	std::ostream stream(&stream_buffer);
	stream << message;
	return Observe(buffer.view());
}

std::uint64_t WriteWithStringStreamBuffer(std::string_view message)
{
	// 这里是和上面WriteWithSmallStreamBuffer（做了小对象优化）缓冲区的对比
	// 过去的实现是直接构建一个stringstream，由其直接负责管理空间，直接负责
	std::stringstream stream;
	stream << message;
	return Observe(stream.view());
}

template<typename Operation>
std::chrono::nanoseconds BenchmarkWrites(Operation&& operation)
{
	std::uint64_t checksum = 0;
	const auto begin = std::chrono::steady_clock::now();
	for(std::uint64_t iter = 0;iter < g_target; ++iter)
	{
		checksum += operation();
	}
	const auto end = std::chrono::steady_clock::now();
	BENCHMARK_SINK.fetch_xor(checksum, std::memory_order_relaxed);
	return std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin);
}
template<typename Benchmark>
double Run(Benchmark&& benchmark)
{
	long double total_nanoseconds = 0.0L;
	for(int run = 0; run < RUNS; ++run)
	{
		total_nanoseconds += static_cast<long double>(benchmark().count());
	}
	return static_cast<double>(total_nanoseconds / RUNS);
}

void PrintResult(std::string_view name, double total_nanoseconds)
{
	const double nanoseconds_per_operation = total_nanoseconds / static_cast<double>(g_target);
	std::cout << std::left << std::setw(30) << name << std::right 
		<< total_nanoseconds << " ns \ttotal, " 
		<< nanoseconds_per_operation << " ns/op\n";
}

int main(int argc, char** argv)
{
	if(argc != 2)
	{
		std::cerr << "Usage : ./test_log_buffer count\n";
		return 1;
	}
	g_target = std::stoull(argv[1]);
	std::cout << std::fixed << std::setprecision(2);
	std::cout << "ioperations: " << g_target << ", average of " << RUNS << " runs\n\n";
	// 测试
	constexpr std::array<std::size_t, 6> MESSAGE_SIZES = {50,100,200,500,1000,2048};
	for(const std::size_t message_size : MESSAGE_SIZES)
	{
		const std::string message(message_size, 'x');
		const double small_stream_buffer_nanoseconds = Run([&](){
			return BenchmarkWrites([&](){return WriteWithSmallStreamBuffer(message);});
		});
		const double stringstream_buffer_nanoseconds = Run([&](){
			return BenchmarkWrites([&](){return WriteWithStringStreamBuffer(message);});
		});
		std::cout << "message: " << message_size << " characters" << std::endl;
		PrintResult("SmallStreamBuffer ", small_stream_buffer_nanoseconds);
		PrintResult("std::stringstream ", stringstream_buffer_nanoseconds);
		std::cout << "speedup: " << stringstream_buffer_nanoseconds / small_stream_buffer_nanoseconds << "x\n\n";
	}
	return 0;
}