// dolphin_state_reader.cpp -- see dolphin_state_reader.h
//
// Every mirror function below is named after the Dolphin DoState it mirrors and follows its
// field order exactly.  Source tree: RecompCore (Dolphin 10f7ac4fe4 + project patches),
// Source/Core/{Core/State.cpp, VideoCommon/VideoState.cpp, Core/HW/HW.cpp, ...}.
//
// PointerWrap encodings (Common/ChunkFile.h, write mode):
//   Do(T)              POD: sizeof(T) raw bytes, host (little-endian) order, padding included
//   Do(bool)           1 byte
//   Do(atomic<T>)      the T
//   Do(Common::Flag)   1 byte
//   Do(vector/string)  u32 count, then count*sizeof(T) raw bytes (DoArray, POD elements)
//   Do(deque/list)     u32 count, then each element via Do
//   Do(map<K,V>)       u32 count, then Do(key) Do(value) per element (iteration order)
//   Do(set<V>)         u32 count, then each value
//   Do(optional<T>)    1 byte present, then T if present
//   Do(pair<A,B>)      Do(first) Do(second)  (no padding between the two)
//   DoArray(T*, n)     n*sizeof(T) raw bytes, no count
//   DoPointer          ptrdiff_t (8 bytes) offset from the base
//   DoExternal(count)  u32 count, then `count` raw bytes
//   DoMarker           u32 cookie (0x42 unless another number is passed)
#include "dolphin_state_reader.h"

#include <cstdio>
#include <cstring>
#include <deque>
#include <string_view>

namespace dsr
{
// -------------------------------------------------------------------------------------------
// Struct sizes taken from the Dolphin definitions.  "probe" = confirmed by compiling a program
// against the real headers (probe/p1.cpp .. p3.cpp, x86-64 Linux, g++ -std=c++23); "reason" =
// derived by reading the definition.  See the report / probe/README for the table.
// -------------------------------------------------------------------------------------------
namespace sz
{
constexpr uint64_t BPMEM = 1024;           // probe: sizeof(BPMemory)
constexpr uint64_t CPSTATE = 240;          // probe: sizeof(CPState)
constexpr uint64_t XFMEM = 16736;          // probe: sizeof(XFMemory)
constexpr uint64_t TEXMEM = 1024 * 1024;   // TMEM_SIZE = 1 MiB (s_tex_mem)
constexpr uint64_t TMEM_UNITS = 8 * 36;    // probe-equivalent: 8 x TextureUnitState (2*16 + int)
constexpr uint64_t FIFO_BUF = 2 * 1024 * 1024;  // FifoManager::FIFO_SIZE
constexpr uint64_t PS_CONST = 1536;        // probe: sizeof(PixelShaderConstants)
constexpr uint64_t VS_CONST = 4112;        // probe: sizeof(VertexShaderConstants)
constexpr uint64_t GS_CONST = 64;          // probe: sizeof(GeometryShaderConstants)
constexpr uint64_t TEXCONFIG = 32;         // probe: sizeof(TextureConfig)
constexpr uint64_t MI_MEM = 72;            // probe: MemoryInterfaceManager::m_mi_mem
constexpr uint64_t MEMCARD_HDR = 38;       // probe: sizeof(Memcard::HeaderData) (pack(1))
constexpr uint64_t SRAM = 68;              // static_assert sizeof(Sram) == 0x44
constexpr uint64_t READREQ = 72;           // probe: DVD::DVDThread::ReadRequest (with padding)
constexpr uint64_t GATHER_PIPE = 512;      // probe: GPFifoManager::m_gather_pipe
constexpr uint64_t ARAM = 0x01000000;      // DSP::ARAM_SIZE (GC)
}  // namespace sz

// Runtime settings the stream does not encode; the values below are what the game's release
// builds use (verified against the 10 test states -- every marker lines up).
struct Cfg
{
  uint64_t aram_size = sz::ARAM;       // DSPManager::m_aram.size, GC (wii_mode == false)
  bool freelook_controller_present = true;  // controller exists whenever m_current_type is set
  bool dsp_lle_jit = false;            // DSPEmitter::DoState adds m_cycles_left if the DSP JIT is on
  bool triforce = false;               // DVDInterface::DoState: AMMediaboard section
};

const char* kind_name(Kind k)
{
  switch (k)
  {
  case Kind::Scalar: return "scalar";
  case Kind::Bytes: return "bytes";
  case Kind::String: return "string";
  case Kind::Count: return "count";
  case Kind::Marker: return "marker";
  }
  return "?";
}

int64_t Field::as_s64() const
{
  if (vfmt == 's')
  {
    if (size >= 8) return static_cast<int64_t>(raw);
    const uint64_t sign = 1ull << (size * 8 - 1);
    return static_cast<int64_t>((raw ^ sign) - sign);
  }
  return static_cast<int64_t>(raw);
}

double Field::as_double() const
{
  if (vfmt == 'f')
  {
    float f;
    uint32_t r = static_cast<uint32_t>(raw);
    std::memcpy(&f, &r, 4);
    return f;
  }
  if (vfmt == 'd')
  {
    double d;
    std::memcpy(&d, &raw, 8);
    return d;
  }
  return vfmt == 's' ? static_cast<double>(as_s64()) : static_cast<double>(raw);
}

const Field* StateWalk::find(const std::string& path) const
{
  auto it = index.find(path);
  return it == index.end() ? nullptr : &fields[it->second];
}

std::span<const uint8_t> StateWalk::bytes(const std::string& path) const
{
  const Field* f = find(path);
  if (!f) return {};
  return {data + f->offset, static_cast<size_t>(f->size)};
}

uint64_t StateWalk::u64(const std::string& path) const
{
  const Field* f = find(path);
  if (!f || !f->has_value) throw ParseError("no scalar field " + path);
  return f->raw;
}

int64_t StateWalk::s64(const std::string& path) const
{
  const Field* f = find(path);
  if (!f || !f->has_value) throw ParseError("no scalar field " + path);
  return f->as_s64();
}

const Section* StateWalk::section(const std::string& name) const
{
  for (const auto& s : sections)
    if (s.name == name) return &s;
  return nullptr;
}

std::vector<const Field*> StateWalk::with_prefix(const std::string& prefix) const
{
  std::vector<const Field*> r;
  for (const auto& f : fields)
    if (f.path.compare(0, prefix.size(), prefix) == 0) r.push_back(&f);
  return r;
}

bool read_texconfig(const StateWalk& w, const std::string& path, TexConfig* out)
{
  auto b = w.bytes(path);
  if (b.size() != sizeof(TexConfig)) return false;
  std::memcpy(out, b.data(), 28);
  out->type = b[28];  // AbstractTextureType : u8, followed by 3 padding bytes
  return true;
}

// -------------------------------------------------------------------------------------------
namespace
{
class R
{
public:
  R(const uint8_t* d, size_t n, StateWalk& o) : data_(d), size_(n), out_(o) {}

