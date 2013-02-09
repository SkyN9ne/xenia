/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2013 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include <xenia/kernel/fs/devices/local_directory_device.h>


using namespace xe;
using namespace xe::kernel;
using namespace xe::kernel::fs;


namespace {


class LocalFileMemoryMapping : public MemoryMapping {
public:
  LocalFileMemoryMapping(uint8_t* address, size_t length, xe_mmap_ref mmap) :
      MemoryMapping(address, length) {
    mmap_ = xe_mmap_retain(mmap);
  }
  virtual ~LocalFileMemoryMapping() {
    xe_mmap_release(mmap_);
  }
private:
  xe_mmap_ref mmap_;
};


class LocalFileEntry : public FileEntry {
public:
  LocalFileEntry(Device* device, const char* path, const xechar_t* local_path) :
      FileEntry(device, path) {
    local_path_ = xestrdup(local_path);
  }
  virtual ~LocalFileEntry() {
    xe_free(local_path_);
  }

  const xechar_t* local_path() { return local_path_; }

  virtual MemoryMapping* CreateMemoryMapping(
      xe_file_mode file_mode, const size_t offset, const size_t length) {
    xe_pal_ref pal = device()->pal();
    xe_mmap_ref mmap = xe_mmap_open(pal, file_mode, local_path_,
                                    offset, length);
    xe_pal_release(pal);
    if (!mmap) {
      return NULL;
    }

    LocalFileMemoryMapping* lfmm = new LocalFileMemoryMapping(
        (uint8_t*)xe_mmap_get_addr(mmap), xe_mmap_get_length(mmap),
        mmap);
    xe_mmap_release(mmap);

    return lfmm;
  }

private:
  xechar_t*   local_path_;
};


}


LocalDirectoryDevice::LocalDirectoryDevice(xe_pal_ref pal, const char* path,
                                           const xechar_t* local_path) :
    Device(pal, path) {
  local_path_ = xestrdup(local_path);
}

LocalDirectoryDevice::~LocalDirectoryDevice() {
  xe_free(local_path_);
}

Entry* LocalDirectoryDevice::ResolvePath(const char* path) {
  // The filesystem will have stripped our prefix off already, so the path will
  // be in the form:
  // some\PATH.foo

  XELOGFS(XT("LocalDirectoryDevice::ResolvePath(%s)"), path);

  xechar_t full_path[XE_MAX_PATH];
#if XE_WCHAR
  xesnprintf(full_path, XECOUNT(full_path), XT("%ls%c%hs"),
             local_path_, XE_PATH_SEPARATOR, path);
#else
  xesnprintf(full_path, XECOUNT(full_path), XT("%s%c%s"),
             local_path_, XE_PATH_SEPARATOR, path);
#endif  // XE_WCHAR

  // Swap around path separators.
  if (XE_PATH_SEPARATOR != '\\') {
    for (size_t n = 0; n < XECOUNT(full_path); n++) {
      if (full_path[n] == 0) {
        break;
      }
      if (full_path[n] == '\\') {
        full_path[n] = XE_PATH_SEPARATOR;
      }
    }
  }

  // TODO(benvanik): get file info
  // TODO(benvanik): fail if does not exit
  // TODO(benvanik): switch based on type

  LocalFileEntry* file_entry = new LocalFileEntry(this, path, full_path);
  return file_entry;
}
