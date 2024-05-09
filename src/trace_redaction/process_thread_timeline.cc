/*
 * Copyright (C) 2024 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "src/trace_redaction/process_thread_timeline.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include "perfetto/base/logging.h"

namespace perfetto::trace_redaction {
namespace {
// Limit the number of iterations to avoid an infinite loop. 10 is a generous
// number of iterations.
constexpr size_t kMaxSearchDepth = 10;

bool OrderByPid(const ProcessThreadTimeline::Event& left,
                const ProcessThreadTimeline::Event& right) {
  return left.pid() < right.pid();
}

}  // namespace

void ProcessThreadTimeline::Append(const Event& event) {
  events_.push_back(event);
  mode_ = Mode::kWrite;
}

void ProcessThreadTimeline::Sort() {
  std::sort(events_.begin(), events_.end(), OrderByPid);
  mode_ = Mode::kRead;
}

ProcessThreadTimeline::Slice ProcessThreadTimeline::Search(uint64_t ts,
                                                           int32_t pid) const {
  PERFETTO_CHECK(mode_ == Mode::kRead);

  Slice s;
  s.pid = pid;
  s.uid = 0;

  auto e = Search(0, ts, pid);
  if (e.has_value()) {
    s.uid = e->uid();
  }

  return s;
}

std::optional<ProcessThreadTimeline::Event>
ProcessThreadTimeline::Search(size_t depth, uint64_t ts, int32_t pid) const {
  PERFETTO_DCHECK(mode_ == Mode::kRead);

  if (depth >= kMaxSearchDepth) {
    return std::nullopt;
  }

  auto event = FindPreviousEvent(ts, pid);

  if (!TestEvent(event)) {
    return event;
  }

  if (event->uid() != 0) {
    return event;
  }

  return Search(depth + 1, ts, event->ppid());
}

std::optional<ProcessThreadTimeline::Event>
ProcessThreadTimeline::FindPreviousEvent(uint64_t ts, int32_t pid) const {
  PERFETTO_DCHECK(mode_ == Mode::kRead);

  Event fake = Event::Close(ts, pid);

  // Events are in ts-order within each pid-group. See Optimize(), Because each
  // group is small (the vast majority will have two events [start + event, no
  // reuse]).
  //
  // Find the first process event. Then perform a linear search. There won't be
  // many events per process.
  auto at = std::lower_bound(events_.begin(), events_.end(), fake, OrderByPid);

  // `pid` was not found in `events_`.
  if (at == events_.end()) {
    return std::nullopt;
  }

  // "no best option".
  std::optional<Event> best;

  // Run through all events (related to this pid) and find the last event that
  // comes before ts. If the events were in order by time, the search could be
  // more efficient, but the gains are margin because:
  //
  // 1. The number of edge cases go up.
  //
  // 2. The code is harder to read.
  //
  // 3. The performance gains are minimal or non-existant because of the small
  //    number of events.
  for (; at != events_.end() && at->pid() == pid; ++at) {
    if (at->ts() > ts) {
      continue;  // Ignore events in the future.
    }

    // All ts values are positive. However, ts_at and ts_best are both less than
    // ts (see early condition), meaning they can be considered negative values.
    //
    //      at        best            ts
    //   <---+-----------+-------------+---->
    //      31          64            93
    //
    //      at        best            ts
    //   <---+-----------+-------------+---->
    //     -62         -29             0
    //
    // This means that the latest ts value under ts is the closest to ts.
    if (!best.has_value() || at->ts() > best->ts()) {
      best = *at;
    }
  }

  if (best.has_value() &&
      best->type() != ProcessThreadTimeline::Event::Type::kOpen) {
    return std::nullopt;
  }

  return best;
}

bool ProcessThreadTimeline::TestEvent(std::optional<Event> event) const {
  if (!event.has_value()) {
    return false;
  }

  // The thread/process was freed. It won't exist until a new open event.
  if (event->type() != Event::Type::kOpen) {
    return false;
  }

  // It is a rare case in production, but a common case in tests, the top-level
  // event will have no parent but will have the uid. So, to avoid make the
  // tests fragile and without taking on any risk, the uid should be checked
  // before the ppid.
  if (event->uid() != 0) {
    return true;
  }

  return event->ppid() != 0;
}

}  // namespace perfetto::trace_redaction
