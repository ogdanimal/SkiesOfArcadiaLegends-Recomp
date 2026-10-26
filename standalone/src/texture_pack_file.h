// The few Dolphin Common helpers texture_pack.cpp (the Dolphin build's
// VideoCommon/TexturePackImport.cpp, kept as it is there) calls: File::, Common::ScopeGuard,
// StringToPath, UTF16ToUTF8, DIR_SEP. Paths are UTF-8 strings, as in Dolphin. The Dreamcast modules
// use them too (dolphin_shim/Common/FileUtil.h adds what else they need).

#pragma once

#include <array>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using s64 = std::int64_t;

#ifdef _WIN32
#define DIR_SEP "\\"
#else
#define DIR_SEP "/"
#endif

inline std::filesystem::path StringToPath(const std::string& path)
{
  return std::filesystem::path(std::u8string(path.begin(), path.end()));
}

inline std::string PathToString(const std::filesystem::path& path)
{
  const std::u8string s = path.u8string();
  return std::string(s.begin(), s.end());
}

inline std::string UTF16ToUTF8(const std::u16string& in)
{
  std::string out;
  for (size_t i = 0; i < in.size(); ++i)
  {
    u32 c = in[i];
    if (c >= 0xD800 && c < 0xDC00 && i + 1 < in.size() && in[i + 1] >= 0xDC00 && in[i + 1] < 0xE000)
      c = 0x10000 + ((c - 0xD800) << 10) + (in[++i] - 0xDC00);
    if (c < 0x80)
    {
      out += static_cast<char>(c);
    }
    else if (c < 0x800)
    {
      out += static_cast<char>(0xC0 | (c >> 6));
      out += static_cast<char>(0x80 | (c & 0x3F));
    }
    else if (c < 0x10000)
    {
      out += static_cast<char>(0xE0 | (c >> 12));
      out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
      out += static_cast<char>(0x80 | (c & 0x3F));
    }
    else
    {
      out += static_cast<char>(0xF0 | (c >> 18));
      out += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
      out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
      out += static_cast<char>(0x80 | (c & 0x3F));
    }
  }
  return out;
}

namespace Common
{
class ScopeGuard
{
public:
  template <class F>
  ScopeGuard(F f) : m_f(std::move(f))
  {
  }
  ~ScopeGuard()
  {
    if (m_f)
      m_f();
  }
  ScopeGuard(const ScopeGuard&) = delete;
  ScopeGuard& operator=(const ScopeGuard&) = delete;

private:
  std::function<void()> m_f;
};
}  // namespace Common

namespace File
{
struct FSTEntry
{
  bool isDirectory = false;
  std::string physicalName;
  std::string virtualName;
  std::vector<FSTEntry> children;
};

inline FSTEntry ScanDirectoryTree(const std::string& directory, bool recursive)
{
  FSTEntry entry;
  entry.isDirectory = true;
  entry.physicalName = directory;
  std::error_code ec;
  for (std::filesystem::directory_iterator it(StringToPath(directory), ec), end; !ec && it != end;
       it.increment(ec))
  {
    FSTEntry child;
    child.virtualName = PathToString(it->path().filename());
    child.physicalName = PathToString(it->path());
    std::error_code type_ec;
    child.isDirectory = it->is_directory(type_ec);
    if (child.isDirectory && recursive)
      child.children = ScanDirectoryTree(child.physicalName, true).children;
    entry.children.push_back(std::move(child));
  }
  return entry;
}

inline bool Exists(const std::string& path)
{
  std::error_code ec;
  return std::filesystem::exists(StringToPath(path), ec);
}

inline bool IsDirectory(const std::string& path)
{
  std::error_code ec;
  return std::filesystem::is_directory(StringToPath(path), ec);
}

inline bool Delete(const std::string& path)
{
  std::error_code ec;
  return std::filesystem::remove(StringToPath(path), ec);
}

inline bool DeleteDirRecursively(const std::string& path)
{
  std::error_code ec;
  std::filesystem::remove_all(StringToPath(path), ec);
  return !ec;
}

// As Dolphin's: the folders of a path up to its last separator (all of it when it ends with one)
inline bool CreateFullPath(const std::string& path)
{
  const size_t slash = path.find_last_of("/\\");
  if (slash == std::string::npos)
    return true;
  std::error_code ec;
  std::filesystem::create_directories(StringToPath(path.substr(0, slash)), ec);
  return !ec;
}

inline bool Rename(const std::string& from, const std::string& to)
{
  std::error_code ec;
  std::filesystem::rename(StringToPath(from), StringToPath(to), ec);
  return !ec;
}

inline bool ReadFileToString(const std::string& path, std::string& out)
{
  std::ifstream in(StringToPath(path), std::ios::binary);
  if (!in)
    return false;
  out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  return true;
}

inline bool WriteStringToFile(const std::string& path, const std::string& text)
{
  std::ofstream out(StringToPath(path), std::ios::binary | std::ios::trunc);
  out.write(text.data(), static_cast<std::streamsize>(text.size()));
  return static_cast<bool>(out);
}

enum class SeekOrigin
{
  Begin,
  Current,
  End,
};

class IOFile
{
public:
  IOFile() = default;
  IOFile(const std::string& path, const char* mode) { Open(path, mode); }
  ~IOFile() { Close(); }
  IOFile(const IOFile&) = delete;
  IOFile& operator=(const IOFile&) = delete;
  IOFile(IOFile&& other) noexcept { *this = std::move(other); }
  IOFile& operator=(IOFile&& other) noexcept
  {
    if (this != &other)
    {
      Close();
      m_file = std::exchange(other.m_file, nullptr);
      m_good = std::exchange(other.m_good, true);
    }
    return *this;
  }

