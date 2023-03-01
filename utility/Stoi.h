#ifndef NESTDAQ_UTILITY_STOI_H_
#define NESTDAQ_UTILITY_STOI_H_

#include <bitset>
#include <cstdint>
#include <string_view>

namespace nestdaq {
int GetBase(std::string_view arg, int debug = 0);
uint64_t Stoi(std::string_view arg, int debug = 0);

//______________________________________________________________________________
template <int nbits, bool msb_first = true>
uint64_t ToBits(std::string_view val)
{
    if (val.find("0x") == 0) {
        return Stoi(val);
    }
    std::bitset<nbits> ret;
    auto i = (msb_first) ? nbits : 0;
    for (auto c : val) {
        if (c == '1') {
            if (msb_first)
                ret[--i] = 1;
            else
                ret[i++] = 1;
        } else if (c == '0') {
            if (msb_first)
                ret[--i] = 0;
            else
                ret[i++] = 0;
        }
    }

    return ret.to_ullong();
}

} // namespace nestdaq

#endif // NESTDAQDAQ_UTILITY_STOI_H_
