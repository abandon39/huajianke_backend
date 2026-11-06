#include <pans/logger/log_level.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <string>

namespace pans
{
std::string_view LogLevel::ToString(Level level) noexcept
{
	switch(level)
	{
// 用宏做了一个字符串拼接，输入name后可以拼接成LOG_LV_name。
#define XX(name) case Level::LOG_LV_##name : return #name;
		XX(DEBUG)
		XX(INFO)
		XX(WARN)
		XX(ERROR)
		XX(FATAL)
		XX(OFF)
#undef XX
	}
	// 都没命中就输出UNKNOWN，总得输出点什么
	return "UNKNOWN";
}

// 定义FromString()，（一般是读配置时）从字符串里解析出日志等级
LogLevel::Level LogLevel::FromString(std::string_view value) noexcept
{
	// 统一转换大写
	std::array<char, 6> normalized{};
	std::transform(value.begin(),value.end(),normalized.begin(),[](unsigned char character)
	{
		return static_cast<char>(std::toupper(character));
	});
	const std::string_view upper_value(normalized.data(),value.size());
// 写一个XX宏，避免重复代码，
#define XX(name) if(upper_value == #name) return Level::LOG_LV_##name;
	XX(DEBUG)
	XX(INFO)
	XX(WARN)
	XX(ERROR)
	XX(FATAL)
	XX(OFF)
#undef XX
	return Level::LOG_LV_OFF;
}

}