#include <cstdio>
#include <iostream>
#include <iomanip>
#include <cstdint>
#include <chrono>
#include <ctime>
#include <map>
#include <regex>

//#if __has_include(<fmt/core.h>)
//#include <fmt/core.h>
//#endif

#include <boost/algorithm/string/case_conv.hpp>

#include "utility/Compressor.h"
//#include "utility/Decompressor.h"
#include "utility/program_options.h"
#include "utility/Stoi.h"
#include "utility/FileUtil.h"

const std::string nestdaq::FileUtil::MyFile{nestdaq::filename(__FILE__)};
const std::string nestdaq::FileUtil::MyClass{nestdaq::stem(__FILE__)};

using namespace std::string_literals;
namespace bpo = boost::program_options;
using opt = nestdaq::FileUtil::OptionKey;
using oopt = nestdaq::FileUtil::OpenmodeOption;
using sopt = nestdaq::FileUtil::SplitOption;
using eopt = nestdaq::FileUtil::ExtensionOption;

namespace Compressor = nestdaq::Compressor;
using CFormat = Compressor::Format;

const std::map<std::string_view, Compressor::Format> fileTypes = {
    {eopt::DAT, CFormat::none},  //
    {eopt::GZ, CFormat::gzip},   //
    {eopt::BZ2, CFormat::bzip2}, //
#ifdef USE_LZMA
    {eopt::XZ, CFormat::lzma}, //
#endif
    {eopt::ZSTD, CFormat::zstd},
};

