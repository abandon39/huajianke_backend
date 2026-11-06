#ifndef PANS_SRC_LOGGER_FORMATTER_H
#define PANS_SRC_LOGGER_FORMATTER_H

#include <memory>
#include <string>
#include <vector>

#include "logger/buffer.h"
#include "logger/buffer_config.h"
#include "logger/log_record.h"

namespace pans::detail
{
// 这里似乎定义的是在前面buffer_config.h里的inline constexpr。
// 第二节补充：这里的InlineBuffer是没有
using FormattedRecordBuffer = InlineBuffer<FORMATTED_RECORD_INLINE_CAPACITY>;

class Formatter final
{
public:
	// 日志的每个组成部分用一个FormatItem类来表示，是所有Item的父类
	// 定义了不同的Item的共同操作的接口，不同Item调用相同接口（数据）但行为不同。
	// 软件设计原则：【面向接口编程】【SOLID的依赖倒置】【SOLID还有开闭原则，单一职责原则，接口隔离原则，依赖倒置原则，里氏替换原则。还有个组合优于继承】
	// 后面修改：删除了这里两个format的noexcept，原因是这里传入的FormattedRecordBuffer会调用InlineBuffer，那里调用的比如reserve等函数会尝试重新申请堆区内存；如果内存真的连一点都分配不下，此时InlineBuffer的构建会抛出异常std::bad_alloc，并不能真的保证不抛异常，因此这里的format函数不能noexcept。
	class FormatItem
	{
	public:
		// 这里将基类析构声明为虚析构，当通过基类指针删除派生类对象时，运行时动态分配到实际类型的析构函数，先析构派生类再析构基类。
		virtual ~FormatItem() = default;
		virtual void format(const LogRecordView& record, FormattedRecordBuffer& output) const = 0; // 纯虚函数，定义了接口规范，派生类不同的实现决定了这些内容如何能放在不同的buffer里
	};
	explicit Formatter(std::string_view pattern); // 拒绝隐式转换
	// 根据下面pattern的格式，按顺序输出日志的内容
	void format(const LogRecordView& record, FormattedRecordBuffer& output) const;
	// 获取当前的日志格式getPattern
	[[nodiscard]] const std::string& getPattern() const noexcept { return m_pattern; }
private:
	// 解析pattern
	int parse();
	// 在m_items可能会插入一些符号，符号会被当成Literal插入到m_pattern里。
	void addLiteral(std::string& literal);
private:
	std::string m_pattern = "";
	std::vector<std::unique_ptr<FormatItem>> m_items; // 定义了所有要在日志里输出的FormatItem。
};
}


#endif