  uint64_t pos() const { return pos_; }
  uint64_t remaining() const { return size_ - pos_; }
  Cfg cfg;

  // ---- path scopes ----
  struct Scope
  {
    R& r;
    size_t len;
    Scope(R& rr, const std::string& name) : r(rr), len(rr.prefix_.size())
    {
      r.prefix_ += name;
      r.prefix_ += '/';
    }
    ~Scope() { r.prefix_.resize(len); }
  };
  static std::string idx(const char* base, uint64_t i) { return std::string(base) + "[" + std::to_string(i) + "]"; }

  // ---- primitives ----
  uint64_t scalar(const std::string& name, unsigned bytes, char fmt)
  {
    need(bytes, name);
    uint64_t v = 0;
    std::memcpy(&v, data_ + pos_, bytes);  // little-endian host assumed by the stream
    Field& f = add(name, bytes, Kind::Scalar);
    f.has_value = true;
    f.vfmt = fmt;
    f.raw = v;
    pos_ += bytes;
    return v;
  }
  uint8_t u8(const std::string& n) { return static_cast<uint8_t>(scalar(n, 1, 'u')); }
  uint16_t u16(const std::string& n) { return static_cast<uint16_t>(scalar(n, 2, 'u')); }
  uint32_t u32(const std::string& n) { return static_cast<uint32_t>(scalar(n, 4, 'u')); }
  uint64_t u64(const std::string& n) { return scalar(n, 8, 'u'); }
  int32_t s32(const std::string& n) { return static_cast<int32_t>(scalar(n, 4, 's')); }
  int64_t s64(const std::string& n) { return static_cast<int64_t>(scalar(n, 8, 's')); }
  void f32(const std::string& n) { scalar(n, 4, 'f'); }
  void f64(const std::string& n) { scalar(n, 8, 'd'); }
  bool boolean(const std::string& n)
  {
    uint64_t v = scalar(n, 1, 'b');
    return v != 0;
  }
  // u32 element count preceding a list
  uint32_t count(const std::string& n)
  {
    need(4, n);
    uint32_t v;
    std::memcpy(&v, data_ + pos_, 4);
    Field& f = add(n, 4, Kind::Count);
    f.has_value = true;
    f.raw = v;
    pos_ += 4;
    return v;
  }
  void blob(const std::string& name, uint64_t n)
  {
    need(n, name);
    add(name, n, Kind::Bytes);
    pos_ += n;
  }
  // std::string: u32 length + chars
  std::string str(const std::string& name)
  {
    need(4, name);
    uint32_t len;
    std::memcpy(&len, data_ + pos_, 4);
    need(4ull + len, name);
    Field& f = add(name, 4ull + len, Kind::String);
    f.text.assign(reinterpret_cast<const char*>(data_ + pos_ + 4), len);
    pos_ += 4ull + len;
    return f.text;
  }
  // std::vector<POD>: u32 count + count*elem bytes.  Records <name>/count and <name> (Bytes).
  uint32_t vec(const std::string& name, uint64_t elem)
  {
    Scope s(*this, name);
    uint32_t n = count("count");
    blob("data", static_cast<uint64_t>(n) * elem);
    return n;
  }
  // DoExternal: u32 count + raw bytes (same wire shape as vec with elem=1)
  uint32_t external(const std::string& name) { return vec(name, 1); }

  void marker(const std::string& name, uint32_t expect = 0x42)
  {
    const uint64_t at = pos_;
    need(4, "marker " + name);
    uint32_t v;
    std::memcpy(&v, data_ + pos_, 4);
    if (v != expect)
    {
      char b[256];
      std::snprintf(b, sizeof b,
                    "marker \"%s\" mismatch at offset %llu (0x%llx): found 0x%x, expected 0x%x\n",
                    name.c_str(), (unsigned long long)at, (unsigned long long)at, v, expect);
      throw ParseError(std::string(b) + "section began at offset " + std::to_string(sec_begin_) +
                       "\nlast fields before it:\n" + last_fields(10));
    }
    std::string saved = prefix_;
    prefix_ = "marker/";
    Field& f = add(name, 4, Kind::Marker);
    prefix_ = saved;
    f.has_value = true;
    f.raw = v;
    pos_ += 4;
    Section s;
    s.name = name;
    s.begin = sec_begin_;
    s.marker_offset = at;
    s.end = pos_;
    out_.sections.push_back(s);
    sec_begin_ = pos_;
  }

