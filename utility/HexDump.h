#ifndef HexDump_h
#define HexDump_h

#include <type_traits>
#include <cstdint>
#include <iostream>
#include <vector>
#include <string>

class HexDump {
public:
    template <typename T>
    static constexpr bool false_v = false;

    HexDump(int nWordInLine = 8, std::ostream &os = std::cout) : fOut(os), fNWordInLine(nWordInLine)
    {
        fFlags = fOut.flags();
        fFill = fOut.fill('0');

        // If gcc supports endianness check, please replace the following code.
        const int tmp = 1;
        fIsLittleEndian = (reinterpret_cast<const uint8_t *>(&tmp)[0] == 1);
    }

    ~HexDump()
    {
        fOut << std::endl;
        fOut.flags(fFlags);
        fOut.fill(fFill);
    }

    template <typename Ret, typename T>
    static Ret cast(const T &data)
    {
        Ret ret{0};
        if constexpr (std::is_standard_layout_v<T>) {
            const int tmp = 1;
            auto isLittleEndian = (reinterpret_cast<const uint8_t *>(&tmp)[0] == 1);
            auto p = reinterpret_cast<const uint8_t *>(&data);
            for (int i = 0, n = sizeof(T); i < n; ++i) {
                auto b = (isLittleEndian ? (n - i - 1) : i);
                ret = ret << 8;
                ret += p[b];
            }
        } else {
            static_assert(false_v<T>);
        }
        return ret;
    }

    template <typename T>
    void operator()(const T &data)
    {
        if (0 == fCount % fNWordInLine) {
            fOut << " " << fAscii.data();
            fOut << "\n";
            fOut.width(8);
            fOut << std::hex << fCount << " : ";
            fAscii.clear();
        }

        if constexpr (std::is_integral_v<T>) {
            fOut.width(2 * sizeof(T));
            uint64_t mask = (1 << sizeof(T) * 8) - 1;
            fOut << std::hex << (((sizeof(T) <= sizeof(uint8_t)) ? static_cast<uint16_t>(data) : data) & mask) << " ";
            std::vector<char> b(&data, &data + sizeof(T));
            for (auto c : b) {
                if (c >= 0x21 && c <= 0x7e) {
                    fAscii.append(&c, 1);
                } else {
                    char cc = '.';
                    fAscii.append(&cc, 1);
                }
            }
        } else if constexpr (std::is_floating_point_v<T>) {
            fOut.width(3 * sizeof(T));
            fOut << std::hexfloat << data << " ";
        } else if constexpr (std::is_standard_layout_v<T>) {
            fOut << std::hex;
            auto p = reinterpret_cast<const uint8_t *>(&data);
            for (auto i = 0ul, n = sizeof(T); i < n; ++i) {
                fOut.width(2 * sizeof(uint8_t));
                fOut << static_cast<uint16_t>(p[(fIsLittleEndian ? (n - i - 1) : i)] & 0xff);
            }
            fOut << " ";
        } else {
            static_assert(false_v<T>);
        }

        ++fCount;
    }

private:
    std::ostream &fOut;
    std::ios_base::fmtflags fFlags;
    char fFill;
    int fNWordInLine;
    int fCount{0};
    bool fIsLittleEndian{true};
    std::string fAscii;
};

#endif
