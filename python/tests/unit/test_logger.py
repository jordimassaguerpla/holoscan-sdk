# SPDX-FileCopyrightText: Copyright (c) 2022-2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import os

import pytest

from holoscan.logger import (
    LogLevel,
    disable_backtrace,
    dump_backtrace,
    enable_backtrace,
    flush,
    flush_level,
    flush_on,
    log_level,
    set_log_level,
    set_log_pattern,
    should_backtrace,
)


def test_set_log_pattern():
    set_log_pattern(r"[%Y-%m-%d %H:%M:%S.%e] [%l] [%n] %v")


@pytest.mark.parametrize(
    "level",
    (
        LogLevel.TRACE,
        LogLevel.DEBUG,
        LogLevel.INFO,
        LogLevel.WARN,
        LogLevel.ERROR,
        LogLevel.OFF,
    ),
)
def test_set_log_level(level):
    # remember existing environment variable
    orig_env = os.environ.get("HOLOSCAN_LOG_LEVEL")
    # remember current log level
    orig_level = log_level()
    try:
        # remove the environment variable
        if "HOLOSCAN_LOG_LEVEL" in os.environ:
            del os.environ["HOLOSCAN_LOG_LEVEL"]
        set_log_level(level)
        assert log_level() == level
        # set INFO to the environment variable
        os.environ["HOLOSCAN_LOG_LEVEL"] = "INFO"
        # now set_log_level should not change the log level
        set_log_level(level)
        assert log_level() == LogLevel.INFO
    finally:
        # restore the environment variable
        if orig_env is None:
            if "HOLOSCAN_LOG_LEVEL" in os.environ:
                del os.environ["HOLOSCAN_LOG_LEVEL"]
        else:
            os.environ["HOLOSCAN_LOG_LEVEL"] = orig_env
        # restore the logging level prior to the test
        set_log_level(orig_level)


def test_logger_flush():
    level = flush_level()

    flush_on(LogLevel.TRACE)
    assert flush_level() == LogLevel.TRACE

    flush()

    # restore original flush level
    flush_on(level)


def test_logger_backtrace():
    # determine initial setting
    enabled = should_backtrace()

    enable_backtrace(32)
    assert should_backtrace()
    dump_backtrace()

    disable_backtrace()
    assert not should_backtrace()

    # restore original backtrace setting
    if enabled:
        enable_backtrace(32)
    else:
        disable_backtrace()
