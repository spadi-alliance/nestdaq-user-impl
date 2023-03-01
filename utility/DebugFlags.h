#ifndef NESTDAQ_DEBUG_FLAGS_H_
#define NESTDAQ_DEBUG_FLAGS_H_

#include "utility/enum_bitset.h"

namespace nestdaq::debug {
enum class Flag : std::size_t {
    RbcpEndpoint = 1,
    RbcpHeader = 2,
    RbcpBody = 3,

    ScEndpoint = 1,
    ScCommand = 2,
    ScPayload = 3,
    ScRegName = 4,

    RegisterMapParse = 10,
    RegisterMap = 11,
    OptionsParse = 12,
    Options = 13,

    SocketTimeout = 20,
    CompleteFrame = 21,

    App0 = 30,
    App1 = 31,

    AppConstruct = 40,
    AppDestruct = 41,
    AppFunctionStart = 42,
    AppFunctionEnd = 43,
};
} // namespace nestdaq::debug

namespace nestdaq {

// template specialization
template <>
constexpr std::size_t enum_bitset_size<daq::debug::Flag>()
{
    return static_cast<std::size_t>(daq::debug::Flag::AppFunctionEnd) + 1;
}

using DebugFlags = nestdaq::enum_bitset<nestdaq::debug::Flag>;
} // namespace nestdaq

#endif // NESTDAQ_DEBUG_FLAGS_H_
