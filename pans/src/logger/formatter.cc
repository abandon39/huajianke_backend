//包含头文件
#include "logger/formatter.h"
#include <unordered_map>

namespace pans::detail
{
// 年月日时分秒
constexpr std::string_view DEFAULT_DATE_FORMAT = "%Y-%m-%d %H:%M:%s";

Formatter::Formatter(std::string_view pattern)
	: m_pattern(pattern)
{
	if(parse())
	{
		// 这里是因为，日志模块还没定义好，没有日志工具可以用，因此遇到解析失败的问题必须用一些比较强力的告警（抛出异常）来处理
		throw std::invalid_argument("invalid log format, parse failed.");
	}
}
// 后续修改，同理删掉了noexcept，原因见前面formatter的定义。
void Formatter::format(const LogRecordView& record, FormattedRecordBuffer& output) const
{
	for(const auto& item : m_items)
	{
		item->format(record, output);
	}
}

// 定义一个从数字类型转换成字符串的，但不用std::to_string，这是因为std::to_string可能会在堆上开辟内存，如果堆区没内存了还会报异常，需要try/catch等，很麻烦，所以要自己写。
// 具体实现是，在栈上开辟临时数组，不用创建std::string，也不需要考虑异常（可以写noexcept）
// 后续修改，同理删掉了noexcept，原因见前面formatter的定义。
template<typename T>
void AppendInteger(FormattedRecordBuffer& output, T value)
{
	// 设定字符长度最大是24字节（似乎uint最大是20字节左右，因此24B在我们的场景里似乎足够了。
	std::array<char, 24> buffer{};
	const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
	// 如果转换失败就assert
	ASSERT_RETNONE2(result.ec ==std::errc(), "trans " << value << " to chars failed.");
	output.append(buffer.data(), static_cast<std::size_t>(result.ptr - buffer.data()));
}

class LiteralFormatItem final : public Formatter::FormatItem
{
public:
	// 后续修改：将value转换成了std::move(value)
	explicit LiteralFormatItem(std::string value)
		: m_value(std::move(value))
	{}
	// 后续修改，同理删掉了noexcept，原因见前面formatter的定义。
	void format(const LogRecordView&, FormattedRecordBuffer& output) const
	{
		output.append(m_value);
	}
private:
	std::string m_value;
	
};
class MessageFormatItem final : public Formatter::FormatItem
{
public:
	void format(const LogRecordView& record, FormattedRecordBuffer& output) const
	{
		output.append(record.m_message);
	}
};
class LevelFormatItem final : public Formatter::FormatItem
{
public:
	void format(const LogRecordView& record, FormattedRecordBuffer& output) const
	{
		output.append(LogLevel::ToString(record.m_level));
	}
};
// 记录时间，开服以来已经过了多长时间。
class ElapsedFormatItem final : public Formatter::FormatItem
{
public:
	void format(const LogRecordView& record, FormattedRecordBuffer& output) const override
	{
		AppendInteger(output, std::chrono::duration_cast<std::chrono::milliseconds>(record.m_elapsed).count());
	}
};
// 
class LoggerNameFormatItem final : public Formatter::FormatItem
{
public:
	void format(const LogRecordView& record, FormattedRecordBuffer& output) const override
	{
		output.append(record.m_loggerName);
	}
};
// 线程id
class ThreadIdFormatItem final : public Formatter::FormatItem
{
public:
	void format(const LogRecordView& record, FormattedRecordBuffer& output) const override
	{
		AppendInteger(output, record.m_threadId);
	}
};
// 换行，就是在日志里加一个换行符
class NewLineFormatItem final : public Formatter::FormatItem
{
public:
	void format(const LogRecordView&, FormattedRecordBuffer& output) const override
	{
		output.append('\n');
	}
};
// 日期格式的，这个似乎会想当麻烦，会占用很长时间，放到第二个视频里（我就直接一起写了）。
class DateTimeFormatItem final : public Formatter::FormatItem
{
public:
	explicit DateTimeFormatItem(std::string_view format)
		: m_format(format.empty() ? DEFAULT_DATE_FORMAT : format)
	{}
	// 实现format接口
	void format(const LogRecordView& record, FormattedRecordBuffer& output) const override
	{
		// 这里做了一个额外的修改，在同一秒内多次写入的日志尝试进行合并，否则一秒数百条的日志，记录时间戳的这个转换的性能开销也很大，可能会很亏。
		static thread_local time_t last_second = 0; // thread_local是让变量在每个线程都有一份
		static thread_local char cached_date_time[20] = {'\0'};
		const auto duration = record.m_timestamp.time_since_epoch();
		const time_t current_second = static_cast<time_t>(std::chrono::duration_cast<std::chrono::seconds>(duration).count());
		if (current_second != last_second)
		{
			std::tm buffer{};
#if defined(_WIN32)
			const errno_t result = localtime_s(&buffer, &current_second);
			ASSERT_RETNONE2(result == 0, "failed to convert log time to local time");
#else
			// 这里localtime_r是linux的时间获取接口，获取时间的消耗会很高。
			// 这里的_r表示这是一个线程安全的接口，本身取时间就可能需要排队。
			// 而且还需要额外进行时区转换等原因
			// 如果还需要考虑时区的实时变化，那么每次localtime_r还需要额外读文件，
			// /etc/localtime这个文件，还要发起磁盘IO
			// 这里的时间实际上是放在的内存共享区，这行代码本身只是去往内存共享区读一次内存
			// 真正的性能消耗发生在OS层面，似乎需要后面详细了解
			const std::tm* result = localtime_r(&current_second, &buffer);
			ASSERT_RETNONE2(result != nullptr, "failed to convert log time to local time");
#endif
			const std::size_t size = std::strftime(cached_date_time, sizeof(cached_date_time), m_format.c_str(), &buffer);
			ASSERT_RETNONE2(size != 0, "failed to format log time");
			last_second = current_second;
		}
		output.append(cached_date_time);
	}
private:
	std::string m_format;
};

class MicrosecondsFormatItem final : public Formatter::FormatItem
{
public:
	void format(const LogRecordView& record, FormattedRecordBuffer& output) const override
	{
		const auto total_microseconds = std::chrono::duration_cast<std::chrono::microseconds>(record.m_timestamp.time_since_epoch()).count();
		int64_t microseconds = total_microseconds % 1000000;
		if(microseconds < 0)
		{
			microseconds += 1000000;
		}
		// 这里是对其六位字符，10:31:59.001234最后的六位分秒，前面的0不能省略。
		std::array<char, MICROSECONDS_WIDTH> digits{};
		auto remaining= static_cast<uint32_t>(microseconds);
		for (std::size_t index = digits.size(); index-- > 0;)
		{
			digits[index] = static_cast<char>('0' + remaining % 10);
			remaining /= 10;
		}
		output.append(digits.data(), digits.size());
	}
private:
	static constexpr std::size_t MICROSECONDS_WIDTH = 6;
};

// 文件名插入到日志缓冲区
class FileNameFormatItem final : public Formatter::FormatItem
{
public:
	void format(const LogRecordView& record, FormattedRecordBuffer& output) const override
	{
		output.append(record.m_fileName);
	}
};

// 行号插入到日志缓冲区
class LineFormatItem final : public Formatter::FormatItem
{
public:
	void format(const LogRecordView& record, FormattedRecordBuffer& output) const override
	{
		AppendInteger(output, record.m_line);
	}
};

// Tab键插入到日志缓冲区（就是一个字符）
class TabFormatItem final : public Formatter::FormatItem
{
public:
	void format(const LogRecordView&, FormattedRecordBuffer& output) const override
	{
		output.append('\t');
	}
};

// 协程id插入到日志缓冲区
class FiberIdFormatItem final : public Formatter::FormatItem
{
public:
	void format(const LogRecordView& record, FormattedRecordBuffer& output) const override
	{
		AppendInteger(output, record.m_fiberId);
	}
};

// 线程名字
class ThreadNameFormatItem final : public Formatter::FormatItem
{
public:
	void format(const LogRecordView& record, FormattedRecordBuffer& output) const override
	{
		output.append(record.m_threadName);
	}
};

// 定义Format的一个工厂，定义了个函数指针，返回的是FormatItem这个父类指针的unique_ptr智能指针
using FormatItemFactory = std::unique_ptr<Formatter::FormatItem> (*)(std::string_view);
// 创建两个工厂函数，签名是一致的，负责创建formatItem。
template <typename Item>
[[nodiscard]] std::unique_ptr<Formatter::FormatItem> CreateSimpleFormatItem(std::string_view)
{
	return std::make_unique<Item>();
}
template <typename Item>
[[nodiscard]] std::unique_ptr<Formatter::FormatItem> CreateConfiguredFormatItem(std::string_view format)
{
	return std::make_unique<Item>(format);
}

// 定义一个map负责将所有日志进行整合并真正构造？
// "%d%Tthread=%t%Tfiber=%F%T%N%T[%p]%T%f:%l%T%m%n"
[[nodiscard]] const std::unordered_map<char, FormatItemFactory>& GetFormatItemFactories()
{
	static const std::unordered_map<char, FormatItemFactory> FORMAT_ITEM_FACTORIES
	{
		{'m', &CreateSimpleFormatItem<MessageFormatItem>}, //m表示MessageFormatItem
		{'p', &CreateSimpleFormatItem<LevelFormatItem>}, // p表示LevelFormatItem
		{'r', &CreateSimpleFormatItem<ElapsedFormatItem>}, //r表示ElapsedFormatItem
		{'c', &CreateSimpleFormatItem<LoggerNameFormatItem>}, // c表示日志器名称
		{'t', &CreateSimpleFormatItem<ThreadIdFormatItem>}, // t表示线程id
		{'n', &CreateSimpleFormatItem<NewLineFormatItem>}, // n表示换行
		{'d', &CreateConfiguredFormatItem<DateTimeFormatItem>}, // d表示datetime
		{'u', &CreateSimpleFormatItem<MicrosecondsFormatItem>}, // u表示毫秒
		{'f', &CreateSimpleFormatItem<FileNameFormatItem>}, // 小f表示文件名
		{'l', &CreateSimpleFormatItem<LineFormatItem>}, // l表示行号
		{'T', &CreateSimpleFormatItem<TabFormatItem>}, // T表示制表符tab
		{'F', &CreateSimpleFormatItem<FiberIdFormatItem>}, // F表示协程id
		{'N', &CreateSimpleFormatItem<ThreadNameFormatItem>}, // N表示线程名
	};
	return FORMAT_ITEM_FACTORIES;
}

[[nodiscard]] std::unique_ptr<Formatter::FormatItem> CreateFormatItem(char directive, std::string_view format)
{
	// 先找到工厂模板
	const auto& factories = GetFormatItemFactories();
	// 然后根据占位符找到函数指针，
	const auto it = factories.find(directive);
	// 如果找不出来就Assert
	ASSERT_RETVAL2(it != factories.end(), nullptr, "unknown logger format directive: " << directive);
    return it->second(format);
}

void Formatter::addLiteral(std::string& literal)
{
	if(literal.empty())
	{
		return;
	}
	// 如果literal不为空，先将其转换为右值引用，然后在LiteralFormatItem里发生资源转移
	m_items.push_back(std::make_unique<LiteralFormatItem>(std::move(literal)));
	// C++移动过的字符串是处于有效但是没有指定的状态，可以安全调用clear、size等，但不能再引用字符串内容
	literal.clear();
}

// "%d{%Y-%m-%d %H:%M:%S}.%u%Tthread=%t%Tfiber=%F%T[%p]%T%f:%l%T%m%n"
// %p那边似乎是literal的内容？（边界暂时不清楚，先记住）
int Formatter::parse()
{
	std::string literal;
	for(std::size_t index = 0; index < m_pattern.size(); index++)
	{
		if (m_pattern[index] != '%')
		{
			literal.push_back(m_pattern[index]);
			continue;
		}
		// 就是index+1要小于上面那个注释的格式长度，否则会报错说
		ASSERT_RETVAL2(index + 1 < m_pattern.size(), -1, "logger format pattern ends with an incomplete directive");
		const char directive = m_pattern[++index];
		if (directive == '%')
		{
			literal.push_back('%');
			continue;
		}
		addLiteral(literal);
		// item_format放右括号的位置。
		std::string_view item_format;
		// 如果下一个是左括号，就找到右括号位置
		if (index + 1 < m_pattern.size() && m_pattern[index + 1] == '{')
		{
			const std::size_t closing_brace = m_pattern.find('}', index + 2);
			// 用Assert做了一个断言，就是不能刚好是{}空的花括号，里面必须有点东西
			ASSERT_RETVAL2(closing_brace != std::string::npos && closing_brace != (index + 2), -2, "missing a closing brace or empty");
			item_format = std::string_view(m_pattern).substr(index + 2, closing_brace - index - 2);
			index = closing_brace;
		}
		// 把花括号中间的内容加进去
		m_items.push_back(CreateFormatItem(directive, item_format));
	}
	addLiteral(literal);
	// 最后检查一下所有的指针都要是合法的
	for(const auto& item : m_items)
	{
		ASSERT_RETVAL2(item != nullptr, -3, "logger formatter contains a null format item");
	}
	return 0;
}
}