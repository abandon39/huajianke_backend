#ifndef PANS_INCLUDE_PANS_LOGGER_LOG_H
#define PANS_INCLUDE_PANS_LOGGER_LOG_H

#include <cstddef>
#include <iosfwd> // 是一个比iostream更轻量的包，只有接口（可以学一下）
#include <string_view>

#include <pans/logger/appender.h>
#include <pans/logger/log_level.h>
#include <pans/logger/logger.h>
#include <pans/macros.h>

// pans（命名空间）存放的是模块调用者（程序员）可见（可以直接使用）的内容
// detail（命名空间）存放的是编译器可见的内容，不应该对程序员开放，只是为了让编译器能处理正确的语法，必须要处理一些编译依赖。因此虽然绝大多数东西定义在public权限下，但并不是要给程序员可见的。
namespace pans::detail
{
class PANS_API LogLine final
{
public:
	LogLine(Logger& logger, LogLevel::Level level, uint32_t line, std::string_view file_name);
	~LogLine() noexcept;
	LogLine(const LogLine&) = delete;
	LogLine& operator=(const LogLine&) = delete;
	LogLine(LogLine&&) = delete;
	LogLine& operator=(LogLine&&) = delete;
	
	// 只提供一个“流方法”？
	[[nodiscard]] std::ostream& stream() noexcept;
private:
	// 老师的习惯：struct只用来组织数据方便传参使用，里面只是一些参数；
	// 这里的区别，LogLine的Impl只是声明了一个结构体对象，而没有声明指针对象unique_ptr指向其具体实现类型。
	// 原因：其他比如Appender里面的对象只需要new一次，而这里LogLine的对象是每打印一行日志都会创建一次析构一次，每次写日志都要构造一个LogLine对象，因此尽量让这个对象在栈上分配而不是在堆上分配；如果使用unique_ptr（在堆上分配），那么前面做的那些小对象优化等就白费了。
	struct Impl;
	
	// 创建一个大小1KB的栈上数组
	static constexpr std::size_t LOG_LINE_IMPL_SIZE = 1024;
	// 这行的作用是，让1KB的栈上数组的首地址满足当前平台最高要求的对齐限制，所有的标量（int、double等）要按照对齐要求最大的那个去对齐。
	// 可以查一下alignas(std::max_align_t)【会将内存对齐，从1字节对齐→8字节对齐】
	// 1字节对齐可能会导致程序崩溃（CPU要求严格时出错）或性能显著下降（OS的内存对齐，众所周知）
	alignas(std::max_align_t) std::byte m_implStorage[LOG_LINE_IMPL_SIZE];
	
	[[nodiscard]] Impl& getImpl() noexcept;
};

// 变参函数，是处理printf格式的接口（另一种是LOG_INFO << ... << ...，用stream有关的）
// 这种是PrintLog("%d%s..", );
PANS_API void LogPrintf(Logger& logger, LogLevel::Level level, uint32_t line, std::string_view file_name, const char* format, ...);
}

// 这里定义了一个宏，似乎主要做的是对宏参数(logger, level)的求值方面的简化
// 用了一个if-else语句，似乎就是在利用logger和level做判断时，将它们最终的值取出来
// 然后做判断，这样对于多句的判断只需要进行一次logger和level表达式的求值。
#define PANS_LOG_LEVEL(logger, level)\
	if(auto pans_log_logger = (logger); !pans_log_logger){}\
	else if(const auto pans_log_level = (level); !pans_log_logger->shouldLog(pans_log_level)) {}\
	else pans::detail::LogLine(*pans_log_logger, pans_log_level, __LINE__, __FILE__).stream()	

#define PANS_LOG_DEBUG(logger) PANS_LOG_LEVEL((logger), pans::LogLevel::Level::LOG_LV_DEBUG)
#define PANS_LOG_INFO(logger) PANS_LOG_LEVEL((logger), pans::LogLevel::Level::LOG_LV_INFO)
#define PANS_LOG_WARN(logger) PANS_LOG_LEVEL((logger), pans::LogLevel::Level::LOG_LV_WARN)
#define PANS_LOG_ERROR(logger) PANS_LOG_LEVEL((logger), pans::LogLevel::Level::LOG_LV_ERROR)
#define PANS_LOG_FATAL(logger) PANS_LOG_LEVEL((logger), pans::LogLevel::Level::LOG_LV_FATAL)

// 提供一个格式化输出的接口
// C++20的VA_OPT表示如果后面有参数包的话就帮忙生成逗号？
// VA_ARGS似乎是可变参数宏的写法，是可变参数的一个参数包，负责提供额外参数
#define PANS_LOG_FMT_LEVEL(logger, level, format, ...)\
	if(auto pans_log_logger = (logger); !pans_log_logger) {}\
	else if(const auto pans_log_level = (level); !pans_log_logger->shouldLog(pans_log_level)) {}\
	else pans::detail::LogPrintf(*pans_log_logger, pans_log_level, __LINE__, __FILE__, (format) __VA_OPT__(,) __VA_ARGS__)

#define PANS_LOG_FMT_DEBUG(logger, format, ...) PANS_LOG_FMT_LEVEL((logger), pans::LogLevel::Level::LOG_LV_DEBUG, (format) __VA_OPT__(,) __VA_ARGS__)
#define PANS_LOG_FMT_INFO(logger, format, ...) PANS_LOG_FMT_LEVEL((logger), pans::LogLevel::Level::LOG_LV_INFO, (format) __VA_OPT__(,) __VA_ARGS__)
#define PANS_LOG_FMT_WARN(logger, format, ...) PANS_LOG_FMT_LEVEL((logger), pans::LogLevel::Level::LOG_LV_WARN, (format) __VA_OPT__(,) __VA_ARGS__)
#define PANS_LOG_FMT_ERROR(logger, format, ...) PANS_LOG_FMT_LEVEL((logger), pans::LogLevel::Level::LOG_LV_ERROR, (format) __VA_OPT__(,) __VA_ARGS__)
#define PANS_LOG_FMT_FATAL(logger, format, ...) PANS_LOG_FMT_LEVEL((logger), pans::LogLevel::Level::LOG_LV_FATAL, (format) __VA_OPT__(,) __VA_ARGS__)

// 写两个获取日志器的接口
#define PANS_LOG_ROOT() pans::GetRootLogger()
#define PANS_LOG_NAME(name) pans::GetLogger((name))

// 写几个隐藏参数的接口，只针对特定类型
#define LOG_DEBUG PANS_LOG_DEBUG(g_logger)
#define LOG_INFO PANS_LOG_INFO(g_logger)
#define LOG_WARN PANS_LOG_WARN(g_logger)
#define LOG_ERROR PANS_LOG_ERROR(g_logger)
#define LOG_FATAL PANS_LOG_FATAL(g_logger)

#endif