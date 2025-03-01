/*
 * SPDX-FileCopyrightText: Copyright (c) 2022-2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef HOLOINFER_INFERENCE_TEST_SETTINGS_HPP
#define HOLOINFER_INFERENCE_TEST_SETTINGS_HPP

#include <holoinfer.hpp>
#include <holoinfer_utils.hpp>

#define use_onnxruntime 1

namespace HoloInfer = holoscan::inference;

static const bool is_x86_64 = !HoloInfer::is_platform_aarch64();

#endif
