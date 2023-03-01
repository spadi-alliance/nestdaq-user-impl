#ifndef DECOMPRESS_HELPER_H_
#define DECOMPRESS_HELPER_H_

#include <functional>
#include <iostream>
#include <string>
#include <string_view>

#include "utility/Decompressor.h"
#include "utility/filesystem.h"

namespace nestdaq::Decompressor {

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

} // namespace nestdaq::Decompressor

#endif
