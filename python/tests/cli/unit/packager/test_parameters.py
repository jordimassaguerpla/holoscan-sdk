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
import pathlib
import platform

import pytest

from holoscan.cli.common.constants import Constants, DefaultValues
from holoscan.cli.common.enum_types import ApplicationType
from holoscan.cli.common.exceptions import UnknownApplicationType
from holoscan.cli.packager.parameters import PackageBuildParameters


class TestPackageBuildParameters:
    def test_with_aarch64(self, monkeypatch):
        monkeypatch.setattr(platform, "processor", lambda: "aarch64")

        build_parameters = PackageBuildParameters()
        assert build_parameters.holoscan_deb_arch == "arm64"
        assert build_parameters.cuda_deb_arch == "sbsa"

    def test_with_x64(self, monkeypatch):
        monkeypatch.setattr(platform, "processor", lambda: "x86_64")

        build_parameters = PackageBuildParameters()
        assert build_parameters.holoscan_deb_arch == "amd64"
        assert build_parameters.cuda_deb_arch == "x86_64"

    def test_set_application_python_dir(self, monkeypatch):
        input_dir = pathlib.Path("/path/to/my/python/app/dir")

        def mock_isdir(path):
            if path == input_dir:
                return True
            return False

        def mock_exists(path):
            if path == (input_dir / Constants.PYTHON_MAIN_FILE):
                return True

            if path == (input_dir / "requirements.txt"):
                return True
            return False

        monkeypatch.setattr(os.path, "isdir", mock_isdir)
        monkeypatch.setattr(os.path, "exists", mock_exists)
        build_parameters = PackageBuildParameters()
        build_parameters.application = input_dir

        assert build_parameters.application == input_dir
        assert build_parameters.application_type == ApplicationType.PythonModule
        assert build_parameters._data["application_type"] == ApplicationType.PythonModule.name
        assert build_parameters.application_directory == input_dir
        assert (
            build_parameters.command
            == f'["{Constants.PYTHON_EXECUTABLE}", "{build_parameters.app_dir}"]'
        )
        assert build_parameters.command_filename == os.path.basename(input_dir)

    def test_set_application_python_file(self, monkeypatch):
        input_dir = pathlib.Path("/path/to/my/python/app.py")

        def mock_isdir(path):
            if path == input_dir:
                return False
            return False

        def mock_exists(path):
            if path == (input_dir / Constants.PYTHON_MAIN_FILE):
                return True

            if path == (input_dir / "requirements.txt"):
                return True
            return False

        def mock_isfile(path):
            if path == input_dir:
                return True
            return False

        monkeypatch.setattr(os.path, "isdir", mock_isdir)
        monkeypatch.setattr(os.path, "isfile", mock_isfile)
        monkeypatch.setattr(os.path, "exists", mock_exists)
        build_parameters = PackageBuildParameters()
        build_parameters.application = input_dir

        assert build_parameters.application == input_dir
        assert build_parameters.application_type == ApplicationType.PythonFile
        assert build_parameters._data["application_type"] == ApplicationType.PythonFile.name
        assert build_parameters.application_directory == pathlib.Path(os.path.dirname(input_dir))
        assert build_parameters.command == (
            f'["{Constants.PYTHON_EXECUTABLE}", '
            + f'"{os.path.join(DefaultValues.HOLOSCAN_APP_DIR, os.path.basename(input_dir))}"]'
        )
        assert build_parameters.command_filename == os.path.basename(input_dir)

    def test_set_application_cpp_dir(self, monkeypatch):
        input_dir = pathlib.Path("/path/to/my/cpp/source/dir")

        def mock_isdir(path):
            if path == input_dir:
                return True
            return False

        def mock_exists(path):
            if path == (input_dir / Constants.CPP_CMAKELIST_FILE):
                return True

            if path == (input_dir / "requirements.txt"):
                return True
            return False

        monkeypatch.setattr(os.path, "isdir", mock_isdir)
        monkeypatch.setattr(os.path, "exists", mock_exists)
        build_parameters = PackageBuildParameters()
        build_parameters.application = input_dir

        assert build_parameters.application == input_dir
        assert build_parameters.application_type == ApplicationType.CppCMake
        assert build_parameters._data["application_type"] == ApplicationType.CppCMake.name
        assert build_parameters.application_directory == input_dir
        assert (
            f'["{Constants.PYTHON_EXECUTABLE}", '
            + f'"{os.path.join(DefaultValues.HOLOSCAN_APP_DIR, os.path.basename(input_dir))}"]'
        )
        assert build_parameters.command_filename == os.path.basename(input_dir)

    def test_set_application_binary_app(self, monkeypatch):
        input_dir = pathlib.Path("/path/to/my/cpp/app/exe")

        def mock_isdir(path):
            if path == input_dir:
                return False
            return False

        def mock_isfile(path):
            if path == input_dir:
                return True
            return False

        monkeypatch.setattr(os.path, "isdir", mock_isdir)
        monkeypatch.setattr(os.path, "isfile", mock_isfile)
        monkeypatch.setattr(os, "access", lambda x, y: True)
        build_parameters = PackageBuildParameters()
        build_parameters.application = input_dir

        assert build_parameters.application == input_dir
        assert build_parameters.application_type == ApplicationType.Binary
        assert build_parameters._data["application_type"] == ApplicationType.Binary.name
        assert build_parameters.application_directory == pathlib.Path(os.path.dirname(input_dir))
        assert (
            f'["{Constants.PYTHON_EXECUTABLE}", '
            + f'"{os.path.join(DefaultValues.HOLOSCAN_APP_DIR, os.path.basename(input_dir))}"]'
        )
        assert build_parameters.command_filename == os.path.basename(input_dir)

    def test_set_application_unsupported(self, monkeypatch):
        input_dir = pathlib.Path("/some/path")
        monkeypatch.setattr(os.path, "isdir", lambda x: False)
        monkeypatch.setattr(os.path, "isfile", lambda x: False)
        build_parameters = PackageBuildParameters()

        with pytest.raises(UnknownApplicationType):
            build_parameters.application = input_dir
