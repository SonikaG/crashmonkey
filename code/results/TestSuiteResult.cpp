#include "DataTestResult.h"
#include "FileSystemTestResult.h"
#include "TestSuiteResult.h"

namespace fs_testing {

using std::endl;;
using std::ostream;
using std::vector;

using fs_testing::tests::DataTestResult;
using fs_testing::FileSystemTestResult;
using fs_testing::SingleTestInfo;

void TestSuiteResult::AddCompletedTest(const SingleTestInfo& done) {
  completed_.push_back(done);
}

unsigned int TestSuiteResult::GetCompleted() const {
  return completed_.size();
}

void TestSuiteResult::PrintResults(ostream& os) const {
  unsigned int num_failed = 0;
  unsigned int num_passed_fixed = 0;
  unsigned int num_passed = 0;

  unsigned int old_file_persisted = 0;
  unsigned int file_missing = 0;
  unsigned int file_data_corrupted = 0;
  unsigned int file_metadata_corrupted = 0;
  unsigned int other = 0;

  for (const auto& result : completed_) {
    if (result.fs_test.GetError() == FileSystemTestResult::kClean
        && result.data_test.GetError() == DataTestResult::kClean) {
      ++num_passed;
    } else if (result.fs_test.GetError() == FileSystemTestResult::kFixed
        && result.data_test.GetError() == DataTestResult::kClean) {
      ++num_passed_fixed;
    } else {
      ++num_failed;
      switch (result.data_test.GetError()) {
        case DataTestResult::kOldFilePersisted:
          ++old_file_persisted;
          break;
        case DataTestResult::kFileMissing:
          ++file_missing;
          break;
        case DataTestResult::kFileDataCorrupted:
          ++file_data_corrupted;
          break;
        case DataTestResult::kFileMetadataCorrupted:
          ++file_metadata_corrupted;
          break;
        case DataTestResult::kOther:
          ++other;
          break;
      }
    }
  }

  os << "Ran " << num_failed + num_passed_fixed + num_passed << " tests with"
    << "\n\tpassed cleanly: " << num_passed
    << "\n\tpassed fixed: " << num_passed_fixed
    << "\n\tfailed: " << num_failed
    << "\n\t\told file persisted: " << old_file_persisted
    << "\n\t\tfile missing: " << file_missing
    << "\n\t\tfile data corrupted: " << file_data_corrupted
    << "\n\t\tfile metadata corrupted: " << file_metadata_corrupted
    << "\n\t\tother: " << other << endl;
}

}  // namespace fs_testing
