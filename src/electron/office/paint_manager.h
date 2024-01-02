// Copyright (c) 2023 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#pragma once

#include <vector>
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "office/cancellation_flag.h"
#include "office/document_holder.h"
#include "office/lok_tilebuffer.h"

namespace electron::office {

class PaintManager {
 public:
  class Client {
   public:
    virtual void InvalidatePluginContainer() = 0;
    virtual base::WeakPtr<Client> GetWeakClient() = 0;
    virtual scoped_refptr<office::TileBuffer> GetTileBuffer() = 0;
  };

  explicit PaintManager(Client* client);
  PaintManager(Client* client, std::unique_ptr<PaintManager> other);
  ~PaintManager();

  PaintManager(const PaintManager& other) = delete;
  PaintManager& operator=(const PaintManager& other) = delete;

  void SchedulePaint(DocumentHolderWithView document,
                     int y_pos,
                     int view_height,
                     float scale,
                     bool full_paint,
                     std::vector<TileRange> tile_ranges_);

  // this should be called after the container is invalidated and the canvas is
  // painted by the TileBuffer
  bool ScheduleNextPaint(std::vector<TileRange> tile_ranges_ = {});

  // should be used to prevent lingering tasks during zooms
  void ClearTasks();

  // should be used to block invalidations while a client is being destroyed
  void OnDestroy();

  void PausePaint();
  void ResumePaint(bool paint_next = true);

 private:
  PaintManager();

  class Task {
   public:
    Task(DocumentHolderWithView document,
         int y_pos,
         int view_height,
         float scale,
         bool full_paint,
         std::vector<TileRange> tile_ranges);

    ~Task();

    // disable copy
    Task(const Task& other) = delete;
    Task& operator=(const Task& other) = delete;

    std::size_t ContextHash() const noexcept;

    DocumentHolderWithView document_;
    const int y_pos_;
    const int view_height_;
    const float scale_;
    const bool full_paint_;
    const std::vector<TileRange> tile_ranges_;
    const CancelFlagPtr skip_paint_flag_;
    const CancelFlagPtr skip_invalidation_flag_;

    bool CanMergeWith(Task& other);

    // other takes precendence for coordinates and tile ranges, basically
    // assumes other is the replacement assumes other can merge with this task
    std::unique_ptr<Task> MergeWith(Task& other,
                                    office::TileBuffer& tile_buffer);
    std::unique_ptr<Task> MergeWith(std::vector<TileRange> tile_ranges,
                                    office::TileBuffer& tile_buffer);
  };

  void PostCurrentTask();
  static bool PaintTile(scoped_refptr<office::TileBuffer> tile_buffer,
                        CancelFlagPtr cancel_flag,
                        DocumentHolderWithView document,
                        unsigned int tile_index,
                        std::size_t context_hash,
                        const base::RepeatingClosure& completed);
  static void PaintTileRange(scoped_refptr<office::TileBuffer> tile_buffer,
                             CancelFlagPtr cancel_flag,
                             DocumentHolderWithView document,
                             TileRange range,
                             std::size_t context_hash,
                             const base::RepeatingClosure& completed);

  const scoped_refptr<base::TaskRunner> task_runner_;
  Client* client_;
  bool skip_render_ = false;

  std::unique_ptr<Task> current_task_ = nullptr;
  std::unique_ptr<Task> next_task_ = nullptr;
  base::TimeTicks last_paint_time_ = {};
  CancelFlagPtr cancel_invalidate_;
};

}  // namespace electron::office
