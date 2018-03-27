/*
Reproducing ftest generic/090

1. Create file with data and then sync it 
2. Add a hard link to a file f
3. Sync the filesystem 
4. Write to file f, increasing its size
5. fsync that file
6. Crash and see if the datat written to the file is there

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
#define TEST_MNT "/mnt/snapshot"
#define TEST_DIR_A "test_dir_a"
#define TEST_TEXT_SIZE 65536

using fs_testing::tests::DataTestResult;
using fs_testing::user_tools::api::WriteData;
using fs_testing::user_tools::api::WriteDataMmap;
using fs_testing::user_tools::api::Checkpoint;
using std::string;

#define TEST_FILE_PERMS  ((mode_t) (S_IRWXU | S_IRWXG | S_IRWXO))

namespace fs_testing {
namespace tests {

class Generic090: public BaseTestCase{
  public:
    virtual int setup() override {
    
      //create test file 
      const int fd_foo = open(foo_path.c_str(), O_RDWR | O_CREAT, TEST_FILE_PERMS);
      if (fd_foo < 0){
        return -1;
      }
    
      //write 32KB of data to the file 
      if(WriteData(fd_foo, 0, 32768) < 0){
        close(fd_foo);
        return -2;
      }

      //fsync the file 
      if(fsync(fd_foo) < 0){
        close(fd_foo);
        return -3;
      }
      //create a link bar to foo
      if(link(foo_path.c_str(), bar_path.c_str()) < 0){
        return -4;
      }
   
      //sync changes
      sync();

      //close the file foo
      close(fd_foo);
      return 0;
    }

    virtual int run() override{
    
      //open foo again
      const int fd_foo = open(foo_path.c_str(), O_RDWR | O_CREAT, TEST_FILE_PERMS);
      if (fd_foo < 0){
        return -1;
      }
 
      //write another 32kB to the file
      if(WriteData(fd_foo, 32768, 32768) < 0){
        close(fd_foo);
        return -2;
      }
      
      //fsync the file
      if(fsync(fd_foo) < 0){
        return -3;
      }

      //copy the file's content to check later
      unsigned int bytes_read = 0;
      do{
        int res = read(fd_foo, (void *)((unsigned long)text + bytes_read),
          TEST_TEXT_SIZE - bytes_read);
        if(read < 0){
          close(fd_foo);
          return -4;
        }
        bytes_read += res;
      }while(bytes_read < TEST_TEXT_SIZE);    
      
      if(Checkpoint()<0){
        close(fd_foo);
        return -5;
      }
      
      close(fd_foo);
      return 0;
    }

  virtual int check_test(unsigned int last_checkpoint, 
    DataTestResult *test_result) override {
    
    //if we never made it the enlargement of the file and fsync 
    //or if we didn't get a chance to copy the file 
    if(last_checkpoint < 1){
      return -1; 
    }
   
    FILE *f = fopen(foo_path.c_str(), "rw");
    int res = 0;
    unsigned int bytes_read = 0;
    char* buf = (char*) calloc(TEST_TEXT_SIZE, sizeof(char));
    if (buf == NULL) {
      test_result->SetError(DataTestResult::kOther);
    }


    bytes_read = fread(buf, 1, TEST_TEXT_SIZE, f);

    if (bytes_read != TEST_TEXT_SIZE) {
      //error reading the file
      std::cout << "Error reading file" << std::endl;
      test_result->SetError(DataTestResult::kOther);
    }

    else{
      //if the file is not TEST_TEXT_SIZE length
      if(strlen(buf) != TEST_TEXT_SIZE){
        test_result->SetError(DataTestResult::kFileDataCorrupted);
        test_result->error_description = "addition to file not persisted after fsync";
      }
      
      //if it's the right length, check if the contents are the same
      else if(memcmp(text, buf, TEST_TEXT_SIZE) != 0){
        test_result->SetError(DataTestResult::kFileDataCorrupted);
        test_result->error_description = "addition to file not persisted after fsync";
      }
    }

    fclose(f);
    free(buf);

    return 0; 
        
   } 
   
   private:
     char text[TEST_TEXT_SIZE]; 
     const string foo_path = TEST_MNT "/" TEST_FILE_FOO;
     const string bar_path = TEST_MNT "/" TEST_FILE_BAR;

};

} //namespace fs_testing
} //namespace tests

extern "C" fs_testing::tests::BaseTestCase *test_case_get_instance(){
  return new fs_testing::tests::Generic090;
}

extern "C" void test_case_delete_instance(fs_testing::tests::BaseTestCase *tc){
  delete tc;
}
