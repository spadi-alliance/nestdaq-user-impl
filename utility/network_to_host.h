#ifndef NESTDAQ_UTILITY_NETWORK_TO_HOST_H_
#define NESTDAQ_UTILITY_NETWORK_TO_HOST_H_

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <type_traits>
#include <boost/asio.hpp>

#if __has_include(<boost/endian/detail/order.hpp>)
#include <boost/endian/detail/order.hpp>
#else
#include <boost/endian/conversion.hpp>
#endif

namespace nestdaq {

namespace my_ntoh { // temporary namespace for network_to_host header file
template <typename T>
static constexpr bool false_v = false;
}

//______________________________________________________________________________
// reverse byte in place
template <typename T, std::size_t N>
void network_to_host(T (&src)[N])
{
   // std::cout << __FUNCTION__ << ":" << __LINE__ << std::endl;

   if constexpr (boost::endian::order::big == boost::endian::order::native) {
      // do nothing
   } else if constexpr (std::is_standard_layout_v<T>) {
      if constexpr (sizeof(T) == 1) {
         // std::cout << __FUNCTION__ << ":" << __LINE__ << std::endl;
         std::reverse(src, src + N);
      } else {
         // std::cout << __FUNCTION__ << ":" << __LINE__ << std::endl;
         for (auto i = 0; i < N; ++i) {
            auto p = reinterpret_cast<uint8_t *>(src[i]);
            std::reverse(p, p + sizeof(T));
         }
      }
   } else {
      static_assert(my_ntoh::false_v<T>);
   }
}

//______________________________________________________________________________
// reverse byte in place
template <typename T>
void network_to_host(T *src, int n)
{
   if constexpr (boost::endian::order::big == boost::endian::order::native) {
      // do nothing
   } else if constexpr (std::is_same_v<T, int8_t> || std::is_same_v<T, uint8_t>) {
      // std::cout << __FUNCTION__ << ":" << __LINE__ << std::endl;
      std::reverse(src, src + n);
   } else {
      // std::cout << __FUNCTION__ << ":" << __LINE__ << std::endl;
      for (auto i = 0; i < n; ++i) {
         auto p = reinterpret_cast<uint8_t *>(src[i]);
         std::reverse(p, p + sizeof(T));
      }
   }
}

//______________________________________________________________________________
template <typename T, typename Iterator>
T network_to_host(Iterator itr)
{
   namespace net = boost::asio;
   using T_t = typename std::decay<T>::type;
   auto d = *reinterpret_cast<T *>(&(*itr));

   if constexpr (std::is_same_v<uint16_t, T_t> || std::is_same_v<int16_t, T_t>) {
      // std::cout << __FUNCTION__ << ":" << __LINE__ << std::endl;
      return net::detail::socket_ops::network_to_host_short(d);
   } else if constexpr (std::is_same_v<uint32_t, T_t> || std::is_same_v<int32_t, T_t>) {
      // std::cout << __FUNCTION__ << ":" << __LINE__ << std::endl;
      return net::detail::socket_ops::network_to_host_long(d);
   } else {
      static_assert(my_ntoh::false_v<T>);
   }
}

} // namespace nestdaq

#endif
