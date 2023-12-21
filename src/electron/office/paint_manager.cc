// Copyright (c) 2023 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "paint_manager.h"

#include <memory>

#include "base/barrier_closure.h"
#include "base/task/bind_post_task.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "office/cancellation_flag.h"

// Uncomment to log paint manager events
// #define DEBUG_PAINT_MANAGER

namespace electron::office {

PaintManager::Task::Task(DocumentHolderWithView document,
                         int y_pos,
                         int view_height,
                         float scale,
                         bool full_paint,
                         std::vector<TileRange> tile_ranges)
    : document_(document),
      y_pos_(y_pos),
      view_height_(view_height),
      scale_(scale),
      full_paint_(full_paint),
      tile_ranges_(std::move(tile_ranges)),
      skip_paint_flag_(CancelFlag::Create()),
      skip_invalidation_flag_(CancelFlag::Create()) {}

PaintManager::Task::~Task() = default;

std::size_t PaintManager::Task::ContextHash() const noexcept {
  return std::hash<void*>{}(document_.holder().get()) ^
         (std::hash<float>{}(scale_) << 1);
}

PaintManager::PaintManager(Client* client)
    : task_runner_(base::ThreadPool::CreateTaskRunner(
          {base::TaskPriority::USER_VISIBLE})),
      client_(client),
      current_task_(nullptr),
      next_task_(nullptr),
      cancel_invalidate_(CancelFlag::Create()) {}

PaintManager::PaintManager(Client* client, std::unique_ptr<PaintManager> other)
    : task_runner_(base::ThreadPool::CreateTaskRunner(
          {base::TaskPriority::USER_VISIBLE})),
      client_(client),
      current_task_(std::move(other->current_task_)),
      next_task_(std::move(other->next_task_)),
      cancel_invalidate_(CancelFlag::Create()) {}

PaintManager::PaintManager() = default;
PaintManager::~PaintManager() {
  ClearTasks();
}

void PaintManager::SchedulePaint(DocumentHolderWithView document,
                                 int y_pos,
                                 int view_height,
                                 float scale,
                                 bool full_paint,
                                 std::vector<TileRange> tile_ranges_) {
  // nothing scheduled, start immediately
  if (!current_task_) {
    current_task_ = std::make_unique<Task>(document, y_pos, view_height, scale,
                                           full_paint, tile_ranges_);
    PostCurrentTask();
    return;
  }

  if (next_task_ && next_task_->document_ == document) {
    tile_ranges_.insert(tile_ranges_.end(), next_task_->tile_ranges_.begin(),
                        next_task_->tile_ranges_.end());
    full_paint = full_paint || next_task_->full_paint_;

    if (current_task_->document_ == document) {
      tile_ranges_.insert(tile_ranges_.end(),
                          current_task_->tile_ranges_.begin(),
                          current_task_->tile_ranges_.end());
      full_paint = full_paint || current_task_->full_paint_;
    }
  }

  next_task_ = std::make_unique<Task>(document, y_pos, view_height, scale,
                                      full_paint, SimplifyRanges(tile_ranges_));
  ScheduleNextPaint();
}

bool PaintManager::Task::CanMergeWith(Task& other) {
  // clang-format off
  return document_ == other.document_ && // same document
  std::abs(other.scale_ - scale_) < 0.001 && // same scale
  (other.y_pos_ >= y_pos_ && other.y_pos_ < y_pos_ + view_height_) && // overlapping
  (y_pos_ >= other.y_pos_ && y_pos_ < other.y_pos_ + other.view_height_); // overlapping
  // clang-format on
}

std::unique_ptr<PaintManager::Task> PaintManager::Task::MergeWith(
    Task& other,
    office::TileBuffer& tile_buffer) {
  auto clipped_ranges = tile_buffer.ClipRanges(
      tile_ranges_, tile_buffer.LimitIndex(other.y_pos_, other.view_height_));
  clipped_ranges.insert(clipped_ranges.end(), other.tile_ranges_.begin(),
                        other.tile_ranges_.end());

  return std::make_unique<Task>(
      other.document_, other.y_pos_, other.view_height_, other.scale_,
      full_paint_ || other.full_paint_, SimplifyRanges(clipped_ranges));
}

std::unique_ptr<PaintManager::Task> PaintManager::Task::MergeWith(
    std::vector<TileRange> tile_ranges,
    office::TileBuffer& tile_buffer) {
  auto limit = tile_buffer.LimitIndex(y_pos_, view_height_);

  std::vector<TileRange> tile_ranges_joined(tile_ranges_);
  tile_ranges_joined.insert(tile_ranges_joined.end(), tile_ranges.begin(),
                            tile_ranges.end());

  auto clipped_ranges = tile_buffer.ClipRanges(tile_ranges_joined, limit);

  return std::make_unique<Task>(document_, y_pos_, view_height_, scale_,
                                full_paint_ || full_paint_,
                                SimplifyRanges(clipped_ranges));
}

// this duplicates a lot of the above and is generally a hacky mess to get
// things to paint consistently
bool PaintManager::ScheduleNextPaint(std::vector<TileRange> tile_ranges_) {
  // merge tile_ranges_ with next
  if (!tile_ranges_.empty() && (current_task_ || next_task_) &&
      client_->GetTileBuffer()) {
    next_task_ =
        next_task_
            ? next_task_->MergeWith(tile_ranges_, *client_->GetTileBuffer())
            : current_task_->MergeWith(tile_ranges_, *client_->GetTileBuffer());
  }

  // merge tile_ranges_ with current remaining
  if (client_->GetTileBuffer() && next_task_ && current_task_ &&
      current_task_->CanMergeWith(*next_task_)) {
    auto remaining = client_->GetTileBuffer()->InvalidRangesRemaining(
        current_task_->tile_ranges_);

    if (!remaining.empty()) {
      next_task_ = next_task_->MergeWith(remaining, *client_->GetTileBuffer());
    }
  }

  if (next_task_) {
    // guarantees that the range is clipped, regardless of merge cases above
    next_task_ = next_task_->MergeWith({}, *client_->GetTileBuffer());
  }

  if (next_task_ && current_task_) {
    bool is_same_task = current_task_->document_ == next_task_->document_;
    auto n_it = next_task_->tile_ranges_.cbegin();
    for (auto& it : current_task_->tile_ranges_) {
      is_same_task = is_same_task && n_it != next_task_->tile_ranges_.end() &&
                     it == *n_it++;
    }

    if (!is_same_task) {
      // LOG(ERROR) << "NOT SAME TASK";
      // y-pos are different, so assume scrolling and not an in-place update
      if (current_task_->y_pos_ != next_task_->y_pos_) {
        CancelFlag::Set(current_task_->skip_paint_flag_);
        CancelFlag::Set(current_task_->skip_invalidation_flag_);
      }
      current_task_.reset();
    } else {
      // LOG(ERROR) << "DUPLICATE TASK";
      current_task_.reset();
    }
  } else if (current_task_ && !next_task_) {
    // LOG(ERROR) << "NO NEXT TASK";
    current_task_.reset();
  }
  current_task_.swap(next_task_);

  if (current_task_) {
    PostCurrentTask();
		return true;
	} else {
		return false;
	}
}

void PaintManager::PostCurrentTask() {
  if (skip_render_ || !current_task_ || CancelFlag::IsCancelled(cancel_invalidate_))
    return;

  std::size_t hash = 0;
  if (auto tile_buffer = client_->GetTileBuffer()) {
    hash = current_task_->ContextHash();
#ifdef DEBUG_PAINT_MANAGER
    LOG(ERROR) << "PCT " << current_task_->scale_ << " @ " << std::hex << hash;
#endif

    if (tile_buffer->IsEmpty()) {
      return;
    }

    tile_buffer->SetActiveContext(hash);
  }
  auto simplified_ranges = SimplifyRanges(current_task_->tile_ranges_);
  auto tile_count = TileCount(simplified_ranges);
  base::RepeatingClosure completed = base::BarrierClosure(
      tile_count,
      base::BindPostTask(task_runner_,
      base::BindOnce([](CancelFlagPtr task_cancel_flag, CancelFlagPtr manager_cancel_flag, Client* client) {
        if (!CancelFlag::IsCancelled(manager_cancel_flag) && !CancelFlag::IsCancelled(task_cancel_flag)) {
          client->InvalidatePluginContainer();
        }
      }, current_task_->skip_invalidation_flag_, cancel_invalidate_, base::Unretained(client_))));
  for (auto& it : simplified_ranges) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&PaintManager::PaintTileRange, client_->GetTileBuffer(),
                       current_task_->skip_paint_flag_,
                       current_task_->document_, it, hash, completed));
  }
}

