#pragma once

// pqrs::cf::run_loop_thread v3.2.0

// (C) Copyright Takayama Fumihiko 2018.
// Distributed under the Boost Software License, Version 1.0.
// (See https://www.boost.org/LICENSE_1_0.txt)

// `pqrs::cf::run_loop_thread` can be used safely in a multi-threaded environment.

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <pqrs/cf/cf_ptr.hpp>
#include <pqrs/gsl.hpp>
#include <pqrs/thread_wait.hpp>
#include <thread>
#include <utility>
#include <vector>

namespace pqrs::cf {
class run_loop_thread final {
public:
  class execution_scope {
  public:
    virtual ~execution_scope() = default;
  };

  using execution_scope_factory = std::function<std::unique_ptr<execution_scope>()>;

  enum class failure_policy {
    abort,
    exit,
  };

  class extra {
  public:
#include "run_loop_thread/extra/shared_run_loop_thread.hpp"
  };

  run_loop_thread(const run_loop_thread&) = delete;
  run_loop_thread& operator=(const run_loop_thread&) = delete;

  run_loop_thread(failure_policy policy = failure_policy::abort)
      : run_loop_thread(policy, {}) {
  }

  run_loop_thread(failure_policy policy,
                  execution_scope_factory execution_scope_factory)
      : failure_policy_(policy),
        execution_scope_factory_(std::move(execution_scope_factory)) {
    using namespace std::chrono_literals;

    std::atomic<bool> ready{false};

    thread_ = std::thread([this, &ready] {
      // Append a source to prevent immediately quitting of `CFRunLoopRun`.

      auto context = CFRunLoopSourceContext();
      context.info = reinterpret_cast<void*>(&ready);
      context.perform = [](void* _Nullable info) {
        auto ready = reinterpret_cast<std::atomic<bool>*>(info);
        ready->store(true);
      };

      {
        std::lock_guard<std::mutex> lock(initial_source_mutex_);

        run_loop_ = CFRunLoopGetCurrent();

        auto observer_context = CFRunLoopObserverContext();
        observer_context.info = this;

        begin_observer_ = adopt_cf_ptr(CFRunLoopObserverCreate(
            kCFAllocatorDefault,
            kCFRunLoopEntry | kCFRunLoopAfterWaiting,
            true,
            std::numeric_limits<CFIndex>::min(),
            static_begin_observer_callback,
            &observer_context));

        end_observer_ = adopt_cf_ptr(CFRunLoopObserverCreate(
            kCFAllocatorDefault,
            kCFRunLoopBeforeWaiting | kCFRunLoopExit,
            true,
            std::numeric_limits<CFIndex>::max(),
            static_end_observer_callback,
            &observer_context));

        CFRunLoopAddObserver(*run_loop_,
                             *begin_observer_,
                             kCFRunLoopCommonModes);

        CFRunLoopAddObserver(*run_loop_,
                             *end_observer_,
                             kCFRunLoopCommonModes);

        initial_source_ = adopt_cf_ptr(CFRunLoopSourceCreate(kCFAllocatorDefault,
                                                             0,
                                                             &context));

        CFRunLoopAddSource(*run_loop_,
                           *initial_source_,
                           kCFRunLoopCommonModes);
      }

      // Run

      CFRunLoopRun();

      // Remove source

      execution_scopes_.clear();

      CFRunLoopRemoveObserver(*run_loop_,
                              *end_observer_,
                              kCFRunLoopCommonModes);

      CFRunLoopRemoveObserver(*run_loop_,
                              *begin_observer_,
                              kCFRunLoopCommonModes);

      CFRunLoopRemoveSource(*run_loop_,
                            *initial_source_,
                            kCFRunLoopCommonModes);
    });

    // Wait until CFRunLoop is running.

    auto since = std::chrono::system_clock::now();
    while (!ready) {
      auto now = std::chrono::system_clock::now();
      if (now - since > 3s) {
        // Although this does not usually happen, it is reached when CFRunLoop processing does not start due to a problem with CFRunLoop.
        // Abort because it is irrecoverable.
        if (failure_policy_ == failure_policy::exit) {
          // `std::quick_exit` causes a "Symbol not found: _quick_exit" error on macOS 13,
          // so we use `std::_Exit` instead.
          std::_Exit(EXIT_FAILURE);
        } else {
          abort();
        }
      }

      {
        std::lock_guard<std::mutex> lock(initial_source_mutex_);

        if (run_loop_ && initial_source_) {
          CFRunLoopSourceSignal(*initial_source_);
          CFRunLoopWakeUp(*run_loop_);
        }
      }

      // The period of time should be as short as possible, as the thread sleeps at least once here.
      std::this_thread::sleep_for(10ms);
    }
  }

