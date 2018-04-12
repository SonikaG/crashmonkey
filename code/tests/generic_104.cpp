/*
Reporducing fstest generic/104

1. Add two files, foo and bar to the same directory
2. Add hard links for both files
3. Only fsync bar
4. Check that the hardlinks exist after crash and that metadata is consistent

*/

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
#define TEST_FILE_FOO_LINK "foo_link"
#define TEST_FILE_BAR_LINK "bar_link"
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


class Generic104: public BaseTestCase {
 public:

  virtual int setup() override {
    // Create test directory A.
    int res = mkdir(TEST_MNT "/" TEST_DIR_A, 0777);
    if (res < 0) {
      return -1;
    }
    sync();
    return 0;
  }

  virtual int run() override {
    
    //create files foo and bar  
    const int fd_foo = open(foo_path.c_str(), O_RDWR | O_CREAT, TEST_FILE_PERMS);
      if (fd_foo < 0) {
        return -1;
      }
  
    const int fd_bar = open(bar_path.c_str(), O_RDWR | O_CREAT, TEST_FILE_PERMS);
      if (fd_bar < 0){
        return -2;
      }
  
    //create a link foo_link to foo
    if (link(foo_path.c_str(), foo_link_path.c_str()) < 0){
      return -3;
    }

    //crate a link bar_link to bar
    if (link(bar_path.c_str(), bar_link_path.c_str()) < 0){
      return -4;
    }

    //fsync bar only  
    if(fsync(fd_bar) < 0){
      close(fd_bar);
      return -5;
    }

    //Make a user checkpoint here. Checkpoint must be one beyond this point. 
    if(Checkpoint() < 0){
      return -6;
    }

    //close open files 
    close(fd_foo);
    close(fd_bar);
  }

  virtual int check_test(unsigned int last_checkpoint, 
    DataTestResult *test_result) override{
    
    struct stat filestat;
    if(stat(TEST_DIR_A, &filestat) == -1){
      return -1;
    }
    
    //some error occured--both links did not persist
    if (filestat.st_nlink != 2){
       return -2;
    }
    
    //need to add code to count links
    if (remove(foo_ilnk_path.c_str()) < 0 ){
      return -3;
    }
   
    if(remove(bar_link_path.c_str()) < 0 ){
      return -4;
     }
     

  }

   private:
    const string foo_path = TEST_MNT "/" TEST_DIR_A "/" TEST_FILE_FOO;
    const string bar_path = TEST_MNT "/" TEST_DIR_A "/" TEST_FILE_BAR;
    const string foo_link_path = TEST/MNT "/" TEST_DIR_A "/" TEST_FILE_FOO_LINK;
    const string bar_link_path = TEST/MNT "/" TEST_DIR_A "/" TEST_FILE_BAR_LINK;

};

}  // namespace tests
}  // namespace fs_testing

extern "C" fs_testing::tests::BaseTestCase *test_case_get_instance() {
  return new fs_testing::tests::Generic039;
}

extern "C" void test_case_delete_instance(fs_testing::tests::BaseTestCase *tc) {
  delete tc;
}

