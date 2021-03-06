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

#include "opencensus/stats/internal/view_data_impl.h"

#include <cstdint>
#include <iostream>

#include "opencensus/stats/distribution.h"

namespace opencensus {
namespace stats {

namespace {

ViewDataImpl::Type TypeForDescriptor(const ViewDescriptor& descriptor) {
  switch (descriptor.aggregation_window().type()) {
    case AggregationWindow::Type::kCumulative:
      switch (descriptor.aggregation().type()) {
        case Aggregation::Type::kSum:
          return ViewDataImpl::Type::kDouble;
        case Aggregation::Type::kCount:
          return ViewDataImpl::Type::kInt64;
        case Aggregation::Type::kDistribution:
          return ViewDataImpl::Type::kDistribution;
      }
    case AggregationWindow::Type::kInterval:
      return ViewDataImpl::Type::kStatsObject;
  }
}

}  // namespace

ViewDataImpl::ViewDataImpl(absl::Time start_time,
                           const ViewDescriptor& descriptor)
    : aggregation_(descriptor.aggregation()),
      aggregation_window_(descriptor.aggregation_window()),
      type_(TypeForDescriptor(descriptor)),
      start_time_(start_time) {
  switch (type_) {
    case Type::kDouble: {
      new (&double_data_) DataMap<double>();
      break;
    }
    case Type::kInt64: {
      new (&int_data_) DataMap<int64_t>();
      break;
    }
    case Type::kDistribution: {
      new (&distribution_data_) DataMap<Distribution>();
      break;
    }
    case Type::kStatsObject: {
      new (&interval_data_) DataMap<IntervalStatsObject>();
      break;
    }
  }
}

ViewDataImpl::ViewDataImpl(const ViewDataImpl& other, absl::Time now)
    : aggregation_(other.aggregation()),
      aggregation_window_(other.aggregation_window()),
      type_(other.aggregation().type() == Aggregation::Type::kDistribution
                ? Type::kDistribution
                : Type::kDouble),
      start_time_(std::max(other.start_time(),
                           now - other.aggregation_window().duration())),
      end_time_(now) {
  ABSL_ASSERT(aggregation_window_.type() == AggregationWindow::Type::kInterval);
  switch (aggregation_.type()) {
    case Aggregation::Type::kSum:
    case Aggregation::Type::kCount: {
      new (&double_data_) DataMap<double>();
      for (const auto& row : other.interval_data()) {
        row.second.SumInto(absl::Span<double>(&double_data_[row.first], 1),
                           now);
      }
      break;
    }
    case Aggregation::Type::kDistribution: {
      new (&distribution_data_) DataMap<Distribution>();
      for (const auto& row : other.interval_data()) {
        const std::pair<DataMap<Distribution>::iterator, bool>& it =
            distribution_data_.emplace(
                row.first, Distribution(&aggregation_.bucket_boundaries()));
        Distribution& distribution = it.first->second;
        row.second.DistributionInto(
            &distribution.count_, &distribution.mean_,
            &distribution.sum_of_squared_deviation_, &distribution.min_,
            &distribution.max_,
            absl::Span<uint64_t>(distribution.bucket_counts_), now);
      }
    }
  }
}

ViewDataImpl::~ViewDataImpl() {
  switch (type_) {
    case Type::kDouble: {
      double_data_.~DataMap<double>();
      break;
    }
    case Type::kInt64: {
      int_data_.~DataMap<int64_t>();
      break;
    }
    case Type::kDistribution: {
      distribution_data_.~DataMap<Distribution>();
      break;
    }
    case Type::kStatsObject: {
      interval_data_.~DataMap<IntervalStatsObject>();
      break;
    }
  }
}

ViewDataImpl::ViewDataImpl(const ViewDataImpl& other)
    : aggregation_(other.aggregation_),
      aggregation_window_(other.aggregation_window_),
      type_(other.type()),
      start_time_(other.start_time_),
      end_time_(other.end_time_) {
  switch (type_) {
    case Type::kDouble: {
      new (&double_data_) DataMap<double>(other.double_data_);
      break;
    }
    case Type::kInt64: {
      new (&int_data_) DataMap<int64_t>(other.int_data_);
      break;
    }
    case Type::kDistribution: {
      new (&distribution_data_) DataMap<Distribution>(other.distribution_data_);
      break;
    }
    case Type::kStatsObject: {
      std::cerr
          << "StatsObject ViewDataImpl cannot (and should not) be copied. "
             "(Possibly failed to convert to export data type?)";
      ABSL_ASSERT(0);
      break;
    }
  }
}

void ViewDataImpl::Add(double value, const std::vector<std::string>& tag_values,
                       absl::Time now) {
  end_time_ = std::max(end_time_, now);
  switch (type_) {
    case Type::kDouble: {
      double_data_[tag_values] += value;
      break;
    }
    case Type::kInt64: {
      ++int_data_[tag_values];
      break;
    }
    case Type::kDistribution: {
      DataMap<Distribution>::iterator it = distribution_data_.find(tag_values);
      if (it == distribution_data_.end()) {
        it = distribution_data_.emplace_hint(
            it, tag_values, Distribution(&aggregation_.bucket_boundaries()));
      }
      it->second.Add(value);
      break;
    }
    case Type::kStatsObject: {
      DataMap<IntervalStatsObject>::iterator it =
          interval_data_.find(tag_values);
      if (aggregation_.type() == Aggregation::Type::kDistribution) {
        const auto& buckets = aggregation_.bucket_boundaries();
        if (it == interval_data_.end()) {
          it = interval_data_.emplace_hint(
              it, std::piecewise_construct, std::make_tuple(tag_values),
              std::make_tuple(buckets.num_buckets() + 5,
                              aggregation_window_.duration(), now));
        }
        it->second.AddToDistribution(value, buckets.BucketForValue(value), now);
      } else {
        if (it == interval_data_.end()) {
          it = interval_data_.emplace_hint(
              it, std::piecewise_construct, std::make_tuple(tag_values),
              std::make_tuple(1, aggregation_window_.duration(), now));
        }
        if (aggregation_ == Aggregation::Count()) {
          it->second.MutableCurrentBucket(now)[0] += 1.0;
        } else {
          it->second.MutableCurrentBucket(now)[0] += value;
        }
      }
    }
  }
}

}  // namespace stats
}  // namespace opencensus