  std::string last_fields(size_t n) const
  {
    std::string r;
    size_t start = out_.fields.size() > n ? out_.fields.size() - n : 0;
    for (size_t i = start; i < out_.fields.size(); i++)
    {
      const Field& f = out_.fields[i];
      char b[512];
      std::snprintf(b, sizeof b, "  %s  @%llu +%llu %s\n", f.path.c_str(),
                    (unsigned long long)f.offset, (unsigned long long)f.size, kind_name(f.kind));
      r += b;
    }
    return r;
  }

  void need(uint64_t n, const std::string& what)
  {
    if (pos_ + n > size_ || pos_ + n < pos_)
      throw ParseError("stream ends: need " + std::to_string(n) + " bytes for \"" + prefix_ + what +
                       "\" at offset " + std::to_string(pos_) + " but only " +
                       std::to_string(size_ - pos_) + " remain\nlast fields:\n" + last_fields(10));
  }

  bool at_end() const { return pos_ == size_; }

private:
  Field& add(const std::string& name, uint64_t size, Kind kind)
  {
    Field f;
    f.path = prefix_ + name;
    f.offset = pos_;
    f.size = size;
    f.kind = kind;
    std::string key = f.path;
    if (out_.index.count(key))
    {
      int n = 2;
      while (out_.index.count(key + "#" + std::to_string(n))) n++;
      f.path = key + "#" + std::to_string(n);
    }
    out_.index[f.path] = out_.fields.size();
    out_.fields.push_back(std::move(f));
    return out_.fields.back();
  }

