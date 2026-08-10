[![Build Status](https://github.com/pqrs-org/cpp-osx-iokit_hid_device_events_monitor/workflows/CI/badge.svg)](https://github.com/pqrs-org/cpp-osx-iokit_hid_device_events_monitor/actions)
[![License](https://img.shields.io/badge/license-Boost%20Software%20License-blue.svg)](https://github.com/pqrs-org/cpp-osx-iokit_hid_device_events_monitor/blob/main/LICENSE.md)

# cpp-osx-iokit_hid_device_events_monitor

A wrapper for observing HID input values, input reports, or both in the same
device lifecycle. Input value observation is enabled by default.

See [example/main.cpp](example/main.cpp) for configuration examples.

## Requirements

cpp-osx-iokit_hid_device_events_monitor depends on the following classes.

- [Nod](https://github.com/fr00b0/nod)
- [pqrs::cf::run_loop_thread](https://github.com/pqrs-org/cpp-cf-run_loop_thread)
- [pqrs::dispatcher](https://github.com/pqrs-org/cpp-dispatcher)
- [pqrs::osx::iokit_hid_device](https://github.com/pqrs-org/cpp-osx-iokit_hid_device)
- [pqrs::osx::iokit_return](https://github.com/pqrs-org/cpp-osx-iokit_return)

## Install

Copy `include/pqrs` and `vendor/vendor/include` directories into your include directory.
