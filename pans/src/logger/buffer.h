#ifndef PANS_SRC_LOGGER_BUFFER_H
#define PANS_SRC_LOGGER_BUFFER_H

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <streambuf>
#include <string_view>
#include <vector>

namespace pans::detail
{
// 考虑开始正式实现日志。一行日志一般都很短（不超过200字符）
// 需要考虑在什么地方构建，在堆上构建需要new/delete或者malloc/free，性能会低很多；
// 因此在字符很短的时候，一般都考虑在栈上构建；只有超过栈空间的大日志才会在堆上构建。
// 原来（应该是sylar老师）的做法是用std::stringstream来管理分配
// 优点是做起来比较简单，不需要手动管理空间，缺点是stringstream是都在堆上分配内存
// 日志写完即释放，且每次写日志都需要进行内存分配，空间的缓冲区不能复用；
// 因此尝试定义一个自定义缓冲区，针对高频小数量的日志场景在栈上分配缓冲区。
// 真的遇到超长的日志的时候再在堆上分配内存。
// std::stringstream还有个问题就是拷贝，每次赋值必须要拷贝构造一个std::string
// 改用std::string_view极轻量的拿到底层数据，性能会好很多。

template<std::size_t INLINE_CAPACITY>
class InlineBuffer
{
public:
	void append(const char* data, std::size_t size)
	{
		if(size == 0)
		{
			return;
		}
		// 如果空间够，那么就把数据拷贝进来
		if(m_overflow.empty() && m_size + size <= INLINE_CAPACITY)
		{
			// 把数据加进来，加偏移量，然后把data放进去
			std::memcpy(m_inline.data() + m_size, data, size);
			m_size += size;
			return;
		}
		if(m_overflow.empty())
		{
			const std::size_t required_capacity = m_size + size;
			m_overflow.reserve(std::max(INLINE_CAPACITY * 2, required_capacity));
			m_overflow.insert(m_overflow.end(),m_inline.data(),m_inline.data() + m_size);
		}
		m_overflow.insert(m_overflow.end(),data, data + size);
		m_size = m_overflow.size();
	}
	// 这里的std::string_view需要自己看
	void append(std::string_view value)
	{
		append(value.data(), value.size());
	}
	void append(char value)
	{
		append(&value, 1);
	}
	[[nodiscard]] const char* data() const noexcept
	{
		return m_overflow.empty() ? m_inline.data() : m_overflow.data();
	}
	[[nodiscard]] std::size_t size() const noexcept
	{
		return m_size;
	}
	[[nodiscard]] std::string_view view() const noexcept
	{
		// 返回数据起始地址和长度，是个元组
		return {data(), size()};
	}
private:
	std::array<char, INLINE_CAPACITY> m_inline{}; // 类似于int m_inline[256]，短日志
	std::vector<char> m_overflow; // 长日志，vector的对象在栈上，但指向的内存空间在堆上
	std::size_t m_size = 0; // 日志现在有多长
};

// 定义一个接口，这里使用std::streambuf是为了兼容std::ostream流式输入接口
// 这里是一个std::ostream到InlineBuffer的适配器，第二遍写pans再回来了解。
template<std::size_t INLINE_CAPACITY>
class SmallStreamBuffer final : public std::streambuf
{
public:
	// explicit表示禁止编译器进行隐式类型转换？
	explicit SmallStreamBuffer(InlineBuffer<INLINE_CAPACITY>& buffer) noexcept : m_buffer(buffer)
	{}
protected:
	std::streamsize xsputn(const char* data, std::streamsize size) override
	{
		if (size <= 0)
		{
			return 0;
		}
		m_buffer.append(data, static_cast<std::size_t>(size));
		return size;
	}
	// 这里做了一个重载，是为了兼容std::ostream的“单字符输出”，std::ostream的单字符输出有单独的实现，可以去自行了解streambuffer的设计
	// streambuffer设计了一个put area，在put area还有空间时会将单个字符插入到其中。
	// 如果没有空间了就会调用overflow把字符插入进去。
	// 如果overflow也没定义就会走一个默认（似乎暂时跳过）
	// 在这里老师没有设计put area（这样似乎需要单独维护一个空间，需要维护指针等，很麻烦）
	// 而是直接重载了overflow，简单能用即可。
	int_type overflow(int_type character) override
	{
		if(traits_type::eq_int_type(character, traits_type::eof()))
		{
			return traits_type::not_eof(character);
		}
		m_buffer.append(traits_type::to_char_type(character));
		return character;
	}
private:
	InlineBuffer<INLINE_CAPACITY>& m_buffer;
};

}
#endif