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
import datetime

import pytest

from holoscan.conditions import (
    BooleanCondition,
    CountCondition,
    DownstreamMessageAffordableCondition,
    MessageAvailableCondition,
    PeriodicCondition,
)
from holoscan.core import Application, Condition, ConditionType, Operator
from holoscan.gxf import Entity, GXFCondition


class TestBooleanCondition:
    def test_kwarg_based_initialization(self, app, capfd):
        cond = BooleanCondition(fragment=app, name="boolean", enable_tick=True)
        assert isinstance(cond, GXFCondition)
        assert isinstance(cond, Condition)
        assert cond.gxf_typename == "nvidia::gxf::BooleanSchedulingTerm"

        # assert no warnings or errors logged
        captured = capfd.readouterr()
        assert "error" not in captured.err
        assert "warning" not in captured.err

    def test_enable_tick(self, app, capfd):
        cond = BooleanCondition(fragment=app, name="boolean", enable_tick=True)
        cond.disable_tick()
        assert not cond.check_tick_enabled()

        cond.enable_tick()
        assert cond.check_tick_enabled()

    def test_default_initialization(self, app):
        BooleanCondition(app)

    def test_positional_initialization(self, app):
        BooleanCondition(app, False, "bool")


class TestCountCondition:
    def test_kwarg_based_initialization(self, app, capfd):
        cond = CountCondition(fragment=app, name="count", count=100)
        assert isinstance(cond, GXFCondition)
        assert isinstance(cond, Condition)
        assert cond.gxf_typename == "nvidia::gxf::CountSchedulingTerm"

        # assert no warnings or errors logged
        captured = capfd.readouterr()
        assert "error" not in captured.err
        assert "warning" not in captured.err

    def test_count(self, app, capfd):
        cond = CountCondition(fragment=app, name="count", count=100)
        cond.count = 10
        assert cond.count == 10

    def test_default_initialization(self, app):
        CountCondition(app)

    def test_positional_initialization(self, app):
        CountCondition(app, 100, "counter")


class TestDownstreamMessageAffordableCondition:
    def test_kwarg_based_initialization(self, app, capfd):
        cond = DownstreamMessageAffordableCondition(
            fragment=app, name="downstream_affordable", min_size=10
        )
        assert isinstance(cond, GXFCondition)
        assert isinstance(cond, Condition)
        assert cond.gxf_typename == "nvidia::gxf::DownstreamReceptiveSchedulingTerm"

        # assert no warnings or errors logged
        captured = capfd.readouterr()
        assert "error" not in captured.err
        assert "warning" not in captured.err

    def test_default_initialization(self, app):
        DownstreamMessageAffordableCondition(app)

    def test_positional_initialization(self, app):
        DownstreamMessageAffordableCondition(app, 4, "affordable")


class TestMessageAvailableCondition:
    def test_kwarg_based_initialization(self, app, capfd):
        cond = MessageAvailableCondition(
            fragment=app, name="message_available", min_size=1, front_stage_max_size=10
        )
        assert isinstance(cond, GXFCondition)
        assert isinstance(cond, Condition)
        assert cond.gxf_typename == "nvidia::gxf::MessageAvailableSchedulingTerm"

        # assert no warnings or errors logged
        captured = capfd.readouterr()
        assert "error" not in captured.err
        assert "warning" not in captured.err

    def test_default_initialization(self, app):
        MessageAvailableCondition(app)

    def test_positional_initialization(self, app):
        MessageAvailableCondition(app, 1, 4, "available")