  const uint8_t* data_;
  uint64_t size_;
  uint64_t pos_ = 0;
  uint64_t sec_begin_ = 0;
  std::string prefix_;
  StateWalk& out_;
};

using Scope = R::Scope;

// ===========================================================================================
// Movie::MovieManager::DoState                     Core/Movie.cpp:940
void do_movie(R& r)
{
  Scope s(r, "movie");
  r.u64("current_frame");
  r.u64("current_byte");
  r.u64("current_lag_count");
  r.u64("current_input_count");
  r.boolean("polled");
  r.u64("tick_count_at_last_input");
}

// TextureCacheBase::SerializeTexture / DeserializeTexture: TextureConfig, DoExternal blob
void do_serialized_texture(R& r, const std::string& name)
{
  Scope s(r, name);
  r.blob("config", sz::TEXCONFIG);
  r.external("data");  // u32 total_size + that many bytes (all layers, all levels)
}

// ------------------------------------------------------------------------------------------
// VideoCommon sub-sections
void do_tmem(R& r)  // TMEM::DoState: DoArray(s_unit) -- 8 x TextureUnitState{even,odd BankConfig, state}
{
  Scope s(r, "tmem");
  for (int i = 0; i < 8; i++)
  {
    Scope u(r, R::idx("unit", i));
    for (const char* bank : {"even", "odd"})
    {
      Scope b(r, bank);
      r.u32("width");
      r.u32("height");
      r.u32("base");
      r.u32("size");
    }
    r.u32("state");
  }
}

void do_fifo(R& r)  // Fifo::FifoManager::DoState
{
  Scope s(r, "fifo");
  r.blob("video_buffer", sz::FIFO_BUF);  // DoArray(m_video_buffer, FIFO_SIZE)
  r.s64("write_ptr_offset");             // DoPointer: ptrdiff_t from m_video_buffer
  r.s64("read_ptr_offset");
  r.s32("sync_ticks");
  r.boolean("syncing_suspended");
}

void do_command_processor(R& r)  // CommandProcessorManager::DoState + SCPFifoStruct::DoState
{
  Scope s(r, "command_processor");
  r.u16("status_reg");
  r.u16("ctrl_reg");
  r.u16("clear_reg");
  r.u16("perf_select");
  r.u16("unk_0a_reg");
  {
    Scope f(r, "fifo");
    for (const char* n : {"CPBase", "CPEnd", "CPHiWatermark", "CPLoWatermark", "CPReadWriteDistance",
                          "CPWritePointer", "CPReadPointer", "CPBreakpoint", "SafeCPReadPointer",
                          "bFF_GPLinkEnable", "bFF_GPReadEnable", "bFF_BPEnable", "bFF_BPInt",
                          "bFF_Breakpoint", "bFF_LoWatermarkInt", "bFF_HiWatermarkInt",
                          "bFF_LoWatermark", "bFF_HiWatermark"})
      r.u32(n);
  }
  r.boolean("interrupt_set");
  r.boolean("interrupt_waiting");
}

void do_pixel_engine(R& r)  // PixelEngineManager::DoState
{
  Scope s(r, "pixel_engine");
  for (const char* n : {"z_conf", "alpha_conf", "dst_alpha_conf", "alpha_mode_conf", "alpha_read",
                        "control", "token", "token_pending"})
    r.u16(n);
  for (const char* n : {"token_interrupt_pending", "finish_interrupt_pending", "event_raised",
                        "signal_token_interrupt", "signal_finish_interrupt"})
    r.boolean(n);
}

void do_pixel_shader_manager(R& r)
{
  Scope s(r, "pixel_shader_manager");
  r.boolean("fog_range_adjusted_changed");
  r.boolean("viewport_changed");
  r.boolean("indirect_dirty");
  r.boolean("dest_alpha_dirty");
  r.blob("constants", sz::PS_CONST);
}

void do_vertex_shader_manager(R& r)
{
  Scope s(r, "vertex_shader_manager");
  r.blob("projection_matrix", 64);  // std::array<float,16>
  {
    // FreeLookCamera::DoState (write mode): optional<ControlType> then, if a controller exists, its state
    Scope f(r, "freelook");
    bool present = r.boolean("type_present");
    if (present)
    {
      uint32_t type = r.u32("type");  // FreeLook::ControlType : int  {SixAxis=0, FPS=1, Orbital=2}
      if (r.cfg.freelook_controller_present)
      {
        // CameraControllerInput::DoState
        r.f32("speed");
        r.f32("fov_x_multiplier");
        r.f32("fov_y_multiplier");
        if (type == 0) r.blob("mat", 64);  // SixAxisController: Matrix44
        else if (type == 1)                // FPSController: Vec3 rotation, Quaternion, Vec3 position
        {
          r.blob("rotation", 12);
          r.blob("rotate_quat", 16);
          r.blob("position", 12);
        }
        else if (type == 2)  // OrbitalController: Vec3 rotation, Quaternion, float distance
        {
          r.blob("rotation", 12);
          r.blob("rotate_quat", 16);
          r.f32("distance");
        }
        else throw ParseError("unknown free look control type " + std::to_string(type));
      }
    }
  }
  r.blob("constants", sz::VS_CONST);
}

void do_geometry_shader_manager(R& r)
{
  Scope s(r, "geometry_shader_manager");
  r.boolean("projection_changed");
  r.boolean("viewport_changed");
  r.blob("constants", sz::GS_CONST);
}

void do_vertex_manager(R& r)  // VertexManagerBase::DoState
{
  Scope s(r, "vertex_manager");
  r.blob("zslope", 16);  // Slope{dfdx,dfdy,f0,dirty(bool + 3 pad)}
  r.blob("normal_cache", 16);
  r.blob("tangent_cache", 16);
  r.blob("binormal_cache", 16);
}

void do_framebuffer_manager(R& r)  // FramebufferManager::DoState
{
  Scope s(r, "framebuffer");
  r.u32("prev_efb_format");  // PixelFormat : u32
  bool save = r.boolean("save_efb_state");  // GFX_SAVE_TEXTURE_CACHE_TO_STATE, stored in the stream
  if (!save) return;
  do_serialized_texture(r, "efb_color");  // ResolveEFBColorTexture, RGBA8-like host format
  do_serialized_texture(r, "efb_depth");
}

void do_texture_cache(R& r)  // TextureCacheBase::DoState / DoSaveState
{
  Scope s(r, "texture_cache");
  r.u64("last_entry_id");
  uint32_t n = r.count("entry_count");
  for (uint32_t i = 0; i < n; i++)
  {
    Scope e(r, R::idx("entry", i));
    r.blob("config", sz::TEXCONFIG);
    r.external("data");
    // TCacheEntry::DoState
    r.u32("addr");
    r.u32("size_in_bytes");
    r.u64("base_hash");
    r.u64("hash");
    r.u32("format_texfmt");  // TextureAndTlutFormat{TextureFormat texfmt; TLUTFormat tlutfmt;} (2 x u32)
    r.u32("format_tlutfmt");
    r.u32("memory_stride");
    for (const char* b : {"is_efb_copy", "is_custom_tex", "may_have_overlapping_textures",
                          "invalidated", "has_arbitrary_mips", "should_force_safe_hashing",
                          "is_xfb_copy", "is_xfb_container"})
      r.boolean(b);
    r.u64("id");
    r.boolean("reference_changed");
    r.u32("native_width");
    r.u32("native_height");
    r.u32("native_levels");
    r.u32("frameCount");
  }
  r.marker("TextureCacheEntries");
  struct L
  {
    const char* name;
    unsigned a, b;
  };
  // pair<u32,u32> references; pair<u32,u32> by_address; pair<u64,u32> by_hash; pair<u32,u32> bound
  const L lists[] = {{"references", 4, 4}, {"by_address", 4, 4}, {"by_hash", 8, 4}, {"bound", 4, 4}};
  for (const L& l : lists)
  {
    Scope ls(r, l.name);
    uint32_t c = r.count("count");
    for (uint32_t i = 0; i < c; i++)
    {
      Scope p(r, R::idx("item", i));
      r.scalar("first", l.a, 'u');
      r.scalar("second", l.b, 'u');
    }
  }
}

void do_presenter(R& r)  // Presenter::DoState (FrameDumper::DoState writes nothing)
{
  Scope s(r, "presenter");
  r.u64("frame_count");
  r.u64("last_xfb_ticks");
  r.u32("last_xfb_addr");
  r.u32("last_xfb_width");
  r.u32("last_xfb_stride");
  r.u32("last_xfb_height");
}

void do_bounding_box(R& r)  // BoundingBox::DoState (write mode)
{
  Scope s(r, "bounding_box");
  r.blob("fallback", 8);  // array<u16,4>
  r.boolean("is_active");
  r.blob("values", 16);   // array<s32,4>
  r.blob("dirty", 4);     // array<bool,4>
  r.boolean("is_valid");
  r.vec("backend_values", 4);  // vector<s32>
}

void do_xf_state_manager(R& r)
{
  Scope s(r, "xf_state_manager");
  r.blob("minmax_transform_matrices_changed", 8);  // array<int,2>
  r.blob("minmax_normal_matrices_changed", 8);
  r.blob("minmax_post_transform_matrices_changed", 8);
  r.blob("minmax_lights_changed", 8);
  r.u32("materials_changed");  // BitSet32
  r.blob("tex_matrices_changed", 2);  // array<bool,2>
  r.boolean("pos_normal_matrix_changed");
  r.boolean("projection_changed");
  r.boolean("viewport_changed");
  r.boolean("tex_mtx_info_changed");
  r.boolean("lighting_config_changed");
}

// VideoBackendBase::DoState -> VideoCommon_DoState                 VideoCommon/VideoState.cpp
void do_video(R& r)
{
  Scope s(r, "video");
  r.boolean("software");
  r.blob("bpmem", sz::BPMEM);
  r.marker("BP Memory");
  r.blob("cpmem", sz::CPSTATE);  // g_main_cp_state
  r.marker("CP Memory");
  r.blob("xfmem", sz::XFMEM);
  r.marker("XF Memory");
  r.blob("texmem", sz::TEXMEM);  // s_tex_mem (TMEM backing store)
  r.marker("texMem");
  do_tmem(r);
  r.marker("TMEM");
  do_fifo(r);
  r.marker("Fifo");
  do_command_processor(r);
  r.marker("CommandProcessor");
  do_pixel_engine(r);
  r.marker("PixelEngine");
  do_pixel_shader_manager(r);
  r.marker("PixelShaderManager");
  do_vertex_shader_manager(r);
  r.marker("VertexShaderManager");
  do_geometry_shader_manager(r);
  r.marker("GeometryShaderManager");
  do_vertex_manager(r);
  r.marker("VertexManager");
  do_framebuffer_manager(r);
  r.marker("FramebufferManager");
  do_texture_cache(r);
  r.marker("TextureCache");
  do_presenter(r);
  r.marker("Presenter");
  do_bounding_box(r);
  r.marker("Bounding Box");
  r.boolean("widescreen_is_game_widescreen");
  r.marker("Widescreen");
  do_xf_state_manager(r);
  r.marker("XFStateManager");
}

// ===========================================================================================
// CoreTiming::CoreTimingManager::DoState                         Core/CoreTiming.cpp:173
void do_coretiming(R& r)
{
  Scope s(r, "coretiming");
  r.s32("slice_length");
  r.s64("global_timer");
  r.s64("idled_cycles");
  r.u32("fake_dec_start_value");
  r.u64("fake_dec_start_ticks");
  r.u64("fake_TB_start_value");
  r.u64("fake_TB_start_ticks");
  r.f32("last_oc_factor");
  r.u64("event_fifo_id");
  r.marker("CoreTimingData");
  uint32_t n = r.count("event_count");  // DoEachElement(m_event_queue): u32 size + elements
  for (uint32_t i = 0; i < n; i++)
  {
    Scope e(r, R::idx("event", i));
    r.s64("time");
    r.u64("fifo_order");
    r.u64("userdata");
    r.str("name");
  }
  r.marker("CoreTimingEvents");
}

// ===========================================================================================
// HW
void do_memory(R& r)  // MemoryManager::DoState                    Core/HW/Memmap.cpp:498
{
  Scope s(r, "memory");
  uint32_t ram = r.u32("ram_size");
  uint32_t l1 = r.u32("l1_cache_size");
  bool have_fake = r.boolean("have_fake_vmem");
  uint32_t fake = r.u32("fake_vmem_size");
  bool have_ex = r.boolean("have_exram");
  uint32_t ex = r.u32("exram_size");
  r.blob("ram", ram);
  r.blob("l1_cache", l1);
  r.marker("Memory RAM");
  if (have_fake) r.blob("fake_vmem", fake);
  r.marker("Memory FakeVMEM");
  if (have_ex) r.blob("exram", ex);
  r.marker("Memory EXRAM");
}

void do_video_interface(R& r)  // VideoInterfaceManager::DoState
{
  Scope s(r, "vi");
  r.u16("vertical_timing_register");
  r.u16("display_control_register");
  r.u32("h_timing_0");
  r.u32("h_timing_1");
  r.u32("vblank_timing_odd");
  r.u32("vblank_timing_even");
  r.u32("burst_blanking_odd");
  r.u32("burst_blanking_even");
  r.u32("xfb_info_top");
  r.u32("xfb_info_bottom");
  r.u32("xfb_3d_info_top");
  r.u32("xfb_3d_info_bottom");
  r.blob("interrupt_register", 16);  // array<UVIInterruptRegister,4> (4 bytes each)
  r.blob("latch_register", 8);       // array<UVILatchRegister,2>
  r.u16("picture_configuration");
  r.u16("horizontal_scaling");
  r.blob("filter_coef_tables_02", 12);  // Tables02: 3 x u32
  r.blob("filter_coef_tables_36", 16);  // Tables36: 4 x u32
  r.u32("unknown_aa_register");
  r.u16("clock");
  r.u16("dtv_status");
  r.u16("fb_width");
  r.u32("border_hblank");
  r.f64("target_refresh_rate");
  r.u32("target_refresh_rate_numerator");
  r.u32("target_refresh_rate_denominator");
  r.u64("ticks_last_line_start");
  r.u32("half_line_count");
  r.u32("half_line_of_next_si_poll");
  r.u32("even_field_first_hl");
  r.u32("odd_field_first_hl");
  r.u32("even_field_last_hl");
  r.u32("odd_field_last_hl");
}

void do_serial_interface(R& r)  // SerialInterfaceManager::DoState  Core/HW/SI/SI.cpp:203
{
  Scope s(r, "si");
  for (int i = 0; i < 4; i++)  // MAX_SI_CHANNELS
  {
    Scope c(r, R::idx("channel", i));
    r.u32("in_hi");
    r.u32("in_lo");
    r.u32("out");
    r.boolean("has_recent_device_unplug");
    uint32_t type = r.u32("device_type");  // SIDevices (int)
    switch (type)
    {
    case 0:   // NONE
    case 1:   // N64_MIC
    case 2:   // N64_KEYBOARD
    case 3:   // N64_MOUSE
    case 4:   // N64_CONTROLLER
    case 5:   // GC_GBA
      break;  // ISIDevice::DoState writes nothing
    case 6:   // GC_CONTROLLER
    case 8:   // GC_STEERING   (CSIDevice_GCController subclasses without their own DoState)
    case 9:   // DANCEMAT
    case 10:  // GC_TARUKONGA
    case 12:  // WIIU_ADAPTER
    {
      Scope d(r, "gc_controller");
      r.blob("origin", 10);  // SOrigin{u16 button; u8 x8}
      r.u8("mode");
      r.u64("timer_button_combo_start");
      r.u32("last_button_combo");  // EButtonCombo
      break;
    }
    case 7:  // GC_KEYBOARD
    {
      Scope d(r, "keyboard");
      r.u8("counter");
      break;
    }
    default:
      throw ParseError("unsupported SI device type " + std::to_string(type) + " (GBAEmu/AMBaseboard) on channel " +
                       std::to_string(i));
    }
  }
  r.u32("poll");
  r.u32("com_csr");
  r.u32("status_reg");
  r.u32("exi_clock_count");
  r.blob("si_buffer", 128);
}

void do_processor_interface(R& r)  // ProcessorInterfaceManager::DoState
{
  Scope s(r, "pi");
  r.u32("interrupt_mask");
  r.u32("interrupt_cause");
  r.u32("fifo_cpu_base");
  r.u32("fifo_cpu_end");
  r.u32("fifo_cpu_write_pointer");
  r.u32("reset_code");
}

// ---- DSP HLE: DSPHLE::DoState + UCode DoStates ----
void do_ucode_shared(R& r)  // UCodeInterface::DoStateShared
{
  r.boolean("upload_setup_in_progress");
  r.blob("next_ucode", 28);  // NextUCodeInfo (7 x u32)
  r.s32("next_ucode_steps");
  r.boolean("needs_resume_mail");
}

void do_accelerator(R& r)  // DSP::Accelerator::DoState
{
  Scope s(r, "accelerator");
  r.u32("start_address");
  r.u32("end_address");
  r.u32("current_address");
  r.u16("sample_format");
  r.u16("yn1");
  r.u16("yn2");
  r.u16("pred_scale");
  r.boolean("reads_stopped");
}

void do_ucode(R& r, const char* name, uint32_t crc)
{
  Scope s(r, name);
  const char* kind = crc == 0 ? "rom" : crc == 1 ? "init" : crc == 0x65d6cc6f ? "card" : crc == 0xdd7e72d5 ? "gba" : "ax";
  Scope k(r, kind);
  if (crc == 0x00000000)  // ROMUCode
  {
    r.blob("current_ucode", 20);  // UCodeBootInfo
    r.s32("boot_task_num_steps");
    r.u32("next_parameter");
    do_ucode_shared(r);
  }
  else if (crc == 0x00000001)  // INITUCode: nothing
  {
  }
  else if (crc == 0x65d6cc6f)  // CARDUCode
  {
    do_ucode_shared(r);
    r.u32("state");
  }
  else if (crc == 0xdd7e72d5)  // GBAUCode
  {
    do_ucode_shared(r);
    r.u32("mail_state");
  }
  else
  {
    switch (crc)
    {
    // ZeldaUCode group, AXWii group, homebrew ASnd/AESnd: not supported (never used by these builds)
    case 0x86840740: case 0x6ca33a6d: case 0xd643001f: case 0x6ba3b3ea: case 0x24b22038:
    case 0x2fcdf1ec: case 0xdf059f68: case 0x4be6a5cb: case 0x267fd05a: case 0x42f64ac4:
    case 0x56d36052: case 0x6c3f6f94: case 0xb7eb9a9c: case 0xeaeb38cc:
      throw ParseError("DSP HLE ZeldaUCode state not implemented (crc " + std::to_string(crc) + ")");
    case 0x2ea36ce6: case 0x5ef56da3: case 0x347112ba: case 0xfa450138: case 0xadbc06bd:
    case 0x4cc52064: case 0xd9c4bf34: case 0x7699af32:
      throw ParseError("DSP HLE AXWii state not implemented (crc " + std::to_string(crc) + ")");
    default:
      break;  // AXUCode (the explicit AX CRCs and the GC default of UCodeFactory)
    }
    // AXUCode::DoState = DoStateShared + DoAXState
    do_ucode_shared(r);
    r.blob("cmdlist", 1024);  // u16[512]
    r.u32("cmdlist_size");
    r.u32("mail_state");
    for (const char* n : {"samples_main_left", "samples_main_right", "samples_main_surround",
                          "samples_auxA_left", "samples_auxA_right", "samples_auxA_surround",
                          "samples_auxB_left", "samples_auxB_right", "samples_auxB_surround"})
      r.blob(n, 640);  // int[32*5]
    bool has_ck = r.boolean("coeffs_checksum_present");  // optional<u32>
    if (has_ck) r.u32("coeffs_checksum");
    r.u16("compressor_pos");
    do_accelerator(r);
  }
}

void do_dsp_hle(R& r)  // DSPHLE::DoState                          Core/HW/DSPHLE/DSPHLE.cpp:99
{
  Scope s(r, "hle");
  r.u16("dsp_control");
  r.u64("control_reg_init_code_clear_time");
  r.u32("cpu_mailbox");
  r.u32("dsp_mailbox");
  uint32_t ucode_crc = r.u32("ucode_crc");
  uint32_t last_crc = r.u32("last_ucode_crc");
  if (ucode_crc != 0xFFFFFFFFu)  // UCODE_NULL -> no object
    do_ucode(r, "ucode", ucode_crc);
  if (last_crc != 0xFFFFFFFFu) do_ucode(r, "last_ucode", last_crc);
  {
    Scope m(r, "mail_handler");  // CMailHandler::DoState
    uint32_t n = r.count("pending_count");  // deque<pair<u32,bool>>
    for (uint32_t i = 0; i < n; i++)
    {
      Scope p(r, R::idx("mail", i));
      r.u32("value");
      r.boolean("is_last");
    }
    r.u32("last_mail");
    r.boolean("halted");
  }
}

void do_dsp_lle(R& r)  // DSPLLE::DoState -> DSPCore::DoState -> SDSP::DoState   (no LLE states available to verify)
{
  Scope s(r, "lle");
  r.blob("r", 72);  // SDSP::r (probe: 72 bytes)
  r.u16("pc");
  r.u16("control_reg");
  r.u64("control_reg_init_code_clear_time");
  r.blob("reg_stack_ptrs", 4);
  r.u8("exceptions");
  r.boolean("external_interrupt_waiting");
  for (int i = 0; i < 4; i++) r.blob(R::idx("reg_stack", i), 64);  // u16[32] each
  r.u64("step_counter");
  r.blob("ifx_regs", 512);
  do_accelerator(r);
  r.u32("mailbox0");
  r.u32("mailbox1");
  r.blob("iram", 0x1000 * 2);
  r.blob("dram", 0x1000 * 2);
  if (r.cfg.dsp_lle_jit) r.s32("jit_cycles_left");
  r.u32("cycle_count");
}

void do_dsp(R& r, StateWalk& w)  // DSPManager::DoState                      Core/HW/DSP.cpp:79
{
  Scope s(r, "dsp");
  r.blob("aram", r.cfg.aram_size);  // !wii_mode: DoArray(m_aram.ptr, m_aram.size)
  r.u16("dsp_control");
  {
    Scope a(r, "audio_dma");  // DSPManager::AudioDMA (16 bytes with padding, probe/DSP.h)
    r.u32("current_source_address");
    r.u16("remaining_blocks_count");
    r.u16("pad0");  // struct padding (indeterminate)
    r.u32("source_address");
    r.u16("control");  // UAudioDMAControl: NumBlocks:15, Enable:1
    r.u16("pad1");
  }
  {
    Scope a(r, "aram_dma");  // DSPManager::ARAM_DMA (12 bytes)
    r.u32("mm_addr");
    r.u32("ar_addr");
    r.u32("cnt");  // UARAMCount: count:31, dir:1
  }
  r.u16("aram_info");
  r.u16("aram_mode");
  r.u16("aram_refresh");
  r.s32("dsp_slice");
  // DSPEmulator::DoState: first byte is_hle
  bool hle = r.boolean("is_hle");
  w.dsp_engine = hle ? "HLE" : "LLE";
  if (hle) do_dsp_hle(r);
  else do_dsp_lle(r);
}

void do_dvd(R& r)  // DVDInterface::DoState + DVDThread::DoState + ADPCMDecoder
{
  Scope s(r, "dvd");
  r.u32("DISR");
  r.u32("DICVR");
  r.blob("DICMDBUF", 12);
  r.u32("DIMAR");
  r.u32("DILENGTH");
  r.u32("DICR");
  r.u32("DIIMMBUF");
  r.u32("DICFG");
  r.boolean("stream");
  r.boolean("stop_at_track_end");
  r.u64("audio_position");
  r.u64("current_start");
  r.u32("current_length");
  r.u64("next_start");
  r.u32("next_length");
  r.u32("pending_blocks");
  r.boolean("enable_dtk");
  r.u8("dtk_buffer_length");
  r.u8("drive_state");  // DriveState : u8
  r.u32("error_code");  // DriveError (int)
  r.u64("read_buffer_start_time");
  r.u64("read_buffer_end_time");
  r.u64("read_buffer_start_offset");
  r.u64("read_buffer_end_offset");
  r.str("disc_path_to_insert");
  {
    Scope t(r, "thread");  // DVDThread::DoState
    uint32_t n = r.count("result_count");  // map<u64, pair<ReadRequest, vector<u8>>>
    for (uint32_t i = 0; i < n; i++)
    {
      Scope e(r, R::idx("result", i));
      r.u64("id_key");
      r.blob("request", sz::READREQ);  // ReadRequest raw (padding included)
      r.vec("data", 1);
    }
    r.u64("next_id");
    r.boolean("had_disc");
  }
  {
    Scope a(r, "adpcm");
    r.s32("histl1");
    r.s32("histl2");
    r.s32("histr1");
    r.s32("histr2");
  }
  if (r.cfg.triforce) throw ParseError("Triforce/AMMediaboard states not supported");
}

void do_gpfifo(R& r)
{
  Scope s(r, "gpfifo");
  r.blob("gather_pipe", sz::GATHER_PIPE);
  r.u32("pipe_count");
}

void do_exi_device(R& r, uint32_t type)
{
  using T = uint32_t;
  switch (static_cast<T>(type))
  {
  case 0:  // Dummy
  case 4:  // Microphone
  case 7:  // Gecko
  case 0xFF:  // None
    break;
  case 1:  // MemoryCard (CEXIMemoryCard::DoState)
  case 8:  // MemoryCardFolder (GCI folder card: same CEXIMemoryCard::DoState)
  {
    Scope d(r, "memcard");
    bool store = r.boolean("store_contents");  // Movie::IsMovieActive(); false in normal play
    if (store)
      throw ParseError("EXI memory card with storeContents (movie recording) is not supported");
    break;
  }
  case 2:  // MaskROM = CEXIIPL
  {
    Scope d(r, "ipl");
    r.blob("sram", sz::SRAM);
    r.u32("rtc_flags");  // g_rtc_flags
    r.u32("command");
    r.u32("command_bytes_received");
    r.u32("cursor");
    r.str("buffer");
    r.boolean("fonts_loaded");
    break;
  }
  case 3:  // AD16
  {
    Scope d(r, "ad16");
    r.u32("position");
    r.u32("command");
    r.u32("ad16_register");
    break;
  }
  case 6:  // Baseboard
  {
    Scope d(r, "baseboard");
    r.u32("position");
    r.boolean("interrupt_set");
    r.u32("command");
    break;
  }
  case 12: case 13:  // ModemTapServer: nothing
  case 5: case 10: case 11: case 14: case 15:  // Ethernet*: tx_fifo + BBA mem (sizes: not needed here)
    throw ParseError("EXI Ethernet/AGP device state not supported (type " + std::to_string(type) + ")");
  default:
    throw ParseError("unsupported EXI device type " + std::to_string(type));
  }
}

void do_exi(R& r)  // ExpansionInterfaceManager::DoState -> CEXIChannel::DoState
{
  Scope s(r, "exi");
  for (int c = 0; c < 3; c++)  // MAX_EXI_CHANNELS
  {
    Scope ch(r, R::idx("channel", c));
    r.u32("status");
    r.u32("dma_memory_address");
    r.u32("dma_length");
    r.u32("control");
    r.u32("imm_data");
    r.blob("memcard_header_data", sz::MEMCARD_HDR);
    for (int d = 0; d < 3; d++)  // NUM_DEVICES
    {
      Scope dv(r, R::idx("device", d));
      uint32_t type = r.u32("type");  // EXIDeviceType (int)
      do_exi_device(r, type);
    }
  }
}

void do_audio_interface(R& r)  // AudioInterfaceManager::DoState + Mixer::DoState
{
  Scope s(r, "ai");
  r.u32("control");
  r.u32("volume");
  r.u32("sample_counter");
  r.u32("interrupt_timing");
  r.u64("last_cpu_time");
  r.u32("ais_sample_rate_divisor");
  r.u32("aid_sample_rate_divisor");
  r.u64("cpu_cycles_per_sample");
  Scope m(r, "mixer");
  // dma, streaming, wiimote_speaker[4], skylander_portal, gba[4]
  const char* names[] = {"dma",           "streaming",       "wiimote0", "wiimote1", "wiimote2",
                         "wiimote3",      "skylander_portal", "gba0",     "gba1",     "gba2",
                         "gba3"};
  for (const char* n : names)
  {
    Scope f(r, n);
    r.u32("input_sample_rate_divisor");
    r.s32("l_volume");
    r.s32("r_volume");
  }
}

void do_hsp(R& r)  // HSPManager::DoState
{
  Scope s(r, "hsp");
  uint32_t type = r.u32("device_type");  // HSPDeviceType (int): 0 = None
  if (type != 0) throw ParseError("HSP ARAM expansion device state not supported");
}

void do_hw(R& r, StateWalk& w)  // HW::DoState                             Core/HW/HW.cpp
{
  Scope s(r, "hw");
  do_memory(r);
  r.marker("Memory");
  {
    Scope m(r, "mi");
    r.blob("mem", sz::MI_MEM);
  }
  r.marker("MemoryInterface");
  do_video_interface(r);
  r.marker("VideoInterface");
  do_serial_interface(r);
  r.marker("SerialInterface");
  do_processor_interface(r);
  r.marker("ProcessorInterface");
  do_dsp(r, w);
  r.marker("DSP");
  do_dvd(r);
  r.marker("DVDInterface");
  do_gpfifo(r);
  r.marker("GPFifo");
  do_exi(r);
  r.marker("ExpansionInterface");
  do_audio_interface(r);
  r.marker("AudioInterface");
  do_hsp(r);
  r.marker("HSP");
  r.marker("WIIHW");  // not Wii: no IOS sections
}

// PowerPCManager::DoState                                    Core/PowerPC/PowerPC.cpp:70
void do_ppc(R& r)
{
  Scope s(r, "ppc");
  {
    Scope p(r, "ppcState");
    r.blob("gpr", 128);  // u32[32]
    r.u32("pc");
    r.u32("npc");
    r.blob("cr_fields", 64);  // ConditionRegister::fields: array<u64,8>
    r.u32("msr");
    r.u32("fpscr");
    r.u32("Exceptions");
    r.s32("downcount");
    r.u8("xer_ca");
    r.u8("xer_so_ov");
    r.u16("xer_stringctrl");
    r.blob("ps", 512);   // PairedSingle[32] {u64 ps0, ps1}
    r.blob("sr", 64);    // array<u32,16>
    r.blob("spr", 4096); // u32[1024]
    r.blob("tlb", 4608); // array<array<TLBEntry(36),64>,2>
    r.u32("pagetable_base");
    r.u32("pagetable_mask");
    r.boolean("pagetable_update_pending");
    r.boolean("reserve");
    r.u32("reserve_address");
    for (const char* cache : {"icache", "dcache"})  // Cache::DoState
    {
      Scope c(r, cache);
      r.blob("data", 128 * 8 * 8 * 4);  // [sets][ways][8 words] u32
      r.blob("plru", 128);
      r.blob("valid", 128);
      r.blob("addrs", 128 * 8 * 4);
      r.blob("modified", 128);
    }
  }
  r.vec("mmu/page_table", 1);  // MMU::DoState (write): vector<u8> m_page_table
}

}  // namespace

// -------------------------------------------------------------------------------------------
StateWalk walk_state(const uint8_t* data, size_t size)
{
  StateWalk w;
  w.data = data;
  w.size = size;
  w.fields.reserve(4096);
  R r(data, size, w);

  // Core::State::DoState                                             Core/State.cpp:96
  w.is_wii = r.boolean("is_wii");
  r.u32("mem1_size");
  r.u32("mem2_size");
  do_movie(r);
  r.marker("Movie");
  do_video(r);
  r.marker("video_backend");
  do_coretiming(r);
  r.marker("CoreTiming");
  do_hw(r, w);
  r.marker("HW");
  do_ppc(r);
  r.marker("PowerPC");
  // Wiimote::DoState only for Wii
  r.marker("Wiimote");
  {
    Scope g(r, "gecko");
    r.u32("code_handler_installed");  // Gecko::Installation (enum class, int)
  }
  r.marker("Gecko");
  if (!r.at_end())
  {
    // AchievementManager::DoState (USE_RETRO_ACHIEVEMENTS, only written when RA is enabled)
    Scope a(r, "achievements");
    uint64_t n = r.u64("progress_size");  // size_t
    r.blob("progress", n);
    r.marker("AchievementManager");
    w.has_achievements = true;
  }
  w.end_offset = r.pos();
  w.ended_exactly = r.at_end();
  return w;
}

}  // namespace dsr
