// Skies of Arcadia Legends "Uncensored" converter: applies a manifest entry (a list of model / texture
// edits) to a GameCube MLD file, using the Japanese Dreamcast MLD as the source; for Gilder's victory
// pose, to an MLK archive or a battle script. Port of the Python prototype (uncensor.py,
// mldsplice.py, block2gc.py, nj2gc.py, pvr.py, gvr.py, aklzc.py, victory.py), verified byte-exact
// against its outputs. Standard library only, no Dolphin headers, no JSON.
#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace DiscIO::SkiesUncensor
{
struct Op
{
  enum class Kind
  {
    Model,    // gc_entry, slot, gc_sha1 (GameCube block), jp_entry, jp_sha1 (Dreamcast block)
    Tex,      // name, gc_sha1, jp_sha1: re-encode a texture the GameCube file has
    TexAdd,   // name, after, jp_sha1: add a Japanese-only texture after texture `after`
    TexBlank,    // name: zero the pixels of a texture nothing uses any more
    MlkEntry,    // id, tex_index: replace an MLK archive entry with the converted Dreamcast one
    StdRecords,  // runs, layouts: rebuild a battle script from GameCube and Japanese records
    Pad,         // zero-pad to the original's decompressed size
    Motion,      // gc_entry, slot, gc_sha1, jp_entry, jp_sha1: replace a motion (NMDM) block
    TexSwap      // name, new_name, gc_sha1, jp_sha1: texture `name`'s slot becomes `new_name`
  };
  // A run of battle-script records: GameCube records first..first+count-1, or Japanese ones
  // converted with layouts["type/size"] (a letter per word: H 16-bit pair, W 32-bit, B bytes kept)
  struct Run
  {
    bool jp = false;
    uint32_t first = 0;
    uint32_t count = 0;
  };
  Kind kind = Kind::Model;
  std::string name;
  std::string after;
  std::string new_name;
  std::string gc_sha1;
  std::string jp_sha1;
  uint32_t gc_entry = 0;
  uint32_t slot = 0;
  uint32_t jp_entry = 0;
  uint32_t id = 0;
  uint32_t tex_index = 0;
  std::vector<Run> runs;
  std::map<std::string, std::string> layouts;
};

// One manifest file entry (hashes are lowercase hex SHA-1 strings).
struct FileOps
{
  std::string gc_sha1;   // the original (AKLZ) GameCube file
  std::string out_sha1;  // the reference output file (informational; the compressed bytes may differ)
  std::string dc_sha1;   // the Japanese Dreamcast MLD
  std::string dec_sha1;  // the decompressed converted file
  uint32_t size = 0;     // size of the converted (compressed) file; equals the original's
  int dc_disc = 0;
  std::string dc_path;
  std::vector<Op> ops;
};

// gc_file: the AKLZ-compressed GameCube file; dc_file: the raw Dreamcast file. Verifies every hash
// and returns false with a message in *error on any mismatch or malformed data. *out is the
// AKLZ-compressed converted file, exactly entry.size bytes.
bool Convert(const FileOps& entry, const std::vector<uint8_t>& gc_file,
             const std::vector<uint8_t>& dc_file, std::vector<uint8_t>* out, std::string* error);

// AKLZ helpers. Decompress optionally reports how many input bytes were consumed.
bool AklzDecompress(const uint8_t* data, size_t size, std::vector<uint8_t>* out,
                    size_t* consumed = nullptr);
// Compresses to exactly `size` bytes (the game's decoder stops on input exhaustion); false if the
// data does not fit.
bool AklzCompress(const std::vector<uint8_t>& data, size_t size, std::vector<uint8_t>* out,
                  std::string* error);

std::string Sha1Hex(const uint8_t* data, size_t size);
}  // namespace DiscIO::SkiesUncensor
