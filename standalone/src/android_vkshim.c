// The custom GPU driver's way into Dawn (Android): Dawn loads "libvulkan.so" from its search paths and takes only
// vkGetInstanceProcAddr from it. main.cpp opens the custom driver through libadrenotools (as the Dolphin build
// does), hands this library the driver's vkGetInstanceProcAddr, and puts a libvulkan.so symlink to this library in
// a folder Aurora gives Dawn first (AURORA_VULKAN_LIBRARY_DIR). Without a driver this library isn't used.
// SPDX-License-Identifier: GPL-3.0-or-later
#include <stddef.h>

typedef void (*SkiesVkVoidFunction)(void);
typedef SkiesVkVoidFunction (*SkiesVkGetInstanceProcAddr)(void* instance, const char* name);

static SkiesVkGetInstanceProcAddr s_get_instance_proc_addr;

__attribute__((visibility("default"))) void skies_vkshim_set(SkiesVkGetInstanceProcAddr get_instance_proc_addr)
{
  s_get_instance_proc_addr = get_instance_proc_addr;
}

__attribute__((visibility("default"))) SkiesVkVoidFunction vkGetInstanceProcAddr(void* instance, const char* name)
{
  return s_get_instance_proc_addr ? s_get_instance_proc_addr(instance, name) : NULL;
}
