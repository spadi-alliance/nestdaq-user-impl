#include <algorithm>
#include <iomanip>
#include <iostream>
#include <string>
#include <sstream>
#include <fstream>

#include "utility/filesystem.h"
#include "utility/mcs_parser.h"

using namespace nestdaq::mcs;

constexpr std::size_t NChar_Start = 1;
constexpr std::size_t NChar_ByteCount = 2;
constexpr std::size_t NChar_Address = 4;
constexpr std::size_t NChar_Type = 2;
constexpr std::size_t NChar_CheckSum = 2;

constexpr std::size_t Begin_Start = 0;
constexpr std::size_t Begin_ByteCount = Begin_Start + NChar_Start;
constexpr std::size_t Begin_Address = Begin_ByteCount + NChar_ByteCount;
constexpr std::size_t Begin_Type = Begin_Address + NChar_Address;
constexpr std::size_t Begin_Data = Begin_Type + NChar_Type;

//______________________________________________________________________________
template <typename T>
std::string Format(std::size_t n, T v)
{
   std::ostringstream oss;
   oss << std::setw(n) << std::setfill('0') << static_cast<uint16_t>(v);
   return oss.str();
}

//______________________________________________________________________________
void Record::Print() const
{
   std::cout << std::hex << " byteCount = " << Format(NChar_ByteCount, byteCount)
             << "\n address = " << Format(NChar_Address, address) << "\n type  = " << Format(NChar_Type, type)
             << "\n data = ";
   std::for_each(data.cbegin(), data.cend(), [](auto x) { std::cout << Format(2, x); });
   std::cout << "\n checksum = " << Format(NChar_CheckSum, checkSum);
   std::cout << std::dec << std::endl;
}

//______________________________________________________________________________
std::vector<Record> Parse(std::string_view filename)
{
   std::ifstream file(fielname.data());
   std::string l;
   std::vector<Record> ret;

   while (file) {
      std::getline(file, l);
      if (l.empty())
         continue;

      std::cout << l << std::endl;
      auto nchar_data = l.length() - NChar_Start - NChar_ByteCount - NChar_Address - NChar_Type - NChar_CheckSum;

      Record r;
      r.byteCount = std::stoul(l.substr(Begin_ByteCount, NChar_ByteCount), nullptr, 16);
      r.address = std::stoul(l.substr(Begin_Address, NChar_Address), nullptr, 16);
      r.type = static_cast<Type>(std::stoul(l.substr(Begin_Type, NChar_Type), nullptr, 16));
      std::size_t n = nchar_data / 2;
      r.data.reserve(n);
      for (auto i = 0; i < n; ++i) {
         r.data.push_back(std::stoul(l.substr(Begin_Data + i * 2, 2), nullptr, 16));
      }
      r.checkSum = std::stoul(l.substr(l.length() - NChar_CheckSum), nullptr, 16);
      // r.Print();

      ret.push_back(std::move(r));
   }
   return std::move(ret);
}
