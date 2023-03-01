#ifndef NESTDAQ_Utility_GrayCode_h
#define NESTDAQ_Utility_GrayCode_h

#include <bitset>

namespace nestdaq {

//_____________________________________________________________________________
template <std::size_t N>
std::bitset<N> binary_to_gray(const std::bitset<N> &b)
{
    return b ^ (b >> 1);
}

//_____________________________________________________________________________
template <std::size_t N>
std::bitset<N> gray_to_binary(const std::bitset<N> &g)
{
    std::bitset<N> b;
    b[N - 1] = g[N - 1];
    for (auto i = 1ul; i < N; ++i) {
        b[N - 1 - i] = b[N - i] ^ g[N - 1 - i];
    }
    return b;
}

} // namespace nestdaq

#endif