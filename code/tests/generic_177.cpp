#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <string>
#include <iostream>
#include <dirent.h>
#include <cstring>
#include <errno.h>

#include "BaseTestCase.h"
#include "../user_tools/api/workload.h"
#include "../user_tools/api/actions.h"
#define TEST_FILE_FOO "foo"
#define TEST_FILE_BAR "bar"
#define TEST_MNT "/mnt/snapshot"
#define TEST_DIR_A "test_dir_a"


using fs_testing::tests::DataTestResult;
using fs_testing::user_tools::api::WriteData;
using fs_testing::user_tools::api::WriteDataMmap;
using fs_testing::user_tools::api::Checkpoint;
using std::string;

#define TEST_FILE_PERMS  ((mode_t) (S_IRWXU | S_IRWXG | S_IRWXO))

namespace fs_testing {
namespace tests {


class Generic177: public BaseTestCase {
 public:
  virtual int setup() override {
    // Create test directory A.
    int res = mkdir(TEST_MNT "/" TEST_DIR_A, 0777);
    if (res < 0) {
      return -1;
    }

    //Create file foo in TEST_DIR_A 
    const int fd_foo = open(foo_path.c_str(), O_RDWR | O_CREAT, TEST_FILE_PERMS);
    if (fd_foo < 0) {
      return -1;
    }

    //Write 128KB of data to the file
    if(WriteData(fd_foo, 0, 131072) < 0){
      close(fd_foo);
      return -2;
    }


  }

   virtual int run() override {



   }



   private:
    const string foo_path = TEST_MNT "/" TEST_DIR_A "/" TEST_FILE_FOO;
    const string bar_path = TEST_MNT "/" TEST_DIR_A "/" TEST_FILE_BAR;

};

}  // namespace tests
}  // namespace fs_testing

extern "C" fs_testing::tests::BaseTestCase *test_case_get_instance() {
  return new fs_testing::tests::Generic039;
}

extern "C" void test_case_delete_instance(fs_testing::tests::BaseTestCase *tc) {
  delete tc;
}

