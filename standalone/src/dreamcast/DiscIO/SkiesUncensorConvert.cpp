// See SkiesUncensorConvert.h. Floating point must not be contracted into FMAs (the CMPR endpoint
// search has to match the Python prototype's IEEE double arithmetic bit for bit).
#include "DiscIO/SkiesUncensorConvert.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <unordered_map>

#if defined(__clang__)
#pragma STDC FP_CONTRACT OFF
#elif defined(__GNUC__)
#pragma GCC optimize("fp-contract=off")
#endif

namespace DiscIO::SkiesUncensor
{
namespace
{
using Bytes = std::vector<uint8_t>;

// No exceptions anywhere (Dolphin builds with -fno-exceptions): the first failure is recorded in a
// thread-local message, the failing function returns a dummy value / false, and callers check.
thread_local std::string g_err;
bool Fail(const std::string& m)
{
  if (g_err.empty())
    g_err = m;
  return false;
}
bool Failed()
{
  return !g_err.empty();
}
#define FAILV(msg, val) \
  do                    \
  {                     \
    Fail(msg);          \
    return val;         \
  } while (0)

// ---------------------------------------------------------------------------------------------
// SHA-1
// ---------------------------------------------------------------------------------------------
class Sha1
{
public:
  void Update(const uint8_t* p, size_t n)
  {
    total_ += n;
    while (n)
    {
      size_t c = std::min<size_t>(n, 64 - fill_);
      std::memcpy(buf_ + fill_, p, c);
      fill_ += c;
      p += c;
      n -= c;
      if (fill_ == 64)
      {
        Block(buf_);
        fill_ = 0;
      }
    }
  }
  std::string Final()
  {
    uint64_t bits = total_ * 8;
    uint8_t pad = 0x80;
    Update(&pad, 1);
    uint8_t z = 0;
    while (fill_ != 56)
      Update(&z, 1);
    uint8_t len[8];
    for (int i = 0; i < 8; i++)
      len[i] = uint8_t(bits >> (56 - 8 * i));
    Update(len, 8);
    static const char* hex = "0123456789abcdef";
    std::string s;
    for (int i = 0; i < 5; i++)
      for (int k = 28; k >= 0; k -= 4)
        s += hex[(h_[i] >> k) & 15];
    return s;
  }

private:
  static uint32_t Rol(uint32_t v, int n) { return (v << n) | (v >> (32 - n)); }
  void Block(const uint8_t* b)
  {
    uint32_t w[80];
    for (int i = 0; i < 16; i++)
      w[i] = uint32_t(b[4 * i]) << 24 | uint32_t(b[4 * i + 1]) << 16 | uint32_t(b[4 * i + 2]) << 8 |
             b[4 * i + 3];
    for (int i = 16; i < 80; i++)
      w[i] = Rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    uint32_t a = h_[0], bb = h_[1], c = h_[2], d = h_[3], e = h_[4];
    for (int i = 0; i < 80; i++)
    {
      uint32_t f, k;
      if (i < 20)
        f = (bb & c) | (~bb & d), k = 0x5A827999;
      else if (i < 40)
        f = bb ^ c ^ d, k = 0x6ED9EBA1;
      else if (i < 60)
        f = (bb & c) | (bb & d) | (c & d), k = 0x8F1BBCDC;
      else
        f = bb ^ c ^ d, k = 0xCA62C1D6;
      uint32_t t = Rol(a, 5) + f + e + k + w[i];
      e = d, d = c, c = Rol(bb, 30), bb = a, a = t;
    }
    h_[0] += a, h_[1] += bb, h_[2] += c, h_[3] += d, h_[4] += e;
  }
  uint32_t h_[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
  uint8_t buf_[64];
  size_t fill_ = 0;
  uint64_t total_ = 0;
};

// ---------------------------------------------------------------------------------------------
// byte helpers
// ---------------------------------------------------------------------------------------------
uint32_t Rd(const Bytes& d, size_t p, bool le)
{
  if (p + 4 > d.size())
    FAILV("read past end of data", 0);
  const uint8_t* b = &d[p];
  return le ? uint32_t(b[0]) | uint32_t(b[1]) << 8 | uint32_t(b[2]) << 16 | uint32_t(b[3]) << 24 :
              uint32_t(b[3]) | uint32_t(b[2]) << 8 | uint32_t(b[1]) << 16 | uint32_t(b[0]) << 24;
}
void Wr(Bytes& d, size_t p, uint32_t v, bool le)
{
  if (p + 4 > d.size())
    FAILV("write past end of data", );
  for (int i = 0; i < 4; i++)
    d[p + i] = uint8_t(le ? v >> (8 * i) : v >> (24 - 8 * i));
}
void Put32(Bytes& o, uint32_t v, bool le)
{
  for (int i = 0; i < 4; i++)
    o.push_back(uint8_t(le ? v >> (8 * i) : v >> (24 - 8 * i)));
}
void Put16BE(Bytes& o, uint32_t v)
{
  o.push_back(uint8_t(v >> 8));
  o.push_back(uint8_t(v));
}
uint16_t Rd16(const uint8_t* b, bool le)
{
  return le ? uint16_t(b[0] | b[1] << 8) : uint16_t(b[1] | b[0] << 8);
}
// bytes.find(pattern) -> offset or -1
long Find(const Bytes& d, const char* pat, size_t from = 0)
{
  if (d.size() < 4 || from + 4 > d.size())
    return -1;
  for (size_t i = from; i + 4 <= d.size(); i++)
    if (d[i] == uint8_t(pat[0]) && d[i + 1] == uint8_t(pat[1]) && d[i + 2] == uint8_t(pat[2]) &&
        d[i + 3] == uint8_t(pat[3]))
      return long(i);
  return -1;
}
std::string Sha(const uint8_t* p, size_t n)
{
  Sha1 s;
  s.Update(p, n);
  return s.Final();
}

// ---------------------------------------------------------------------------------------------
// AKLZ
// ---------------------------------------------------------------------------------------------
const uint8_t kHeader[12] = {'A', 'K', 'L', 'Z', 0x7E, 0x3F, 0x51, 0x64, 0x3D, 0xCC, 0xCC, 0xCD};

struct Tok
{
  uint16_t dist;
  uint8_t len;  // 0 = literal
  uint8_t lit;
};

// Hash chains over exact 3-byte keys. The table is hashed (sized from the input) and entries whose
// key differs are skipped without counting against the depth, so the candidates visited are the
// same, in the same order, as one list per exact key (newest first).
class Chains
{
public:
  explicit Chains(size_t n) : prev_(n, -1)
  {
    bits_ = 10;
    while (bits_ < 16 && (size_t(1) << bits_) < n)
      bits_++;
    head_.assign(size_t(1) << bits_, 0);
  }
  uint32_t Slot(const uint8_t* p) const
  {
    uint32_t key = uint32_t(p[0]) << 16 | uint32_t(p[1]) << 8 | p[2];
    return (key * 2654435761u) >> (32 - bits_);
  }
  void Add(const uint8_t* d, size_t n, size_t i)
  {
    if (i + 3 <= n)
    {
      int32_t& h = head_[Slot(d + i)];
      prev_[i] = h - 1;
      h = int32_t(i) + 1;
    }
  }
  // longest match (len 18 max) among the newest `depth` candidates within 4095; bestp -> position
  int Best(const uint8_t* d, size_t n, size_t i, int depth, size_t* bestp) const
  {
    int best = 0;
    if (i + 3 > n)
      return 0;
    int32_t p = head_[Slot(d + i)] - 1;
    size_t m = std::min<size_t>(18, n - i);
    int c = 0;
    for (; p >= 0 && c < depth; p = prev_[p])
    {
      if (i - size_t(p) > 4095)
        break;
      if (d[p] != d[i] || d[p + 1] != d[i + 1] || d[p + 2] != d[i + 2])
        continue;  // another key in the same bucket
      c++;
      size_t l = 3;
      while (l < m && d[size_t(p) + l] == d[i + l])
        l++;
      if (int(l) > best)
        best = int(l), *bestp = size_t(p);
      if (l == 18)
        break;
    }
    return best;
  }

private:
  std::vector<int32_t> prev_;
  std::vector<int32_t> head_;
  int bits_;
};

std::vector<Tok> TokensGreedy(const Bytes& data, int depth)
{
  size_t n = data.size();
  const uint8_t* d = data.data();
  Chains ch(n);
  std::vector<Tok> out;
  size_t i = 0;
  while (i < n)
  {
    size_t p = 0;
    int l = ch.Best(d, n, i, depth, &p);
    if (l >= 3 && l < 18)
    {
      ch.Add(d, n, i);
      size_t p2;
      int l2 = ch.Best(d, n, i + 1, depth, &p2);
      if (l2 > l + 1)
      {
        out.push_back({0, 0, d[i]});
        i += 1;
        continue;
      }
      // the Python pops i again and re-adds it with the rest of the match: same chain state
      if (l >= 3)
      {
        out.push_back({uint16_t(i - p), uint8_t(l), 0});
        for (int k = 1; k < l; k++)
          ch.Add(d, n, i + k);
        i += l;
      }
      continue;
    }
    if (l >= 3)
    {
      out.push_back({uint16_t(i - p), uint8_t(l), 0});
      for (int k = 0; k < l; k++)
        ch.Add(d, n, i + k);
      i += l;
    }
    else
    {
      out.push_back({0, 0, d[i]});
      ch.Add(d, n, i);
      i += 1;
    }
  }
  return out;
}

std::vector<Tok> TokensOptimal(const Bytes& data, int depth)
{
  size_t n = data.size();
  const uint8_t* d = data.data();
  Chains ch(n);
  std::vector<uint8_t> mlen(n, 0);
  std::vector<uint16_t> mdist(n, 0);
  for (size_t i = 0; i < n; i++)
  {
    size_t p = 0;
    int best = ch.Best(d, n, i, depth, &p);
    if (best >= 3)
      mlen[i] = uint8_t(best), mdist[i] = uint16_t(i - p);
    ch.Add(d, n, i);
  }
  std::vector<int32_t> cost(n + 1, 0);
  std::vector<uint8_t> choice(n, 0);
  for (size_t i = n; i-- > 0;)
  {
    int32_t c = 9 + cost[i + 1];
    uint8_t chs = 0;
    for (int l = 3; l <= mlen[i]; l++)
    {
      int32_t v = 17 + cost[i + l];
      if (v < c)
        c = v, chs = uint8_t(l);
    }
    cost[i] = c;
    choice[i] = chs;
  }
  std::vector<Tok> out;
  size_t i = 0;
  while (i < n)
  {
    if (choice[i])
    {
      out.push_back({mdist[i], choice[i], 0});
      i += choice[i];
    }
    else
    {
      out.push_back({0, 0, d[i]});
      i++;
    }
  }
  return out;
}

size_t EncodedSize(const std::vector<Tok>& t)
{
  size_t s = 12 + 4 + (t.size() + 7) / 8;
  for (const Tok& k : t)
    s += k.len ? 2 : 1;
  return s;
}

bool Fit(const std::vector<Tok>& toks, const Bytes& data, size_t target, std::vector<Tok>* res)
{
  size_t size = EncodedSize(toks);
  if (size > target)
    return Fail("compresses to " + std::to_string(size) + " bytes, " + std::to_string(size - target) +
                " more than the " + std::to_string(target) + " allowed");
  long deficit = long(target - size);
  if (deficit == 0)
  {
    *res = toks;
    return true;
  }
  long T = long(toks.size());
  auto fl = [](long t) { return (t + 7) / 8; };
  long nops = 0, m = 0;
  bool found = false;
  for (nops = 0; nops <= deficit; nops++)
  {
    m = deficit - nops - (fl(T + nops) - fl(T));
    if (0 <= m && m <= nops)
    {
      found = true;
      break;
    }
  }
  if (!found)
    return Fail("no exact fit");
  long a = nops - m;
  std::vector<Tok> out;
  size_t pos = 0;
  for (const Tok& t : toks)
  {
    if (t.len)
    {
      int l = t.len;
      while (m && l >= 6)
      {
        out.push_back({t.dist, 3, 0});
        pos += 3, l -= 3, m--;
      }
      while (a && l >= 4)
      {
        out.push_back({0, 0, data[pos]});
        pos++, l--, a--;
      }
      out.push_back({t.dist, uint8_t(l), 0});
      pos += l;
    }
    else
    {
      out.push_back(t);
      pos++;
    }
  }
  if (a || m)
    return Fail("not enough references to grow the stream");
  *res = std::move(out);
  return true;
}

Bytes Encode(const std::vector<Tok>& toks, size_t n)
{
  Bytes out(kHeader, kHeader + 12);
  Put32(out, uint32_t(n), false);
  size_t pos = 0;
  for (size_t g = 0; g < toks.size(); g += 8)
  {
    size_t flagpos = out.size();
    out.push_back(0);
    unsigned flags = 0;
    for (size_t bit = 0; bit < 8 && g + bit < toks.size(); bit++)
    {
      const Tok& t = toks[g + bit];
      if (!t.len)
      {
        flags |= 1u << bit;
        out.push_back(t.lit);
        pos++;
      }
      else
      {
        unsigned off = (unsigned(pos) - t.dist + 0xFEE) & 0xFFF;
        out.push_back(uint8_t(off & 0xFF));
        out.push_back(uint8_t(((off >> 4) & 0xF0) | (t.len - 3)));
        pos += t.len;
      }
    }
    out[flagpos] = uint8_t(flags);
  }
  return out;
}

// ---------------------------------------------------------------------------------------------
// Dreamcast -> GameCube object blocks (nj2gc.py, block2gc.py)
// ---------------------------------------------------------------------------------------------
class NjConv
{
public:
  explicit NjConv(const uint8_t* d, size_t n) : d_(d), n_(n), o_(d, d + n), done_(n, 0) {}
  bool Run()
  {
    std::set<size_t> seen;
    Obj(0, true, &seen);
    return !Failed();
  }
  Bytes Result() const
  {
    Bytes o = o_;
    for (size_t i = 0; i < o.size(); i++)
      if (!done_[i])
        o[i] = d_[i];
    return o;
  }

private:
  bool Trunc() { return Fail("model data truncated"); }
  uint32_t U32(size_t p)
  {
    if (p + 4 > n_)
      FAILV("model data truncated", 0);
    return uint32_t(d_[p]) | uint32_t(d_[p + 1]) << 8 | uint32_t(d_[p + 2]) << 16 | uint32_t(d_[p + 3]) << 24;
  }
  uint32_t U16(size_t p)
  {
    if (p + 2 > n_)
      FAILV("model data truncated", 0);
    return uint32_t(d_[p]) | uint32_t(d_[p + 1]) << 8;
  }
  void Sw32(size_t p, size_t cnt = 1)
  {
    for (size_t i = 0; i < cnt; i++)
    {
      size_t q = p + 4 * i;
      if (q + 4 > n_)
        FAILV("model data truncated", );
      for (int k = 0; k < 4; k++)
        o_[q + k] = d_[q + 3 - k], done_[q + k] = 1;
    }
  }
  void Sw16(size_t p, size_t cnt = 1)
  {
    for (size_t i = 0; i < cnt; i++)
    {
      size_t q = p + 2 * i;
      if (q + 2 > n_)
        FAILV("model data truncated", );
      o_[q] = d_[q + 1], o_[q + 1] = d_[q];
      done_[q] = done_[q + 1] = 1;
    }
  }
  void Keep(size_t p, size_t n)
  {
    if (p + n > n_)
      FAILV("model data truncated", );
    for (size_t i = 0; i < n; i++)
      o_[p + i] = d_[p + i], done_[p + i] = 1;
  }
  // The root object may sit at offset 0; children and siblings stop at 0.
  void Obj(size_t p, bool root, std::set<size_t>* seen)
  {
    while ((p || root) && !seen->count(p) && p + 0x34 <= n_ && !Failed())
    {
      seen->insert(p);
      root = false;
      Sw32(p, 13);
      uint32_t model = U32(p + 4), child = U32(p + 0x2c), sib = U32(p + 0x30);
      if (Failed())
        return;
      if (model)
        Model(model);
      if (child)
        Obj(child, false, seen);
      p = sib;
    }
  }
  void Model(size_t p)
  {
    if (p >= n_)
      FAILV("model data truncated", );
    if (done_[p])
      return;
    Sw32(p, 6);
    uint32_t vl = U32(p), pl = U32(p + 4);
    if (Failed())
      return;
    if (vl)
      VList(vl);
    if (pl)
      PList(pl);
  }
  void VList(size_t p)
  {
    while (!Failed())
    {
      uint32_t h = U32(p);
      if (Failed())
        return;
      if ((h & 0xff) == 0xff)
      {
        Sw32(p);
        return;
      }
      uint32_t size = h >> 16;
      Sw32(p, 1 + size);
      p += 4 + size_t(size) * 4;
    }
  }
  void PList(size_t p)
  {
    while (!Failed())
    {
      uint32_t h = U16(p), t = h & 0xff;
      if (Failed())
        return;
      if (t == 0xff)
      {
        Sw16(p);
        return;
      }
      if (t == 0 || (1 <= t && t <= 7))
      {
        Sw16(p);
        p += 2;
        continue;
      }
      if (8 <= t && t <= 15)
      {
        Sw16(p, 2);
        p += 4;
        continue;
      }
      uint32_t size = U16(p + 2);
      Sw16(p, 2);
      size_t body = p + 4;
      if (16 <= t && t <= 31)
      {
        size_t n = 0;
        for (uint32_t bit : {1u, 2u, 4u})
          if ((t - 16) & bit)
          {
            Keep(body + 4 * n, 4);
            n++;
          }
        if (size * 2 > 4 * n)
          Sw16(body + 4 * n, (size * 2 - 4 * n) / 2);
      }
      else
        Sw16(body, size);
      p = body + size_t(size) * 2;
    }
  }
  const uint8_t* d_;
  size_t n_;
  Bytes o_;
  Bytes done_;
};

// sw32 over a slice, a trailing partial word reversed on its own length
void Sw32Slice(Bytes& out, const Bytes& src, size_t a, size_t len)
{
  size_t end = std::min(a + len, src.size());
  for (size_t i = a; i < end; i += 4)
  {
    size_t e = std::min(i + 4, end);
    for (size_t k = 0; k < e - i; k++)
      out[i + k] = src[e - 1 - k];
  }
}

bool BlockToGc(const Bytes& blk, Bytes* result)
{
  Bytes out = blk;
  if (blk.size() < 16)
    return Fail("object block too short");
  Sw32Slice(out, blk, 0, 16);
  size_t p = 16;
  while (p + 8 <= blk.size())
  {
    bool tag = !std::memcmp(&blk[p], "NJTL", 4) || !std::memcmp(&blk[p], "NJCM", 4) ||
               !std::memcmp(&blk[p], "POF0", 4);
    if (!tag)
    {
      p += 4;
      continue;
    }
    uint32_t size = Rd(blk, p + 4, true);
    Wr(out, p + 4, size, false);
    if (Failed())
      return false;
    if (p + 8 + size > blk.size())
      return Fail("object block chunk truncated");
    if (!std::memcmp(&blk[p], "NJTL", 4))
    {
      Bytes body(blk.begin() + p + 8, blk.begin() + p + 8 + size);
      Bytes nb = body;
      uint32_t lst = Rd(body, 0, true), cnt = Rd(body, 4, true);
      if (Failed())
        return false;
      Sw32Slice(nb, body, 0, 8);
      for (uint32_t i = 0; i < cnt; i++)
      {
        size_t q = size_t(lst) + 12 * size_t(i);
        if (q >= body.size())
          break;  // an empty slice in the Python, and every later one is too
        Sw32Slice(nb, body, q, 12);
      }
      std::copy(nb.begin(), nb.end(), out.begin() + p + 8);
    }
    else if (!std::memcmp(&blk[p], "NJCM", 4))
    {
      NjConv c(&blk[p + 8], size);
      if (!c.Run())
        return false;
      Bytes o = c.Result();
      std::copy(o.begin(), o.end(), out.begin() + p + 8);
    }
    p += 8 + size;
    p = (p + 3) & ~size_t(3);
  }
  *result = std::move(out);
  return true;
}

// ---------------------------------------------------------------------------------------------
// Images, PVR decode, GVR encode
// ---------------------------------------------------------------------------------------------
struct Px
{
  uint8_t c[4];  // r g b a
  bool operator==(const Px& o) const { return !std::memcmp(c, o.c, 4); }
  bool operator<(const Px& o) const { return std::memcmp(c, o.c, 4) < 0; }  // tuple order
};
Px MakePx(int r, int g, int b, int a)
{
  return Px{{uint8_t(r), uint8_t(g), uint8_t(b), uint8_t(a)}};
}
struct Image
{
  int w = 0, h = 0;
  std::vector<Px> p;
  Px& at(int x, int y) { return p[size_t(y) * w + x]; }
  const Px& at(int x, int y) const { return p[size_t(y) * w + x]; }
};

uint32_t Twiddle(uint32_t x, uint32_t y)
{
  uint32_t r = 0, m = std::max(x, y);
  for (int i = 0; (1u << i) <= m; i++)
    r |= ((y >> i) & 1) << (2 * i) | ((x >> i) & 1) << (2 * i + 1);
  return r;
}
Px PvrPx(uint32_t v, int pf)
{
  if (pf == 0)
    return MakePx(((v >> 10) & 31) * 255 / 31, ((v >> 5) & 31) * 255 / 31, (v & 31) * 255 / 31,
                  (v & 0x8000) ? 255 : 0);
  if (pf == 1)
    return MakePx(((v >> 11) & 31) * 255 / 31, ((v >> 5) & 63) * 255 / 63, (v & 31) * 255 / 31, 255);
  if (pf == 2)
    return MakePx(((v >> 8) & 15) * 17, ((v >> 4) & 15) * 17, (v & 15) * 17, ((v >> 12) & 15) * 17);
  FAILV("PVR pixel format not supported", MakePx(0, 0, 0, 0));
}
int VqMipOff(int w)
{
  switch (w)
  {
  case 1: return 0;
  case 2: return 1;
  case 4: return 2;
  case 8: return 6;
  case 16: return 0x16;
  case 32: return 0x56;
  case 64: return 0x156;
  case 128: return 0x556;
  case 256: return 0x1556;
  case 512: return 0x5556;
  case 1024: return 0x15556;
  }
  FAILV("PVR VQ size not supported", 0);
}

// chunk = bytes starting at 'PVRT'
bool PvrDecode(const uint8_t* c, size_t csize, Image* result)
{
  if (csize < 16)
    return Fail("PVR too short");
  uint32_t size = uint32_t(c[4]) | uint32_t(c[5]) << 8 | uint32_t(c[6]) << 16 | uint32_t(c[7]) << 24;
  int pf = c[8], df = c[9];
  int w = c[12] | c[13] << 8, h = c[14] | c[15] << 8;
  size_t dend = std::min<size_t>(csize, size_t(8) + size);
  const uint8_t* d = c + 16;
  size_t dlen = dend > 16 ? dend - 16 : 0;
  if (pf < 0 || pf > 2)
    return Fail("PVR pixel format not supported");
  Image img;
  img.w = w, img.h = h;
  img.p.assign(size_t(w) * h, MakePx(0, 0, 0, 0));
  if (df == 3 || df == 4 || df == 0x10 || df == 0x11)
  {
    int ncb;
    if (df == 3 || df == 4)
      ncb = 256;
    else
    {
      bool mip = df == 0x11;
      if (w <= 16) ncb = 16;
      else if (w == 32) ncb = mip ? 64 : 32;
      else if (w == 64) ncb = mip ? 256 : 128;
      else ncb = 256;
    }
    if (size_t(ncb) * 8 > dlen)
      return Fail("PVR codebook truncated");
    size_t off = size_t(ncb) * 8 + ((df == 4 || df == 0x11) ? VqMipOff(w) : 0);
    if (Failed())
      return false;
    for (int y = 0; y < h / 2; y++)
      for (int x = 0; x < w / 2; x++)
      {
        size_t io = off + Twiddle(x, y);
        if (io >= dlen)
          return Fail("PVR index out of range");
        const uint8_t* e = d + (d[io] % ncb) * 8;
        img.at(2 * x, 2 * y) = PvrPx(Rd16(e, true), pf);
        img.at(2 * x, 2 * y + 1) = PvrPx(Rd16(e + 2, true), pf);
        img.at(2 * x + 1, 2 * y) = PvrPx(Rd16(e + 4, true), pf);
        img.at(2 * x + 1, 2 * y + 1) = PvrPx(Rd16(e + 6, true), pf);
      }
  }
  else if (df == 1 || df == 2)
  {
    size_t base = 0;
    if (df == 2)
    {
      if (size_t(w) * h * 2 > dlen)
        return Fail("PVR data truncated");
      base = dlen - size_t(w) * h * 2;
    }
    for (int y = 0; y < h; y++)
      for (int x = 0; x < w; x++)
      {
        size_t o = base + Twiddle(x, y) * 2;
        if (o + 2 > dlen)
          return Fail("PVR data truncated");
        img.at(x, y) = PvrPx(Rd16(d + o, true), pf);
      }
  }
  else if (df == 9)
  {
    for (int y = 0; y < h; y++)
      for (int x = 0; x < w; x++)
      {
        size_t o = (size_t(y) * w + x) * 2;
        if (o + 2 > dlen)
          return Fail("PVR data truncated");
        img.at(x, y) = PvrPx(Rd16(d + o, true), pf);
      }
  }
  else
    return Fail("PVR data format not supported");
  *result = std::move(img);
  return true;
}

uint32_t Rgb5a3(const Px& p)
{
  uint32_t r = p.c[0], g = p.c[1], b = p.c[2], a = p.c[3];
  if (a >= 0xe0)
    return 0x8000 | (r >> 3) << 10 | (g >> 3) << 5 | (b >> 3);
  return (a >> 5) << 12 | (r >> 4) << 8 | (g >> 4) << 4 | (b >> 4);
}
void Tiles(const Image& img, Bytes& out)
{
  int h = img.h, w = img.w;
  for (int ty = 0; ty < std::max(h, 4); ty += 4)
    for (int tx = 0; tx < std::max(w, 4); tx += 4)
      for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++)
          Put16BE(out, Rgb5a3(img.at(std::min(tx + x, w - 1), std::min(ty + y, h - 1))));
}
bool Half(const Image& img, Image* out)
{
  int w = img.w, h = img.h;
  if (w == 1 && h == 1)
    return false;
  int nw = std::max(1, w / 2), nh = std::max(1, h / 2);
  Image o;
  o.w = nw, o.h = nh;
  o.p.resize(size_t(nw) * nh);
  for (int y = 0; y < nh; y++)
    for (int x = 0; x < nw; x++)
    {
      int s[4] = {0, 0, 0, 0};
      for (int dy = 0; dy < 2; dy++)
        for (int dx = 0; dx < 2; dx++)
        {
          const Px& p = img.at(std::min(2 * x + dx, w - 1), std::min(2 * y + dy, h - 1));
          for (int i = 0; i < 4; i++)
            s[i] += p.c[i];
        }
      for (int i = 0; i < 4; i++)
        o.at(x, y).c[i] = uint8_t(s[i] / 4);
    }
  *out = std::move(o);
  return true;
}

uint32_t To565(const Px& c) { return uint32_t(c.c[0] >> 3) << 11 | uint32_t(c.c[1] >> 2) << 5 | (c.c[2] >> 3); }
std::array<int, 3> From565(uint32_t v)
{
  int r = (v >> 11) & 31, g = (v >> 5) & 63, b = v & 31;
  return {(r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)};
}

void CmprBlock(const Px* px, Bytes& out)
{
  bool clear[16];
  int solid[16], ns = 0;
  bool anyClear = false;
  for (int i = 0; i < 16; i++)
  {
    clear[i] = px[i].c[3] < 0x80;
    if (clear[i])
      anyClear = true;
    else
      solid[ns++] = i;
  }
  if (!ns)
  {
    Put16BE(out, 0);
    Put16BE(out, 0xFFFF);
    for (int i = 0; i < 4; i++)
      out.push_back(0xff);
    return;
  }
  double mean[3];
  for (int i = 0; i < 3; i++)
  {
    long s = 0;
    for (int k = 0; k < ns; k++)
      s += px[solid[k]].c[i];
    mean[i] = double(s) / double(ns);
  }
  double axis[3] = {1.0, 1.0, 1.0};
  for (int it = 0; it < 8; it++)
  {
    double v[3] = {0.0, 0.0, 0.0};
    for (int k = 0; k < ns; k++)
    {
      const Px& p = px[solid[k]];
      double d[3];
      for (int i = 0; i < 3; i++)
        d[i] = double(p.c[i]) - mean[i];
      double t = 0.0;
      for (int i = 0; i < 3; i++)
      {
        double pr = d[i] * axis[i];
        t = t + pr;
      }
      for (int i = 0; i < 3; i++)
      {
        double pr = d[i] * t;
        v[i] = v[i] + pr;
      }
    }
    double n = std::max({std::fabs(v[0]), std::fabs(v[1]), std::fabs(v[2])});
    if (n == 0.0)
      n = 1.0;
    for (int i = 0; i < 3; i++)
      axis[i] = v[i] / n;
  }
  int lo = 0, hi = 0;
  double plo = 0, phi = 0;
  for (int k = 0; k < ns; k++)
  {
    const Px& p = px[solid[k]];
    double pj = 0.0;
    for (int i = 0; i < 3; i++)
    {
      double pr = (double(p.c[i]) - mean[i]) * axis[i];
      pj = pj + pr;
    }
    if (k == 0 || pj < plo)
      plo = pj, lo = k;
    if (k == 0 || pj > phi)
      phi = pj, hi = k;
  }
  uint32_t a = To565(px[solid[hi]]), b = To565(px[solid[lo]]);
  std::array<int, 3> pal[4];
  int np;
  if (anyClear)
  {
    if (a > b)
      std::swap(a, b);
    pal[0] = From565(a), pal[1] = From565(b);
    for (int i = 0; i < 3; i++)
      pal[2][i] = (pal[0][i] + pal[1][i]) / 2;
    np = 3;
  }
  else
  {
    if (a == b)
    {
      Put16BE(out, a);
      Put16BE(out, a);
      for (int i = 0; i < 4; i++)
        out.push_back(0);
      return;
    }
    if (a < b)
      std::swap(a, b);
    pal[0] = From565(a), pal[1] = From565(b);
    for (int i = 0; i < 3; i++)
    {
      pal[2][i] = (2 * pal[0][i] + pal[1][i]) / 3;
      pal[3][i] = (pal[0][i] + 2 * pal[1][i]) / 3;
    }
    np = 4;
  }
  int idx[16];
  for (int i = 0; i < 16; i++)
  {
    if (clear[i])
    {
      idx[i] = 3;
      continue;
    }
    int best = 0, bd = 0;
    for (int k = 0; k < np; k++)
    {
      int s = 0;
      for (int j = 0; j < 3; j++)
      {
        int df = int(px[i].c[j]) - pal[k][j];
        s += df * df;
      }
      if (k == 0 || s < bd)
        bd = s, best = k;
    }
    idx[i] = best;
  }
  Put16BE(out, a);
  Put16BE(out, b);
  for (int y = 0; y < 4; y++)
    out.push_back(uint8_t(idx[4 * y] << 6 | idx[4 * y + 1] << 4 | idx[4 * y + 2] << 2 | idx[4 * y + 3]));
}
void Cmpr(const Image& img, Bytes& out)
{
  int h = img.h, w = img.w;
  for (int ty = 0; ty < std::max(h, 8); ty += 8)
    for (int tx = 0; tx < std::max(w, 8); tx += 8)
      for (int s = 0; s < 4; s++)
      {
        int sy = (s >> 1) * 4, sx = (s & 1) * 4;
        Px blk[16];
        for (int y = 0; y < 4; y++)
          for (int x = 0; x < 4; x++)
            blk[y * 4 + x] = img.at(std::min(tx + sx + x, w - 1), std::min(ty + sy + y, h - 1));
        CmprBlock(blk, out);
      }
}

// --- paletted ---
std::vector<Px> Quantize(const std::vector<Px>& pixels, size_t n)
{
  std::vector<Px> uniq = pixels;
  std::sort(uniq.begin(), uniq.end());
  uniq.erase(std::unique(uniq.begin(), uniq.end()), uniq.end());
  if (uniq.size() <= n)
    return uniq;
  auto range = [](const std::vector<Px>& b, int i) {
    int mn = 255, mx = 0;
    for (const Px& p : b)
      mn = std::min<int>(mn, p.c[i]), mx = std::max<int>(mx, p.c[i]);
    return mx - mn;
  };
  std::vector<std::vector<Px>> boxes;
  boxes.push_back(pixels);
  while (boxes.size() < n)
  {
    std::vector<std::pair<int, size_t>> keys;  // (key, original index) for a stable sort
    keys.reserve(boxes.size());
    for (size_t i = 0; i < boxes.size(); i++)
    {
      int k = -1;
      if (boxes[i].size() > 1)
        k = std::max({range(boxes[i], 0), range(boxes[i], 1), range(boxes[i], 2), range(boxes[i], 3)});
      keys.push_back({k, i});
    }
    std::stable_sort(keys.begin(), keys.end(),
                     [](const auto& a, const auto& b) { return a.first < b.first; });
    std::vector<std::vector<Px>> sorted;
    sorted.reserve(boxes.size() + 1);
    for (auto& k : keys)
      sorted.push_back(std::move(boxes[k.second]));
    boxes = std::move(sorted);
    std::vector<Px> b = std::move(boxes.back());
    boxes.pop_back();
    bool two = false;
    for (const Px& p : b)
      if (!(p == b[0]))
      {
        two = true;
        break;
      }
    if (!two)
    {
      boxes.push_back(std::move(b));
      break;
    }
    int ch = 0, bestr = -1;
    for (int i = 0; i < 4; i++)
    {
      int r = range(b, i);
      if (r > bestr)
        bestr = r, ch = i;
    }
    std::stable_sort(b.begin(), b.end(), [ch](const Px& x, const Px& y) { return x.c[ch] < y.c[ch]; });
    size_t m = b.size() / 2;
    boxes.emplace_back(b.begin(), b.begin() + m);
    boxes.emplace_back(b.begin() + m, b.end());
  }
  std::vector<Px> out;
  for (auto& b : boxes)
  {
    if (b.empty())
      continue;
    long s[4] = {0, 0, 0, 0};
    for (const Px& p : b)
      for (int i = 0; i < 4; i++)
        s[i] += p.c[i];
    out.push_back(MakePx(int(s[0] / long(b.size())), int(s[1] / long(b.size())),
                         int(s[2] / long(b.size())), int(s[3] / long(b.size()))));
  }
  return out;
}
uint32_t PalWord(const Px& c, int fmt)
{
  int r = c.c[0], g = c.c[1], b = c.c[2], a = c.c[3];
  if (fmt == 1)
    return uint32_t(r >> 3) << 11 | uint32_t(g >> 2) << 5 | uint32_t(b >> 3);
  if (fmt == 2)
    return Rgb5a3(c);
  if (fmt == 0)
    return uint32_t(a << 8) | uint32_t((r * 30 + g * 59 + b * 11) / 100);
  FAILV("palette format not supported", 0);
}
Px PalRgba(uint32_t w, int fmt)
{
  if (fmt == 1)
  {
    auto c = From565(w);
    return MakePx(c[0], c[1], c[2], 255);
  }
  if (fmt == 2)
  {
    if (w & 0x8000)
      return MakePx(((w >> 10) & 31) * 255 / 31, ((w >> 5) & 31) * 255 / 31, (w & 31) * 255 / 31, 255);
    return MakePx(((w >> 8) & 15) * 17, ((w >> 4) & 15) * 17, (w & 15) * 17, ((w >> 12) & 7) * 255 / 7);
  }
  return MakePx(w & 255, w & 255, w & 255, w >> 8);
}
void Paletted(const Image& img, int levels_flag, int fmt, int bits, Bytes* palbytes, Bytes* data)
{
  std::vector<Image> lv{img};
  while (levels_flag)
  {
    Image h;
    if (!Half(lv.back(), &h))
      break;
    lv.push_back(std::move(h));
  }
  std::vector<Px> pal = Quantize(img.p, size_t(1) << bits);
  std::vector<Px> pr;
  for (const Px& c : pal)
    pr.push_back(PalRgba(PalWord(c, fmt), fmt));
  std::unordered_map<uint32_t, int> cache;
  auto idx = [&](const Px& p) {
    uint32_t key;
    std::memcpy(&key, p.c, 4);
    auto it = cache.find(key);
    if (it != cache.end())
      return it->second;
    int best = 0, bd = 0;
    for (size_t k = 0; k < pr.size(); k++)
    {
      int s = 0;
      for (int i = 0; i < 4; i++)
      {
        int df = int(p.c[i]) - int(pr[k].c[i]);
        s += df * df;
      }
      if (k == 0 || s < bd)
        bd = s, best = int(k);
    }
    cache[key] = best;
    return best;
  };
  int tw = 8, th = bits == 4 ? 8 : 4;
  for (const Image& l : lv)
  {
    for (int ty = 0; ty < std::max(l.h, th); ty += th)
      for (int tx = 0; tx < std::max(l.w, tw); tx += tw)
      {
        std::vector<int> vals;
        for (int y = 0; y < th; y++)
          for (int x = 0; x < tw; x++)
            vals.push_back(idx(l.at(std::min(tx + x, l.w - 1), std::min(ty + y, l.h - 1))));
        if (bits == 4)
          for (size_t i = 0; i < vals.size(); i += 2)
            data->push_back(uint8_t(vals[i] << 4 | vals[i + 1]));
        else
          for (int v : vals)
            data->push_back(uint8_t(v));
      }
  }
  for (const Px& c : pal)
    Put16BE(*palbytes, PalWord(c, fmt));
  for (size_t i = pal.size(); i < (size_t(1) << bits); i++)
    Put16BE(*palbytes, 0);
}

bool ReencodeLike(const Bytes& orig, const Image& img, Bytes* result)
{
  long pp = Find(orig, "GVRT");
  if (pp < 0 || size_t(pp) + 16 > orig.size())
    return Fail("no GVRT in the GameCube texture");
  size_t p = size_t(pp);
  int flags = orig[p + 10] & 0x0F, fmt = orig[p + 11];
  int w = orig[p + 12] << 8 | orig[p + 13], h = orig[p + 14] << 8 | orig[p + 15];
  if (w != img.w || h != img.h)
    return Fail("texture size " + std::to_string(img.w) + "x" + std::to_string(img.h) + " vs " +
                std::to_string(w) + "x" + std::to_string(h));
  Bytes data;
  size_t start = p + 16;
  if (fmt == 0x08 || fmt == 0x09)
  {
    if (!(flags & 8))
      return Fail("external palette not supported");
    Bytes pal;
    Paletted(img, flags & 1, orig[p + 10] >> 4, fmt == 0x08 ? 4 : 8, &pal, &data);
    if (Failed())
      return false;
    data.insert(data.begin(), pal.begin(), pal.end());
  }
  else if (fmt == 0x0E || fmt == 0x05)
  {
    Image lvl = img;
    for (;;)
    {
      if (fmt == 0x0E)
        Cmpr(lvl, data);
      else
        Tiles(lvl, data);
      if (!(flags & 1))
        break;
      Image nx;
      if (!Half(lvl, &nx))
        break;
      lvl = std::move(nx);
    }
  }
  else
    return Fail("GVR format not supported");
  size_t end = start + data.size();
  if (end > orig.size())
    return Fail("encoded data longer than the original");
  Bytes out(orig.begin(), orig.begin() + start);
  out.insert(out.end(), data.begin(), data.end());
  out.insert(out.end(), orig.begin() + end, orig.end());
  *result = std::move(out);
  return true;
}

Bytes NewCmpr(const Image& img, uint32_t index)
{
  Bytes data;
  Image lvl = img;
  for (;;)
  {
    Cmpr(lvl, data);
    Image nx;
    if (!Half(lvl, &nx))
      break;
    lvl = std::move(nx);
  }
  Bytes gvrt{0, 0, 0x01, 0x0E};
  Put16BE(gvrt, img.w);
  Put16BE(gvrt, img.h);
  gvrt.insert(gvrt.end(), data.begin(), data.end());
  Bytes blob{'G', 'C', 'I', 'X'};
  Put32(blob, 8, true);
  Put32(blob, index, false);
  Put32(blob, 0, false);
  for (char ch : {'G', 'V', 'R', 'T'})
    blob.push_back(uint8_t(ch));
  Put32(blob, uint32_t(gvrt.size()), true);
  blob.insert(blob.end(), gvrt.begin(), gvrt.end());
  while (blob.size() % 32)
    blob.push_back(0);
  return blob;
}

// ---------------------------------------------------------------------------------------------
// MLD (mldsplice.py)
// ---------------------------------------------------------------------------------------------
struct TexRec
{
  std::string name;
  size_t rec;   // offset of the 0x2c record
  size_t data;  // offset of the blob
  size_t size;
};

class Mld
{
public:
  Mld(Bytes d, bool le) : d_(std::move(d)), le_(le) {}
  uint32_t U(size_t p) const { return Rd(d_, p, le_); }
  void W(size_t p, uint32_t v) { Wr(d_, p, v, le_); }
  Bytes& Data() { return d_; }
  const Bytes& Data() const { return d_; }
  bool Entries(std::vector<size_t>* e) const
  {
    uint32_t cnt = U(0), idx = U(4);
    if (Failed())
      return false;
    if (size_t(idx) + 0x68 * size_t(cnt) > d_.size())
      return Fail("entry table out of range");
    e->clear();
    for (uint32_t i = 0; i < cnt; i++)
      e->push_back(size_t(idx) + 0x68 * size_t(i));
    return true;
  }
  uint32_t TexTable() const { return U(16); }
  bool ObjBlock(size_t entry, size_t slot, size_t* start_out, size_t* size_out) const
  {
    std::vector<size_t> es;
    if (!Entries(&es))
      return false;
    if (entry >= es.size())
      return Fail("entry index out of range");
    uint32_t objp = U(es[entry] + 0x14);
    uint32_t start = U(size_t(objp) + 4 + 4 * slot);
    uint32_t size = U(size_t(start) + 4);
    if (Failed())
      return false;
    if (size_t(start) + size > d_.size())
      return Fail("object block out of range");
    *start_out = start, *size_out = size;
    return true;
  }
  bool Replace(size_t start, size_t old_len, const Bytes& nw)
  {
    long delta = long(nw.size()) - long(old_len);
    if (delta % 4)
      return Fail("delta not 4-aligned");
    if (start + old_len > d_.size())
      return Fail("replace range out of file");
    std::set<size_t> slots{8, 12, 16};
    std::set<uint32_t> lists;
    std::vector<size_t> es;
    if (!Entries(&es))
      return false;
    for (size_t e : es)
    {
      for (size_t f : {0x08, 0x0c, 0x10, 0x14, 0x18, 0x1c, 0x20})
        slots.insert(e + f);
      for (size_t f : {0x08, 0x0c, 0x14, 0x18, 0x1c})
      {
        uint32_t p = U(e + f);
        if (p)
          lists.insert(p);
      }
    }
    if (Failed())
      return false;
    for (uint32_t p : lists)
    {
      uint32_t n = U(p);
      if (Failed())
        return false;
      if (size_t(n) * 4 + p + 4 > d_.size())
        return Fail("pointer list out of range");
      for (uint32_t k = 0; k < n; k++)
        slots.insert(size_t(p) + 4 + 4 * k);
    }
    std::vector<std::pair<size_t, uint32_t>> vals;
    for (size_t s : slots)
      vals.push_back({s, U(s)});
    if (Failed())
      return false;
    size_t end = d_.size();
    d_.erase(d_.begin() + start, d_.begin() + start + old_len);
    d_.insert(d_.begin() + start, nw.begin(), nw.end());
    for (auto& [s, v] : vals)
    {
      if (v > end)
        continue;  // global texture IDs and the like are not offsets
      if (v > start || (v == start && old_len == 0))
      {
        size_t ns = s >= start + old_len ? size_t(long(s) + delta) : s;
        W(ns, uint32_t(long(v) + delta));
      }
    }
    return !Failed();
  }
  size_t TexDataStart(size_t tt, size_t n) const
  {
    size_t s0 = tt + 4 + 0x2c * n;
    if (s0 + 4 <= d_.size())
    {
      const uint8_t* b = &d_[s0];
      if (!std::memcmp(b, "GCIX", 4) || !std::memcmp(b, "GBIX", 4) || !std::memcmp(b, "PVRT", 4) ||
          !std::memcmp(b, "GVRT", 4))
        return s0;
    }
    return (s0 + 31) & ~size_t(31);
  }
  bool TexAligned() const
  {
    size_t tt = TexTable(), n = U(tt), s0 = tt + 4 + 0x2c * n;
    return TexDataStart(tt, n) != s0 || s0 % 32 == 0;
  }
  bool TexRecords(std::vector<TexRec>* out) const
  {
    size_t tt = TexTable();
    size_t n = U(tt);
    if (Failed())
      return false;
    if (tt + 4 + 0x2c * n > d_.size())
      return Fail("texture table out of range");
    out->clear();
    size_t data = TexDataStart(tt, n);
    for (size_t i = 0; i < n; i++)
    {
      size_t r = tt + 4 + 0x2c * i;
      size_t len = 0;
      while (len < 0x20 && d_[r + len])
        len++;
      uint32_t size = U(r + 0x28);
      out->push_back({std::string(reinterpret_cast<const char*>(&d_[r]), len), r, data, size});
      data += size;
    }
    return !Failed();
  }
  // texture archive rebuild; edit(table, blobs) mutates the lists
  template <class F>
  bool Rebuild(F edit)
  {
    std::vector<TexRec> recs;
    if (!TexRecords(&recs))
      return false;
    size_t tt = TexTable();
    if (recs.empty())
      return Fail("no textures");
    size_t end = recs.back().data + recs.back().size;
    if (end > d_.size())
      return Fail("texture data out of range");
    Bytes tail(d_.begin() + end, d_.end());
    std::vector<Bytes> table, blobs;
    for (auto& r : recs)
    {
      if (r.data + r.size > d_.size())
        return Fail("texture data out of range");
      table.emplace_back(d_.begin() + r.rec, d_.begin() + r.rec + 0x2c);
      blobs.emplace_back(d_.begin() + r.data, d_.begin() + r.data + r.size);
    }
    bool aligned = TexAligned();
    edit(table, blobs);
    Bytes out;
    Put32(out, uint32_t(table.size()), le_);
    for (auto& t : table)
      out.insert(out.end(), t.begin(), t.end());
    while (aligned && (tt + out.size()) % 32)
      out.push_back(0);
    for (auto& b : blobs)
      out.insert(out.end(), b.begin(), b.end());
    out.insert(out.end(), tail.begin(), tail.end());
    d_.resize(tt);
    d_.insert(d_.end(), out.begin(), out.end());
    return !Failed();
  }
  bool TexInsertAfter(const std::string& after, const Bytes& rec, const Bytes& blob)
  {
    size_t i = SIZE_MAX;
    std::vector<TexRec> recs;
    if (!TexRecords(&recs))
      return false;
    for (size_t k = 0; k < recs.size(); k++)
      if (recs[k].name == after)
      {
        i = k;
        break;
      }
    if (i == SIZE_MAX)
      return Fail("texture " + after + " not in the GameCube file");
    return Rebuild([&](std::vector<Bytes>& table, std::vector<Bytes>& blobs) {
      table.insert(table.begin() + i + 1, rec);
      blobs.insert(blobs.begin() + i + 1, blob);
    });
  }
  bool TexReplace(const std::string& name, const Bytes& blob)
  {
    bool le = le_;
    return Rebuild([&](std::vector<Bytes>& table, std::vector<Bytes>& blobs) {
      for (size_t i = 0; i < table.size(); i++)
      {
        size_t len = 0;
        while (len < 0x20 && table[i][len])
          len++;
        if (std::string(reinterpret_cast<const char*>(table[i].data()), len) == name)
        {
          Wr(table[i], 0x28, uint32_t(blob.size()), le);
          blobs[i] = blob;
        }
      }
    });
  }
  // tex_swap: texture `name`'s slot becomes texture `new_name` (renamed, new data)
  bool TexSwap(const std::string& name, const std::string& new_name, const Bytes& blob)
  {
    if (new_name.size() > 0x20)
      return Fail("texture name too long");
    bool le = le_, found = false;
    if (!Rebuild([&](std::vector<Bytes>& table, std::vector<Bytes>& blobs) {
          for (size_t i = 0; i < table.size(); i++)
          {
            size_t len = 0;
            while (len < 0x20 && table[i][len])
              len++;
            if (std::string(reinterpret_cast<const char*>(table[i].data()), len) == name)
            {
              std::fill(table[i].begin(), table[i].begin() + 0x20, 0);
              std::copy(new_name.begin(), new_name.end(), table[i].begin());
              Wr(table[i], 0x28, uint32_t(blob.size()), le);
              blobs[i] = blob;
              found = true;
            }
          }
        }))
    {
      return false;
    }
    return found || Fail("texture " + name + " not in the GameCube file");
  }
  // Motion k of an entry: its NMDM / POF0 chunks, padded to 32 like the next block's start
  bool MotionExtent(size_t entry, size_t k, size_t* start, size_t* end) const
  {
    std::vector<size_t> es;
    if (!Entries(&es))
      return false;
    if (entry >= es.size())
      return Fail("entry index out of range");
    uint32_t p = U(es[entry] + 0x1c);
    if (Failed() || !p || k >= U(p))
      return Fail("motion slot out of range");
    size_t b = U(size_t(p) + 4 + 4 * k), e = b;
    if (Failed() || !b)
      return Fail("no motion in that slot");
    while (e + 8 <= d_.size() &&
           (!std::memcmp(&d_[e], "NMDM", 4) || !std::memcmp(&d_[e], "POF0", 4)))
    {
      e += 8 + size_t(U(e + 4));
      e = (e + 3) & ~size_t(3);
    }
    while (e % 32)
      e += 4;
    if (Failed() || e > d_.size())
      return Fail("motion out of range");
    *start = b, *end = e;
    return true;
  }

private:
  Bytes d_;
  bool le_;
};

// ---------------------------------------------------------------------------------------------
// Gilder's victory pose (victory.py): MLK archives, whole effect MLDs, battle scripts
// ---------------------------------------------------------------------------------------------
#define OK_SW(call) \
  do                \
  {                 \
    if (!(call))    \
      return false; \
  } while (0)

// MLK: u16 0, 0xffff, count, 0xffff; then (id, offset, size, type) u32s. Dreamcast: little-endian,
// type -1. GameCube: big-endian, type = the padding Sega added.
struct MlkEntry
{
  uint32_t id, off, size, type;
};
bool MlkEntries(const Bytes& d, bool le, std::vector<MlkEntry>* out)
{
  if (d.size() < 8)
    return Fail("MLK too short");
  uint32_t n = Rd16(&d[4], le);
  out->clear();
  for (uint32_t i = 0; i < n; i++)
  {
    size_t p = 8 + 16 * size_t(i);
    MlkEntry e{Rd(d, p, le), Rd(d, p + 4, le), Rd(d, p + 8, le), Rd(d, p + 12, le)};
    if (Failed())
      return false;
    if (size_t(e.off) + e.size > d.size())
      return Fail("MLK entry out of range");
    out->push_back(e);
  }
  return true;
}

// sw() of the Python: reverse the 4 bytes of each of n words at p
bool SwWords(Bytes& o, const Bytes& d, size_t p, size_t n)
{
  if (p + 4 * n > d.size())
    return Fail("MLD word out of range");
  for (size_t q = p; q < p + 4 * n; q += 4)
    for (int k = 0; k < 4; k++)
      o[q + k] = d[q + 3 - k];
  return true;
}
bool TagIs(const Bytes& d, size_t p, const char* tag)
{
  return p + 4 <= d.size() && !std::memcmp(&d[p], tag, 4);
}

// A motion block: NMDM chunks (every word 32-bit except the (type, inp_fn) u16 pair at body+8),
// then POF0 (kept, only its size swapped).
bool MotionBlockToGc(const Bytes& d, size_t b, size_t end, Bytes& o)
{
  size_t p = b;
  while (p + 8 <= end)
  {
    uint32_t size = Rd(d, p + 4, true);
    if (Failed() || p + 8 + size > end)
      return Fail("motion chunk out of range");
    if (TagIs(d, p, "NMDM"))
    {
      Wr(o, p + 4, size, false);
      OK_SW(SwWords(o, d, p + 8, size / 4));
      o[p + 16] = d[p + 17], o[p + 17] = d[p + 16], o[p + 18] = d[p + 19], o[p + 19] = d[p + 18];
    }
    else if (TagIs(d, p, "POF0"))
      Wr(o, p + 4, size, false);
    else
      break;
    p += 8 + size;
    p = (p + 3) & ~size_t(3);
  }
  return !Failed();
}

void Rgb565Tiles(const Image& img, Bytes& out)
{
  int h = img.h, w = img.w;
  for (int ty = 0; ty < std::max(h, 4); ty += 4)
    for (int tx = 0; tx < std::max(w, 4); tx += 4)
      for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++)
          Put16BE(out, To565(img.at(std::min(tx + x, w - 1), std::min(ty + y, h - 1))));
}

// A whole Dreamcast MLD (an MLK entry) to the GameCube form, as Sega's converter did.
bool MldToGc(const Bytes& d, uint32_t tex_index, Bytes* result)
{
  static const char* const kModelTags[] = {"NJTL", "NJCM", "POF0", "\0OF0"};
  Bytes o = d;
  uint32_t cnt = Rd(d, 0, true), idx = Rd(d, 4, true), tt = Rd(d, 16, true);
  OK_SW(SwWords(o, d, 0, 5));
  std::set<uint32_t> done;
  for (uint32_t i = 0; i < cnt; i++)
  {
    size_t e = size_t(idx) + 0x68 * size_t(i);
    // The name is 16 bytes; Sega's converter swapped the words after it too, name overflow included.
    OK_SW(SwWords(o, d, e, 9));
    OK_SW(SwWords(o, d, e + 0x34, 13));
    for (size_t f = 0x08; f < 0x24; f += 4)
    {
      uint32_t p = Rd(d, e + f, true);
      if (Failed())
        return false;
      if (!p || done.count(p))
        continue;
      done.insert(p);
      if (f == 0x20)
      {
        // NJS_TEXLIST (textures, count) -> NJS_TEXNAME (filename, attr, texaddr) each
        OK_SW(SwWords(o, d, p, 2));
        uint32_t t = Rd(d, p, true), nt = Rd(d, p + 4, true);
        if (t && !done.count(t))
        {
          done.insert(t);
          OK_SW(SwWords(o, d, t, 3 * size_t(nt)));
        }
        continue;
      }
      uint32_t n = Rd(d, p, true);
      OK_SW(SwWords(o, d, p, 1 + size_t(n)));
      for (uint32_t k = 0; k < n; k++)
      {
        uint32_t b = Rd(d, p + 4 + 4 * size_t(k), true);
        if (Failed())
          return false;
        if (!b || done.count(b))
          continue;
        done.insert(b);
        if (f == 0x14)
        {
          size_t end = size_t(b) + 16;
          while (end + 8 <= d.size() &&
                 std::any_of(std::begin(kModelTags), std::end(kModelTags),
                             [&](const char* t) { return !std::memcmp(&d[end], t, 4); }))
          {
            end += 8 + size_t(Rd(d, end + 4, true));
            end = (end + 3) & ~size_t(3);
          }
          if (end > d.size())
            return Fail("object block out of range");
          Bytes conv;
          OK_SW(BlockToGc(Bytes(d.begin() + b, d.begin() + end), &conv));
          std::copy(conv.begin(), conv.end(), o.begin() + b);
          // BlockToGc steps over a clobbered POF0 tag (a texture name's terminator overwrote its
          // P); its size is still swapped
          for (size_t q = size_t(b) + 16; q < end;)
          {
            if (q + 4 <= d.size() && !std::memcmp(&d[q], "\0OF0", 4))
              OK_SW(SwWords(o, d, q + 4, 1));
            q += 8 + size_t(Rd(d, q + 4, true));
            q = (q + 3) & ~size_t(3);
          }
        }
        else if (f == 0x1c)
        {
          size_t end = b;
          while (end + 8 <= d.size() && (TagIs(d, end, "NMDM") || TagIs(d, end, "POF0")))
          {
            end += 8 + size_t(Rd(d, end + 4, true));
            end = (end + 3) & ~size_t(3);
          }
          if (end > d.size())
            return Fail("motion block out of range");
          OK_SW(MotionBlockToGc(d, b, end, o));
        }
      }
    }
  }
  if (Failed())
    return false;
  if (!tt)
  {
    *result = std::move(o);
    return true;
  }
  uint32_t n = Rd(d, tt, true);
  if (Failed())
    return false;
  if (n == 0)
  {
    Bytes out(o.begin(), o.begin() + tt + 4);
    std::fill(out.begin() + tt, out.end(), 0);
    while (out.size() % 8)
      out.push_back(0);
    *result = std::move(out);
    return true;
  }
  // texture archive: count, 0x2c records (name, 3 words, the last the size), then the blobs, with no
  // gap (as on the Dreamcast), each a multiple of 32 bytes
  size_t data = size_t(tt) + 4 + 0x2c * size_t(n);
  if (!TagIs(d, data, "GBIX") && !TagIs(d, data, "PVRT"))
    return Fail("aligned texture table: not seen in effect files");
  Bytes recs, blobs;
  for (uint32_t i = 0; i < n; i++)
  {
    size_t r = size_t(tt) + 4 + 0x2c * size_t(i);
    uint32_t size = Rd(d, r + 0x28, true);
    if (Failed() || data + size > d.size())
      return Fail("effect texture out of range");
    Bytes blob(d.begin() + data, d.begin() + data + size);
    data += size;
    long pv = Find(blob, "PVRT");
    if (pv < 0 || size_t(pv) + 16 > blob.size())
      return Fail("no PVRT in the effect texture");
    Image img;
    OK_SW(PvrDecode(blob.data() + pv, blob.size() - pv, &img));
    int pf = blob[pv + 8], df = blob[pv + 9];
    // As Sega's converter: ARGB4444 -> RGB5A3 (alpha), a plain (non-VQ) RGB565 stays RGB565, the
    // rest (VQ, 1555) -> CMPR
    int fmt = pf == 2 ? 0x05 : (pf == 1 && (df == 1 || df == 2)) ? 0x04 : 0x0E;
    bool mips = df == 2 || df == 4 || df == 0x11 || df == 0x12;
    Bytes px;
    Image lvl = img;
    for (;;)
    {
      if (fmt == 0x05)
        Tiles(lvl, px);
      else if (fmt == 0x04)
        Rgb565Tiles(lvl, px);
      else
        Cmpr(lvl, px);
      Image nx;
      if (!mips || !Half(lvl, &nx))
        break;
      lvl = std::move(nx);
    }
    Bytes gvrt{0, 0, uint8_t(mips ? 0x01 : 0), uint8_t(fmt)};
    Put16BE(gvrt, img.w);
    Put16BE(gvrt, img.h);
    gvrt.insert(gvrt.end(), px.begin(), px.end());
    Bytes g{'G', 'C', 'I', 'X'};
    Put32(g, 8, true);
    Put32(g, tex_index, false);
    Put32(g, 0, false);
    for (char ch : {'G', 'V', 'R', 'T'})
      g.push_back(uint8_t(ch));
    Put32(g, uint32_t(gvrt.size()), true);
    g.insert(g.end(), gvrt.begin(), gvrt.end());
    while (g.size() % 32)
      g.push_back(0);
    Bytes rec(d.begin() + r, d.begin() + r + 0x2c);
    for (size_t q = 0x20; q < 0x2c; q += 4)
      std::reverse(rec.begin() + q, rec.begin() + q + 4);
    Wr(rec, 0x28, uint32_t(g.size()), false);
    recs.insert(recs.end(), rec.begin(), rec.end());
    blobs.insert(blobs.end(), g.begin(), g.end());
  }
  // the entry's end is padded to 32 (the MLK's type field records that padding)
  Bytes out(o.begin(), o.begin() + tt);
  Put32(out, n, false);
  out.insert(out.end(), recs.begin(), recs.end());
  out.insert(out.end(), blobs.begin(), blobs.end());
  while (out.size() % 32)
    out.push_back(0);
  *result = std::move(out);
  return !Failed();
}

// mlk_entry: the GameCube archive with entry `id` replaced by the converted Dreamcast one (type 0),
// entries packed in order
bool MlkReplace(Bytes& gc, const Bytes& jp, uint32_t id, uint32_t tex_index)
{
  std::vector<MlkEntry> ge, je;
  OK_SW(MlkEntries(gc, false, &ge));
  OK_SW(MlkEntries(jp, true, &je));
  auto j = std::find_if(je.begin(), je.end(), [&](const MlkEntry& e) { return e.id == id; });
  if (j == je.end())
    return Fail("MLK entry " + std::to_string(id) + " not in the Dreamcast file");
  if (std::none_of(ge.begin(), ge.end(), [&](const MlkEntry& e) { return e.id == id; }))
    return Fail("MLK entry " + std::to_string(id) + " not in the GameCube file");
  Bytes conv;
  OK_SW(MldToGc(Bytes(jp.begin() + j->off, jp.begin() + j->off + j->size), tex_index, &conv));
  Bytes out{0, 0, 0xff, 0xff};
  Put16BE(out, uint32_t(ge.size()));
  Put16BE(out, 0xffff);
  Bytes body;
  size_t base = 8 + 16 * ge.size();
  for (const MlkEntry& e : ge)
  {
    const bool rep = e.id == id;
    Put32(out, e.id, false);
    Put32(out, uint32_t(base + body.size()), false);
    Put32(out, uint32_t(rep ? conv.size() : e.size), false);
    Put32(out, rep ? 0 : e.type, false);
    if (rep)
      body.insert(body.end(), conv.begin(), conv.end());
    else
      body.insert(body.end(), gc.begin() + e.off, gc.begin() + e.off + e.size);
  }
  out.insert(out.end(), body.begin(), body.end());
  gc = std::move(out);
  return true;
}

// Battle scripts (bchara/*.std): u16 count (incl. end), u16 4, 0, 0, u32 total; then count records
// of (u16 type, u16 3, 0, size, offset from 0x10); type 0xffff ends. Record data 8-aligned.
struct StdRecord
{
  uint16_t type;
  size_t off, size;
};
bool StdRecords(const Bytes& d, bool le, std::vector<StdRecord>* out)
{
  if (d.size() < 16)
    return Fail("battle script too short");
  uint32_t n = Rd16(&d[0], le);
  out->clear();
  for (uint32_t i = 0; i < n; i++)
  {
    size_t p = 16 + 16 * size_t(i);
    if (p + 16 > d.size())
      return Fail("battle script table out of range");
    uint16_t t = Rd16(&d[p], le);
    size_t sz = Rd(d, p + 8, le), off = Rd(d, p + 12, le);
    if (t == 0xffff)
      continue;
    if (16 + off + sz > d.size())
      return Fail("battle script record out of range");
    out->push_back({t, 16 + off, sz});
  }
  return true;
}

// std_records: the script rebuilt from runs of GameCube records and converted Japanese ones
bool StdRebuild(Bytes& gc, const Bytes& jp, const std::vector<Op::Run>& runs,
                const std::map<std::string, std::string>& layouts)
{
  std::vector<StdRecord> g, j;
  OK_SW(StdRecords(gc, false, &g));
  OK_SW(StdRecords(jp, true, &j));
  std::vector<std::pair<uint16_t, Bytes>> recs;
  for (const Op::Run& run : runs)
  {
    const std::vector<StdRecord>& src = run.jp ? j : g;
    if (size_t(run.first) + run.count > src.size())
      return Fail("battle script run out of range");
    for (uint32_t i = run.first; i < run.first + run.count; i++)
    {
      const StdRecord& r = src[i];
      const Bytes& d = run.jp ? jp : gc;
      Bytes rec(d.begin() + r.off, d.begin() + r.off + r.size);
      if (run.jp)
      {
        auto lay = layouts.find(std::to_string(r.type) + "/" + std::to_string(r.size));
        if (lay == layouts.end() || lay->second.size() * 4 != r.size)
          return Fail("no layout for battle script record type " + std::to_string(r.type));
        for (size_t w = 0; w < lay->second.size(); w++)
        {
          uint8_t* q = &rec[4 * w];
          if (lay->second[w] == 'H')
            std::swap(q[0], q[1]), std::swap(q[2], q[3]);
          else if (lay->second[w] == 'W')
            std::reverse(q, q + 4);
          else if (lay->second[w] != 'B')
            return Fail("byte-level field in a record to convert");
        }
      }
      recs.emplace_back(r.type, std::move(rec));
    }
  }
  // table: (type, 3) as u16s, 0, size, offset from 0x10; header: (count, 4), 0, 0, total
  const size_t n = recs.size() + 1, base = 16 * n;
  Bytes table, body;
  for (const auto& [t, r] : recs)
  {
    Put16BE(table, t);
    Put16BE(table, 3);
    Put32(table, 0, false);
    Put32(table, uint32_t(r.size()), false);
    Put32(table, uint32_t(base + body.size()), false);
    body.insert(body.end(), r.begin(), r.end());
    while (body.size() % 8)
      body.push_back(0);
  }
  Put16BE(table, 0xffff);
  Put16BE(table, 0xffff);
  Put32(table, 0, false);
  Put32(table, 0, false);
  Put32(table, uint32_t(base + body.size()), false);
  Bytes out;
  Put16BE(out, uint32_t(n));
  Put16BE(out, 4);
  Put32(out, 0, false);
  Put32(out, 0, false);
  Put32(out, uint32_t(base + body.size()), false);
  out.insert(out.end(), table.begin(), table.end());
  out.insert(out.end(), body.begin(), body.end());
  gc = std::move(out);
  return true;
}

// SHA-1 of d[a, a+n), false when out of range
bool HashRange(const Bytes& d, size_t a, size_t n, std::string* hex)
{
  if (a + n > d.size())
    return Fail("range out of file");
  *hex = Sha(d.data() + a, n);
  return true;
}

}  // namespace