class TestPeriodicCondition:
    def test_kwarg_based_initialization(self, app, capfd):
        cond = PeriodicCondition(fragment=app, name="periodic", recess_period=100)
        assert isinstance(cond, GXFCondition)
        assert isinstance(cond, Condition)
        assert cond.gxf_typename == "nvidia::gxf::PeriodicSchedulingTerm"

        # assert no warnings or errors logged
        captured = capfd.readouterr()
        assert "error" not in captured.err
        assert "warning" not in captured.err

    @pytest.mark.parametrize(
        "period",
        [
            1000,
            datetime.timedelta(minutes=1),
            datetime.timedelta(seconds=1),
            datetime.timedelta(milliseconds=1),
            datetime.timedelta(microseconds=1),
        ],
    )
    def test_periodic_constructors(self, app, capfd, period):
        cond = PeriodicCondition(fragment=app, name="periodic", recess_period=period)
        if isinstance(period, int):
            expected_ns = period
        else:
            expected_ns = int(period.total_seconds() * 1_000_000_000)

        assert cond.recess_period_ns() == expected_ns

    @pytest.mark.parametrize(
        "period",
        [
            1000,
            datetime.timedelta(minutes=1),
            datetime.timedelta(seconds=1),
            datetime.timedelta(milliseconds=1),
            datetime.timedelta(microseconds=1),
        ],
    )
    def test_recess_period_method(self, app, capfd, period):
        cond = PeriodicCondition(fragment=app, name="periodic", recess_period=1)
        cond.recess_period(period)
        if isinstance(period, int):
            expected_ns = period
        else:
            expected_ns = int(period.total_seconds() * 1_000_000_000)

        assert cond.recess_period_ns() == expected_ns

    def test_positional_initialization(self, app):
        PeriodicCondition(app, 100000, "periodic")

    def test_invalid_recess_period_type(self, app):
        with pytest.raises(TypeError):
            PeriodicCondition(app, recess_period="100s", name="periodic")


####################################################################################################
# Test Ping app with no conditions on Rx operator
####################################################################################################


class PingTxOpNoCondition(Operator):
    def __init__(self, *args, **kwargs):
        self.index = 0
        # Need to call the base class constructor last
        super().__init__(*args, **kwargs)

    def setup(self, spec):
        spec.output("out1")
        spec.output("out2")

    def compute(self, op_input, op_output, context):
        self.index += 1
        if self.index == 1:
            print(f"#TX{self.index}")  # no emit
        elif self.index == 2:
            print(f"#T1O{self.index}")  # emit only out1
            op_output.emit(self.index, "out1")
        elif self.index == 3:
            print(f"#T2O{self.index}")  # emit only out2 (Entity object)
            entity = Entity(context)
            op_output.emit(entity, "out2")
        elif self.index == 4:
            print(f"#TO{self.index}")  # emit both out1 and out2 (out2 is Entity object)
            op_output.emit(self.index, "out1")
            entity = Entity(context)
            op_output.emit(entity, "out2")
        else:
            print(f"#TX{self.index}")  # no emit


class PingRxOpNoInputCondition(Operator):
    def __init__(self, *args, **kwargs):
        self.index = 0
        # Need to call the base class constructor last
        super().__init__(*args, **kwargs)

    def setup(self, spec):
        # No input condition
        spec.input("in1").condition(ConditionType.NONE)
        spec.input("in2").condition(ConditionType.NONE)

    def compute(self, op_input, op_output, context):
        self.index += 1
        value1 = op_input.receive("in1")
        value2 = op_input.receive("in2")

        # Since value can be an empty dict, we need to check for None explicitly
        if value1 is not None and value2 is None:
            print(f"#R1O{self.index}")
        elif value1 is None and value2 is not None:
            print(f"#R2O{self.index}")
        elif value1 is not None and value2 is not None:
            print(f"#RO{self.index}")
        else:
            print(f"#RX{self.index}")


class PingRxOpNoInputConditionApp(Application):
    def compose(self):
        tx = PingTxOpNoCondition(self, CountCondition(self, 5), name="tx")
        rx = PingRxOpNoInputCondition(self, CountCondition(self, 5), name="rx")
        self.add_flow(tx, rx, {("out1", "in1"), ("out2", "in2")})


def test_ping_no_input_condition(capfd):
    app = PingRxOpNoInputConditionApp()
    app.run()

    captured = capfd.readouterr()

    sequence = (line[1:] if line.startswith("#") else "" for line in captured.out.splitlines())
    # The following sequence is expected:
    #   TX1->RX1, T1O2-> R1O2, T2O3->R2O3, TO4->RO4, TX5->RX5
    assert "".join(sequence) == "TX1RX1T1O2R1O2T2O3R2O3TO4RO4TX5RX5"

    error_msg = captured.err.lower()
    assert "error" not in error_msg
    assert "warning" not in error_msg
