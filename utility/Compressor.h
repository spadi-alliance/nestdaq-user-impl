#ifndef Compressor_h
#define Compressor_h

#include <iostream>

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

namespace nestdaq::Compressor {
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

// traits type to check whether T is std::vector or not
template <typename T>
struct is_vector : std::false_type {
};
template <typename T>
struct is_vector<std::vector<T>> : std::true_type {
};
template <typename T>
constexpr bool is_vector_v = is_vector<T>::value;

using Buffer = std::vector<char>;
using BufferPtr = std::unique_ptr<std::vector<char>>;
using FilterPtr = std::unique_ptr<boost::iostreams::filtering_ostream>;

//____________________________________________________________________________
inline FilterPtr CreateFilter(Format format = Format::none, int bufferSize = -1)
{
    namespace io = boost::iostreams;

    // filtering_stream with write method
    auto filter = std::make_unique<io::filtering_ostream>();

    int n = (bufferSize <= 0) ? io::default_device_buffer_size : bufferSize;

    // add compression filter ------------------------------
    if (format == Format::none) {
        // do nothing
    }
#if __has_include(<boost/iostreams/filter/gzip.hpp>)
    else if (format == Format::gzip) {
        filter->push(io::gzip_compressor{io::gzip::default_compression, n});
    }
#endif
#if __has_include(<boost/iostreams/filter/bzip2.hpp>)
    else if (format == Format::bzip2) {
        filter->push(io::bzip2_compressor{io::bzip2::default_block_size, n});
    }
#endif
#ifdef USE_LZMA
#if __has_include(<boost/iostreams/filter/lzma.hpp>)
    else if (format == Format::lzma) {
        filter->push(io::lzma_compressor{io::lzma::default_compression, n});
    }
#endif
#endif
#if __has_include(<boost/iostreams/filter/zstd.hpp>)
    else if (format == Format::zstd) {
        filter->push(io::zstd_compressor{io::zstd::default_compression, n});
    }
#endif

    return std::move(filter);
}

//____________________________________________________________________________
inline std::pair<BufferPtr, FilterPtr>
CreateFilter(Format format = Format::none, unsigned int nReserve = 0, int bufferSize = -1)
{
    namespace io = boost::iostreams;
    // add output buffer
    auto buf = std::make_unique<std::vector<char>>();
    buf->reserve(nReserve);
    auto filter = CreateFilter(format, bufferSize);
    filter->push(io::back_inserter(*buf));

    return {std::move(buf), std::move(filter)};
}

//____________________________________________________________________________
template <typename T, std::size_t N>
inline Buffer Compress(const T (&src)[N], Format format = Format::none, unsigned int nReserve = 0, int bufferSize = -1)
{
    auto [buf, filter] = CreateFilter(format, nReserve, bufferSize);
    if constexpr (std::is_standard_layout_v<T>) {
        filter->write(reinterpret_cast<const char *>(src), sizeof(T) * N);
    } else {
        static_assert(false_v<T>);
    }
    boost::iostreams::close(*filter);
    return std::move(*buf);
}

//____________________________________________________________________________
template <typename T>
inline Buffer
Compress(const T *src, int n, Format format = Format::none, unsigned int nReserve = 0, int bufferSize = -1)
{
    auto [buf, filter] = CreateFilter(format, nReserve, bufferSize);
    if constexpr (std::is_standard_layout_v<T>) {
        filter->write(reinterpret_cast<const char *>(src), sizeof(T) * n);
    } else {
        static_assert(false_v<T>);
    }
    boost::iostreams::close(*filter);
    return std::move(*buf);
}

//____________________________________________________________________________
template <typename T>
inline Buffer Compress(const T &src, Format format = Format::none, unsigned int nReserve = 0, int bufferSize = -1)
{
    auto [buf, filter] = CreateFilter(format, nReserve, bufferSize);
    // write data
    if constexpr (is_vector_v<T>) {
        // T is std::vector
        using Val = typename T::value_type;
        if constexpr (std::is_standard_layout_v<Val>) {
            // T is std::vector<Val> and Val is a simple type
            filter->write(reinterpret_cast<const char *>(src.data()), src.size() * sizeof(Val));
        } else {
            static_assert(false_v<T>);
        }
    } else if constexpr (std::is_standard_layout_v<T>) {
        // T is a simple type
        filter->write(reinterpret_cast<const char *>(&src), sizeof(src));
    } else {
        static_assert(false_v<T>);
    }

    // finalize buffer writing
    boost::iostreams::close(*filter);
    return std::move(*buf);
}

} // namespace nestdaq::Compressor

#endif
