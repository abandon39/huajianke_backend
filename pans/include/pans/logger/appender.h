#ifndef PANS_INCLUDE_PANS_LOGGER_APPENDER
#define PANS_INCLUDE_PANS_LOGGER_APPENDER

#include <memory> // 智能指针
#include <string>

#include <pans/export.h>
#include <pans/logger/log_level.h>

namespace pans
{

// 似乎有点奇怪，在pans::detail命名空间单独提前声明了一个AppenderAccess类。
namespace detail
{
// 负责代理内部权限，严格控制谁能访问Appender
// 采用接口和实现隔离的方式，通过Implement和接口隔离的方式来实现接口和实现的隔离，隐藏内部细节。
// 这个AppenderAccess相当于一个友元代理？（没听懂）
class AppenderAccess;
}

class PANS_API Appender final
{
public:
	// 先声明一个public权限的内部实现类，在appender_impl.h实现
	class Impl;
	~Appender() = default;
	// 删除拷贝构造、拷贝赋值，移动构造、移动赋值
	Appender(const Appender&) = delete;
	Appender& operator=(const Appender&) = delete;
	Appender(Appender&&) = delete;
	Appender& operator=(Appender&&) = delete;
	// 定义设置日志等级和获取日志等级的两个接口
	// Appender的日志等级决定的是是否要输出这条日志；在控制台上可以随便输出，写入文件可以只写入error或者fatal等级的日志，网络只传输info级别的日志等。
	void setLevel(LogLevel::Level level) noexcept;
	[[nodiscard]] LogLevel::Level getLevel() const noexcept;
	// 定义两个刷新接口，用于将日志从进程的缓冲区（进程里会打开很多“流”，比如文件流或者控制台的流等）输出到操作系统的缓冲区（操作系统的脏页）
	// flush()是系统级别的调用，会强制程序缓冲区刷新（清空程序的缓冲区，提交给OS写入内存脏页）
	void flush(); // std::endl = '\n' + std::flush();
	void sync(); // sync()是系统级别的调用，强制OS把内存脏页写到磁盘缓冲区中。
private:
	// 声明一个私有的构造函数。
	// 而且这里将要设计一个额外的受控的工厂，用户只能通过工厂来实现对函数的组装，这样可以进一步减少暴露面等。
	explicit Appender(std::unique_ptr<Impl> impl) noexcept;
	std::unique_ptr<Impl> m_impl;
	friend class detail::AppenderAccess;
};

using AppenderPtr = std::shared_ptr<Appender>;

// 创建控制台Appender和文件Appender（网络Appender，Pans库暂时不提供）
[[nodiscard]] PANS_API AppenderPtr MakeStdoutAppender();
[[nodiscard]] PANS_API AppenderPtr MakeFileAppender(std::string file_name);
}
#endif