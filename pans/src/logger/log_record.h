#ifndef PANS_SRC_LOGGER_LOG_RECORD_H
#define PANS_SRC_LOGGER_LOG_RECORD_H

#include <chrono>
#include <cstdint>
// #include <source_location> // C++20 标准用来标识代码位置的，在编译期完成。【删了】
#include <string_view>

#include <pans/logger/log_level.h>
#include <pans/macros.h>

namespace pans::detail
{
// 定义日志事件所需要的数据有哪些，但是没有写数据的组织顺序（需要再写一个formatter）
struct LogRecordView
{
	LogLevel::Level m_level = LogLevel::Level::LOG_LV_DEBUG;
	std::string_view m_loggerName = ""; // 日志器的名字
	std::string_view m_message = ""; // 日志内容
	std::chrono::system_clock::time_point m_timestamp; // 时间戳
	std::chrono::steady_clock::duration m_elapsed; // 起服到现在已经经过的时间
	uint64_t m_threadId = 0; // 线程id
	uint64_t m_fiberId = 0; // 协程id
	std::string_view m_threadName = ""; // 线程名
	std::string_view m_fileName = ""; // 文件名
	// std::string_view m_functionName = ""; // 函数名
	// std::source_location m_location; // C++20 标准，用来表达文件位置，编译期由编译器自动完成，运行期间0消耗。【删除的原因是这里的位置是绝对路径，会暴露部署位置，且函数名会提供完整签名，因此也删除，重新定义一个行号即可】
	uint32_t m_line = 0; // 行号
};
}
#endif