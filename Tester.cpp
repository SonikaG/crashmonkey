#include <dlfcn.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>

#include "hellow_ioctl.h"
#include "Tester.h"

#define TEST_CLASS_FACTORY    "get_instance"
#define TEST_CLASS_DEFACTORY  "delete_instance"

#define DIRTY_EXPIRE_TIME_PATH "/proc/sys/vm/dirty_expire_centisecs"

#define PV_DISK       "/dev/ram0"
#define VG_DISK       "fs_consist_test"
#define LV_DISK       "fs_consist_test_dev"
#define SN_DISK       "fs_consist_test_snap"
#define FULL_LV_PATH  "/dev/" VG_DISK "/" LV_DISK
#define FULL_SN_PATH  "/dev/" VG_DISK "/" SN_DISK
#define FULL_WRAPPER_PATH "/dev/hwm"

#define LV_SIZE       "50%FREE"
#define SN_SIZE       "100%FREE"

#define TEST_PATH     FULL_LV_PATH

// TODO(ashmrtn): Make a quiet and regular version of commands.
#define INIT_PV     "pvcreate " PV_DISK " > /dev/null 2>&1"
#define DESTROY_PV  "pvremove -f " PV_DISK " > /dev/null 2>&1"
#define INIT_VG     "vgcreate " VG_DISK " " PV_DISK " > /dev/null 2>&1"
#define DESTROY_VG  "vgremove -f " VG_DISK " > /dev/null 2>&1"
#define INIT_LV     "lvcreate " VG_DISK " -n " LV_DISK " -l " LV_SIZE " > /dev/null 2>&1"
#define INIT_SN     \
  "lvcreate -s -n " SN_DISK " -l " SN_SIZE " " FULL_LV_PATH " > /dev/null 2>&1"
#define DESTROY_SN  "lvremove -f " FULL_SN_PATH " > /dev/null 2>&1"

#define MNT_LVM_LV_DEV_PATH  FULL_LV_PATH
#define MNT_LVM_SN_DEV_PATH  FULL_SN_PATH
#define MNT_WRAPPER_DEV_PATH FULL_WRAPPER_PATH
#define MNT_MNT_POINT        "/mnt/snapshot"

#define FMT_FMT_DRIVE   "mkfs -t ext4 " FULL_LV_PATH " > /dev/null 2>&1"

#define INSMOD_MODULE_NAME "hellow.ko"
#define WRAPPER_INSMOD      "insmod " INSMOD_MODULE_NAME " > /dev/null 2>&1"
#define WRAPPER_RMMOD       "rmmod " INSMOD_MODULE_NAME " > /dev/null 2>&1"

#define SECTOR_SIZE 512

// TODO(ashmrtn): Expand to work with other file system types.
#define TEST_CASE_FSCK "fsck -T -t ext4 " FULL_SN_PATH " -- -y > /dev/null 2>&1"