namespace nestdaq {
//______________________________________________________________________________
std::string FileUtil::ToFormattedString(const std::string &form, int64_t a)
{
    //#if __has_include(<fmt/core.h>)
    //   return fmt::format(form, a);
    //#else
    std::regex r{R"((.*)\{:(\d+)d\}(.*))"};
    std::smatch m;
    std::regex_search(form, m, r);
    // for (auto i=0; i<m.size(); ++i) {
    //   std::cout << "match i =  " << i << " " << m[i].str() << std::endl;
    //}

    std::ostringstream ret;
    ret << m[1].str();
    auto num = m[2].str();
    ret << std::setw(std::stoll(num));
    if (num.find("0") == 0) {
        ret << std::setfill('0');
    }
    if ((num.find("x") != std::string::npos) || (num.find("X") != std::string::npos)) {
        ret << std::hex;
    }
    ret << a << m[3].str();
    return ret.str();
    //#endif
}

//______________________________________________________________________________
void FileUtil::AddOptions(bpo::options_description &options, const std::unordered_map<std::string, std::string> &v)
{
    auto &&a = options.add_options();

    auto d = [](auto s) {
        return GetDescriptions().at(s);
    };

    add_option(a, opt::Path, bpo::value<std::string>(), d(opt::Path), v);
    add_option(a, opt::Prefix, bpo::value<std::string>(), d(opt::Prefix), v);
    add_option(a, opt::RunNumberFormat, bpo::value<std::string>(), d(opt::RunNumberFormat), v, {"run{:06d}"});
    add_option(a, opt::BranchNumberFormat, bpo::value<std::string>(), d(opt::BranchNumberFormat), v, {"_{:03d}"});
    add_option(a, opt::FileExtension, bpo::value<std::string>(), d(opt::FileExtension), v);
    add_option(a, opt::BufferSize, bpo::value<std::string>(), d(opt::BufferSize), v, std::to_string(BUFSIZ));
    add_option(a, opt::CBufSize, bpo::value<std::string>(), d(opt::CBufSize), v, {"-1"});
    add_option(a, opt::Split, bpo::value<std::string>(), d(opt::Split), v, {sopt::None.data()});
    add_option(a, opt::FirstBranchNumber, bpo::value<std::string>(), d(opt::FirstBranchNumber), v, {"0"});
    add_option(a, opt::MaxSize, bpo::value<std::string>(), d(opt::MaxSize), v, {"10000"});
    add_option(a, opt::Openmode, bpo::value<std::string>(), d(opt::Openmode), v, {oopt::Recreate.data()});
    add_option(a, opt::Permissions, bpo::value<std::string>(), d(opt::Permissions), v);
}

//______________________________________________________________________________
bool FileUtil::CheckDir(std::string_view dirName, std::string_view openmode) const
{
    using Mode = OpenmodeOption;
    bool modeFound = false;
    for (auto m : {
                Mode::Create, Mode::New, Mode::Recreate, Mode::Read
            }) {
        if (m == openmode) {
            modeFound = true;
            break;
        }
    }
    if (!modeFound) {
        std::cout << "unknown openmode option " << openmode << std::endl;
        return false;
    }

    stdfs::path pathName{dirName};
    if (stdfs::exists(pathName) && stdfs::is_directory(pathName)) {
        auto status{stdfs::status(pathName)};
        auto perm{status.permissions()};
        if (openmode == Mode::Read) {
            if ((perm & stdfs::perms::owner_read) == stdfs::perms::owner_read)
                return true;
            if ((perm & stdfs::perms::group_read) == stdfs::perms::group_read)
                return true;
            if ((perm & stdfs::perms::others_read) == stdfs::perms::others_read)
                return true;
            std::cout << "#E directory " << pathName << " is not readable" << std::endl;
        } else {
            if ((perm & stdfs::perms::owner_write) == stdfs::perms::owner_write)
                return true;
            if ((perm & stdfs::perms::group_write) == stdfs::perms::group_write)
                return true;
            if ((perm & stdfs::perms::others_write) == stdfs::perms::others_write)
                return true;
            std::cout << "#E directory " << pathName << " is not writable" << std::endl;
        }
    }
    std::cout << "#E path " << pathName << " is not a directory" << std::endl;
    return false;
}

//______________________________________________________________________________
void FileUtil::ClearBranch()
{
    fBranchRawSize.clear();
    fBranchCompressedSize.clear();
    fBranchFilePath.clear();
    fBranchNumIteration.clear();
    fBranchNum = fFirstBranchNum;
}

//______________________________________________________________________________
void FileUtil::Close()
{
    if (!fOut.is_open())
        return;

    fOut.flush();
    fOut.close();
    if (fPermissions != stdfs::perms::unknown) {
        stdfs::permissions(stdfs::path(fName), fPermissions);
    }
    fName.clear();
}

//______________________________________________________________________________
const std::string FileUtil::GenerateFileName(std::string_view name)
{
    std::string fileName;
    {
        std::string_view root = ".root";
        for (auto ext : {
                    eopt::DAT, eopt::GZ, eopt::BZ2, eopt::XZ, eopt::ZSTD, root
                }) {
            if (name.find(ext.data()) != std::string_view::npos) {
                return name.data();
            }
        }
    }
    if (!name.empty()) {
        fileName += name;
    } else if (fRun >= 0) {
        fileName += ToFormattedString(fRunNumberFormat, fRun);
    } else {
        std::chrono::system_clock::time_point p = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(p);
        const tm *lt = std::localtime(&t);
        std::ostringstream time;
        if (fDateFormat.empty()) {
            time << std::put_time(lt, "%Y%m%dT%H%M%S");
        } else {
            time << std::put_time(lt, fDateFormat.data());
        }
        fileName += time.str();
    }

    if (fBranchNum >= 0) {
        fileName += ToFormattedString(fBranchNumberFormat, fBranchNum);
    }

    fileName += fExt;
    return fileName;
}

//______________________________________________________________________________
const std::unordered_map<std::string_view, std::string_view> FileUtil::GetDescriptions()
{
    static std::unordered_map<std::string_view, std::string_view> desc{
        {   opt::Path, "File path. If this value is empty, a file path is generated. ('prefix' + 'run-number (formatted)' + "
            "'branch-number (formatted)' + '. filename extension')"
        },
        {opt::Prefix, "Filename prefix (string)"},
        {opt::Run, "Run number (integer: negative value => date is used for output filename) [deprecated]"},
        {opt::RunNumber, "Run number (integer: negative value => date is used for output filename)"},
        {opt::RunNumberFormat, "Run number format"},
        {opt::BranchNumberFormat, "Branch number format"},
        {opt::FileExtension, "File extension or suffix (string: .dat, .gz, .bz2, .xz, .zst)"},
        {opt::BufferSize, "Buffer size for writing file (integer)"},
        {opt::CBufSize, "Buffer size for compression/decompression (integer. if negative, default value will be used)"},
        {opt::Split, "File split option (string: none, raw-size, compressed-size, num-iteration)"},
        {opt::FirstBranchNumber, "First branch number for split file (non-negative integer)"},
        {opt::MaxSize, "Max file size for file splitting (integer in Mega Bytes)"},
        {opt::Openmode, "File open option (string: create/new, recreate, read)"},
        {opt::Permissions, "File permissions after the file is closed"},
    };
    return desc;
}

//______________________________________________________________________________
void FileUtil::Init(const boost::program_options::variables_map &vm)
{
    auto count = [&vm](auto s) {
        return vm.count(s.data());
    };
    auto get = [&vm](auto s) -> std::string {
        if (vm.count(s.data()) < 1) {
            std::cout << " variable: " << s << " not found" << std::endl;
            return "";
        }
        return vm[s.data()].template as<std::string>();
    };

    if (count(opt::Run) > 0) {
        SetRunNumber(std::stoll(get(opt::Run)));
    }
    if (count(opt::RunNumber)) {
        std::cout << opt::RunNumber << " " << get(opt::RunNumber) << std::endl;
        SetRunNumber(std::stoll(get(opt::RunNumber)));
    }

    SetBufferSize(nestdaq::Stoi(get(opt::BufferSize)));
    SetCompBufSize(std::stoi(get(opt::CBufSize)));
    if (count(opt::Path) > 0) {
        SetFilePath(get(opt::Path));
    } else {
        if (count(opt::FileExtension) > 0) {
            SetExtension(get(opt::FileExtension));
        }
        if (count(opt::Prefix) > 0) {
            SetPrefix(get(opt::Prefix));
        }
        fRunNumberFormat = get(opt::RunNumberFormat);
        fBranchNumberFormat = get(opt::BranchNumberFormat);
    }
    if (count(opt::MaxSize) > 0) {
        SetMaxBranchSizeInMegaBytes(nestdaq::Stoi(get(opt::MaxSize)));
    }
    SetSplit(get(opt::Split));
    SetOpenmode(get(opt::Openmode));
    if (count(opt::Permissions) > 0) {
        SetPermissions(get(opt::Permissions));
    }
}

//______________________________________________________________________________
bool FileUtil::Open(std::string_view name)
{
    if (fName.empty()) {
        // if (!CheckDir(fPrefix, fOpenmode)) {
        //    return false;
        // }

        if (!fSplit) {
            fBranchNum = -1;
        }
        stdfs::path p = stdfs::path(fPrefix);
        p /= GenerateFileName(name);
        fName = p.string();
    } else if (fExt.empty()) {
        std::string ext;
        for (auto [k, v] : fileTypes) {
            if (fName.find(k.data()) != std::string::npos) {
                ext = k.data();
            }
        }
        if (ext != eopt::DAT) {
            SetExtension(ext);
        }
    }

    fOut.rdbuf()->pubsetbuf(fBuffer.data(), fBuffer.size());
    auto mode = std::ios::binary;
    stdfs::path filePath{fName};
    if (stdfs::exists(filePath)) {
        for (auto k : {
                    oopt::Create, oopt::New
                }) {
            if (k == fOpenmode) {
                std::cout << "The file is not opened because the file already exists. " << fName << std::endl;
                return false;
            }
        }
        if (fOpenmode == oopt::Update)
            mode |= std::ios::app;
    }
    fOut.open(fName.data(), mode);
    if (fName == "/dev/null") {
        fBranchNum = -1;
        fSplit = nullptr;
    }

    std::cout << " new file opened : " << fName << std::endl;
    fBranchRawSize.emplace_back();
    fBranchCompressedSize.emplace_back();
    fBranchNumIteration.emplace_back();
    fBranchFilePath.emplace_back(fName);
    return true;
}

//______________________________________________________________________________
void FileUtil::Print(std::string_view msg) const
{
    std::cout << MyFunc(__FUNCTION__) << ": " << msg            //
              << "\n name = " << fName                          //
              << "\n run = " << fRun                            //
              << "\n branch file num = " << fBranchNum          //
              << "\n date format = " << fDateFormat             //
              << "\n prefix = " << fPrefix                      //
              << "\n ext = " << fExt                            //
              << "\n comp buf size = " << fCompBufSize          //
              << "\n max branch file size = " << fMaxBranchSize //
              << "\n debug = " << fDebug << std::endl;
}

//______________________________________________________________________________
void FileUtil::SetExtension(std::string_view ext)
{
    std::cout << MyFunc(__FUNCTION__) << " " << ext << std::endl;
    if (fileTypes.find(ext) != fileTypes.end()) {
        fExt = ext;
        auto format = fileTypes.at(ext);
        // register function for in-memory data compression
        if (!fCompressor) {
            fCompressor = [format, this](const char *src, int n) -> std::vector<char> {
                return std::move(Compressor::Compress(src, n, format, n, fCompBufSize));
            };
        } else {
            std::cout << MyFunc(__FUNCTION__) << " compressor function is already set. nothing to do" << std::endl;
        }
    } else if (ext.empty()) {
        std::cout << " no extension --> default format" << std::endl;
    } else {
        std::cout << " unknown extension " << ext << std::endl;
    }
}

//______________________________________________________________________________
bool FileUtil::SetOpenmode(std::string_view mode)
{
    std::cout << MyFunc(__FUNCTION__) << " " << mode << std::endl;
    using oopt = OpenmodeOption;
    auto s = boost::to_lower_copy(std::string(mode.data()));
    for (auto k : {
                oopt::Create, oopt::New, oopt::Recreate, oopt::Read
            }) {
        if (s == k) {
            fOpenmode = mode.data();
            return true;
        }
    }
    return false;
}

//______________________________________________________________________________
void FileUtil::SetPermissions(std::string_view p)
{
    std::cout << MyFunc(__FUNCTION__) << " " << p << std::endl;
    if (p.empty())
        return;

    stdfs::file_status status = stdfs::status(fName.data());
    // octal number
    auto bits = std::stoi(p.data(), nullptr, 8);
    stdfs::perms perm = status.permissions();
    if (p.length() == 1) {
        fPermissions = static_cast<stdfs::perms>((bits & 07) << 6) | (perm & static_cast<stdfs::perms>(07077));
    } else if (p.length() >= 3) {
        fPermissions = static_cast<stdfs::perms>(bits) & stdfs::perms::mask;
    } else {
        std::cout << "invalid permission :" << p << "  length = " << p.length() << std::endl;
    }
}

//______________________________________________________________________________
bool FileUtil::SetSplit(std::string_view option)
{
    std::cout << MyFunc(__FUNCTION__) << " " << option << std::endl;
    auto s = boost::to_lower_copy(std::string(option.data()));
    // register function for split condition evaluation
    if (s == sopt::RawSize) {
        std::cout << " set split function (raw size)" << std::endl;
        fSplit = [this]() {
            // std::cout << " split by raw data size" << std::endl;
            return (fMaxBranchSize <= fBranchRawSize.back());
        };
    } else if (s == sopt::CompressedSize) {
        std::cout << " set split function (compressed size)" << std::endl;
        fSplit = [this]() {
            // std::cout << " split by compressed data size" << std::endl;
            return (fMaxBranchSize <= fBranchCompressedSize.back());
        };
    } else if (s == sopt::NumIteration) {
        std::cout << " set split function (number of iteration)" << std::endl;
        fSplit = [this]() {
            // std::cout << " split by number of iterations" << std::endl;
            return (fMaxBranchSize <= fBranchNumIteration.back());
        };
    } else {
        std::cout << " set split function (null)" << std::endl;
        fSplit = nullptr;
    }

    // std::cout << " !fSplit = " << (!fSplit) << std::endl;
    return false;
}

//______________________________________________________________________________
void FileUtil::Write(const char *src, std::size_t length)
{
    if (fExt.empty() || fExt == eopt::DAT) {
        // std::cout << MyFunc(__FUNCTION__) << " direct file write" << std::endl;
        fOut.write(src, length);
    } else {
        // std::cout << MyFunc(__FUNCTION__) << " compression before file write" << std::endl;
        auto ret = fCompressor(src, length);
        fOut.write(ret.data(), ret.size());
        fBranchCompressedSize.back() += ret.size();
    }

    fBranchRawSize.back() += length;
    ++fBranchNumIteration.back();

    // std::cout << " branch raw size = " << fBranchRawSize.back()
    //           << "\n compressed size = " << fBranchCompressedSize.back()
    //           << "\n num iteration = "  << fBranchNumIteration.back()
    //           << "\n branch.size() = "  << fBranchRawSize.size()
    //           << "\n branch num = "  << fBranchNum
    //           << "\n first branch num = "  << fFirstBranchNum
    //           << "\n max = " << fMaxBranchSize
    //           << "\n split function = " << (!fSplit)
    //           << std::endl;
    if (!fSplit) {
        // std::cout << "\n no split function" << std::endl;
        return;
    } else if (fSplit()) {
        // std::cout << "\n split function called" << std::endl;
        Close();
        if (fBranchNum >= 0) {
            ++fBranchNum;
        }
        Open();
    }
}

} // namespace nestdaq