// ---------------------------------------------------------------------------------------------
// public API
// ---------------------------------------------------------------------------------------------
std::string Sha1Hex(const uint8_t* data, size_t size)
{
  return Sha(data, size);
}

bool AklzDecompress(const uint8_t* data, size_t size, std::vector<uint8_t>* out, size_t* consumed)
{
  if (size < 16 || std::memcmp(data, "AKLZ", 4))
    return false;
  size_t n = size_t(data[12]) << 24 | size_t(data[13]) << 16 | size_t(data[14]) << 8 | data[15];
  std::vector<uint8_t> ring(4096, 0), o;
  o.reserve(n);
  unsigned r = 0xFEE;
  size_t i = 16;
  unsigned flags = 0;
  while (o.size() < n && i < size)
  {
    flags >>= 1;
    if (!(flags & 0x100))
    {
      flags = data[i++] | 0xFF00u;
    }
    if (flags & 1)
    {
      if (i >= size)
        return false;
      uint8_t c = data[i++];
      o.push_back(c);
      ring[r] = c;
      r = (r + 1) & 0xFFF;
    }
    else
    {
      if (i + 2 > size)
        return false;
      unsigned b0 = data[i], b1 = data[i + 1];
      i += 2;
      unsigned off = b0 | ((b1 & 0xF0) << 4);
      for (unsigned k = 0; k < (b1 & 0x0F) + 3; k++)
      {
        uint8_t c = ring[(off + k) & 0xFFF];
        o.push_back(c);
        ring[r] = c;
        r = (r + 1) & 0xFFF;
      }
    }
  }
  if (o.size() > n)
    o.resize(n);
  if (consumed)
    *consumed = i;
  *out = std::move(o);
  return true;
}

