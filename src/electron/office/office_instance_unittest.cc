// Copyright (c) 2023 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "office_instance.h"
#include "base/barrier_closure.h"
#include "base/callback_forward.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "office/document_holder.h"
#include "office/office_load_observer.h"

namespace electron::office {

constexpr base::TimeDelta kLoadTimeout = base::Seconds(3);
constexpr base::TimeDelta kDestroyTimeout = base::Seconds(1);

class OfficeInstanceTest : public ::testing::Test,
                           public OfficeLoadObserver,
                           public DestroyedObserver {
 public:
  bool WaitLoad() { return loaded_.TimedWait(kLoadTimeout) && office_; }
  bool WaitDestroyed() { return destroyed_.TimedWait(kDestroyTimeout); }

 protected:
  lok::Office* office_;
  base::WaitableEvent loaded_;
  base::WaitableEvent destroyed_;
  base::test::TaskEnvironment task_environment_;

  void SetUp() override {
    OfficeInstance::Create();
    ASSERT_NE(OfficeInstance::Get(), nullptr);
    OfficeInstance::Get()->AddLoadObserver(this);
    OfficeInstance::Get()->AddDestroyedObserver(this);
  }

  void TearDown() override {
    ASSERT_NE(OfficeInstance::Get(), nullptr);
    OfficeInstance::Get()->RemoveLoadObserver(this);
    OfficeInstance::Get()->RemoveDestroyedObserver(this);
  }

  void OnLoaded(lok::Office* office) override {
    ASSERT_NE(office, nullptr);
    office_ = office;
    loaded_.Signal();
  }

  void OnDestroyed() override { destroyed_.Signal(); }
};

class WaitedLoadObserver : public OfficeLoadObserver {
 public:
  void OnLoaded(lok::Office* office) override {
    ASSERT_NE(office, nullptr);
    office_ = office;
    run_loop.Quit();
  }

  void Wait() {
    // expect that LOK does not timeout
    run_loop.Run();
  }

 private:
  base::RunLoop run_loop;
  lok::Office* office_;
};

TEST(OfficeInstanceProcessTest, WithCreate) {
  // use a "death" test to run an out-of-process test
  EXPECT_EXIT(
      {
        base::test::TaskEnvironment task_environment;
        base::test::ScopedRunLoopTimeout loop_timeout(FROM_HERE, kLoadTimeout);
        WaitedLoadObserver waited;

        OfficeInstance::Create();
        ASSERT_NE(OfficeInstance::Get(), nullptr);
        OfficeInstance::Get()->AddLoadObserver(&waited);

        // expect that LOK does not timeout
        waited.Wait();

        EXPECT_TRUE(OfficeInstance::IsValid());
        // IsValid should not be destructive
        EXPECT_TRUE(OfficeInstance::IsValid());

        OfficeInstance::Get()->RemoveLoadObserver(&waited);
        base::Process::TerminateCurrentProcessImmediately(0);
      },
      testing::ExitedWithCode(0), ".*");
}

TEST(OfficeInstanceProcessTest, WithoutCreate) {
  // use a "death" test to run an out-of-process test, since OfficeInstance is
  // tied to the process by LOK's global lock, but has a thread-local pointer
  EXPECT_EXIT(
      {
        base::test::TaskEnvironment task_environment;
        base::RunLoop().RunUntilIdle();

        EXPECT_FALSE(OfficeInstance::IsValid());
        // IsValid should not create an instance
        EXPECT_FALSE(OfficeInstance::IsValid());

        base::Process::TerminateCurrentProcessImmediately(0);
      },
      testing::ExitedWithCode(0), ".*");
}

class MockDocumentEventObserver : public DocumentEventObserver {
 public:
  MOCK_METHOD(void,
              DocumentCallback,
              (int type, std::string payload),
              (override));
};

TEST_F(OfficeInstanceTest, ObservesDocumentCallbacks) {
  WaitLoad();

  static constexpr size_t doc_id = 1;
  static constexpr size_t event_type_id = 2;
  static constexpr size_t view_id = 3;
  static constexpr size_t view_id2 = 4;
  static constexpr char payload[] = "this is a payload";

  DocumentEventId id{doc_id, event_type_id, view_id};
  MockDocumentEventObserver mock_observer;
  DocumentEventId id2{doc_id, event_type_id, view_id2};
  MockDocumentEventObserver mock_observer_two;

  OfficeInstance::Get()->AddDocumentObserver(id, &mock_observer);
  OfficeInstance::Get()->AddDocumentObserver(id2, &mock_observer_two);
  EXPECT_CALL(mock_observer, DocumentCallback(event_type_id, payload));
  EXPECT_CALL(mock_observer_two, DocumentCallback(event_type_id, payload));

  base::WaitableEvent waitable;
  // wait for events for both views
  auto signal_barrier =
      base::BarrierClosure(2, base::BindOnce(&base::WaitableEvent::Signal,
                                             base::Unretained(&waitable)));

  auto document_context = std::make_unique<DocumentCallbackContext>(
      doc_id, view_id, OfficeInstance::Get());
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(
          [](const base::RepeatingClosure& signal,
             DocumentCallbackContext* context) {
            OfficeInstance::HandleDocumentCallback(event_type_id, payload,
                                                   context);
            signal.Run();
          },
          signal_barrier, document_context.get()));

