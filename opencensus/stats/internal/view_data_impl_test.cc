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

#include <limits>

#include "absl/time/time.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "opencensus/stats/aggregation.h"
#include "opencensus/stats/aggregation_window.h"
#include "opencensus/stats/bucket_boundaries.h"
#include "opencensus/stats/distribution.h"
#include "opencensus/stats/view_descriptor.h"

namespace opencensus {
namespace stats {
namespace {

TEST(ViewDataImplTest, Sum) {
  const absl::Time start_time = absl::UnixEpoch();
  const absl::Time end_time = absl::UnixEpoch() + absl::Seconds(1);
  const auto descriptor =
      ViewDescriptor()
          .set_aggregation(Aggregation::Sum())
          .set_aggregation_window(AggregationWindow::Cumulative());
  ViewDataImpl data(start_time, descriptor);
  const std::vector<std::string> tags1({"value1", "value2a"});
  const std::vector<std::string> tags2({"value1", "value2b"});

  data.Add(1, tags1, start_time);
  data.Add(2, tags1, start_time);
  data.Add(5, tags2, end_time);

  EXPECT_EQ(Aggregation::Sum(), data.aggregation());
  EXPECT_EQ(AggregationWindow::Cumulative(), data.aggregation_window());
  EXPECT_EQ(start_time, data.start_time());
  EXPECT_EQ(end_time, data.end_time());
  EXPECT_THAT(data.double_data(),
              ::testing::UnorderedElementsAre(::testing::Pair(tags1, 3),
                                              ::testing::Pair(tags2, 5)));
}

TEST(ViewDataImplTest, Count) {
  const absl::Time start_time = absl::UnixEpoch();
  const absl::Time end_time = absl::UnixEpoch() + absl::Seconds(1);
  const auto descriptor =
      ViewDescriptor()
          .set_aggregation(Aggregation::Count())
          .set_aggregation_window(AggregationWindow::Cumulative());
  ViewDataImpl data(start_time, descriptor);
  const std::vector<std::string> tags1({"value1", "value2a"});
  const std::vector<std::string> tags2({"value1", "value2b"});

  data.Add(1, tags1, start_time);
  data.Add(2, tags1, start_time);
  data.Add(5, tags2, end_time);

  EXPECT_EQ(Aggregation::Count(), data.aggregation());
  EXPECT_EQ(AggregationWindow::Cumulative(), data.aggregation_window());
  EXPECT_EQ(start_time, data.start_time());
  EXPECT_EQ(end_time, data.end_time());
  EXPECT_THAT(data.int_data(),
              ::testing::UnorderedElementsAre(::testing::Pair(tags1, 2),
                                              ::testing::Pair(tags2, 1)));
}

TEST(ViewDataImplTest, Distribution) {
  const absl::Time start_time = absl::UnixEpoch();
  const absl::Time end_time = absl::UnixEpoch() + absl::Seconds(1);
  const BucketBoundaries buckets = BucketBoundaries::Explicit({10});
  const auto descriptor =
      ViewDescriptor()
          .set_aggregation(Aggregation::Distribution(buckets))
          .set_aggregation_window(AggregationWindow::Cumulative());
  ViewDataImpl data(start_time, descriptor);
  const std::vector<std::string> tags1({"value1", "value2a"});
  const std::vector<std::string> tags2({"value1", "value2b"});

  data.Add(1, tags1, start_time);
  data.Add(5, tags1, end_time);
  data.Add(15, tags2, end_time);

  EXPECT_EQ(Aggregation::Distribution(buckets), data.aggregation());
  EXPECT_EQ(AggregationWindow::Cumulative(), data.aggregation_window());
  EXPECT_EQ(start_time, data.start_time());
  EXPECT_EQ(end_time, data.end_time());
  EXPECT_EQ(data.distribution_data().size(), 2);
  EXPECT_THAT(data.distribution_data().find(tags1)->second.bucket_counts(),
              ::testing::ElementsAre(2, 0));
  EXPECT_THAT(data.distribution_data().find(tags2)->second.bucket_counts(),
              ::testing::ElementsAre(0, 1));
}

TEST(ViewDataImplTest, StatsObjectToCount) {
  const absl::Duration interval = absl::Minutes(1);
  const absl::Time start_time = absl::UnixEpoch();
  absl::Time time = start_time;
  const auto descriptor =
      ViewDescriptor()
          .set_aggregation(Aggregation::Count())
          .set_aggregation_window(AggregationWindow::Interval(interval));
  ViewDataImpl data(start_time, descriptor);
  const std::vector<std::string> tags1({"value1", "value2a"});
  const std::vector<std::string> tags2({"value1", "value2b"});

  data.Add(1, tags1, time);
  data.Add(2, tags1, time);
  data.Add(2, tags2, time);
  time += interval / 2;
  data.Add(1, tags1, time);

  const ViewDataImpl export_data1(data, time);
  EXPECT_EQ(Aggregation::Count(), export_data1.aggregation());
  EXPECT_EQ(AggregationWindow::Interval(interval),
            export_data1.aggregation_window());
  EXPECT_EQ(start_time, export_data1.start_time());
  EXPECT_EQ(time, export_data1.end_time());
  EXPECT_THAT(export_data1.double_data(),
              ::testing::UnorderedElementsAre(::testing::Pair(tags1, 3),
                                              ::testing::Pair(tags2, 1)));

  time += interval;
  const ViewDataImpl export_data2(data, time);
  EXPECT_EQ(time - interval, export_data2.start_time());
  EXPECT_EQ(time, export_data2.end_time());
  EXPECT_THAT(export_data2.double_data(),
              ::testing::UnorderedElementsAre(::testing::Pair(tags1, 1),
                                              ::testing::Pair(tags2, 0)));
}

TEST(ViewDataImplTest, StatsObjectToSum) {
  const absl::Duration interval = absl::Minutes(1);
  const absl::Time start_time = absl::UnixEpoch();
  absl::Time time = start_time;
  const auto descriptor =
      ViewDescriptor()
          .set_aggregation(Aggregation::Sum())
          .set_aggregation_window(AggregationWindow::Interval(interval));
  ViewDataImpl data(start_time, descriptor);
  const std::vector<std::string> tags1({"value1", "value2a"});
  const std::vector<std::string> tags2({"value1", "value2b"});

  data.Add(1, tags1, time);
  data.Add(3, tags1, time);
  data.Add(2, tags2, time);
  time += interval / 2;
  data.Add(2, tags1, time);

  const ViewDataImpl export_data1(data, time);
  EXPECT_EQ(Aggregation::Sum(), export_data1.aggregation());
  EXPECT_EQ(AggregationWindow::Interval(interval),
            export_data1.aggregation_window());
  EXPECT_EQ(start_time, export_data1.start_time());
  EXPECT_EQ(time, export_data1.end_time());
  EXPECT_THAT(export_data1.double_data(),
              ::testing::UnorderedElementsAre(::testing::Pair(tags1, 6),
                                              ::testing::Pair(tags2, 2)));

  time += interval;
  const ViewDataImpl export_data2(data, time);
  EXPECT_EQ(time - interval, export_data2.start_time());
  EXPECT_EQ(time, export_data2.end_time());
  EXPECT_THAT(export_data2.double_data(),
              ::testing::UnorderedElementsAre(::testing::Pair(tags1, 2),
                                              ::testing::Pair(tags2, 0)));
}

TEST(ViewDataImplTest, StatsObjectToDistribution) {
  const absl::Duration interval = absl::Minutes(1);
  const absl::Time start_time = absl::UnixEpoch();
  absl::Time time = start_time;
  const BucketBoundaries buckets = BucketBoundaries::Explicit({10});
  const auto descriptor =
      ViewDescriptor()
          .set_aggregation(Aggregation::Distribution(buckets))
          .set_aggregation_window(AggregationWindow::Interval(interval));
  ViewDataImpl data(start_time, descriptor);
  const std::vector<std::string> tags1({"value1", "value2a"});
  const std::vector<std::string> tags2({"value1", "value2b"});

  data.Add(5, tags1, time);
  data.Add(15, tags1, time);
  data.Add(0, tags2, time);
  time += interval / 2;
  data.Add(10, tags1, time);

  const ViewDataImpl export_data1(data, time);
  EXPECT_EQ(Aggregation::Distribution(buckets), export_data1.aggregation());
  EXPECT_EQ(AggregationWindow::Interval(interval),
            export_data1.aggregation_window());
  EXPECT_EQ(start_time, export_data1.start_time());
  EXPECT_EQ(time, export_data1.end_time());
  EXPECT_EQ(2, export_data1.distribution_data().size());
  const Distribution& distribution_1_1 =
      export_data1.distribution_data().find(tags1)->second;
  EXPECT_EQ(3, distribution_1_1.count());
  EXPECT_EQ(10, distribution_1_1.mean());
  EXPECT_EQ(50, distribution_1_1.sum_of_squared_deviation());
  EXPECT_EQ(5, distribution_1_1.min());
  EXPECT_EQ(15, distribution_1_1.max());
  EXPECT_THAT(distribution_1_1.bucket_counts(), ::testing::ElementsAre(1, 2));
  const Distribution& distribution_2_1 =
      export_data1.distribution_data().find(tags2)->second;
  EXPECT_EQ(1, distribution_2_1.count());
  EXPECT_EQ(0, distribution_2_1.mean());
  EXPECT_EQ(0, distribution_2_1.sum_of_squared_deviation());
  EXPECT_EQ(0, distribution_2_1.min());
  EXPECT_EQ(0, distribution_2_1.max());
  EXPECT_THAT(distribution_2_1.bucket_counts(), ::testing::ElementsAre(1, 0));

  time += interval;
  const ViewDataImpl export_data2(data, time);
  EXPECT_EQ(time - interval, export_data2.start_time());
  EXPECT_EQ(time, export_data2.end_time());
  EXPECT_EQ(2, export_data2.distribution_data().size());
  const Distribution& distribution_1_2 =
      export_data2.distribution_data().find(tags1)->second;
  EXPECT_EQ(1, distribution_1_2.count());
  EXPECT_EQ(10, distribution_1_2.mean());
  EXPECT_EQ(0, distribution_1_2.sum_of_squared_deviation());
  EXPECT_EQ(10, distribution_1_2.min());
  EXPECT_EQ(10, distribution_1_2.max());
  EXPECT_THAT(distribution_1_2.bucket_counts(), ::testing::ElementsAre(0, 1));
  const Distribution& distribution_2_2 =
      export_data2.distribution_data().find(tags2)->second;
  EXPECT_EQ(0, distribution_2_2.count());
  EXPECT_EQ(0, distribution_2_2.mean());
  EXPECT_EQ(0, distribution_2_2.sum_of_squared_deviation());
  EXPECT_EQ(std::numeric_limits<double>::infinity(), distribution_2_2.min());
  EXPECT_EQ(-std::numeric_limits<double>::infinity(), distribution_2_2.max());
  EXPECT_THAT(distribution_2_2.bucket_counts(), ::testing::ElementsAre(0, 0));
}

}  // namespace
}  // namespace stats
}  // namespace opencensus
