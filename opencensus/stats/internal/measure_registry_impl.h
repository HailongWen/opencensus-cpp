// Copyright 2017, OpenCensus Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef OPENCENSUS_STATS_INTERNAL_MEASURE_REGISTRY_IMPL_H_
#define OPENCENSUS_STATS_INTERNAL_MEASURE_REGISTRY_IMPL_H_

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "opencensus/stats/measure.h"
#include "opencensus/stats/measure_descriptor.h"

namespace opencensus {
namespace stats {

// MeasureRegistryImpl implements MeasureRegistry and holds internal-only
// helpers for Measure.
// MeasureRegistryImpl is thread-safe.
class MeasureRegistryImpl {
 public:
  static MeasureRegistryImpl* Get();

  MeasureDouble RegisterDouble(absl::string_view name, absl::string_view units,
                               absl::string_view description)
      LOCKS_EXCLUDED(mu_);
  MeasureInt RegisterInt(absl::string_view name, absl::string_view units,
                         absl::string_view description) LOCKS_EXCLUDED(mu_);

  const MeasureDescriptor& GetDescriptorByName(absl::string_view name) const
      LOCKS_EXCLUDED(mu_);

  MeasureDouble GetMeasureDoubleByName(absl::string_view name) const
      LOCKS_EXCLUDED(mu_);
  MeasureInt GetMeasureIntByName(absl::string_view name) const
      LOCKS_EXCLUDED(mu_);

  // The following methods are for internal use by the library, and not exposed
  // in the public MeasureRegistry.
  uint64_t GetIdByName(absl::string_view name) const LOCKS_EXCLUDED(mu_);

  template <typename MeasureT>
  const MeasureDescriptor& GetDescriptor(Measure<MeasureT> measure) const
      LOCKS_EXCLUDED(mu_);

  // Measure ids contain a sequential index, a validity bit, and a
  // type bit; these functions access the individual parts.
  static bool IdValid(uint64_t id);
  static uint64_t IdToIndex(uint64_t id);
  static MeasureDescriptor::Type IdToType(uint64_t id);

  template <typename MeasureT>
  static uint64_t MeasureToIndex(Measure<MeasureT> measure);

 private:
  MeasureRegistryImpl() = default;

  uint64_t RegisterImpl(MeasureDescriptor descriptor) LOCKS_EXCLUDED(mu_);

  static uint64_t CreateMeasureId(uint64_t index, bool is_valid,
                                  MeasureDescriptor::Type type);

  mutable absl::Mutex mu_;
  // The registered MeasureDescriptors. Measure id are indexes into this
  // vector plus some flags in the high bits.
  std::vector<MeasureDescriptor> registered_descriptors_ GUARDED_BY(mu_);
  // A map from measure names to IDs.
  std::unordered_map<std::string, uint64_t> id_map_ GUARDED_BY(mu_);
};

template <typename MeasureT>
const MeasureDescriptor& MeasureRegistryImpl::GetDescriptor(
    Measure<MeasureT> measure) const {
  absl::ReaderMutexLock l(&mu_);
  if (!measure.IsValid()) {
    static const MeasureDescriptor default_descriptor =
        MeasureDescriptor("", "", "", MeasureDescriptor::Type::kDouble);
    return default_descriptor;
  }
  return registered_descriptors_[IdToIndex(measure.id_)];
}

// static
template <typename MeasureT>
uint64_t MeasureRegistryImpl::MeasureToIndex(Measure<MeasureT> measure) {
  return IdToIndex(measure.id_);
}

}  // namespace stats
}  // namespace opencensus

#endif  // OPENCENSUS_STATS_INTERNAL_MEASURE_REGISTRY_IMPL_H_
