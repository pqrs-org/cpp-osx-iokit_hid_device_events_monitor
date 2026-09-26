#include <boost/ut.hpp>
#include <pqrs/osx/iokit_hid_device_events_monitor.hpp>
#include <stdexcept>

int main() {
  using namespace boost::ut;
  using namespace boost::ut::literals;

  "constructor filter copy exception"_test = [] {
    struct throwing_filter {
      throwing_filter() = default;
      throwing_filter(const throwing_filter&) {
        throw std::runtime_error("filter copy failed");
      }
      throwing_filter(throwing_filter&&) = default;
      bool operator()(uint32_t, std::span<const uint8_t>) const {
        return true;
      }
    };
    auto source = std::make_shared<pqrs::dispatcher::pseudo_time_source>();
    auto dispatcher = std::make_shared<pqrs::dispatcher::dispatcher>(source);
    auto run_loop_thread = std::make_shared<pqrs::cf::run_loop_thread>();
    pqrs::osx::iokit_hid_device_events_monitor::parameters parameters;
    parameters.input_report_filter = throwing_filter{};
    expect(throws<std::runtime_error>([&] {
      pqrs::osx::iokit_hid_device_events_monitor monitor(dispatcher, run_loop_thread, nullptr, parameters);
    }));
    run_loop_thread->terminate();
  };

  "iokit_hid_device_events_monitor"_test = [] {
    auto time_source = std::make_shared<pqrs::dispatcher::hardware_time_source>();
    auto dispatcher = std::make_shared<pqrs::dispatcher::dispatcher>(time_source);
    auto run_loop_thread = std::make_shared<pqrs::cf::run_loop_thread>();

    // Observe input values only.
    auto monitor = std::make_unique<pqrs::osx::iokit_hid_device_events_monitor>(dispatcher,
                                                                                run_loop_thread,
                                                                                nullptr);
    expect(!monitor->seized());
    monitor = nullptr;

    // Observe input reports only.
    monitor = std::make_unique<pqrs::osx::iokit_hid_device_events_monitor>(
        dispatcher,
        run_loop_thread,
        nullptr,
        pqrs::osx::iokit_hid_device_events_monitor::parameters{
            .observe_input_values = false,
            .observe_input_reports = true,
            .input_report_filter = [](auto, auto) {
              return true;
            },
        });
    expect(!monitor->seized());

    monitor = nullptr;

    // Observe both input values and input reports.
    monitor = std::make_unique<pqrs::osx::iokit_hid_device_events_monitor>(
        dispatcher,
        run_loop_thread,
        nullptr,
        pqrs::osx::iokit_hid_device_events_monitor::parameters{
            .observe_input_values = true,
            .observe_input_reports = true,
        });
    expect(!monitor->seized());

    monitor->async_start(kIOHIDOptionsTypeNone,
                         std::chrono::milliseconds(3000));
    monitor->async_stop();
    expect(!monitor->seized());

    monitor = nullptr;

    {
      auto wait = pqrs::make_thread_wait();

      run_loop_thread->enqueue(^{
        auto monitor = std::make_unique<pqrs::osx::iokit_hid_device_events_monitor>(dispatcher,
                                                                                    run_loop_thread,
                                                                                    nullptr);
        monitor = nullptr;
        wait->notify();
      });

      wait->wait_notice();
    }

    run_loop_thread->terminate();
    run_loop_thread = nullptr;

    dispatcher->terminate();
    dispatcher = nullptr;
  };

  return 0;
}
