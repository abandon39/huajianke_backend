#include "logger/logger_impl.h"

#include <algorithm>
#include <cstdio>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <utility>

#include <pans/macros.h>
#include "logger/appender_impl.h"
#include "logger/buffer.h"

namespace pans
{
// 定义一个logger_format，把支持的格式全都放进去
constexpr std::string_view DEFAULT_LOG_PATTERN = "%d{%Y-%m-%d %H:%M:%S}.%u%Tthread-%t%Tfiber=%F%T[%p]%T%f:%l%T%m%n"
Logger::Logger(std::string name)
	: m_impl(std::make_unique<Impl>(std::move(name)))
{}
Logger::~Logger() = default;

bool Logger::shouldLog(LogLevel::Level level) const noexcept
{
	return m_impl->shouldLog(level);
}

void Logger::setLevel(LogLevel::Level level) noexcept
{
	m_impl->setLevel(level);
}

LogLevel::Level Logger::getLevel() const noexcept
{
	return m_impl->getLevel();
}

std::string_view Logger::getName() const noexcept
{
	return m_impl->getName();
}

void Logger::setFormatter(std::string_view pattern)
{
	auto formatter = std::make_shared<const detail::Formatter>(pattern);
	ASSERT_RETNONE2(formatter->isValid(), "invalid logger format pattern");
	m_impl->setFormatter(std::move(formatter));
}

std::string Logger::getFormatterPattern() const
{
	const auto formatter = m_impl->getFormatter();
	return formatter == nullptr ? std::string() : formatter->getPattern();
}

void addAppender(AppenderPtr appender)
{
	m_impl->addAppender(appender);
}
void removeAppender(const AppenderPtr& appender)
{
	m_impl->removeAppender(appender);
}
void clearAppenders()
{
	m_impl->clearAppenders();
}
void flush()
{
	m_impl->flush();
}
void sync()
{
	m_impl->sync();
}
Logger::Impl::Impl(std::string name)
	: m_Name(std::move(name))
{
	if(m_name.empty())
	{
		throw std::invalid_argument("logger name should not be empty.");
	}
	m_formatter = std::make_shared<const detail::Formatter>(DEFAULT_LOG_PATTERN);
}

// 判断是否需要打日志
bool Logger::Impl::shouldLog(LogLevel::Level level) const noexcept
{
	if(static_cast<uint8_t>(level) < static_cast<uint8_t>(getLevel()))
	{
		return false;
	}
	// 判断有没有读取日志
	{
		std::shared_lock<std::shared_mutex> lock(m_mutex);
		if(!m_appenders.empty())
		{
			return true;
		}
	}
	// 如果这个日志器没有Appender就交给主日志器完成。
	return m_root != nullptr && m_root->shouldLog(level);
}
// submit负责把内存里已经构造好的日志交到Appender里去，由Appender输出给各个终端。
void Logger::Impl::submit(const detail::LogRecordView& record) noexcept
{
	if(static_cast<uint8_t>(record.m_level) < static_cast<uint8_t>(getLevel()))
	{
		return;
	}
	std::shared_lock<std::shared_mutex> lock(m_mutex);
	if(m_appenders.empty())
	{
		lock.unlock();
		// 如果主日志器不为空，就交给主日志器
		if(m_root != nullptr)
		{
			m_root->m_impl->submit(record);
		}
		return;
	}
	try
	{
		detail::FormattedRecordBuffer formatted_record;
		m_formatter->format(record, formatted_record);
		for(const AppenderPtr& appender : m_appenders)
		{
			detail::AppenderAccess::Append(appender, record.m_level, formatted_record.view());
		}
	}
	catch(...) // 内存一点都不剩了会抛std::bad_alloc异常
	{
		constexpr std::string_view message = "pans logger: record formatting failed.\n";
		std::fwrite(message.data(), 1, message.size(), stderr);
	}
}
void Logger::Impl::setLevel(LogLevel::Level level) noexcept
{
	m_level.store(level, std::memory_order_release);
}

LogLevel::Level Logger::Impl::getLevel() const noexcept
{
	return m_level.load(std::memory_order_acquire);
}

std::string_view Logger::Impl::getName() const noexcept
{
	return m_name;
}

void Logger::Impl::setFormatter(std::shared_ptr<const detail::Formatter> formatter)
{
	ASSERT_RETNONE2(formatter != nullptr, "logger formatter cannot be null.");
	std::unique_lock<std::shared_mutex> lock(m_mutex);
	m_formatter = std::move(formatter);
}
std::shared_ptr<const detail::Formatter> Logger::Impl::getFormatter() const noexcept
{
	std::shared_lock<std::shared_mutex> lock(m_mutex);
	return m_formatter;
}

void Logger::Impl::addAppender(AppenderPtr appender)
{
	ASSERT_RETNONE2(appender != nullptr, "logger appender cannot be null");
	std::unique_lock<std::shared_mutex> lock(m_mutex);
	m_appender.push_back(std::move(appender));
}

void Logger::Impl::removeAppender(const AppenderPtr& appender)
{
	std::unique_lock<std::shared_mutex> lock(m_mutex);
	std::erase(m_appenders, appender);
}

void Logger::Impl::clearAppenders()
{
	std::unique_lock<std::shared_mutex> lock(m_mutex);
	m_appenders.clear();
}

void Logger::Impl::flush()
{
	std::shared_lock<std::shared_mutex> lock(m_mutex);
	if(m_appenders.empty())
	{
		lock.unlock();
		if(m_root != nullptr)
		{
			m_root->flush();
		}
		return;
	}
	for(const AppenderPtr& appender : m_appenders)
	{
		appender->flush();
	}
}

void Logger::Impl::sync()
{
	std::shared_lock<std::shared_mutex> lock(m_mutex);
	if(m_appenders.empty())
	{
		lock.unlock();
		if(m_root != nullptr)
		{
			m_root->sync();
		}
		return;
	}
	for(const AppenderPtr& appender : m_appenders)
	{
		appender->sync();
	}
}

// 设置主日志器的入口
void Logger::Impl::setRoot(LoggerPtr root) noexcept
{
	m_root = root;
}
void detail::LoggerAccess::Submit(Logger& logger, const LogRecordView& record) noexcept
{
	logger.m_impl->submit(record);
}
void detail::LoggerAccess::SetRoot(Logger& logger, LoggerPtr& root) noexcept
{
	logger.m_impl->SetRoot(root);
}

// 定义logManager
class LoggerManager final
{
public:
	LoggerManager()
		: m_root(std::make_shared<Logger>("root"))
	{
		m_root->addAppender(MakeStdoutAppender());
		m_loggers.emplace("root", m_root);
	}
	[[nodiscard]] LoggerPtr getRoot() const noexcept
	{
		return m_root;
	}
	[[nodiscard]] LoggerPtr getLogger(std::string_view name)
	{
		ASSERT_RETVAL2(!name.empty(), nullptr, "logger name cannot be empty.");
		std::lock_guard<std::mutex> lock(m_mutex);
		// 根据名字如果没有找到日志器就创建一个对应的日志器
		const auto it = m_loggers.find(std::string(name));
		if(it != m_loggers.end())
		{
			return it->second;
		}
		auto logger = std::make_shared<Logger>(std::string(name));
		detail::LoggerAccess::SetRoot(*logger, m_root);
		m_loggers.emplace(logger->getName(), logger);
		return logger;
	}
private:
	mutable std::mutex m_mutex;
	std::unordered_map<std::string, LoggerPtr> m_loggers;
	LoggerPtr m_root;
};

// 实现一个单例函数，在函数内设置一个静态对象，这个实例会在函数第一次调用时被创建
// 实例的生命周期一直持续到进程结束，且C++11后能保证这个操作一定是原子性的，不可分割。
// 是线程安全的，比new要好。【下节会将单例，讲为什么要用这个而不用new】
LoggerManager& GetLoggerManager()
{
	static LoggerManager mgr;
	return mgr;
}

LoggerPtr GetRootLogger()
{
	return GetLoggerManager().getRoot();
}

LoggerPtr GetLogger(std::string_view name)
{
	return GetLoggerManager().getLogger(name);
}

}