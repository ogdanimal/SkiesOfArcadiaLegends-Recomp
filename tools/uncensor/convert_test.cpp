// Verifies SkiesUncensorConvert against the Python prototype's reference outputs.
// Build: g++ -std=c++20 -O2 -I lib/ModernGekko/vendor/dolphin/Source/Core -I lib/ModernGekko/vendor/dolphin/Externals/picojson tools/uncensor/convert_test.cpp lib/ModernGekko/vendor/dolphin/Source/Core/DiscIO/SkiesUncensorConvert.cpp -o convert_test
// Run:   ./convert_test [research_root] [game_filter e.g. GEAE8P] [path_substring]
//   research_root defaults to ~/projects/GameCube-Research/targets/SkiesOfArcadiaLegends
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

#include "DiscIO/SkiesUncensorConvert.h"
#include "picojson.h"

using namespace DiscIO::SkiesUncensor;

static bool ReadFile(const std::string& path, std::vector<uint8_t>* out)
{
  std::ifstream f(path, std::ios::binary);
  if (!f)
    return false;
  out->assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
  return true;
}

static FileOps ParseEntry(const picojson::object& o)
{
  FileOps e;
  e.gc_sha1 = o.at("gc_sha1").get<std::string>();
  e.out_sha1 = o.at("out_sha1").get<std::string>();
  e.dc_sha1 = o.at("dc_sha1").get<std::string>();
  e.dec_sha1 = o.at("dec_sha1").get<std::string>();
  e.size = uint32_t(o.at("size").get<double>());
  e.dc_disc = int(o.at("dc_disc").get<double>());
  e.dc_path = o.at("dc_path").get<std::string>();
  for (const auto& v : o.at("ops").get<picojson::array>())
  {
    const auto& p = v.get<picojson::object>();
    Op op;
    std::string k = p.at("op").get<std::string>();
    auto str = [&](const char* n) { return p.count(n) ? p.at(n).get<std::string>() : std::string(); };
    auto num = [&](const char* n) { return p.count(n) ? uint32_t(p.at(n).get<double>()) : 0u; };
    if (k == "model")
      op.kind = Op::Kind::Model;
    else if (k == "tex")
      op.kind = Op::Kind::Tex;
    else if (k == "tex_add")
      op.kind = Op::Kind::TexAdd;
    else if (k == "tex_blank")
      op.kind = Op::Kind::TexBlank;
    else if (k == "mlk_entry")
      op.kind = Op::Kind::MlkEntry;
    else if (k == "std_records")
      op.kind = Op::Kind::StdRecords;
    else if (k == "pad")
      op.kind = Op::Kind::Pad;
    else
      throw std::runtime_error("unknown op " + k);
    op.name = str("name"), op.after = str("after"), op.gc_sha1 = str("gc_sha1"), op.jp_sha1 = str("jp_sha1");
    op.gc_entry = num("gc_entry"), op.slot = num("slot"), op.jp_entry = num("jp_entry");
    op.id = num("id"), op.tex_index = num("tex_index");
    if (p.count("records"))
      for (const auto& r : p.at("records").get<picojson::array>())
      {
        const auto& a = r.get<picojson::array>();
        op.runs.push_back({a[0].get<std::string>() == "jp", uint32_t(a[1].get<double>()),
                           uint32_t(a[2].get<double>())});
      }
    if (p.count("layouts"))
      for (const auto& [key, lay] : p.at("layouts").get<picojson::object>())
        op.layouts[key] = lay.get<std::string>();
    e.ops.push_back(std::move(op));
  }
  return e;
}

int main(int argc, char** argv)
{
  std::string root = argc > 1 ? argv[1] : std::string(getenv("HOME")) + "/projects/GameCube-Research/targets/SkiesOfArcadiaLegends";
  std::string gfilter = argc > 2 ? argv[2] : "", pfilter = argc > 3 ? argv[3] : "";
  std::ifstream mf(root + "/uncensor/manifest/uncensor-manifest.json");
  picojson::value m;
  std::string err = picojson::parse(m, mf);
  if (!err.empty())
  {
    fprintf(stderr, "manifest: %s\n", err.c_str());
    return 2;
  }
  struct G { const char* id; const char* gc; const char* ref; };
  const G games[] = {{"GEAE8P", "disc/files", "usa"}, {"GEAP8P", "eu/disc/files", "eu"}, {"GEAJ8P", "jp/disc/files", "jp"}};
  int pass = 0, fail = 0;
  double total = 0, worst = 0;
  std::string worst_name;
  for (const G& g : games)
  {
    if (!gfilter.empty() && gfilter != g.id)
      continue;
    const auto& files = m.get<picojson::object>().at("games").get<picojson::object>().at(g.id).get<picojson::object>();
    for (const auto& [path, val] : files)
    {
      if (!pfilter.empty() && path.find(pfilter) == std::string::npos)
        continue;
      FileOps e = ParseEntry(val.get<picojson::object>());
      std::vector<uint8_t> gcf, dcf, ref, out;
      std::string dcp = root + "/dcjp/files" + std::to_string(e.dc_disc) + "/" + e.dc_path;
      std::string label = std::string(g.id) + " " + path;
      if (!ReadFile(root + "/" + g.gc + "/" + path, &gcf) || !ReadFile(dcp, &dcf) ||
          !ReadFile(root + "/uncensor/manifest/" + g.ref + "/" + path, &ref))
      {
        printf("FAIL %s: cannot read inputs\n", label.c_str());
        fail++;
        continue;
      }
      auto t0 = std::chrono::steady_clock::now();
      std::string error;
      bool ok = Convert(e, gcf, dcf, &out, &error);
      double dt = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
      total += dt;
      if (dt > worst)
        worst = dt, worst_name = label;
      std::string why;
      if (!ok)
        why = error;
      else
      {
        std::vector<uint8_t> a, b;
        size_t used = 0;
        if (out.size() != e.size || out.size() != ref.size())
          why = "size " + std::to_string(out.size()) + " vs " + std::to_string(ref.size());
        else if (!AklzDecompress(out.data(), out.size(), &a, &used) || !AklzDecompress(ref.data(), ref.size(), &b))
          why = "does not decompress";
        else if (used != out.size())
          why = "stream ends at " + std::to_string(used) + " of " + std::to_string(out.size());
        else if (a != b)
          why = "decompressed output differs from the reference";
        else if (Sha1Hex(a.data(), a.size()) != e.dec_sha1)
          why = "dec_sha1 mismatch";
      }
      if (why.empty())
      {
        pass++;
        printf("ok   %-8s %-34s %7.3fs\n", g.id, path.c_str(), dt);
      }
      else
      {
        fail++;
        printf("FAIL %s: %s\n", label.c_str(), why.c_str());
      }
      fflush(stdout);
    }
  }
  printf("\n%d passed, %d failed; total %.2fs, worst %.3fs (%s)\n", pass, fail, total, worst, worst_name.c_str());
  return fail ? 1 : 0;
}
