// dolphin_state_reader.h
//
// Walks the uncompressed PointerWrap stream of a Dolphin save state (state version 191,
// "RecompCore" fork at 10f7ac4fe4) field by field, mirroring every DoState() in the order the
// writing build serialised it.  std-only, C++20.
//
// Usage:
//   dsr::StateWalk w = dsr::walk_state(raw_data, raw_size);   // throws dsr::ParseError on failure
//   const dsr::Field* f = w.find("video/bpmem");              // path -> offset/size/kind
//   std::span<const uint8_t> bytes = w.bytes("hw/memory/ram");
//
// Field paths use '/' and "[i]" for repeated items, e.g. "video/texture_cache/entry[3]/config",
// "coretiming/event[2]/name", "ppc/ppcState/gpr".  All multi-byte values in the stream are in the
// host (little-endian) byte order of the machine that wrote the state.
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace dsr
{
enum class Kind : uint8_t
{
  Scalar,  // integer / float / bool, `value` holds it
  Bytes,   // opaque blob (POD struct, array, buffer)
  String,  // u32 length + chars; offset points at the length prefix, size includes it
  Count,   // u32 element count that precedes a variable-length list / vector
  Marker,  // DoMarker cookie (u32, must equal 0x42)
};

const char* kind_name(Kind k);

struct Field
{
  std::string path;
  uint64_t offset = 0;  // offset into the raw stream
  uint64_t size = 0;    // bytes occupied
  Kind kind = Kind::Bytes;
  // Scalars / counts / markers only:
  bool has_value = false;
  char vfmt = 'u';      // 'u' unsigned, 's' signed, 'f' float32, 'd' float64, 'b' bool
  uint64_t raw = 0;     // little-endian value zero-extended
  std::string text;     // String kind: the characters
  int64_t as_s64() const;
  uint64_t as_u64() const { return raw; }
  double as_double() const;
};

struct Section
{
  std::string name;       // the DoMarker name
  uint64_t begin = 0;     // offset where the section's data starts (after the previous marker)
  uint64_t marker_offset = 0;  // offset of the marker cookie itself
  uint64_t end = 0;       // marker_offset + 4
};

class ParseError : public std::runtime_error
{
public:
  using std::runtime_error::runtime_error;
};

struct StateWalk
{
  const uint8_t* data = nullptr;  // not owned; the buffer passed to walk_state must outlive this
  uint64_t size = 0;
  uint64_t end_offset = 0;  // where the walk ended
  bool ended_exactly = false;
  std::vector<Field> fields;
  std::vector<Section> sections;
  std::unordered_map<std::string, size_t> index;  // path -> fields[] index

  // Extra facts decided from the stream (informational).
  bool is_wii = false;
  std::string dsp_engine;    // "HLE" / "LLE"
  bool has_achievements = false;

  const Field* find(const std::string& path) const;
  // Raw bytes of the field (offset..offset+size) or an empty span if missing.
  std::span<const uint8_t> bytes(const std::string& path) const;
  // Convenience scalar lookups (throw ParseError if missing / not a scalar).
  uint64_t u64(const std::string& path) const;
  int64_t s64(const std::string& path) const;
  const Section* section(const std::string& name) const;
  // All fields whose path starts with `prefix`.
  std::vector<const Field*> with_prefix(const std::string& prefix) const;
};

// Walks the whole stream.  Throws ParseError (message names the marker / field, offset and the
// last fields read) if a marker cookie mismatches or the data runs out.  If the walk finishes
// but does not end on the last byte, `ended_exactly` is false and `end_offset` tells where.
StateWalk walk_state(const uint8_t* data, size_t size);

// VideoCommon TextureConfig as serialised by SerializeTexture (8 x u32 = 32 bytes).
// Texture payload size per layer/level is stride(format, max(w>>l,1)) * max(h>>l,1) (already given
// by the u32 length that follows the config, so a converter rarely needs to recompute it).
struct TexConfig
{
  uint32_t width, height, levels, layers, samples;
  uint32_t format;  // AbstractTextureFormat
  uint32_t flags;
  uint32_t type;    // AbstractTextureType (u8 in the struct at byte 28; bytes 29..31 are uninitialised padding)
};
// Decodes a 32-byte "config" field.  Returns false if the field is missing or not 32 bytes.
bool read_texconfig(const StateWalk& w, const std::string& path, TexConfig* out);

// ---- .sav container (sav_unpack.cpp, needs lz4) ----
struct SavInfo
{
  std::string game_id;
  std::string version_string;
  uint32_t version_cookie = 0;
  uint32_t state_version = 0;  // cookie - 0xBAADBABE
  double timestamp = 0;
  uint16_t compression = 0;
  uint64_t uncompressed_size = 0;
};
// Reads the 24-byte legacy header, version cookie/string, 16-byte extended header and the LZ4 chunks
// (or an uncompressed payload).  Throws ParseError on a malformed file.
std::vector<uint8_t> unpack_sav(const std::string& path, SavInfo* info = nullptr);
// The same from the file's bytes
std::vector<uint8_t> unpack_sav_data(const std::vector<uint8_t>& file, SavInfo* info = nullptr);
}  // namespace dsr