  auto document_context2 = std::make_unique<DocumentCallbackContext>(
      doc_id, view_id2, OfficeInstance::Get());
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(
          [](const base::RepeatingClosure& signal,
             DocumentCallbackContext* context) {
            OfficeInstance::HandleDocumentCallback(event_type_id, payload,
                                                   context);
            signal.Run();
          },
          signal_barrier, document_context2.get()));

  waitable.Wait();
  base::RunLoop().RunUntilIdle();

  OfficeInstance::Get()->RemoveDocumentObservers(doc_id);
}

TEST_F(OfficeInstanceTest, DoesNotObserveByDefault) {
  WaitLoad();

  static constexpr size_t doc_id = 1;
  static constexpr size_t event_type_id = 2;
  static constexpr size_t view_id = 3;
  static constexpr size_t view_id2 = 4;
  static constexpr char payload[] = "this is a payload";

  DocumentEventId id{doc_id, event_type_id, view_id};
  MockDocumentEventObserver mock_observer;
  DocumentEventId id2{doc_id, event_type_id, view_id2};
  MockDocumentEventObserver mock_observer_two;

  EXPECT_CALL(mock_observer, DocumentCallback(event_type_id, payload)).Times(0);
  EXPECT_CALL(mock_observer_two, DocumentCallback(event_type_id, payload))
      .Times(0);

  base::WaitableEvent waitable;
  // wait for events for both views
  auto signal_barrier =
      base::BarrierClosure(2, base::BindOnce(&base::WaitableEvent::Signal,
                                             base::Unretained(&waitable)));

  auto document_context = std::make_unique<DocumentCallbackContext>(
      doc_id, view_id, OfficeInstance::Get());
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(
          [](const base::RepeatingClosure& signal,
             DocumentCallbackContext* context) {
            OfficeInstance::HandleDocumentCallback(event_type_id, payload,
                                                   context);
            signal.Run();
          },
          signal_barrier, document_context.get()));

  auto document_context2 = std::make_unique<DocumentCallbackContext>(
      doc_id, view_id2, OfficeInstance::Get());
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(
          [](const base::RepeatingClosure& signal,
             DocumentCallbackContext* context) {
            OfficeInstance::HandleDocumentCallback(event_type_id, payload,
                                                   context);
            signal.Run();
          },
          signal_barrier, document_context2.get()));

  waitable.Wait();
  base::RunLoop().RunUntilIdle();

  OfficeInstance::Get()->RemoveDocumentObservers(doc_id);
}

TEST_F(OfficeInstanceTest, DoesNotObserveAfterRemovalByEventId) {
  WaitLoad();

  static constexpr size_t doc_id = 1;
  static constexpr size_t event_type_id = 2;
  static constexpr size_t view_id = 3;
  static constexpr size_t view_id2 = 4;
  static constexpr char payload[] = "this is a payload";

  DocumentEventId id{doc_id, event_type_id, view_id};
  MockDocumentEventObserver mock_observer;
  DocumentEventId id2{doc_id, event_type_id, view_id2};
  MockDocumentEventObserver mock_observer_two;

  OfficeInstance::Get()->AddDocumentObserver(id, &mock_observer);
  OfficeInstance::Get()->AddDocumentObserver(id2, &mock_observer_two);
  OfficeInstance::Get()->RemoveDocumentObserver(id, &mock_observer);

  base::WaitableEvent waitable;
  // wait for events for both views
  auto signal_barrier =
      base::BarrierClosure(2, base::BindOnce(&base::WaitableEvent::Signal,
                                             base::Unretained(&waitable)));

  auto document_context = std::make_unique<DocumentCallbackContext>(
      doc_id, view_id, OfficeInstance::Get());

  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(
          [](const base::RepeatingClosure& signal,
             DocumentCallbackContext* context) {
            OfficeInstance::HandleDocumentCallback(event_type_id, payload,
                                                   context);
            signal.Run();
          },
          signal_barrier, document_context.get()));

  auto document_context2 = std::make_unique<DocumentCallbackContext>(
      doc_id, view_id2, OfficeInstance::Get());
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(
          [](const base::RepeatingClosure& signal,
             DocumentCallbackContext* context) {
            OfficeInstance::HandleDocumentCallback(event_type_id, payload,
                                                   context);
            signal.Run();
          },
          signal_barrier, document_context2.get()));

  waitable.Wait();
  base::RunLoop().RunUntilIdle();

  OfficeInstance::Get()->RemoveDocumentObservers(doc_id);
}

