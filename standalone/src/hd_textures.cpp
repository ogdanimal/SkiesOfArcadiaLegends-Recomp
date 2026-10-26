// HD textures: the texture packs the player imported (texture_pack.cpp, the Dolphin build's import)
// into USER/Load/Textures/GEA, drawn through Aurora's texture replacement, the port of Dolphin's
// custom texture loader (GXRuntime graphics/aurora/lib/gfx/texture_replacement.cpp: the same
// tex1_WxH[_m]_<hash>[_<tlut>]_<fmt> names and hashes). GFX.ini [Settings] HiresTextures, as the
// Dolphin build's Load Custom Textures; the menu's Graphics > HD textures turns it on and off live.
// The files are registered when it's turned on and each loads the first time the game uses its
// texture; turning it off drops them and the game's own textures come back.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "host.h"
#include "texture_pack.h"
#include "texture_pack_file.h"

#include <aurora/texture.hpp>

#include <string>

namespace host
{
namespace
{
aurora::texture::ReplacementGroup s_group;
bool s_on = false;
}  // namespace

std::string TexturesDir()
{
  if (SettingsUserDir().empty())
    return {};
  return SettingsUserDir() + DIR_SEP "Load" DIR_SEP "Textures" DIR_SEP;
}

bool TexturePackImported()
{
  const std::string dir = TexturesDir();
  return !dir.empty() && !TexturePackImport::Describe(dir).empty();
}

int TexturePackBc7Skipped()
{
  return aurora::texture::bc7_supported() ? 0 : TexturePackImport::Bc7Count(TexturesDir());
}

void VideoHdTextures(bool on)
{
  if (on == s_on)
    return;
  s_on = on;
  aurora::texture::unregister_replacements(s_group);
  s_group = {};
  if (!on)
  {
    Log("[textures] HD textures off\n");
    return;
  }
  // Dolphin's folders for the game: the region-free GEA (where the import puts packs) and the
  // disc's own ID, if the player made one by hand
  const std::string dir = TexturesDir();
  const std::string disc_id(reinterpret_cast<const char*>(DiscHeader()), 6);
  for (const std::string& folder : {std::string("GEA"), disc_id})
  {
    if (!File::IsDirectory(dir + folder))
      continue;
    aurora::texture::ReplacementGroup group =
        aurora::texture::load_replacement_directory(StringToPath(dir + folder));
    s_group.registrations.insert(s_group.registrations.end(), group.registrations.begin(),
                                 group.registrations.end());
  }
  const std::string pack = TexturePackImport::Describe(dir);
  Log("[textures] HD textures on: %zu files registered (%s)%s\n", s_group.registrations.size(),
      pack.empty() ? "nothing imported" : pack.c_str(),
      TexturePackImport::Bc7Count(dir) > 0 && !aurora::texture::bc7_supported() ?
          ", BC7 skipped on this GPU" :
          "");
}

// Every 600 frames while on: how many of the pack's textures have loaded (the game used them) so far
void HdTexturesLog()
{
  if (!s_on)
    return;
  static aurora::texture::ReplacementStats s_last;
  const aurora::texture::ReplacementStats stats = aurora::texture::replacement_stats();
  if (stats.loaded == s_last.loaded && stats.failed == s_last.failed)
    return;
  s_last = stats;
  Log("[textures] %u of %u loaded, %u failed, %.0f MB; decodes %.0f ms (longest %.1f), uploads %.0f "
      "ms (longest %.1f)\n",
      stats.loaded, stats.registered, stats.failed, static_cast<double>(stats.bytes) / (1 << 20),
      stats.loadMsTotal, stats.loadMsMax, stats.uploadMsTotal, stats.uploadMsMax);
}

int TexturePackCommand(const std::string& pack, bool remove)
{
  const std::string dir = TexturesDir();
  if (remove)
  {
    if (!TexturePackImport::Remove(dir))
    {
      Log("Can't remove %sGEA\n", dir.c_str());
      return 1;
    }
    Log("Texture packs removed\n");
    return 0;
  }
  std::string summary;
  const std::string error = TexturePackImport::Import(
      pack, dir, &summary, [](const std::string& status) { Log("%s\n", status.c_str()); });
  if (!error.empty())
  {
    Log("Import failed: %s\n", error.c_str());
    return 1;
  }
  Log("Imported. Now in %sGEA: %s\n", dir.c_str(), summary.c_str());
  const int bc7 = TexturePackImport::Bc7Count(dir);
  if (bc7 > 0)
    Log("%d of these are BC7 DDS textures, which most phones can't load.\n", bc7);
  return 0;
}
}  // namespace host
