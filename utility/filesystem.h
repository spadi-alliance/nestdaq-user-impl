#ifndef NESTDAQ_UTILITY_FILE_SYSTEM_H_
#define NESTDAQ_UTILITY_FILE_SYSTEM_H_

#if __has_include(<filesystem>)
#include <filesystem>
namespace stdfs = std::filesystem;
#else
#include <experimental/filesystem>
namespace stdfs = std::experimental::filesystem;
#endif

#include <string>
#include <string_view>

namespace nestdaq {
inline std::string filename(std::string_view s)
{
   return stdfs::path(s.data()).filename().string();
}

inline std::string stem(std::string_view s)
{
   return stdfs::path(s.data()).stem().string();
}
} // namespace nestdaq

#endif // NESTDAQ_UTILITY_FILE_SYSTEM_H_
