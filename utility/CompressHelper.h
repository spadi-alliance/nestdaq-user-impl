#ifndef COMPRESS_HELPER_H_
#define COMPRESS_HELPER_H_

#include <functional>
#include <iostream>
#include <string>
#include <string_view>

#include "utility/filesystem.h"
#include "utility/Compressor.h"

namespace nestdaq::Compressor {
inline Format ExtToFormat(std::string_view pathname, bool check = false)
{
   stdfs::path p(pathname.data());
   if (check && !stdfs::exists(p)) {
      std::cout << " file: " << p << " does not exist\n";
      return Format::none;
   }
   auto extension = p.extension().string();
   if (extension.empty()) {
      extension = pathname.data();
   }
   std::cout << " extention = " << extension << std::endl;
   if (extension.find(".gz") != std::string::npos) {
      return Format::gzip;
   } else if (extension.find(".bz") != std::string::npos) {
      return Format::bzip2;
#ifdef USE_LZMA
   } else if (extension.find(".xz") != std::string::npos) {
      return Format::lzma;
#endif
   } else if (extension.find(".zst") != std::string::npos) {
      return Format::zstd;
   }
   return Format::none;
}
} // namespace nestdaq::Compressor

#endif