#ifndef NESTDAQ_UTILITY_FILE_UTIL_H_
#define NESTDAQ_UTILITY_FILE_UTIL_H_

#include <cstdint>
#include <string>
#include <functional>
#include <string_view>
#include <fstream>
#include <unordered_map>

#include "utility/filesystem.h"

namespace boost::program_options {
class options_description;
class variables_map;
} // namespace boost::program_options

namespace nestdaq {

// filename rule:
// if "--path" is given, the value is used.
// if (fRun>=0) then
//   prefix + fRun (formatted)  + (+ fBranchNum (formatted)) + .extension
// else
//   prefix + "yyyymmddTHHMMSS" + (+ fBranchNum (formatted)) + .extension

class FileUtil {
public:
    static std::string ToFormattedString(const std::string &form, int64_t a);
    struct OptionKey {
        [[deprecated]] static constexpr std::string_view Run{"run"};

        static constexpr std::string_view RunNumber{"run_number"};
        static constexpr std::string_view RunNumberFormat{"run-number-format"};
        static constexpr std::string_view Path{"path"};
        static constexpr std::string_view BufferSize{"buffer-size"};
        static constexpr std::string_view CBufSize{"cbuf-size"};
        static constexpr std::string_view FileExtension{"ext"};
        static constexpr std::string_view FirstBranchNumber{"first-branch-number"};
        static constexpr std::string_view BranchNumberFormat{"branch-number-format"};
        static constexpr std::string_view Prefix{"prefix"};
        static constexpr std::string_view MaxSize{"max-size"};
        static constexpr std::string_view Split{"split"};
        static constexpr std::string_view Openmode{"openmode"};
        static constexpr std::string_view Permissions{"permissions"};
    };

    struct SplitOption {
        static constexpr std::string_view None{"none"};
        static constexpr std::string_view RawSize{"raw-size"};
        static constexpr std::string_view CompressedSize{"compressed-size"};
        static constexpr std::string_view NumIteration{"num-iteration"};
    };

    struct OpenmodeOption {
        static constexpr std::string_view Create{"create"};
        static constexpr std::string_view New{"new"};
        static constexpr std::string_view Recreate{"recreate"};
        static constexpr std::string_view Update{"update"};
        static constexpr std::string_view Read{"read"};
    };

    struct ExtensionOption {
        static constexpr std::string_view DAT{".dat"};
        static constexpr std::string_view GZ{".gz"};
        static constexpr std::string_view BZ2{".bz2"};
        static constexpr std::string_view XZ{".xz"};
        static constexpr std::string_view ZSTD{".zst"};
    };

    static const std::string MyFile;
    static const std::string MyClass;

    using CompressFunc = std::function<std::vector<char>(const char *, int)>;
    using SplitFunc = std::function<bool(void)>;

public:
    FileUtil() = default;
    FileUtil(const FileUtil &) = delete;
    FileUtil &operator=(const FileUtil &) = delete;
    ~FileUtil() = default;

    static void AddOptions(boost::program_options::options_description &options,
                           const std::unordered_map<std::string, std::string> &v = {});

    bool CheckDir(std::string_view dirName, std::string_view openmode) const;
    void ClearBranch();
    void Close();
    void Flush() {
        fOut.flush();
    }
    const std::string GenerateFileName(std::string_view name = "");
    std::ofstream &Get() {
        return fOut;
    }
    const std::vector<uint64_t> &GetBranchCompressedSize() const {
        return fBranchCompressedSize;
    }
    const std::vector<std::string> &GetBranchFilePath() const {
        return fBranchFilePath;
    }
    const std::vector<uint64_t> &GetBranchNumIteration() const {
        return fBranchNumIteration;
    }
    const std::vector<uint64_t> &GetBranchRawSize() const {
        return fBranchRawSize;
    }
    std::size_t GetBufferSize() const {
        return fBuffer.size();
    }
    int GetCompBufSize() const {
        return fCompBufSize;
    }
    CompressFunc GetCompressFunc() const {
        return fCompressor;
    }
    const std::string GetDateFormat() const {
        return fDateFormat;
    }
    static const std::unordered_map<std::string_view, std::string_view> GetDescriptions();
    const std::string GetExtension() const {
        return fExt;
    }
    const std::string GetFilePath() const {
        return fName;
    }
    int64_t GetMaxBranchSize() const {
        return fMaxBranchSize;
    }
    int64_t GetMaxBranchSizeInMegaBytes() const {
        return fMaxBranchSize / 1024 / 1024;
    }
    const std::string GetPrefix() const {
        return fPrefix;
    }
    int64_t GetRunNumber() const {
        return fRun;
    }
    void Init(const boost::program_options::variables_map &vm);
    static const std::string MyFunc(const std::string &f, const std::string &arg = "")
    {
        return MyClass + "::" + f + "(" + arg + ")";
    }

    bool Open(std::string_view name = "");
    void Print(std::string_view msg = "") const;
    void SetBranchNumberFormat(std::string_view f) {
        fBranchNumberFormat = f.data();
    }
    void SetBufferSize(int n) {
        fBuffer.resize(n);
    }
    void SetCompBufSize(int n) {
        fCompBufSize = n;
    }
    void SetCompressFunc(CompressFunc f) {
        fCompressor = f;
    }
    void SetDateFormat(std::string_view f) {
        fDateFormat = f.data();
    }
    void SetExtension(std::string_view ext);
    void SetFilePath(std::string_view path) {
        fName = path;
    }
    void SetMaxBranchSize(int64_t n) {
        fMaxBranchSize = n;
    }
    void SetMaxBranchSizeInMegaBytes(int64_t n) {
        SetMaxBranchSize(n * 1024 * 1024);
    }
    bool SetOpenmode(std::string_view mode);
    void SetPermissions(std::string_view p);
    void SetPrefix(std::string_view prefix) {
        fPrefix = prefix.data();
    }
    void SetRunNumber(int64_t runNumber) {
        fRun = runNumber;
    }
    void SetRunNumberFormat(std::string_view f) {
        fRunNumberFormat = f.data();
    }
    bool SetSplit(std::string_view option);
    void Write(const char *src, std::size_t length);

private:
    std::string fName;
    int64_t fRun{-1};
    std::string fRunNumberFormat;

    int fFirstBranchNum{0};
    int fBranchNum{-1};
    std::string fBranchNumberFormat;
    std::ofstream fOut;
    std::string fDateFormat;
    stdfs::perms fPermissions{stdfs::perms::unknown};

    std::vector<uint64_t> fBranchRawSize;
    std::vector<uint64_t> fBranchCompressedSize;
    std::vector<std::string> fBranchFilePath;
    std::vector<uint64_t> fBranchNumIteration;

    std::string fPrefix;
    std::string fExt;
    std::vector<char> fBuffer;
    CompressFunc fCompressor;
    SplitFunc fSplit;

    int fCompBufSize{-1};
    uint64_t fMaxBranchSize; // {1024 * 1024 * 1024};
    std::string fOpenmode;
    int fDebug{0};
};

} // namespace nestdaq

#endif // NESTDAQ_UTILITY_FILE_UTIL_H_H
