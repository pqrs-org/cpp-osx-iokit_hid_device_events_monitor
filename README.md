[![Build Status](https://github.com/pqrs-org/cpp-osx-iokit_hid_device_events_monitor/workflows/CI/badge.svg)](https://github.com/pqrs-org/cpp-osx-iokit_hid_device_events_monitor/actions)
[![License](https://img.shields.io/badge/license-Boost%20Software%20License-blue.svg)](https://github.com/pqrs-org/cpp-osx-iokit_hid_device_events_monitor/blob/main/LICENSE.md)

# cpp-osx-iokit_hid_device_events_monitor

A wrapper that manages `IOHIDQueueRegisterValueAvailableCallback` and optional
`IOHIDDeviceRegisterInputReportCallback` observation in the same device lifecycle.

Raw input report observation is disabled by default. Enable it with
`parameters{.observe_input_reports = true}`. `input_report_arrived` is invoked
from the dispatcher thread, and its report span is valid only during the signal
invocation.

`parameters.input_report_filter` can reject reports before their borrowed IOKit
buffers are copied and enqueued to the dispatcher. The filter is invoked
synchronously on the supplied run loop thread, so it should remain lightweight
and must not destroy the monitor synchronously.

## Requirements

cpp-osx-iokit_hid_device_events_monitor depends on the following classes.

- [Nod](https://github.com/fr00b0/nod)
- [pqrs::cf::run_loop_thread](https://github.com/pqrs-org/cpp-cf-run_loop_thread)
- [pqrs::dispatcher](https://github.com/pqrs-org/cpp-dispatcher)
- [pqrs::osx::iokit_hid_device](https://github.com/pqrs-org/cpp-osx-iokit_hid_device)
- [pqrs::osx::iokit_return](https://github.com/pqrs-org/cpp-osx-iokit_return)

## Install

Copy `include/pqrs` and `vendor/vendor/include` directories into your include directory.
