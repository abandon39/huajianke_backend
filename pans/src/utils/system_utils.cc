#include <pans/utils/system_utils.h>

namespace pans
{
[[nodiscard]] PANS_API std::chrono::steady_clock::duration GetElapsedTime() noexcept
{
	// 定义一个进程级别可见的static局部变量
	static const auto START_TIME = std::chrono::steady_clock::now();
	// 返回的是第一次调用GetElapsedTime到现在的时间，START_TIME会在第一次调用时创建
	// 因而近似于系统启动到现在的时间，但是不那么精准因为系统启动到GetElapsedTime调用会空一小段
	return std::chrono::steady_clock::now() - START_TIME;
}
uint64_t GetFiberId() noexcept
{
	// 协程暂时没有实现，后面实现了回来补。
	return 0;
}
}