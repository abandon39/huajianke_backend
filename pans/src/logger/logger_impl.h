#ifndef PANS_SRC_LOGGER_LOGGER_IMPL_H
#define PANS_SRC_LOGGER_LOGGER_IMPL_H

#include <atomic>
#include <shared_mutex>
#include <vector>

#include <pans/logger/logger.h>

#include "logger/formatter.h"
#include "logger/log_record.h"

namespace pans
{
// 内部声明，外部实现的语法（在logger.h声明）
class Logger::Impl
{
public:
	explicit Impl(std::string name);
	[[nodiscard]] bool shouldLog(LogLevel::Level level) const noexcept;
	void submit(const detail::LogRecordView& record) noexcept;
	void setLevel(LogLevel::Level level) noexcept;
	[[nodiscard]] LogLevel::Level getLevel() const noexcept;
	[[nodiscard]] std::string_view getName() const noexcept; // 获取日志名字
	void setFormatter(std::shared_ptr<const detail::Formatter> formatter);
	[[nodiscard]] std::shared_ptr<const detail::Formatter> getFormatter() const noexcept;
	void addAppender(AppenderPtr appender);
	void removeAppender(const AppenderPtr& appender);
	void clearAppenders();
	
	void flush();
	void sync();
	void setRoot(LoggerPtr root) noexcept;
private:
	std::string m_name;
	std::atomic<LogLevel::Level> m_level{LogLevel::Level::LOG_LV_DEBUG};
	
	mutable std::shared_mutex m_mutex;
	std::shared_ptr<const detail::Formatter> m_formatter;
	std::vector<AppenderPtr> m_appenders;
	LoggerPtr m_root;
};
// 这里是logger.h定义的友元类，不对外公开，可以不暴露过多内容（安全是一方面，另一方面是调用者不需要考虑这里的具体实现，只需要调用logger.h写日志等即可，具体怎么实现的不需要关注，保证类的简洁，如果公开Submit等函数的话还需要公开LogRecordView等一些很底层的内容，会让类变得庞大难以阅读）
// 可以集中控制私有访问，只需要声明LoggerAccess类就行，都不需要提供这里面类的实现。
namespace detail
{
class LoggerAccess final
{
public:
	// 静态方法首字母大写以区分
	static void Submit(Logger& logger, const LogRecordView& record) noexcept;
	static void SetRoot(Logger& logger, LoggerPtr root) noexcept;
};

}
}
#endif