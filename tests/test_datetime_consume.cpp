#include <iostream>
#include <chrono>
#include <ctime>

void test_time(int times)
{
	// 定义了个默认时间格式
	const std::string& format = "%Y-%m-%d %H:%M:%s";
	// 上次定义年月日时分秒是什么时候，具体时间内容是什么
	static thread_local time_t last_second = 0;
	static thread_local char cached_date_time[20] = {'0'};
	auto t1 = std::chrono::steady_clock::now();
	// 空跑循环，作为一个基准，看花了多长时间
	for(int i = 0; i < times; ++i)
	{
		
	}
	// 每次都走localtime_r看花了多长时间
	auto t2 = std::chrono::steady_clock::now();
	for(int i = 0; i < times; ++i)
	{
		struct tm tm;
		time_t now = time(0);
		localtime_r(&now, &tm);
		char buf[64];
		strftime(buf, sizeof(buf), format.c_str(), &tm);
	}
	// 优化后的（用了个数组缓存当前时间？具体可能需要仔细阅读）
	auto t3 = std::chrono::steady_clock::now();
	for (int i = 0; i < times; i++)
	{
		auto now = std::chrono::system_clock::now();
		auto duration = now.time_since_epoch();
		time_t current_second = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
		// long micros = std::chrono::duration_cast<std::chrono::microseconds>(duration % std::chrono::seconds(1)).count();
		// localtime_r和strftime的性能消耗是std::chrono::system_clock的10~15倍
		// 简单的优化是，当秒数发生变化的时候，再去执行转换，如果秒数没变，直接使用上一次的字符串
		if (current_second != last_second)
		{
			struct tm buf;
			localtime_r(&current_second, &buf);
			strftime(cached_date_time, sizeof(cached_date_time), "%Y-%m-%d %H:%M:%S", &buf);
			last_second = current_second;
		}
	}
	auto t4 = std::chrono::steady_clock::now();
	auto base = t2 - t1;
	auto diff1 = t3 - t2 - base;
	auto diff2 = t4 - t3 - base;
	
	auto base_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(base).count();
	auto diff1_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(diff1).count();
	auto diff2_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(diff2).count();
	
	std::cout << "基础耗时：" << base_ns << " ns" << std::endl;
	std::cout << "原耗时：" << diff1_ns << " ns" << std::endl;
	std::cout << "现耗时：" << diff2_ns << " ns" << std::endl;
	std::cout << "优化倍数：" << (double)diff1_ns / diff2_ns << std::endl;
}

int main(int argc, char** argv)
{
	if(argc < 2)
	{
		std::cout << "Usage: " << argv[0] << " <times>" << std::endl;
		return 0;
	}
	int times = std::atoi(argv[1]);
	test_time(times);
	return 0;
}