namespace fs_testing {

using std::calloc;
using std::cerr;
using std::cout;
using std::endl;
using std::free;
using std::shared_ptr;
using std::vector;

using fs_testing::create_t;
using fs_testing::destroy_t;

int Tester::lvm_init() {
  // Start of physical volume on LVM.
  if (system(INIT_PV) != 0) {
    return LVM_PV_INIT_ERR;
  }
  lvm_pv_active = true;

  // Start of volume group on LVM.
  if (system(INIT_VG) != 0) {
    if (system(DESTROY_PV) != 0) {
      return LVM_PV_REMOVE_ERR;
    }
    lvm_pv_active = false;
    lvm_vg_active = false;
    return LVM_VG_INIT_ERR;
  }
  lvm_vg_active = true;

  // Start of logical volume on LVM.
  if (system(INIT_LV) != 0) {
    if (system(DESTROY_VG) != 0) {
      return LVM_VG_REMOVE_ERR;
    }
    lvm_vg_active = false;
    if (system(DESTROY_PV) != 0) {
      return LVM_PV_REMOVE_ERR;
    }
    lvm_pv_active = false;
    return LVM_LV_INIT_ERR;
  }
  return SUCCESS;
}

int Tester::lvm_destroy() {
  // Removing an LVM volume group also removes all logical volumes for the
  // group.
  if (lvm_vg_active) {
    if (system(DESTROY_VG) != 0) {
      return LVM_VG_REMOVE_ERR;
    }
    lvm_vg_active = false;
  }

  // Remove LVM physcial volume.
  if (lvm_pv_active) {
    if (system(DESTROY_PV) != 0) {
      return LVM_PV_REMOVE_ERR;
    }
    lvm_pv_active = false;
  }
  return SUCCESS;
}

int Tester::init_snapshot() {
  if (!lvm_sn_active) {
    if (system(INIT_SN) != 0) {
      lvm_sn_active = false;
      return LVM_SN_INIT_ERR;
    }
    lvm_sn_active = true;
    return SUCCESS;
  }
  return LVM_SN_INIT_ERR;
}

int Tester::destroy_snapshot() {
  if (lvm_sn_active) {
    if (system(DESTROY_SN) != 0) {
      lvm_sn_active = true;
      return LVM_SN_REMOVE_ERR;
    }
  }
  lvm_sn_active = false;
  return SUCCESS;
}

// TODO(ashmrtn): Fix to work with all file system types.
int Tester::mount_device(const int dev, const char* opts) {
  char* path = NULL;
  switch (dev) {
    case MNT_LVM_LV_DEV:
      path = (char*) MNT_LVM_LV_DEV_PATH;
      break;
    case MNT_LVM_SN_DEV:
      path = (char*) MNT_LVM_SN_DEV_PATH;
      break;
    case MNT_WRAPPER_DEV:
      path = (char*) MNT_WRAPPER_DEV_PATH;
      break;
    default:
      disk_mounted = false;
      return MNT_BAD_DEV_ERR;
  }
  if (mount(path, MNT_MNT_POINT, "ext4", 0, (void*) opts) < 0) {
    disk_mounted = false;
    return MNT_MNT_ERR;
  }
  disk_mounted = true;
  return SUCCESS;
}

int Tester::umount_device() {
  if (disk_mounted) {
    if (umount(MNT_MNT_POINT) < 0) {
      disk_mounted = true;
      return MNT_UMNT_ERR;
    }
  }
  disk_mounted = false;
  return SUCCESS;
}

int Tester::insert_wrapper() {
  if (!wrapper_inserted) {
    if (system(WRAPPER_INSMOD) != 0) {
      wrapper_inserted = false;
      return WRAPPER_INSERT_ERR;
    }
  }
  wrapper_inserted = true;
  return SUCCESS;
}

int Tester::remove_wrapper() {
  if (wrapper_inserted) {
    if (system(WRAPPER_RMMOD) != 0) {
      wrapper_inserted = true;
      return WRAPPER_REMOVE_ERR;
    }
  }
  wrapper_inserted = false;
  return SUCCESS;
}

int Tester::get_wrapper_ioctl() {
  ioctl_fd = open(FULL_WRAPPER_PATH, O_RDONLY | O_CLOEXEC);
  if (ioctl_fd == -1) {
    return WRAPPER_OPEN_DEV_ERR;
  }
  return SUCCESS;
}

void Tester::put_wrapper_ioctl() {
  if (ioctl_fd != -1) {
    close(ioctl_fd);
    ioctl_fd = -1;
  }
}

void Tester::begin_wrapper_logging() {
  if (ioctl_fd != -1) {
    ioctl(ioctl_fd, HWM_LOG_ON);
  }
}

void Tester::end_wrapper_logging() {
  if (ioctl_fd != -1) {
    ioctl(ioctl_fd, HWM_LOG_OFF);
  }
}

int Tester::get_wrapper_log() {
  if (ioctl_fd != -1) {
    while (1) {
      disk_write_op_meta meta;

      int result = ioctl(ioctl_fd, HWM_GET_LOG_META, &meta);
      if (result == -1) {
        if (errno == ENODATA) {
          break;
        } else if (errno == EFAULT) {
          cerr << "efault occurred\n";
          log_data.clear();
          return WRAPPER_DATA_ERR;
        }
      }

      char* data = new char[meta.size];
      result = ioctl(ioctl_fd, HWM_GET_LOG_DATA, (void*) data);
      if (result == -1) {
        if (errno == ENODATA) {
          // Should never reach here as loop will break when getting the size
          // above.
          break;
        } else if (errno == EFAULT) {
          cerr << "efault occurred\n";
          log_data.clear();
          return WRAPPER_MEM_ERR;
        }
      }
      log_data.emplace_back(meta, (void*) data);
      delete[] data;

      result = ioctl(ioctl_fd, HWM_NEXT_ENT);
      if (result == -1) {
        if (errno == ENODATA) {
          // Should never reach here as loop will break when getting the size
          // above.
          break;
        } else {
          cerr << "Error getting next log entry\n";
          log_data.clear();
          break;
        }
      }
    }
  }
  return SUCCESS;
}

void Tester::clear_wrapper_log() {
  if (ioctl_fd != -1) {
    ioctl(ioctl_fd, HWM_CLR_LOG);
  }
}

int Tester::test_load_class(const char* path) {
  const char* dl_error = NULL;

  loader_handle = dlopen(path, RTLD_LAZY);
  if (loader_handle == NULL) {
    cerr << "Error loading test from class " << path << endl << dlerror()
      << endl;
    return TEST_CASE_HANDLE_ERR;
  }

  // Get needed methods from loaded class.
  test_factory = dlsym(loader_handle, TEST_CLASS_FACTORY);
  dl_error = dlerror();
  if (dl_error) {
    cerr << "Error gettig factory method " << dl_error << endl;
    dlclose(loader_handle);
    test_factory = NULL;
    test_killer = NULL;
    loader_handle = NULL;
    return TEST_CASE_INIT_ERR;
  }
  test_killer = dlsym(loader_handle, TEST_CLASS_DEFACTORY);
  dl_error = dlerror();
  if (dl_error) {
    cerr << "Error gettig deleter method " << dl_error << endl;
    dlclose(loader_handle);
    test_factory = NULL;
    test_killer = NULL;
    loader_handle = NULL;
    return TEST_CASE_DEST_ERR;
  }
  test = ((create_t*)(test_factory))();
  return SUCCESS;
}

void Tester::test_unload_class() {
  if (loader_handle != NULL && test != NULL) {
    ((destroy_t*)(test_killer))(test);
    dlclose(loader_handle);
    test_factory = NULL;
    test_killer = NULL;
    loader_handle = NULL;
    test = NULL;
  }
}

const char* Tester::update_dirty_expire_time(const char* time) {
  const int expire_fd = open(DIRTY_EXPIRE_TIME_PATH, O_RDWR | O_CLOEXEC);
  if (!read_dirty_expire_time(expire_fd)) {
    close(expire_fd);
    return NULL;
  }
  if (!write_dirty_expire_time(expire_fd, time)) {
    // Attempt to restore old dirty expire time.
    if (!write_dirty_expire_time(expire_fd, dirty_expire_time)) {
    }
    close(expire_fd);
    return NULL;
  }
  close(expire_fd);
  return dirty_expire_time;
}

bool Tester::read_dirty_expire_time(const int fd) {
  int bytes_read = 0;
  do {
    const int res = read(fd, (void*) ((long) dirty_expire_time + bytes_read),
        DIRTY_EXPIRE_TIME_SIZE - 1 - bytes_read);
    if (res == 0) {
      break;
    } else if (res < 0) {
      return false;
    }
    bytes_read += res;
  } while (bytes_read < DIRTY_EXPIRE_TIME_SIZE - 1);
  // Null terminate character array.
  dirty_expire_time[bytes_read] = '\0';
  return true;
}

bool Tester::write_dirty_expire_time(const int fd, const char* time) {
  const int size = strnlen(time, DIRTY_EXPIRE_TIME_SIZE);
  int bytes_written = 0;
  do {
    const int res = write(fd, time + bytes_written, size - bytes_written);
    if (res < 0) {
      return false;
    }
    bytes_written += res;
  } while (bytes_written < size);
  return true;
}

// TODO(ashmrtn): Fix to work with all file system types.
int Tester::format_lvm_drive(const int type) {
  if (system(FMT_FMT_DRIVE) != 0) {
    return FMT_FMT_ERR;
  }
}

int Tester::test_setup() {
  return test->setup();
}

int Tester::test_run() {
  return test->run();
}

int Tester::test_check_random_permutations(const int num_rounds) {
  std::fill(test_test_stats, test_test_stats + 6, 0);
  test_test_stats[TESTS_TESTS_RUN] = 0;
  permuter p = permuter(&log_data);
  vector<disk_write> permutes = log_data;
  const auto start_itr = permutes.begin();
  for (int rounds = 0; rounds < num_rounds; ++rounds) {

    for (auto end_itr = start_itr; end_itr != permutes.end(); ++end_itr) {
    //for (auto end_itr = permutes.end() - 1; end_itr != permutes.end(); ++end_itr) {
      ++test_test_stats[TESTS_TESTS_RUN];
      cout << '.' << std::flush;
      // Kill snapshot.
      if (lvm_sn_active) {
        if (destroy_snapshot() != SUCCESS) {
          cout << endl;
          return LVM_SN_REMOVE_ERR;
        }
      }

      // Create new snapshot.
      if (init_snapshot() != SUCCESS) {
        cout << endl;
        return LVM_SN_INIT_ERR;
      }

      // Write recorded data out to block device in different orders so that we
      // can they are all valid or not.
      const int sn_fd = open(FULL_SN_PATH, O_WRONLY);
      if (sn_fd < 0) {
        cout << endl;
        return TEST_CASE_FILE_ERR;
      }
      if (!test_write_data(sn_fd, start_itr, end_itr)) {
        ++test_test_stats[TESTS_TEST_ERR];
        cout << "test errored in writing data" << endl;
        close(sn_fd);
        continue;
      }
      close(sn_fd);

      const int fsck_res = system(TEST_CASE_FSCK);
      if (!(fsck_res == 0 || WEXITSTATUS(fsck_res) == 1)) {
        cerr << "Error running fsck on snapshot file system: " <<
          WEXITSTATUS(fsck_res) << "\n";
        ++test_test_stats[TESTS_TEST_FSCK_FAIL];
      } else {
        if (mount_device(MNT_LVM_SN_DEV, NULL) != SUCCESS) {
          ++test_test_stats[TESTS_TEST_FSCK_FAIL];
        }
        const int test_check_res = test->check_test();
        if (test_check_res < 0) {
          ++test_test_stats[TESTS_TEST_BAD_DATA];
        } else if (test_check_res == 0 && fsck_res != 0) {
          ++test_test_stats[TESTS_TEST_FSCK_FIX];
        } else if (test_check_res == 0 && fsck_res == 0) {
          ++test_test_stats[TESTS_TEST_PASS];
        } else {
          ++test_test_stats[TESTS_TEST_ERR];
          cout << "test errored for other reason" << endl;
        }
        umount_device();
      }
    }
    p.permute_random(&permutes);
  }
  cout << endl;
  return SUCCESS;
}

bool Tester::test_write_data(const int disk_fd,
    const vector<disk_write>::iterator& start,
    const vector<disk_write>::iterator& end) {
  for (auto current = start; current != end; ++current) {
    // Operation is not a write so skip it.
    if (!(current->has_write_flag())) {
      continue;
    }

    const unsigned long int byte_addr =
      current->metadata.write_sector * SECTOR_SIZE;
    if (lseek(disk_fd, byte_addr, SEEK_SET) < 0) {
      return false;
    }
    int bytes_written = 0;
    shared_ptr<void> data = current->get_data();
    void* data_base_addr = data.get();
    do {
      int res = write(disk_fd,
          (void*) ((unsigned long) data_base_addr + bytes_written),
          current->metadata.size - bytes_written);
      if (res < 0) {
        return false;
      }
      bytes_written += res;
    } while (bytes_written < current->metadata.size);
  }
  return true;
}

void Tester::cleanup_harness() {
  if (umount_device() != SUCCESS) {
    cerr << "Unable to unmount device" << endl;
    test_unload_class();
    return;
  }

  if (remove_wrapper() != SUCCESS) {
    cerr << "Unable to remove wrapper device" << endl;
    test_unload_class();
    return;
  }

  if (lvm_destroy() != SUCCESS) {
    cerr << "Unable to destroy LVM" << endl;
    test_unload_class();
    return;
  }

  test_unload_class();
}

}  // namespace fs_testing