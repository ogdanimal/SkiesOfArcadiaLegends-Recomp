# libchdr's find_package(zstd MODULE) (WITH_SYSTEM_ZSTD): the standalone's own zstd, already a target
# (zstd::zstd, made in CMakeLists.txt from zstd::libzstd)
if(TARGET zstd::zstd)
  set(zstd_FOUND TRUE)
endif()
