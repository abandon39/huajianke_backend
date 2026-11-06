#ifndef PANS_SRC_LOGGER_APPENDER_IMPL_H
#define PANS_SRC_LOGGER_APPENDER_IMPL_H

#include <atomic>
#include <cstdio>
#include <mutex>
#include <string>
#include <string_view>

#include <pans/logger/appender.h>

namespace pans
{
// 这里实现appender.h里提前写的内部类
class Appender::Impl
{
public:
	virtual ~Impl() = default;
	void append(LogLevel::Level level, std::string_view formatted_record) noexcept;
	void setLevel(LogLevel::Level level) noexcept;
	[[nodiscard]] LogLevel::Level getLevel() const noexcept;
	void flush();
	void sync();
protected:
	// 定义一些子类必须实现的方法
	virtual void writeUnlocked(std::string_view formatted_record) noexcept = 0;
	virtual void flushUnlocked() noexcept = 0;
	virtual void syncUnlocked() noexcept = 0;
	
	std::mutex m_mutex;
	
private:
	std::atomic<LogLevel::Level> m_level{LogLevel::Level::LOG_LV_DEBUG};
};
// 定义AppenderAccess类
namespace detail
{
// 控制Appender的访问权限，集中管理Appender的私有成员的访问，用来创建Appender。
// 为后面logger提供内部的Appender通道，而不是直接把底层写日志的接口给调用者。
// Appender只有日志级别配置、刷新和同步的接口，这里是面向对象的封装设计思想。
// 这就是一种很高明的封装和抽象手段，把细节全都屏蔽，只提供有限的接口和访问。
class AppenderAccess final
{
public:
	[[nodiscard]] static AppenderPtr MakeStdoutAppender();
	[[nodiscard]] static AppenderPtr MakeFileAppender(std::string file_name);
	static void Append(const AppenderPtr& appender, LogLevel::Level level, std::string_view formatted_record) noexcept;
};
}
}
#endif