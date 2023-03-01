#ifndef NESTDAQ_UTILITY_TO_SIGNED_H_
#define NESTDAQ_UTILITY_TO_SIGNED_H_

namespace nestdaq {

//______________________________________________________________________________
template <typename T, int N, typename U>
constexpr T to_signed(U a)
{
    static_assert(N > 0);
    return ((-(a >> (N - 1))) << (N - 1)) | a;
}

} // namespace nestdaq

#endif
