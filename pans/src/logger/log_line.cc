#include <array>
#include <cstdarg>
#include <cstdio>
#include <memory>
#include <new>
#include <ostream>
#include <vector>

#include <pans/logger/log.h>
#include <pans/utils/system_utils.h>
#include <pans/utils/thread_utils.h>

#include "logger/buffer.h"
#include "logger/buffer_config.h"
#include "logger/logger_impl.h"
#include "logger/log_record.h"

namespace pans::detail
{
// struct默认的权限就是public，因此不用额外写标识
struct LogLine::Impl final
{
	Impl(Logger& logger, LogLevel::Level level, uint32_t line, std::string_view file_name)
		: m_logger(logger)
		, m_level(level)
		, m_line(line)
		, m_fileName(file_name)
		, m_timestamp(std::chrono::system_clock::now())
		, m_elapsed(GetElapsedTime())
		, m_threadId(GetThreadId())
		, m_fiberId(GetFiberId())
		, m_threadName(GetThreadName())
		, m_streamBuffer(m_inlineBuffer)
		, m_stream(&m_streamBuffer)
	{}
	// 定义一个操作接口
	[[nodiscard]] LogRecordView getRecord() const noexcept
	{
		return {
			m_level,
			m_logger.getName(),
			m_inlineBuffer.view(),
			m_timestamp,
			m_elapsed,
			m_threadId,
			m_fiberId,
			m_threadName,
			m_fileName,
			m_line
		};
	}
	Logger& m_logger;
	LogLevel::Level m_level;
	uint32_t m_line = 0;
	std::string_view m_fileName;
	std::chrono::system_clock::time_point m_timestamp; // 时间戳
	std::chrono::steady_clock::duration m_elapsed;
	uint64_t m_threadId = 0;
	uint64_t m_fiberId = 0;
	std::string_view m_threadName;
	InlineBuffer<LOG_MESSAGE_INLINE_CAPACITY> m_inlineBuffer;
	SmallStreamBuffer<LOG_MESSAGE_INLINE_CAPACITY> m_streamBuffer; // 是用来封装InlineBuffer的，并不真正持有存储空间，只是引用。【有关的内容在buffer那一节，可能需要回看】
	std::ostream m_stream; // 提供流式输出的接口
};

LogLine::LogLine(Logger& logger, LogLevel::Level level, uint32_t line, std::string_view file_name)
{
	static_assert(sizeof(Impl) <= LOG_LINE_IMPL_SIZE, "LogLine inline implementation storage is too small");
	static_assert(alignof(Impl) <= alignof(std::max_align_t), "Never");
	// construct_at是用来在战术组m_implStorage上构造对象Impl，调用构造函数
	// 面试很可能会问placement new类似的问题，如何在特定位置上构造函数。
	std::construct_at(reinterpret_cast<Impl*>(m_implStorage), logger, level, line, file_name);
}

LogLine::Impl& LogLine::getImpl() noexcept
{
	// std::launder用来安全的返回一个指向一个位于已有地址上的一个指针，避免指针乱飞。
	return *std::launder(reinterpret_cast<Impl*>(m_implStorage));
}

LogLine::~LogLine() noexcept
{
	Impl& impl = getImpl();
	LoggerAccess::Submit(impl.m_logger, impl.getRecord());
	std::destroy_at(&impl);
}

std::ostream& LogLine::stream() noexcept
{
	return getImpl().m_stream;
}

void LogPrintf(Logger& logger, LogLevel::Level level, uint32_t line, std::string_view file_name, const char* format, ...)
{
	LogLine log_line(logger, level, line, file_name);
	if(format == nullptr)
	{
		log_line.stream() << "<null-format>";
		return;
	}
	std::array<char, PRINTF_FORMAT_INLINE_CAPACITY> inline_buffer;
	// 这里的va_list是一个结构体，似乎负责用寄存器硬件来处理可变参数的函数【具体没太听懂】
	// 这里似乎做的工作是，可变参数读完了后，保留的寄存器也会被破坏掉
	// 为了让写日志失败时，需要再次读到arguments，需要将可变参数拷贝下来。
	// 思路是先构造一个LogLine对象，把可变参数的数据捕获起来；
	// 然后借助vsnprintf进行格式化。
	// 接着根据可变参数的内容是否足够放到栈上进行不同的处理，类似小对象优化
	// 最终执行完后，调用析构函数时，才会调用Submit将其刷到Logger里。
	va_list arguments;
	va_list arguments_copy;
	va_start(arguments, format);
	va_copy(arguments_copy, arguments);
	const int required_size = std::vsnprintf(inline_buffer.data(), inline_buffer.size(), format, arguments);
	va_end(arguments);
	if(required_size < 0)
	{
		va_end(arguments_copy);
		log_line.stream() << "<format-error>";
		return;
	}
	const std::size_t message_size = static_cast<std::size_t>(required_size);
	if(message_size < inline_buffer.size())
	{
		va_end(arguments_copy);
		log_line.stream().write(inline_buffer.data(), static_cast<std::streamsize>(message_size));
		return;
	}
	std::vector<char> overflow_buffer(message_size + 1);
	const int second_result = std::vsnprintf(overflow_buffer.data(), overflow_buffer.size(), format, arguments_copy);
	va_end(arguments_copy);
	if(second_result < 0)
	{
		log_line.stream() << "<format-error>";
		return;
	}
	log_line.stream().write(overflow_buffer.data(), static_cast<std::streamsize>(message_size));
}


}