  ~run_loop_thread() noexcept {
    if (thread_.joinable()) {
      // We have to call `terminate` before destroy run_loop_thread.
      abort();
    }

    initial_source_ = nullptr;
    end_observer_ = nullptr;
    begin_observer_ = nullptr;
    run_loop_ = nullptr;
  }

  void terminate() {
    std::lock_guard<std::mutex> lock(thread_mutex_);

    if (thread_.joinable()) {
      enqueue(^{
        CFRunLoopStop(*run_loop_);
      });

      thread_.join();
    }
  }

  [[nodiscard]] CFRunLoopRef _Nonnull get_run_loop() const noexcept {
    return *run_loop_;
  }

  void wake() const noexcept {
    CFRunLoopWakeUp(*run_loop_);
  }

  void add_source(CFRunLoopSourceRef _Nullable source,
                  CFRunLoopMode _Nonnull mode = kCFRunLoopCommonModes) noexcept {
    if (source) {
      CFRunLoopAddSource(*run_loop_,
                         source,
                         mode);

      CFRunLoopWakeUp(*run_loop_);
    }
  }

  void remove_source(CFRunLoopSourceRef _Nullable source,
                     CFRunLoopMode _Nonnull mode = kCFRunLoopCommonModes) noexcept {
    if (source) {
      CFRunLoopRemoveSource(*run_loop_,
                            source,
                            mode);

      CFRunLoopWakeUp(*run_loop_);
    }
  }

  void enqueue(void (^_Nonnull block)()) const noexcept {
    CFRunLoopPerformBlock(*run_loop_,
                          kCFRunLoopCommonModes,
                          block);

    CFRunLoopWakeUp(*run_loop_);
  }

private:
  static void static_begin_observer_callback(CFRunLoopObserverRef _Nonnull,
                                             CFRunLoopActivity activity,
                                             void* _Nullable info) {
    if (auto self = reinterpret_cast<run_loop_thread*>(info)) {
      switch (activity) {
        case kCFRunLoopEntry:
          self->execution_scopes_.push_back(self->make_execution_scope());
          break;

        case kCFRunLoopAfterWaiting:
          if (!self->execution_scopes_.empty() &&
              !self->execution_scopes_.back()) {
            self->execution_scopes_.back() = self->make_execution_scope();
          }
          break;

        default:
          break;
      }
    }
  }

  static void static_end_observer_callback(CFRunLoopObserverRef _Nonnull,
                                           CFRunLoopActivity activity,
                                           void* _Nullable info) {
    if (auto self = reinterpret_cast<run_loop_thread*>(info)) {
      switch (activity) {
        case kCFRunLoopBeforeWaiting:
          if (!self->execution_scopes_.empty()) {
            self->execution_scopes_.back().reset();
          }
          break;

        case kCFRunLoopExit:
          if (!self->execution_scopes_.empty()) {
            self->execution_scopes_.back().reset();
            self->execution_scopes_.pop_back();
          }
          break;

        default:
          break;
      }
    }
  }

  [[nodiscard]] std::unique_ptr<execution_scope> make_execution_scope() const {
    if (execution_scope_factory_) {
      return execution_scope_factory_();
    }

    return nullptr;
  }

  std::thread thread_;
  std::mutex thread_mutex_;
  cf_ptr<CFRunLoopRef> run_loop_;

  cf_ptr<CFRunLoopObserverRef> begin_observer_;
  cf_ptr<CFRunLoopObserverRef> end_observer_;
  cf_ptr<CFRunLoopSourceRef> initial_source_;
  std::mutex initial_source_mutex_;
  failure_policy failure_policy_;
  execution_scope_factory execution_scope_factory_;
  std::vector<std::unique_ptr<execution_scope>> execution_scopes_;
};
} // namespace pqrs::cf
