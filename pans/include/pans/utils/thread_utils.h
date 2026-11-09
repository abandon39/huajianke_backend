#ifndef PANS_INCLUDE_PANS_UTILS_THREAD_UTILS_H
#define PANS_INCLUDE_PANS_UTILS_THREAD_UTILS_H

#include <string>
#include <string_view>

#include <pans/export.h>
#include <pans/macros.h>

namespace pans
{
// 线程id是伴随整个线程运行生命周期不会变的属性。
// 在linux平台下就是top提供的id，windows平台返回的也是id，在其他平台下返回std::thread的哈希
[[nodiscard]] PANS_API uint64_t GetThreadId() noexcept;
PANS_API void SetThreadName(std::string name);

[[nodiscard]] PANS_API std::string_view GetThreadName() noexcept;
}

#endif