#ifndef NESTDAQ_UTILITY_ENUM_BITSET_H_
#define NESTDAQ_UTILITY_ENUM_BITSET_H_

#include <bitset>
#include <type_traits>

namespace nestdaq {

// This template function must be speciallized in the application to return the enum size (= bitset size).
template <typename T>
constexpr std::size_t enum_bitset_size()
{
    return 0;
}

template <typename T, typename = std::enable_if_t<!!enum_bitset_size<T>()>>
class enum_bitset {
public:
    using b_type = std::bitset<enum_bitset_size<T>()>;
    using b_reference = typename b_type::reference;
    using u_type = typename std::underlying_type<T>::type;

    constexpr enum_bitset() = default;
    constexpr enum_bitset(unsigned long long val) : b(val) {}
    constexpr enum_bitset(const T pos) : b(from_enum(pos)) {}
    constexpr enum_bitset(std::initializer_list<T> vals)
    {
        for (auto v : vals) {
            set(v);
        }
    }
    ~enum_bitset() = default;

    enum_bitset &operator&=(const enum_bitset &rhs)
    {
        b &= rhs.b;
        return *this;
    }
    enum_bitset &operator|=(const enum_bitset &rhs)
    {
        b |= rhs.b;
        return *this;
    }
    enum_bitset &operator^=(const enum_bitset &rhs)
    {
        b ^= rhs.b;
        return *this;
    }
    enum_bitset &operator<<=(std::size_t pos)
    {
        b <<= pos;
        return *this;
    }
    enum_bitset &operator>>=(std::size_t pos)
    {
        b >>= pos;
        return *this;
    }
    enum_bitset &set()
    {
        b.set();
        return *this;
    }
    enum_bitset &set(const T pos, bool val = true)
    {
        b.set(from_enum(pos));
        return *this;
    }
    enum_bitset &reset()
    {
        b.reset();
        return *this;
    }
    enum_bitset &reset(const T pos)
    {
        b.reset(from_enum(pos));
        return *this;
    }
    enum_bitset &flip()
    {
        b.flip();
        return *this;
    }
    enum_bitset &flip(const T pos)
    {
        b.flip(from_enum(pos));
        return *this;
    }

    enum_bitset operator~() const {
        return enum_bitset(*this).flip();
    }
    b_reference operator[](const T pos) {
        return b[from_enum(pos)];
    }
    constexpr bool operator[](const T pos) const {
        return b[from_enum(pos)];
    }

    unsigned long to_ulong() const {
        return b.to_ulong();
    }
    unsigned long long to_ullong() const {
        return b.to_ullong();
    }
    std::string to_string() const {
        return b.to_string();
    }
    std::string to_string(char zero, char one = '1') const {
        return b.to_string(zero, one);
    }
    std::size_t count() const {
        return b.count();
    }
    constexpr std::size_t size() const {
        return b.size();
    }

    bool operator==(const enum_bitset &rhs) const {
        return b == rhs.b;
    }
    bool operator!=(const enum_bitset &rhs) const {
        return b != rhs.b;
    }
    bool test(const T pos) const {
        return b.test(from_enum(pos));
    }
    bool all() const {
        return b.all();
    }
    bool any() const {
        return b.any();
    }
    bool none() const {
        return b.none();
    }

    enum_bitset operator<<(std::size_t pos) const {
        return enum_bitset(*this) <<= pos;
    }
    enum_bitset operator>>(std::size_t pos) const {
        return enum_bitset(*this) >>= pos;
    }

    b_type &get() {
        return b;
    }
    const b_type &get() const {
        return b;
    }

private:
    u_type from_enum(T v) const {
        return static_cast<u_type>(v);
    }
    b_type b;
};

namespace enum_bitset_op {
//___________________________________________________________________________
template <typename T>
enum_bitset<T> operator&(const enum_bitset<T> &lhs, const enum_bitset<T> &rhs)
{
    enum_bitset<T> result(lhs);
    result &= rhs;
    return result;
}

//___________________________________________________________________________
template <typename T>
enum_bitset<T> operator|(const enum_bitset<T> &lhs, const enum_bitset<T> &rhs)
{
    enum_bitset<T> result(lhs);
    result &= rhs;
    return result;
}

//___________________________________________________________________________
template <typename T>
enum_bitset<T> operator^(const enum_bitset<T> &lhs, const enum_bitset<T> &rhs)
{
    enum_bitset<T> result(lhs);
    result &= rhs;
    return result;
}

//___________________________________________________________________________
template <class CharT, class Traits, typename T>
std::basic_istream<CharT, Traits> &operator>>(std::basic_istream<CharT, Traits> &s, enum_bitset<T> &x)
{
    return s >> x.get();
}

//___________________________________________________________________________
template <class CharT, class Traits, typename T>
std::basic_ostream<CharT, Traits> &operator<<(std::basic_ostream<CharT, Traits> &s, enum_bitset<T> &x)
{
    return s << x.get();
}
} // namespace enum_bitset_op

} // namespace nestdaq

#endif
