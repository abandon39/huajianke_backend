#include <algorithm>
#include <array>
#include <cstring>
#include <functional>
#include <memory>
#include <thread>
#include <utility>

// 如果是linux平台，还需要额外包含几个头文件用来处理线程名称等
#if defined(__linux__)
#include <pthread.h>
#include <sys/syscall.h>
#elif defined(_WIN32)

// 定义了一个特殊的宏，作用是告诉头文件，排除掉不常用且体积庞大的API声明，包括一些很古早的Windows版本兼容代码以及一些古老的网络API，还有就是一些shell相关的图形界面的冷门代码。 
// 能够大幅缩短编译时间，还能减少项目冲突
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

// 会阻止windows在全局命名空间中定义min和max两个宏，这样在使用std::min和std::max会编译报错
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#endif

#include <pans/utils/thread_utils.h>
#include <pans/utils/string_utils.h>
#include <pans/macros.h>

namespace pans
{
namespace
{
#if defined(_WIN32)
struct LocalFreeDeleter
{
	void operator()(wchar_t* value) const noexcept
	{
		if(value != nullptr)
		{
			// ::LocalFree表示全局命名空间下的Windows API的LocalFree函数，释放内存
			// 强制返回值转换成(void)避免未使用返回值警告
			(void)::LocalFree(value);
		}
	}
};
#endif

[[nodiscard]] std::string GetNativeThreadName() noexcept
{
	try
	{
#if defined(__linux__)
		std::array<char, 16> native_name{};
		if(pthread_getname_np(pthread_self(), native_name.data(), native_name.size()) != 0)
		{
			// return{}是C++11之后的列表初始化？会将std::string转成一个空字符串？
			return {};
		}
		return std::string(native_name.data());
#elif defined(_WIN32)
		PWSTR raw_name = nullptr;
		if(FAILED(::GetThreadDescription(::GetCurrentThread(), &raw_name)))
		{
			return {};
		}
		const std::unique_ptr<wchar_t,LocalFreeDeleter> native_name(raw_name);
		if(native_name == nullptr)
		{
			return {};
		}
		return StringUtils::WStringToString(std::string_view(native_name.get()));
#else
		return {}; // 既不是windows也不是linux平台，那就什么都不做
#endif
	}
	catch(...)
	{
		return {};
	}
}
void SetNativeThreadName(std::string_view name) noexcept
{
#if defined(__linux__)
	std::array<char, 16> native_name{};
	const std::size_t name_size = std::min(name.size(), native_name.size() - 1);
	std::memcpy(native_name.data(), name.data(), name_size);
	(void)pthread_setname_np(pthread_self(), native_name.data());
#elif (_WIN32)
	try
	{
		const std::wstring wide_name = StringUtils::StringToWString(name);
		if(!wide_name.empty())
		{
			// 设置windows线程名称的接口
			(void)::SetThreadDescription(::GetCurrendThread(), wide_name.c_str());
		}
	}
	catch(...)
	{}
#else
	// 把返回值处理成void，消除警告；不然前面
	(void)name;
#endif
}
constexpr std::string_view DEFAULT_THREAD_NAME = "UNKNOWN";
thread_local std::string t_thread_name = GetNativeThreadName();

} // 匿名命名空间

// 获取线程id
// static表示当前的变量是一个局部静态变量，生命周期是第一次调用时构造
// 后续再次调用时，static变量不会再被创建；C++11保证这是线程安全的。
// thread_local表示这个变量都有这个变量的副本，存储线程id。
// 这样就保证了在日志这种需要高频率打印线程id的时候，可以只进行一次昂贵的系统调用，大幅减少开销
uint64_t GetThreadId() noexcept
{
#if defined(__linux__)
	static thread_local const uint64_t THREAD_ID = static_cast<uint64_t>(::syscall(SYS_gettid));
#elif defined(_WIN32)
	static thread_local const uint64_t THREAD_ID = static_cast<uint64_t>(::GetCurrentThreadId());
#else
	static thread_local const uint64_t THREAD_ID = static_cast<uint64_t>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
#endif
	return THREAD_ID;
}
void SetThreadName(std::string name)
{
	// 传入的是非法参数，进行兜底（但是ASSERT一下）
	if(name.empty())
	{
		ASSERT_NOEFFECT2(false, "you passed empty thread name");
		std::string native_name = GetNativeThreadName();
		t_thread_name = native_name.empty() ? std::string(DEFAULT_THREAD_NAME) : std::move(native_name);
		return;
	}
	t_thread_name = std::move(name);
	SetNativeThreadName(t_thread_name);
}
std::string_view GetThreadName() noexcept
{
	return t_thread_name;
}
} // pans命名空间