void PaintManager::PaintTileRange(scoped_refptr<office::TileBuffer> tile_buffer,
                                  CancelFlagPtr cancel_flag,
                                  DocumentHolderWithView document,
                                  TileRange it,
                                  std::size_t context_hash,
                                  const base::RepeatingClosure& completed) {
#ifdef DEBUG_PAINT_MANAGER
  LOG(ERROR) << "PTR " << it.index_start << " - " << it.index_end
             << " CH: " << std::hex << context_hash;
#endif

  for (unsigned int tile_index = it.index_start; tile_index <= it.index_end;
       ++tile_index) {
    if (!PaintTile(tile_buffer, cancel_flag, document, tile_index, context_hash,
                   completed))
      break;
  }
}

bool PaintManager::PaintTile(scoped_refptr<office::TileBuffer> tile_buffer,
                             CancelFlagPtr cancel_flag,
                             DocumentHolderWithView document,
                             unsigned int tile_index,
                             std::size_t context_hash,
                             const base::RepeatingClosure& completed) {
  bool res =
      tile_buffer->PaintTile(cancel_flag, document, tile_index, context_hash);
  completed.Run();
  return res;
}


void PaintManager::OnDestroy() {
  CancelFlag::CancelAndReset(cancel_invalidate_);
}

void PaintManager::ClearTasks() {
  if (current_task_) {
    CancelFlag::Set(current_task_->skip_paint_flag_);
    CancelFlag::Set(current_task_->skip_invalidation_flag_);
  }
  current_task_.reset();
  if (next_task_) {
    CancelFlag::Set(next_task_->skip_paint_flag_);
    CancelFlag::Set(next_task_->skip_invalidation_flag_);
  }
  next_task_.reset();
}

void PaintManager::PausePaint() {
  skip_render_ = true;
}

void PaintManager::ResumePaint(bool paint_next) {
  skip_render_ = false;
  if (!paint_next)
    return;

  if (current_task_) {
    PostCurrentTask();
  } else if (next_task_) {
    current_task_.swap(next_task_);
    PostCurrentTask();
  }
}

}  // namespace electron::office