  bool Open(const std::string& path, const char* mode)
  {
    Close();
#ifdef _WIN32
    const std::wstring wide_mode(mode, mode + std::char_traits<char>::length(mode));
    m_file = _wfopen(StringToPath(path).c_str(), wide_mode.c_str());
#else
    m_file = std::fopen(path.c_str(), mode);
#endif
    m_good = m_file != nullptr;
    return m_good;
  }

  bool IsOpen() const { return m_file != nullptr; }
  bool IsGood() const { return m_good; }
  explicit operator bool() const { return IsGood() && IsOpen(); }
  std::FILE* GetHandle() const { return m_file; }
  void ClearError()
  {
    m_good = true;
    if (m_file)
      std::clearerr(m_file);
  }

  bool Close()
  {
    if (!m_file)
      return m_good;
    const bool ok = std::fclose(m_file) == 0 && m_good;
    m_file = nullptr;
    m_good = true;
    return ok;
  }

  template <typename T>
  bool ReadArray(T* data, size_t length, size_t* read = nullptr)
  {
    size_t n = 0;
    if (!m_file || length != (n = std::fread(data, sizeof(T), length, m_file)))
      m_good = false;
    if (read)
      *read = n;
    return m_good;
  }
  template <typename T, size_t N>
  bool ReadArray(std::array<T, N>* data, size_t* read = nullptr)
  {
    return ReadArray(data->data(), N, read);
  }
  bool ReadBytes(void* data, size_t length)
  {
    return ReadArray(static_cast<u8*>(data), length);
  }
  bool WriteBytes(const void* data, size_t length)
  {
    if (!m_file || std::fwrite(data, 1, length, m_file) != length)
      m_good = false;
    return m_good;
  }

  bool Seek(s64 offset, SeekOrigin origin)
  {
    const int whence = origin == SeekOrigin::Begin ? SEEK_SET :
                       origin == SeekOrigin::Current ? SEEK_CUR :
                                                       SEEK_END;
#ifdef _WIN32
    if (!m_file || _fseeki64(m_file, offset, whence) != 0)
#else
    if (!m_file || fseeko(m_file, offset, whence) != 0)
#endif
      m_good = false;
    return m_good;
  }
  u64 Tell() const
  {
#ifdef _WIN32
    return m_file ? static_cast<u64>(_ftelli64(m_file)) : 0;
#else
    return m_file ? static_cast<u64>(ftello(m_file)) : 0;
#endif
  }
  u64 GetSize()
  {
    if (!m_file)
      return 0;
    const u64 pos = Tell();
    Seek(0, SeekOrigin::End);
    const u64 size = Tell();
    Seek(static_cast<s64>(pos), SeekOrigin::Begin);
    return size;
  }

private:
  std::FILE* m_file = nullptr;
  bool m_good = true;
};
}  // namespace File
