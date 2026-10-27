// The release runner's part (ModernGekko's tools/moderngekko_run.cpp RunMain): what the Dolphin
// build's skies-run did before its runtime started, so the launchers start this program in its
// place with the same command line (--game G --user-dir U [--load-state F] [-X11 | --wayland]) and
// an update over an earlier release carries on with the same data folder, game and settings.
//
// Without --user-dir the data folder is found as the launcher finds it (portable.txt,
// data-folder.txt, the default folder), and without --game it is default-game.txt's. config.ini sets
// the resolution, full screen and the title's FPS at every start. A release build (SKIES_RELEASE_GAMES:
// the release's disc IDs, pinned DOLs and DOL patches, release_games.hpp) patches the game's
// main.dol, refuses a DOL it doesn't pin, and keeps another version's or a romhack's save states in
// their own folder, all with the runner's own code (lib/ModernGekko/tools). A module is looked for
// beside the program, then in USER/StaticRecompModules.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "host.h"

#include "dol_patch.hpp"
#include "frontend_config.hpp"
#include "release_games.hpp"
#include "user_directory.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#ifdef SKIES_STANDALONE_AURORA
#include <SDL3/SDL.h>
#endif
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

#ifndef SKIES_USER_DIRECTORY_NAME
#define SKIES_USER_DIRECTORY_NAME "Skies of Arcadia Legends"
#endif

namespace host
{
WindowOptions g_window;

namespace
{
namespace fs = std::filesystem;
namespace frontend = moderngekko::frontend;

fs::path ProgramDirectory(const char* argv0)
{
  std::error_code ec;
#if defined(_WIN32)
  wchar_t path[MAX_PATH * 4];
  const DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH * 4);
  if (length > 0 && length < MAX_PATH * 4)
    return fs::path(path).parent_path();
#elif defined(__linux__)
  const fs::path self = fs::read_symlink("/proc/self/exe", ec);
  if (!ec)
    return self.parent_path();
#elif defined(__APPLE__)
  uint32_t size = 0;
  _NSGetExecutablePath(nullptr, &size);
  std::string path(size, '\0');
  if (_NSGetExecutablePath(path.data(), &size) == 0)
  {
    const fs::path resolved = fs::weakly_canonical(fs::path(path.c_str()), ec);
    if (!ec)
      return resolved.parent_path();
  }
#endif
  ec.clear();
  const fs::path resolved = fs::weakly_canonical(fs::path(argv0), ec);
  return ec ? fs::current_path() : resolved.parent_path();
}

// The release's data (default-game.txt, portable.txt, patches, Sys): the program's folder, except in
// a macOS app bundle (Contents/MacOS holds the programs and modules, Contents/Resources the data)
fs::path DataDirectory(const fs::path& program_directory)
{
#if defined(__APPLE__)
  std::error_code ec;
  const fs::path contents = program_directory.parent_path();
  if (program_directory.filename() == "MacOS" && contents.filename() == "Contents" &&
      fs::is_directory(contents / "Resources", ec))
    return contents / "Resources";
#endif
  return program_directory;
}

std::string ModuleName(const std::string& disc_id)
{
#if defined(_WIN32)
  return "g" + disc_id + "_recomp.dll";
#elif defined(__APPLE__)
  return "g" + disc_id + "_recomp.dylib";
#else
  return "g" + disc_id + "_recomp.so";
#endif
}

void SetEnv(const char* name, const char* value, bool overwrite)
{
#if defined(_WIN32)
  if (overwrite || !std::getenv(name))
    _putenv_s(name, value);
#else
  setenv(name, value, overwrite ? 1 : 0);
#endif
}

std::string s_config_user_dir;  // where config.ini is ("" before FrontendPrepare)
}  // namespace

void FrontendError(const char* title, const std::string& message)
{
  Log("%s: %s\n", title, message.c_str());
#ifdef SKIES_STANDALONE_AURORA
  SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, message.c_str(), nullptr);
#endif
}

