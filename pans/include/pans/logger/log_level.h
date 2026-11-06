// 日志模块是为了记录程序在运行期间出现的关键信息，比如走到哪步了；
// 然后最重要的是帮我们记录业务信息，帮助生产环境快速筛选故障，触发监控告警等。
// 比如调试信息，连接数据库成功、玩家成功登录等
// 比如告警信息，出了一些告警说明有点问题但是程序还能继续运行。
// 比如操作失败，一些error信息等。
// 比如严重故障，fatal error，可能直接影响程序运行，或者新物品生成逻辑不对，可以刷取物品等。
// 但是日志需要划分级别，开发环境中日志可以比较密集方便检查；
// 但是部署后，密集日志会浪费性能、会直接写爆磁盘、或者排查问题时根本不知道去哪里找。
// 因此需要定义一个日志级别类，

#ifndef PANS_INCLUDE_PANS_LOGGER_LOG_LEVEL_H
#define PANS_INCLUDE_PANS_LOGGER_LOG_LEVEL_H

#include <cstdint>
#include <string_view> // C++17标准，是一个关于字符串的非常轻量的封装（不修改字符串时好用）

#include <pans/export.h>

// 因此需要定义一个日志级别类
namespace pans
{
class PANS_API LogLevel final
{
public:
	enum class Level : std::uint8_t
	{
		LOG_LV_DEBUG = 1, // 记录调试细节，如变量值，程序执行流程等
		LOG_LV_INFO = 2, // 记录正常运行的信息，如启动服务器，数据库连接成功，玩家登录成功等
		LOG_LV_WARN = 3, // 可能存在问题，但是程序还能继续运行
		LOG_LV_ERROR = 4, // 操作失败，或者功能异常，需要检查或处理
		LOG_LV_FATAL = 5, // 严重故障，程序继续执行可能存在较大问题，如充值
		LOG_LV_OFF = 6, // 用来关闭日志，做性能测试时等，希望关闭日志避免干扰性能。
	};
	// [[nodiscard]] 表示不要忽略函数返回值
	// ToString是显示日志级别的名而不是等级数字？
	[[nodiscard]] static std::string_view ToString(Level level) noexcept;
	// 配置做好时，通过配置告诉系统，现在需要什么样的日志级别
	[[nodiscard]] static LogLevel::Level FromString(std::string_view value) noexcept;
};
}
#endif