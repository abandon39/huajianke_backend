#ifndef PANS_INCLUDE_PANS_LOGGER_LOGGER_H
#define PANS_INCLUDE_PANS_LOGGER_LOGGER_H

#include <memory>
#include <string>
#include <string_view>

#include <pans/export.h>
#include <pans/logger/appender.h>
#include <pans/logger/log_level.h>

namespace pans
{
// 定义一个LoggerAccess
namespace detail
{
// 在detail里声明一个“桥接类”LoggerAccess，用来代理Logger的访问权限。
class LoggerAccess;
}

// 同样用接口-实现分离的定义方式，声明一个Impl内部类负责具体实现Logger的逻辑
// 然后在Logger中只放一些接口，具体实现放到Impl中。
class PANS_API Logger final
{
public:
	class Impl;
	explicit Logger(std::string name = "root"); // explicit拒绝隐式转换
	~Logger();
	// Logger不允许发生赋值或者拷贝发生控制权的转移，因此删除掉有关默认的函数。
	Logger(const Logger&) = delete;
	Logger& operator=(const Logger&) = delete;
	Logger(Logger&&) = delete;
	Logger& operator=(Logger&&) = delete;
	
	// 判断要不要构造日志
	[[nodiscard]] bool shouldLog(LogLevel::Level level) const noexcept;
	void setLevel(LogLevel::Level level) noexcept;
	[[nodiscard]] LogLevel::Level getLevel() const noexcept;
	[[nodiscard]] std::string_view getName() const noexcept;
	// 不同的日志可以有不同的Formatter设置不同的格式等，比如控制台就可以输出的全一些，而文件可能更为正式，明显是调试的内容就不需要计入文件；网络里的在另一台机器上进行运算，可能不需要关心很多内容比如协程号线程号等。
	void setFormatter(std::string_view pattern);
	[[nodiscard]] std::string getFormatterPattern() const;
	
	// 日志器CURD
	void addAppender(AppenderPtr appender);
	void removeAppender(const AppenderPtr& appender);
	void clearppenders();
	
	// 日志器刷新
	void flush();
	void sync();
private:
	std::unique_ptr<Impl> m_impl;
	friend class detail::LoggerAccess;
};
using LoggerPtr = std::shared_ptr<Logger>;
[[nodiscard]] PANS_API LoggerPtr GetRootLogger();
[[nodiscard]] PANS_API LoggerPtr GetLogger(std::string_view name);
}
#endif