int FrontendPrepare(const char* argv0, LaunchOptions& o)
{
  // Steam's virtual controller counts as a controller (the runner sets the same)
  SetEnv("SDL_GAMECONTROLLER_ALLOW_STEAM_VIRTUAL_GAMEPAD", "1", false);
  if (o.window_system == LaunchOptions::WindowSystem::X11)
    SetEnv("SDL_VIDEO_DRIVER", "x11", true);
  else if (o.window_system == LaunchOptions::WindowSystem::Wayland)
    SetEnv("SDL_VIDEO_DRIVER", "wayland", true);

  const fs::path program_directory = ProgramDirectory(argv0);
  const fs::path data_directory = DataDirectory(program_directory);
  if (o.user.empty())
  {
    const frontend::UserDirectoryChoice choice = frontend::FindUserDirectory(
        data_directory, frontend::DefaultUserDirectory(SKIES_USER_DIRECTORY_NAME, false));
    // Started by itself (not by the launcher) with the data moved to a folder that isn't there:
    // starting would make a new, empty one
    if (choice.missing)
    {
      FrontendError("Data folder not found",
                    "The data folder isn't there:\n" + frontend::PathUtf8(choice.path) +
                        "\n\nConnect the drive it is on, or open the launcher to choose another.");
      return 1;
    }
    o.user = frontend::PathUtf8(choice.path);
  }
  const fs::path user = frontend::Utf8Path(o.user);
  if (o.game.empty())
  {
    std::ifstream file(user / "default-game.txt");
    std::string line;
    std::getline(file, line);
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    const fs::path game = frontend::ResolveDefaultGame(line, data_directory, user);
    if (game.empty())
    {
      Log("no game configured: use --game once or create %s\n",
          frontend::PathUtf8(user / "default-game.txt").c_str());
      return 2;
    }
    o.game = frontend::PathUtf8(game);
  }
  const fs::path game = frontend::Utf8Path(o.game);

  // config.ini, as the launcher writes it: the resolution at every start (the in-game menu's
  // changes go back into it, FrontendResolutionChanged), full screen, FPS in the title
  const frontend::ConfigResult config = frontend::LoadConfig(user, true);
  if (!config)
  {
    Log("invalid config.ini: %s\n", config.error.c_str());
    return 2;
  }
  s_config_user_dir = o.user;
  o.efb_scale = config.dolphin_scale;
  g_window.fullscreen = config.fullscreen;
  g_window.fps_in_title = config.show_fps_in_title;
  o.frame_interpolation = config.frame_interpolation;
  // The controller profile the launcher picked, in GCPadNew.ini when there's none yet (its Controls
  // page reads that file)
  if (!config.controller.empty())
  {
    std::string message;
    if (!frontend::EnsureControllerConfig(user, config.controller, &message))
      Log("controller configuration: %s\n", message.c_str());
  }

  // The release's game: its DOL patched (an update re-patches with the new release's patch), the
  // states of a romhack or another version apart
  const std::string disc_id = frontend::ReadDiscId(game);
  if (const frontend::ReleaseGame* release = frontend::FindReleaseGame(disc_id);
      release && !release->dol_patch.empty())
  {
    const fs::path patch = data_directory / release->dol_patch;
    bool changed = false;
    std::string error;
    frontend::DolPatchNotes notes;
    if (!frontend::ApplyDolPatchManifest(game / "sys" / "main.dol", patch, &changed, &error, &notes))
    {
      Log("DOL patching failed: %s\n", error.c_str());
      // Almost always a game imported by an earlier release; the launcher's Select Disc updates it
      FrontendError("The game needs updating",
                    "This game was imported by an earlier version. Open the launcher and choose "
                    "Select Disc with the same disc image to update it. Your saves and settings "
                    "are kept.");
      return frontend::RUNNER_EXIT_EXPLAINED;
    }
    if (changed)
      Log("Applied native DOL patches\n");
    if (notes.modified_data)
      Log("main.dol: modified game data (a romhack)\n");
    if (const std::string tag = frontend::RomhackTag(game, patch); !tag.empty())
      o.state_dir = frontend::PathUtf8(frontend::StateDirectory(user, tag));
    else if (const std::string folder = frontend::StateFolderFor(release->disc_id); !folder.empty())
    {
      const fs::path directory = frontend::StateDirectory(user, {}, folder);
      frontend::MoveDiscStates(user, directory, release->disc_id);
      o.state_dir = frontend::PathUtf8(directory);
    }
    if (!o.state_dir.empty())
      Log("save states in %s\n", o.state_dir.c_str());
  }
  if (frontend::IsReleaseBuild())
  {
    const frontend::ReleaseGame* release = frontend::FindReleaseGame(disc_id);
    if (!release)
    {
      Log("unsupported disc ID: expected %s, got %s\n", frontend::ReleaseDiscIds().c_str(),
          disc_id.c_str());
      return 2;
    }
    if (!release->dol_sha256.empty() &&
        !frontend::DolMatchesPin(game / "sys" / "main.dol",
                                 release->dol_patch.empty() ? fs::path{}
                                                            : data_directory / release->dol_patch,
                                 release->dol_sha256))
    {
      Log("unsupported main DOL: this release requires its pinned game build\n");
      return 2;
    }
  }

  // The module beside the program, else one the player put in the data folder
  if (o.module.empty() && !disc_id.empty())
  {
    const std::string name = ModuleName(disc_id);
    std::error_code ec;
    if (fs::is_regular_file(program_directory / name, ec))
      o.module = frontend::PathUtf8(program_directory / name);
    else if (fs::is_regular_file(user / "StaticRecompModules" / name, ec))
      o.module = frontend::PathUtf8(user / "StaticRecompModules" / name);
    else
    {
      // Usually a partly extracted download, or antivirus
      FrontendError("Game files missing",
                    name + " is missing from the game's folder:\n" +
                        frontend::PathUtf8(program_directory) +
                        "\n\nExtract the whole download again, keeping its files together. If the "
                        "file goes missing again, your antivirus may have removed it.");
      return frontend::RUNNER_EXIT_EXPLAINED;
    }
  }
  // The release's Sys folder: the DSP coefficients and the Dreamcast files' manifests
  {
    std::error_code ec;
    if (o.dsp_coef.empty() && fs::is_regular_file(data_directory / "Sys" / "GC" / "dsp_coef.bin", ec))
      o.dsp_coef = frontend::PathUtf8(data_directory / "Sys" / "GC" / "dsp_coef.bin");
    if (o.sys_dir.empty() && fs::is_directory(data_directory / "Sys" / "Skies", ec))
      o.sys_dir = frontend::PathUtf8(data_directory / "Sys");
  }
  return -1;
}

void FrontendResolutionChanged(int scale)
{
  // Only the scales config.ini holds (its LoadConfig refuses others at the next start); the window's
  // own (0) leaves it as it was
  if (s_config_user_dir.empty() || scale < 1 || scale > 12)
    return;
  const fs::path user = frontend::Utf8Path(s_config_user_dir);
  frontend::ConfigResult saved = frontend::LoadConfig(user, false);
  if (!saved)
    return;
  saved.resolution = std::to_string(640 * scale) + "x" + std::to_string(528 * scale);
  for (const auto& option : frontend::SupportedResolutions())
  {
    if (option.dolphin_scale == scale)
      saved.resolution = option.text;
  }
  std::string error;
  if (!frontend::SaveConfig(user, saved, &error))
    Log("can't save the new resolution to config.ini: %s\n", error.c_str());
}
}  // namespace host
