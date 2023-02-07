#include <algorithm>
#include <iostream>
#include <string>
#include <stdexcept>

#include "utility/Stoi.h"

constexpr int NumBasePrefix = 2;
constexpr int BinaryBase = 2;
constexpr int OctalBase = 8;
constexpr int DecimalBase = 10;
constexpr int HexadecimalBase = 16;

//______________________________________________________________________________
int nestdaq::GetBase(std::string_view arg, int debug)
{
   auto base = 0;
   if (debug > 0) {
      std::cout << " arg size = " << arg.size() << "\n";
   }
   if (arg.size() > NumBasePrefix) {
      const auto b = arg.substr(0, NumBasePrefix);
      if (b == "0b" || b == "0B") {
         if (debug > 0) {
            std::cout << " binary "
                      << "\n:";
         }
         base = BinaryBase;
      } else if (b == "0x" || b == "0X") {
         if (debug > 0) {
            std::cout << " hex "
                      << "\n:";
         }
         base = HexadecimalBase;
      } else if (b[0] == '0') {
         if (debug > 0) {
            std::cout << " octal "
                      << "\n:";
         }
         base = OctalBase;
      }
   }

   if (base == 0 && std::all_of(arg.cbegin(), arg.cend(), isdigit)) {
      base = DecimalBase;
   }
   // std::cout << " base =  " << base << "\n:";

   return base;
}

//______________________________________________________________________________
uint64_t nestdaq::Stoi(std::string_view arg, int debug)
{
   auto base = GetBase(arg, debug);
   if (base <= 0) {
      throw std::runtime_error(std::string("nestdaqdaq::Stoi() arg is not digit: ") + arg.data());
   }

   return std::stoull(arg.data(), nullptr, base);
}