TEST_F(OfficeInstanceTest, DoesNotObserveAfterRemovalByDocumentId) {
  WaitLoad();

  static constexpr size_t doc_id = 1;
  static constexpr size_t event_type_id = 2;
  static constexpr size_t view_id = 3;
  static constexpr size_t view_id2 = 4;
  static constexpr char payload[] = "this is a payload";

  DocumentEventId id{doc_id, event_type_id, view_id};
  MockDocumentEventObserver mock_observer;
  DocumentEventId id2{doc_id, event_type_id, view_id2};
  MockDocumentEventObserver mock_observer_two;

  OfficeInstance::Get()->AddDocumentObserver(id, &mock_observer);
  OfficeInstance::Get()->AddDocumentObserver(id2, &mock_observer_two);
  OfficeInstance::Get()->RemoveDocumentObservers(doc_id, &mock_observer);
  EXPECT_CALL(mock_observer, DocumentCallback(event_type_id, payload)).Times(0);
  EXPECT_CALL(mock_observer_two, DocumentCallback(event_type_id, payload));

  base::WaitableEvent waitable;
  // wait for events for both views
  auto signal_barrier =
      base::BarrierClosure(2, base::BindOnce(&base::WaitableEvent::Signal,
                                             base::Unretained(&waitable)));

  auto document_context = std::make_unique<DocumentCallbackContext>(
      doc_id, view_id, OfficeInstance::Get());
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(
          [](const base::RepeatingClosure& signal,
             DocumentCallbackContext* context) {
            OfficeInstance::HandleDocumentCallback(event_type_id, payload,
                                                   context);
            signal.Run();
          },
          signal_barrier, document_context.get()));

  auto document_context2 = std::make_unique<DocumentCallbackContext>(
      doc_id, view_id2, OfficeInstance::Get());
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(
          [](const base::RepeatingClosure& signal,
             DocumentCallbackContext* context) {
            OfficeInstance::HandleDocumentCallback(event_type_id, payload,
                                                   context);
            signal.Run();
          },
          signal_barrier, document_context2.get()));

  waitable.Wait();
  base::RunLoop().RunUntilIdle();

  OfficeInstance::Get()->RemoveDocumentObservers(doc_id);
}

TEST_F(OfficeInstanceTest, DoesNotObserveAfterRemovingAllByDocumentId) {
  WaitLoad();

  static constexpr size_t doc_id = 1;
  static constexpr size_t event_type_id = 2;
  static constexpr size_t view_id = 3;
  static constexpr size_t view_id2 = 4;
  static constexpr char payload[] = "this is a payload";

  DocumentEventId id{doc_id, event_type_id, view_id};
  MockDocumentEventObserver mock_observer;
  DocumentEventId id2{doc_id, event_type_id, view_id2};
  MockDocumentEventObserver mock_observer_two;

  OfficeInstance::Get()->AddDocumentObserver(id, &mock_observer);
  OfficeInstance::Get()->AddDocumentObserver(id2, &mock_observer_two);
  OfficeInstance::Get()->RemoveDocumentObservers(doc_id);
  EXPECT_CALL(mock_observer, DocumentCallback(event_type_id, payload)).Times(0);
  EXPECT_CALL(mock_observer_two, DocumentCallback(event_type_id, payload))
      .Times(0);

  base::WaitableEvent waitable;
  // wait for events for both views
  auto signal_barrier =
      base::BarrierClosure(2, base::BindOnce(&base::WaitableEvent::Signal,
                                             base::Unretained(&waitable)));

  auto document_context = std::make_unique<DocumentCallbackContext>(
      doc_id, view_id, OfficeInstance::Get());
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(
          [](const base::RepeatingClosure& signal,
             DocumentCallbackContext* context) {
            OfficeInstance::HandleDocumentCallback(event_type_id, payload,
                                                   context);
            signal.Run();
          },
          signal_barrier, document_context.get()));

  auto document_context2 = std::make_unique<DocumentCallbackContext>(
      doc_id, view_id2, OfficeInstance::Get());
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(
          [](const base::RepeatingClosure& signal,
             DocumentCallbackContext* context) {
            OfficeInstance::HandleDocumentCallback(event_type_id, payload,
                                                   context);
            signal.Run();
          },
          signal_barrier, document_context2.get()));

  waitable.Wait();
  base::RunLoop().RunUntilIdle();
}

}  // namespace electron::office
