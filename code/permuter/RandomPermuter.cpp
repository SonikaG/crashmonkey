#include <iterator>
#include <list>
#include <numeric>
#include <vector>

#include <cassert>

#include "Permuter.h"
#include "RandomPermuter.h"

namespace fs_testing {
namespace permuter {
using std::advance;
using std::iota;
using std::list;
using std::mt19937;
using std::uniform_int_distribution;
using std::vector;

using fs_testing::utils::disk_write;

RandomPermuter::RandomPermuter() {
  rand = mt19937(42);
}

RandomPermuter::RandomPermuter(vector<disk_write> *data) {
  // TODO(ashmrtn): Make a flag to make it random or not.
  rand = mt19937(42);
}

void RandomPermuter::init_data(vector<epoch> *data) {
}

bool RandomPermuter::gen_one_state(vector<epoch_op>& res) {
  unsigned int total_elements = 0;
  // Find how many elements we will be returning (randomly determined).
  uniform_int_distribution<unsigned int> permute_epochs(1, GetEpochs()->size());
  unsigned int num_epochs = permute_epochs(rand);
  // Don't subtract 1 from this size so that we can send a complete epoch if we
  // want.
  uniform_int_distribution<unsigned int> permute_requests(1,
      GetEpochs()->at(num_epochs - 1).ops.size());
  unsigned int num_requests = permute_requests(rand);
  for (unsigned int i = 0; i < num_epochs - 1; ++i) {
    total_elements += GetEpochs()->at(i).ops.size();
  }
  total_elements += num_requests;
  res.resize(total_elements);

  auto curr_iter = res.begin();
  for (unsigned int i = 0; i < num_epochs; ++i) {
    if (GetEpochs()->at(i).overlaps || i == num_epochs - 1) {
      unsigned int size =
        (i != num_epochs - 1) ? GetEpochs()->at(i).ops.size() : num_requests;
      auto res_end = curr_iter + size;
      permute_epoch(curr_iter, res_end, GetEpochs()->at(i));

      curr_iter = res_end;
    } else {
      // Use a for loop since vector::insert inserts new elements and we
      // resized above to the exact size we will have.
      // We will only ever be placing the full epoch here because the above if
      // will catch the case where we place only part of an epoch.
      for (auto epoch_iter = GetEpochs()->at(i).ops.begin();
          epoch_iter != GetEpochs()->at(i).ops.end(); ++epoch_iter) {
        *curr_iter = *epoch_iter;
        ++curr_iter;
      }
    }
  }
  return true;
}

void RandomPermuter::permute_epoch(
      vector<epoch_op>::iterator& res_start,
      vector<epoch_op>::iterator& res_end,
      epoch& epoch) {
  assert(distance(res_start, res_end) <= epoch.ops.size());

  // Even if the number of bios we're placing is less than the number in the
  // epoch, allow any bio but the barrier (if present) to be picked.
  unsigned int slots = epoch.ops.size();
  if (epoch.has_barrier) {
    --slots;
  }

  // Fill the list with the empty slots, either [0, epoch.size() - 1] or
  // [0, epoch.size() - 2]. Prefer a list so that removals are fast. We have
  // this so that each time we pick a number we can find a bio which we haven't
  // already placed.
  list<unsigned int> empty_slots(slots);
  iota(empty_slots.begin(), empty_slots.end(), 0);

  // First case is when we are placing a subset of the bios, the second is when
  // we are placing all the bios but a barrier operation is present.
  while (res_start != res_end && !empty_slots.empty()) {
    // Uniform distribution includes both ends, so we need to subtract 1 from
    // the size.
    uniform_int_distribution<unsigned int> uid(0, empty_slots.size() - 1);
    auto shift = empty_slots.begin();
    advance(shift, uid(rand));
    *res_start = epoch.ops.at(*shift);
    ++res_start;
    empty_slots.erase(shift);
  }

  // We are only placing part of an epoch so we need to return here.
  if (res_start == res_end) {
    return;
  }

  assert(epoch.has_barrier);

  // Place the barrier operation if it exists since the entire vector already
  // exists (i.e. we won't cause extra shifting when adding the other elements).
  // Decrement out count of empty slots since we have filled one.
  *res_start = epoch.ops.back();
}

}  // namespace permuter
}  // namespace fs_testing

extern "C" fs_testing::permuter::Permuter* permuter_get_instance(
    std::vector<fs_testing::utils::disk_write> *data) {
  return new fs_testing::permuter::RandomPermuter(data);
}

extern "C" void permuter_delete_instance(fs_testing::permuter::Permuter* p) {
  delete p;
}
