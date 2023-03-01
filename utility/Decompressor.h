#ifndef Decompressor_h
#define Decompressor_h

#include <vector>
#include <memory>
#include <type_traits>

#include <boost/iostreams/filtering_stream.hpp>

#if __has_include(<boost/iostreams/filter/gzip.hpp>)
#include <boost/iostreams/filter/gzip.hpp>
#else
#warning <boost/iostreams/filter/gzip.hpp> is not found
#endif

#if __has_include(<boost/iostreams/filter/bzip2.hpp>)
#include <boost/iostreams/filter/bzip2.hpp>
#else
#warning <boost/iostreams/filter/bzip2.hpp> is not found
#endif

#ifdef USE_LZMA
#if __has_include(<boost/iostreams/filter/lzma.hpp>)
#include <boost/iostreams/filter/lzma.hpp>
#else
#warning <boost/iostreams/filter/lzma.hpp> is not found
#endif
#endif

#if __has_include(<boost/iostreams/filter/zstd.hpp>)
#include <boost/iostreams/filter/zstd.hpp>
#else
#warning <boost/iostreams/filter/zstd.hpp> is not found
#endif

#include <boost/iostreams/device/back_inserter.hpp>

namespace nestdaq::Decompressor {
enum Format : int {
    none,
    gzip,
    bzip2,
#ifdef USE_LZMA
    lzma,
#endif
    zstd
};

// false value for static assert to invoke compilation error
template <typename T>
static constexpr bool false_v = false;

using Buffer = std::vector<char>;
using BufferPtr = std::unique_ptr<std::vector<char>>;
using IFilterPtr = std::unique_ptr<boost::iostreams::filtering_istream>;
using OFilterPtr = std::unique_ptr<boost::iostreams::filtering_ostream>;

//____________________________________________________________________________
template <typename T>
inline std::unique_ptr<T> CreateFilter(Format format = Format::none, int bufferSize = -1)
{
    namespace io = boost::iostreams;

    // filtering_stream with write method
    auto filter = std::make_unique<T>();

    int n = (bufferSize <= 0) ? io::default_device_buffer_size : bufferSize;

    // add decompression filter ----------------------------
    if (format == Format::none) {
        // do nothing
    }
#if __has_include(<boost/iostreams/filter/gzip.hpp>)
    else if (format == Format::gzip) {
        filter->push(io::gzip_decompressor{io::gzip::default_window_bits, n});
    }
#endif
#if __has_include(<boost/iostreams/filter/bzip2.hpp>)
    else if (format == Format::bzip2) {
        filter->push(io::bzip2_decompressor{io::bzip2::default_small, n});
    }
#endif
#ifdef USE_LZMA
#if __has_include(<boost/iostreams/filter/lzma.hpp>)
    else if (format == Format::lzma) {
        filter->push(io::lzma_decompressor{n});
    }
#endif
#endif
#if __has_include(<boost/iostreams/filter/zstd.hpp>)
    else if (format == Format::zstd) {
        filter->push(io::zstd_decompressor{n});
    }
#endif

    return std::move(filter);
}

//____________________________________________________________________________
inline std::pair<BufferPtr, OFilterPtr>
CreateFilter(Format format = Format::none, unsigned int nReserve = 0, int bufferSize = -1)
{
    namespace io = boost::iostreams;
    // add temporary buffer to hold char type data
    auto buf = std::make_unique<std::vector<char>>();
    buf->reserve(nReserve);
    auto filter = CreateFilter<io::filtering_ostream>(format, bufferSize);
    filter->push(io::back_inserter(*buf));
    return {std::move(buf), std::move(filter)};
}

//____________________________________________________________________________
template <typename T>
inline Buffer
Decompress(const std::vector<char> &src, Format format = Format::none, unsigned int nReserve = 0, int bufferSize = -1)
{
    auto [tmp, filter] = CreateFilter(format, nReserve, bufferSize);
    // write data
    if constexpr (std::is_standard_layout_v<T>) {
        filter->write(src.data(), src.size());
    } else {
        static_assert(false_v<T>);
    }

    // finalize buffer writing
    boost::iostreams::close(*filter);

    auto srcBegin = reinterpret_cast<T *>(tmp->data());
    auto srcEnd = srcBegin + tmp->size() / sizeof(T);

    auto buf = std::make_unique<std::vector<T>>(srcBegin, srcEnd);
    return std::move(*buf);
}

} // namespace nestdaq::Decompressor

#endif
