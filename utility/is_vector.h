#ifndef NESTDAQ_UTILITY_IS_VECTOR_H_
#define NESTDAQ_UTILITY_IS_VECTOR_H_

#include <type_traits>
#include <vector>

namespace nestdaq {
template <typename T>
static constexpr bool false_v = false;

template <typename T>
struct is_vector : std::false_type {
};

template <typename T>
struct is_vector<std::vector<T>> : std::true_type {
};

template <typename T>
static constexpr bool is_vector_v = is_vector<T>::value;
} // namespace nestdaq

#endif // NESTDAQ_UTILITY_IS_VECTOR_H_
