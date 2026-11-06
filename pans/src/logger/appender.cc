#include "logger/appender_impl.h"
#include <cerrno>
#include <iostream>
#include <stdexcept>
#include <system_error>
#include <utility>

#include <pans/macros.h>

// 这里似乎主要是为了用sync()这个接口（把OS内核的数据搬到硬盘缓冲区）
#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

// 实现一些接口。Appender是负责“传递调用请求”，然后把请求传递给一个具体的实现。
// 暂时具体的实现还没有定义，是通过外部注入的。
namespace pans
{
// 这是一个构造函数（似乎需要额外的理解，写完日志再回来读），依赖外部具体注入的实现
// 通过外部注入到impl，然后再用std::move转移所有权
// 否则unique_ptr全局只能有一个指针指向它
Appender::Appender(std::unique_ptr<Impl> impl) noexcept
	: m_impl(std::move(impl))
{}

void Appender::setLevel(LogLevel::Level level) noexcept
{
	// 让具体实现设置等级？
	m_impl->setLevel(level);
}
LogLevel::Level Appender::getLevel() const noexcept
{
	return m_impl->getLevel();
}
// 这是暴露给外面的接口
void Appender::flush()
{
	m_impl->flush();
}
void Appender::sync()
{
	m_impl->sync();
}

void Appender::Impl::append(LogLevel::Level level, std::string_view formatted_record) noexcept
{
	// 如果等级低于Appender接收的日志等级，就不往Appender里发
	if(static_cast<uint8_t>(level) < static_cast<uint8_t>(getLevel()))
	{
		return;
	}
	try
	{
		// 第一句获取锁不是异常安全的，加锁可能会发生异常std::system_error
		std::lock_guard<std::mutex> lock(m_mutex);
		writeUnlocked(formatted_record);
		if(level == LogLevel::Level::LOG_LV_FATAL)
		{
			flushUnlocked();
		}
	}
	catch(...)
	{
		constexpr std::string_view message = "pans logger: appender operation failed.\n";
		std::fwrite(message.data(), 1, message.size(), stderr);
	}
}
void Appender::Impl::setLevel(LogLevel::Level level) noexcept
{
	m_level.store(level, std::memory_order_release);
}

LogLevel::Level Appender::Impl::getLevel() const noexcept
{
	return m_level.load(std::memory_order_acquire);
}

void Appender::Impl::flush()
{
	std::lock_guard<std::mutex> lock(m_mutex);
	flushUnlocked();
}

void Appender::Impl::sync()
{
	std::lock_guard<std::mutex> lock(m_mutex);
	syncUnlocked();
}

// 这里是Appender输出到控制台的接口
class StdoutAppenderImpl final : public Appender::Impl
{
protected:
	void writeUnlocked(std::string_view formatted_record) noexcept override
	{
		std::cout.write(formatted_record.data(), static_cast<std::streamsize>(formatted_record.size()));
	}
	void flushUnlocked() noexcept override
	{
		std::cout.flush();
	}
	// 控制台不需要sync写入硬盘，因此直接指向flush。
	void syncUnlocked() noexcept override
	{
		flushUnlocked();
	}
};

// 这里是Appender输出到文件的接口
class FileAppenderImpl final : public Appender::Impl
{
public:
	explicit FileAppenderImpl(const std::string& file_name)
	{
		if (file_name.empty())
		{
			throw std::invalid_argument("logger file name cannot be empty.");
		}
		// a是追加写入，如果文件不存在则自动创建，如果文件存在则每次在末尾追加；b是二进制。
		// 在windows下加b会
		m_file = std::fopen(file_name.c_str(), "ab");
		if(m_file == nullptr)
		{
			throw std::system_error(errno, std::generic_category(), "failed to open logger file : " + file_name);
		}
	}
	~FileAppenderImpl() override
	{
		// 析构时先写入文件再关文件句柄
		if(m_file != nullptr)
		{
			std::fflush(m_file);
			std::fclose(m_file);
		}
	}
protected:
	void writeUnlocked(std::string_view formatted_record) noexcept override
	{
		std::fwrite(formatted_record.data(), 1, formatted_record.size(), m_file);
	}
	void flushUnlocked() noexcept override
	{
		std::fflush(m_file);
	}
	
	void syncUnlocked() noexcept override
	{
#if defined (_WIN32)
		::_commit(::_fileno(m_file));
#else
		::fdatasync(::fileno(m_file));
#endif
	}
private:
	// 只有一个成员，就是文件的file指针，这里用了C风格的file指针而不是ofstream
	// 原因：在C++20标准中，ofstream没有提供标准的可移植的方法获取底层的文件描述符，导致非常难实现sync接口（C++26标准才提供）；第二是FILE*指针在调用fwrite和flush接口的时候，不会抛异常，只会通过返回值和全局的error number去报错。
	std::FILE* m_file = nullptr;
};
AppenderPtr detail::AppenderAccess::MakeStdoutAppender()
{
	return AppenderPtr(new Appender(std::make_unique<StdoutAppenderImpl>()));
}
AppenderPtr detail::AppenderAccess::MakeFileAppender(std::string file_name)
{
	return AppenderPtr(new Appender(std::make_unique<FileAppenderImpl>(file_name)));
} 

void detail::AppenderAccess::Append(const AppenderPtr& appender, LogLevel::Level level, std::string_view formatted_record) noexcept
{
	if(appender != nullptr)
	{
		appender->m_impl->append(level, formatted_record);
	}
}
AppenderPtr MakeStdoutAppender()
{
	return detail::AppenderAccess::MakeStdoutAppender();
}
AppenderPtr MakeFileAppender(std::string file_name)
{
	return detail::AppenderAccess::MakeFileAppender(std::move(file_name));
}

}