bool AklzCompress(const std::vector<uint8_t>& data, size_t size, std::vector<uint8_t>* out,
                  std::string* error)
{
  g_err.clear();
  std::vector<Tok> toks = TokensGreedy(data, 64);
  if (EncodedSize(toks) > size)
    toks = TokensOptimal(data, 64);
  if (EncodedSize(toks) > size)
    toks = TokensOptimal(data, 4096);
  std::vector<Tok> fitted;
  if (!Fit(toks, data, size, &fitted))
  {
    if (error)
      *error = g_err;
    return false;
  }
  *out = Encode(fitted, data.size());
  return true;
}

namespace
{
#define CHECK(cond, msg)  \
  do                      \
  {                       \
    if (!(cond))          \
      return Fail(msg);   \
  } while (0)
#define OK(call)          \
  do                      \
  {                       \
    if (!(call))          \
      return false;       \
  } while (0)

bool ConvertImpl(const FileOps& entry, const std::vector<uint8_t>& gc_file,
                 const std::vector<uint8_t>& dc_file, std::vector<uint8_t>* out, std::string* stage)
{
  CHECK(Sha(gc_file.data(), gc_file.size()) == entry.gc_sha1,
        "the GameCube file is not the expected one (SHA-1 mismatch)");
  CHECK(Sha(dc_file.data(), dc_file.size()) == entry.dc_sha1,
        "the Dreamcast file is not the expected one (SHA-1 mismatch)");
  Bytes dec;
  CHECK(AklzDecompress(gc_file.data(), gc_file.size(), &dec), "the GameCube file does not decompress");
  const size_t original_size = dec.size();
  Mld gc(std::move(dec), false);
  Mld jp(dc_file, true);
  for (size_t oi = 0; oi < entry.ops.size(); oi++)
  {
    const Op& op = entry.ops[oi];
    *stage = "op " + std::to_string(oi) + " (" + op.name + ")";
    switch (op.kind)
    {
    case Op::Kind::Model:
    {
      *stage = "op " + std::to_string(oi) + " (model entry " + std::to_string(op.gc_entry) + " slot " +
               std::to_string(op.slot) + ")";
      size_t c, csz, a, asz;
      std::string h;
      OK(gc.ObjBlock(op.gc_entry, op.slot, &c, &csz));
      OK(HashRange(gc.Data(), c, csz, &h));
      CHECK(h == op.gc_sha1, "GameCube block hash mismatch");
      OK(jp.ObjBlock(op.jp_entry, op.slot, &a, &asz));
      OK(HashRange(jp.Data(), a, asz, &h));
      CHECK(h == op.jp_sha1, "Dreamcast block hash mismatch");
      Bytes jb(jp.Data().begin() + a, jp.Data().begin() + a + asz), conv;
      OK(BlockToGc(jb, &conv));
      OK(gc.Replace(c, csz, conv));
      break;
    }
    case Op::Kind::Tex:
    case Op::Kind::TexAdd:
    {
      // Japanese blob: the last record with the name
      TexRec jr{};
      bool jf = false;
      std::vector<TexRec> recs;
      OK(jp.TexRecords(&recs));
      for (auto& r : recs)
        if (r.name == op.name)
          jr = r, jf = true;
      CHECK(jf, "texture not in the Dreamcast file");
      CHECK(jr.data + jr.size <= jp.Data().size(), "Dreamcast texture out of range");
      Bytes blob(jp.Data().begin() + jr.data, jp.Data().begin() + jr.data + jr.size);
      CHECK(Sha(blob.data(), blob.size()) == op.jp_sha1, "Dreamcast texture hash mismatch");
      long pv = Find(blob, "PVRT");
      CHECK(pv >= 0, "no PVRT in the Dreamcast texture");
      Image img;
      OK(PvrDecode(blob.data() + pv, blob.size() - pv, &img));
      if (op.kind == Op::Kind::Tex)
      {
        Bytes orig;
        bool f = false;
        OK(gc.TexRecords(&recs));
        for (auto& r : recs)
          if (r.name == op.name)
          {
            CHECK(r.data + r.size <= gc.Data().size(), "texture out of range");
            orig.assign(gc.Data().begin() + r.data, gc.Data().begin() + r.data + r.size);
            f = true;
            break;
          }
        CHECK(f, "texture not in the GameCube file");
        CHECK(Sha(orig.data(), orig.size()) == op.gc_sha1, "GameCube texture hash mismatch");
        Bytes enc;
        OK(ReencodeLike(orig, img, &enc));
        OK(gc.TexReplace(op.name, enc));
      }
      else
      {
        long q = Find(blob, "GBIX");
        uint32_t index = q >= 0 ? Rd(blob, size_t(q) + 8, true) : 0;
        OK(!Failed());
        Bytes gv = NewCmpr(img, index);
        size_t pr = SIZE_MAX;
        OK(gc.TexRecords(&recs));
        for (auto& r : recs)
          if (r.name == op.after)
          {
            pr = r.rec;
            break;
          }
        CHECK(pr != SIZE_MAX, "texture " + op.after + " not in the GameCube file");
        Bytes rec(op.name.begin(), op.name.end());
        CHECK(rec.size() <= 0x20, "texture name too long");
        rec.resize(0x20, 0);
        CHECK(pr + 0x28 <= gc.Data().size(), "texture table out of range");
        rec.insert(rec.end(), gc.Data().begin() + pr + 0x20, gc.Data().begin() + pr + 0x28);
        Put32(rec, uint32_t(gv.size()), false);
        OK(gc.TexInsertAfter(op.after, rec, gv));
      }
      break;
    }
    case Op::Kind::TexBlank:
    {
      std::vector<TexRec> recs;
      OK(gc.TexRecords(&recs));
      for (auto& r : recs)
        if (r.name == op.name)
        {
          Bytes& d = gc.Data();
          CHECK(r.data + r.size <= d.size(), "texture out of range");
          Bytes blob(d.begin() + r.data, d.begin() + r.data + r.size);
          long p = Find(blob, "GVRT");
          CHECK(p >= 0, "no GVRT in the texture to blank");
          if (size_t(p) + 16 < r.size)
            std::fill(d.begin() + r.data + p + 16, d.begin() + r.data + r.size, 0);
          break;
        }
      break;
    }
    case Op::Kind::Motion:
    {
      *stage = "op " + std::to_string(oi) + " (motion entry " + std::to_string(op.gc_entry) + " slot " +
               std::to_string(op.slot) + ")";
      size_t g0, g1, j0, j1;
      std::string h;
      OK(gc.MotionExtent(op.gc_entry, op.slot, &g0, &g1));
      OK(HashRange(gc.Data(), g0, g1 - g0, &h));
      CHECK(h == op.gc_sha1, "GameCube motion hash mismatch");
      OK(jp.MotionExtent(op.jp_entry, op.slot, &j0, &j1));
      OK(HashRange(jp.Data(), j0, j1 - j0, &h));
      CHECK(h == op.jp_sha1, "Dreamcast motion hash mismatch");
      Bytes jb(jp.Data().begin() + j0, jp.Data().begin() + j1), conv = jb;
      OK(MotionBlockToGc(jb, 0, jb.size(), conv));
      OK(gc.Replace(g0, g1 - g0, conv));
      break;
    }
    case Op::Kind::TexSwap:
    {
      std::vector<TexRec> recs;
      Bytes blob, orig;
      OK(jp.TexRecords(&recs));
      for (auto& r : recs)
        if (r.name == op.new_name && r.data + r.size <= jp.Data().size())
          blob.assign(jp.Data().begin() + r.data, jp.Data().begin() + r.data + r.size);
      CHECK(!blob.empty(), "texture " + op.new_name + " not in the Dreamcast file");
      CHECK(Sha(blob.data(), blob.size()) == op.jp_sha1, "Dreamcast texture hash mismatch");
      OK(gc.TexRecords(&recs));
      for (auto& r : recs)
        if (r.name == op.name && r.data + r.size <= gc.Data().size())
        {
          orig.assign(gc.Data().begin() + r.data, gc.Data().begin() + r.data + r.size);
          break;
        }
      CHECK(!orig.empty(), "texture " + op.name + " not in the GameCube file");
      CHECK(Sha(orig.data(), orig.size()) == op.gc_sha1, "GameCube texture hash mismatch");
      long pv = Find(blob, "PVRT");
      CHECK(pv >= 0, "no PVRT in the Dreamcast texture");
      Image img;
      OK(PvrDecode(blob.data() + pv, blob.size() - pv, &img));
      long q = Find(blob, "GBIX");
      uint32_t index = q >= 0 ? Rd(blob, size_t(q) + 8, true) : 0;
      OK(!Failed());
      OK(gc.TexSwap(op.name, op.new_name, NewCmpr(img, index)));
      break;
    }
    case Op::Kind::MlkEntry:
      *stage = "op " + std::to_string(oi) + " (MLK entry " + std::to_string(op.id) + ")";
      OK(MlkReplace(gc.Data(), jp.Data(), op.id, op.tex_index));
      break;
    case Op::Kind::StdRecords:
      *stage = "op " + std::to_string(oi) + " (battle script records)";
      OK(StdRebuild(gc.Data(), jp.Data(), op.runs, op.layouts));
      break;
    case Op::Kind::Pad:
      if (gc.Data().size() < original_size)
        gc.Data().resize(original_size, 0);
      break;
    }
  }
  *stage = "result";
  CHECK(Sha(gc.Data().data(), gc.Data().size()) == entry.dec_sha1, "converted file hash mismatch");
  *stage = "compress";
  std::string err;
  CHECK(AklzCompress(gc.Data(), entry.size ? entry.size : gc_file.size(), out, &err), err);
  CHECK(out->size() == entry.size, "compressed size differs from the manifest");
  return true;
}
}  // namespace

bool Convert(const FileOps& entry, const std::vector<uint8_t>& gc_file,
             const std::vector<uint8_t>& dc_file, std::vector<uint8_t>* out, std::string* error)
{
  g_err.clear();
  std::string stage = "start";
  if (ConvertImpl(entry, gc_file, dc_file, out, &stage) && !Failed())
    return true;
  if (error)
    *error = stage + ": " + (g_err.empty() ? std::string("failed") : g_err);
  return false;
}
}  // namespace DiscIO::SkiesUncensor
