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

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include <cuda/cuda_service.hpp>
#include <holoviz/holoviz.hpp>
#include "headless_fixture.hpp"

namespace viz = holoscan::viz;

namespace holoscan::viz {

// define the '<<' operator to get a nice parameter string
std::ostream& operator<<(std::ostream& os, const PrimitiveTopology& topology) {
#define CASE(VALUE)            \
  case VALUE:                  \
    os << std::string(#VALUE); \
    break;

  switch (topology) {
    CASE(viz::PrimitiveTopology::POINT_LIST);
    CASE(viz::PrimitiveTopology::LINE_LIST);
    CASE(viz::PrimitiveTopology::LINE_STRIP);
    CASE(viz::PrimitiveTopology::TRIANGLE_LIST);
    CASE(viz::PrimitiveTopology::CROSS_LIST);
    CASE(viz::PrimitiveTopology::RECTANGLE_LIST);
    CASE(viz::PrimitiveTopology::OVAL_LIST);
    CASE(viz::PrimitiveTopology::POINT_LIST_3D);
    CASE(viz::PrimitiveTopology::LINE_LIST_3D);
    CASE(viz::PrimitiveTopology::LINE_STRIP_3D);
    CASE(viz::PrimitiveTopology::TRIANGLE_LIST_3D);
    default:
      os.setstate(std::ios_base::failbit);
  }
  return os;

#undef CASE
}

}  // namespace holoscan::viz

// Fixture that initializes Holoviz
class PrimitiveTopology : public TestHeadless,
                          public testing::WithParamInterface<viz::PrimitiveTopology> {};

TEST_P(PrimitiveTopology, Primitive) {
  const viz::PrimitiveTopology topology = GetParam();

  uint32_t color_crc, depth_crc;
  uint32_t primitive_count;
  std::vector<float> data;
  switch (topology) {
    case viz::PrimitiveTopology::POINT_LIST:
      primitive_count = 1;
      data.push_back(0.5f);
      data.push_back(0.5f);
      color_crc = 0x3088e839;
      depth_crc = 0x748e4c96;
      break;
    case viz::PrimitiveTopology::LINE_LIST:
      primitive_count = 2;
      data.push_back(0.1f);
      data.push_back(0.1f);
      data.push_back(0.9f);
      data.push_back(0.9f);

      data.push_back(0.7f);
      data.push_back(0.3f);
      data.push_back(0.2f);
      data.push_back(0.4f);
      color_crc = 0xa64ef48a;
      depth_crc = 0x802dbbb0;
      break;
    case viz::PrimitiveTopology::LINE_STRIP:
      primitive_count = 2;
      data.push_back(0.1f);
      data.push_back(0.1f);
      data.push_back(0.7f);
      data.push_back(0.9f);

      data.push_back(0.3f);
      data.push_back(0.2f);
      color_crc = 0xa1e2f97a;
      depth_crc = 0xfae233b9;
      break;
    case viz::PrimitiveTopology::TRIANGLE_LIST:
      primitive_count = 2;
      data.push_back(0.1f);
      data.push_back(0.1f);
      data.push_back(0.5f);
      data.push_back(0.9f);
      data.push_back(0.9f);
      data.push_back(0.1f);

      data.push_back(0.05f);
      data.push_back(0.7f);
      data.push_back(0.15f);
      data.push_back(0.8f);
      data.push_back(0.25f);
      data.push_back(0.6f);
      color_crc = 0x1e4e5c9b;
      depth_crc = 0x101577b;
      break;
    case viz::PrimitiveTopology::CROSS_LIST:
      primitive_count = 2;
      data.push_back(0.5f);
      data.push_back(0.5f);
      data.push_back(0.1f);

      data.push_back(0.1f);
      data.push_back(0.3f);
      data.push_back(0.01f);
      color_crc = 0xa54496e9;
      depth_crc = 0x44098c3f;
      break;
    case viz::PrimitiveTopology::RECTANGLE_LIST:
      primitive_count = 2;
      data.push_back(0.1f);
      data.push_back(0.1f);
      data.push_back(0.9f);
      data.push_back(0.9f);

      data.push_back(0.3f);
      data.push_back(0.2f);
      data.push_back(0.5f);
      data.push_back(0.3f);
      color_crc = 0x77f7b3e8;
      depth_crc = 0xf67bacdc;
      break;
    case viz::PrimitiveTopology::OVAL_LIST:
      primitive_count = 2;
      data.push_back(0.5f);
      data.push_back(0.5f);
      data.push_back(0.2f);
      data.push_back(0.1f);

      data.push_back(0.6f);
      data.push_back(0.4f);
      data.push_back(0.05f);
      data.push_back(0.07f);
      color_crc = 0x83ed3ac2;
      depth_crc = 0x41d7da93;
      break;
    case viz::PrimitiveTopology::POINT_LIST_3D:
      primitive_count = 1;
      data.push_back(-0.5f);
      data.push_back(0.5f);
      data.push_back(0.8f);
      color_crc = 0x83063d37;
      depth_crc = 0x1273ab78;
      break;
    case viz::PrimitiveTopology::LINE_LIST_3D:
      primitive_count = 2;
      data.push_back(-0.1f);
      data.push_back(-0.1f);
      data.push_back(0.1f);
      data.push_back(0.9f);
      data.push_back(0.9f);
      data.push_back(0.3f);

      data.push_back(-0.7f);
      data.push_back(-0.3f);
      data.push_back(0.2f);
      data.push_back(0.2f);
      data.push_back(0.4f);
      data.push_back(0.5f);
      color_crc = 0x30cd7e29;
      depth_crc = 0xa31c2460;
      break;
    case viz::PrimitiveTopology::LINE_STRIP_3D:
      primitive_count = 2;
      data.push_back(-0.1f);
      data.push_back(-0.1f);
      data.push_back(0.1f);
      data.push_back(0.7f);
      data.push_back(0.9f);
      data.push_back(0.3f);

      data.push_back(-0.3f);
      data.push_back(-0.2f);
      data.push_back(0.2f);
      color_crc = 0x6c8cfdee;
      depth_crc = 0xc2d73af1;
      break;
    case viz::PrimitiveTopology::TRIANGLE_LIST_3D:
      primitive_count = 2;
      data.push_back(-0.1f);
      data.push_back(-0.1f);
      data.push_back(0.f);
      data.push_back(0.5f);
      data.push_back(0.9f);
      data.push_back(0.1f);
      data.push_back(0.9f);
      data.push_back(0.1f);
      data.push_back(0.2f);

      data.push_back(-0.05f);
      data.push_back(-0.7f);
      data.push_back(0.3f);
      data.push_back(0.15f);
      data.push_back(0.8f);
      data.push_back(0.2f);
      data.push_back(0.25f);
      data.push_back(0.6f);
      data.push_back(0.5f);
      color_crc = 0xd4cb9a45;
      depth_crc = 0xb45187a8;
      break;
    default:
      EXPECT_TRUE(false) << "Unhandled primitive topoplogy";
  }

  EXPECT_NO_THROW(viz::Begin());

  EXPECT_NO_THROW(viz::BeginGeometryLayer());

  for (uint32_t i = 0; i < 3; ++i) {
    if (i == 1) {
      EXPECT_NO_THROW(viz::Color(1.f, 0.5f, 0.25f, 0.75f));
    } else if (i == 2) {
      EXPECT_NO_THROW(viz::PointSize(4.f));
      EXPECT_NO_THROW(viz::LineWidth(3.f));
    }

    EXPECT_NO_THROW(viz::Primitive(topology, primitive_count, data.size(), data.data()));

    for (auto&& item : data) { item += 0.1f; }
  }
  EXPECT_NO_THROW(viz::EndLayer());

  EXPECT_NO_THROW(viz::End());

  CompareColorResultCRC32({color_crc});
  CompareDepthResultCRC32({depth_crc});
}

INSTANTIATE_TEST_SUITE_P(
    GeometryLayer, PrimitiveTopology,
    testing::Values(viz::PrimitiveTopology::POINT_LIST, viz::PrimitiveTopology::LINE_LIST,
                    viz::PrimitiveTopology::LINE_STRIP, viz::PrimitiveTopology::TRIANGLE_LIST,
                    viz::PrimitiveTopology::CROSS_LIST, viz::PrimitiveTopology::RECTANGLE_LIST,
                    viz::PrimitiveTopology::OVAL_LIST, viz::PrimitiveTopology::POINT_LIST_3D,
                    viz::PrimitiveTopology::LINE_LIST_3D, viz::PrimitiveTopology::LINE_STRIP_3D,
                    viz::PrimitiveTopology::TRIANGLE_LIST_3D));

// Fixture that initializes Holoviz
class GeometryLayer : public TestHeadless {};

TEST_F(GeometryLayer, Text) {
  EXPECT_NO_THROW(viz::Begin());

  EXPECT_NO_THROW(viz::BeginGeometryLayer());
  EXPECT_NO_THROW(viz::Text(0.4f, 0.4f, 0.4f, "Text"));
  EXPECT_NO_THROW(viz::Color(0.5f, 0.9f, 0.7f, 0.9f));
  EXPECT_NO_THROW(viz::Text(0.1f, 0.1f, 0.2f, "Colored"));
  EXPECT_NO_THROW(viz::EndLayer());

  EXPECT_NO_THROW(viz::End());

  CompareColorResultCRC32({0xcb23d3cf});
}

TEST_F(GeometryLayer, TextClipped) {
  EXPECT_NO_THROW(viz::Begin());

  EXPECT_NO_THROW(viz::BeginGeometryLayer());
  EXPECT_NO_THROW(viz::Text(1.1f, 0.4f, 0.4f, "Text"));

  EXPECT_NO_THROW(viz::End());

  CompareColorResultCRC32({0xd8f49994});
}

class GeometryLayerWithFont : public TestHeadless {
 protected:
  void SetUp() override {
    ASSERT_NO_THROW(viz::SetFont("../modules/holoviz/src/fonts/Roboto-Bold.ttf", 12.f));

    // call base class
    ::TestHeadless::SetUp();
  }

  void TearDown() override {
    // call base class
    ::TestHeadless::TearDown();

    ASSERT_NO_THROW(viz::SetFont("", 0.f));
  }
};

TEST_F(GeometryLayerWithFont, Text) {
  EXPECT_NO_THROW(viz::Begin());

  EXPECT_NO_THROW(viz::BeginGeometryLayer());
  EXPECT_NO_THROW(viz::Text(0.1f, 0.1f, 0.7f, "Font"));
  EXPECT_NO_THROW(viz::EndLayer());

  EXPECT_NO_THROW(viz::End());

  CompareColorResultCRC32({0xb149eac7});
}

// Fixture that initializes Holoviz
class DepthMapRenderMode : public TestHeadless,
                           public testing::WithParamInterface<viz::DepthMapRenderMode> {};

TEST_P(DepthMapRenderMode, DepthMap) {
  const viz::DepthMapRenderMode depth_map_render_mode = GetParam();
  const uint32_t map_width = 8;
  const uint32_t map_height = 8;

  // allocate device memory
  viz::CudaService cuda_service(0);
  viz::CudaService::ScopedPush cuda_context = cuda_service.PushContext();

  viz::UniqueCUdeviceptr depth_ptr;
  depth_ptr.reset([this] {
    CUdeviceptr device_ptr;
    EXPECT_EQ(cuMemAlloc(&device_ptr, map_width * map_height * sizeof(uint8_t)), CUDA_SUCCESS);
    return device_ptr;
  }());
  std::vector<uint8_t> depth_data(map_width * map_height);
  for (size_t index = 0; index < depth_data.size(); ++index) { depth_data[index] = index * 4; }
  EXPECT_EQ(cuMemcpyHtoD(depth_ptr.get(), depth_data.data(), depth_data.size()), CUDA_SUCCESS);

  viz::UniqueCUdeviceptr color_ptr;
  color_ptr.reset([this] {
    CUdeviceptr device_ptr;
    EXPECT_EQ(cuMemAlloc(&device_ptr, map_width * map_height * sizeof(uint32_t)), CUDA_SUCCESS);
    return device_ptr;
  }());
  std::vector<uint32_t> color_data(map_width * map_height);
  for (uint32_t index = 0; index < color_data.size(); ++index) {
    color_data[index] = (index << 18) | (index << 9) | index | 0xFF000000;
  }
  EXPECT_EQ(cuMemcpyHtoD(color_ptr.get(), color_data.data(), color_data.size() * sizeof(uint32_t)),
            CUDA_SUCCESS);

  uint32_t crc;
  switch (depth_map_render_mode) {
    case viz::DepthMapRenderMode::POINTS:
      crc = 0xcd990f6d;
      break;
    case viz::DepthMapRenderMode::LINES:
      crc = 0x92a330ea;
      break;
    case viz::DepthMapRenderMode::TRIANGLES:
      crc = 0x97856df3;
      break;
  }
  EXPECT_NO_THROW(viz::Begin());

  EXPECT_NO_THROW(viz::BeginGeometryLayer());
  EXPECT_NO_THROW(viz::DepthMap(depth_map_render_mode,
                                map_width,
                                map_height,
                                viz::ImageFormat::R8_UNORM,
                                depth_ptr.get(),
                                viz::ImageFormat::R8G8B8A8_UNORM,
                                color_ptr.get()));
  EXPECT_NO_THROW(viz::EndLayer());

  EXPECT_NO_THROW(viz::End());

  CompareColorResultCRC32({crc});
}

INSTANTIATE_TEST_SUITE_P(GeometryLayer, DepthMapRenderMode,
                         testing::Values(viz::DepthMapRenderMode::POINTS,
                                         viz::DepthMapRenderMode::LINES,
                                         viz::DepthMapRenderMode::TRIANGLES));

TEST_F(GeometryLayer, Reuse) {
  std::vector<float> data{0.5f, 0.5f};

  for (uint32_t i = 0; i < 2; ++i) {
    EXPECT_NO_THROW(viz::Begin());

    EXPECT_NO_THROW(viz::BeginGeometryLayer());
    EXPECT_NO_THROW(viz::Color(0.1f, 0.2f, 0.3f, 0.4f));
    EXPECT_NO_THROW(viz::LineWidth(2.f));
    EXPECT_NO_THROW(viz::PointSize(3.f));
    EXPECT_NO_THROW(
        viz::Primitive(viz::PrimitiveTopology::POINT_LIST, 1, data.size(), data.data()));
    EXPECT_NO_THROW(viz::Text(0.4f, 0.4f, 0.1f, "Text"));
    EXPECT_NO_THROW(viz::EndLayer());

    EXPECT_NO_THROW(viz::End());
  }
}

TEST_F(GeometryLayer, Errors) {
  std::vector<float> data{0.5f, 0.5f};

  EXPECT_NO_THROW(viz::Begin());

  // it's an error to call geometry functions without calling BeginGeometryLayer first
  EXPECT_THROW(viz::Color(0.f, 0.f, 0.f, 1.f), std::runtime_error);
  EXPECT_THROW(viz::LineWidth(1.0f), std::runtime_error);
  EXPECT_THROW(viz::PointSize(1.0f), std::runtime_error);
  EXPECT_THROW(viz::Primitive(viz::PrimitiveTopology::POINT_LIST, 1, data.size(), data.data()),
               std::runtime_error);
  EXPECT_THROW(viz::Text(0.5f, 0.5f, 0.1f, "Text"), std::runtime_error);

  // it's an error to call BeginGeometryLayer again without calling EndLayer
  EXPECT_NO_THROW(viz::BeginGeometryLayer());
  EXPECT_THROW(viz::BeginGeometryLayer(), std::runtime_error);
  EXPECT_NO_THROW(viz::EndLayer());

  // it's an error to call geometry functions when a different layer is active
  EXPECT_NO_THROW(viz::BeginImageLayer());
  EXPECT_THROW(viz::Color(0.f, 0.f, 0.f, 1.f), std::runtime_error);
  EXPECT_THROW(viz::LineWidth(1.0f), std::runtime_error);
  EXPECT_THROW(viz::PointSize(1.0f), std::runtime_error);
  EXPECT_THROW(viz::Primitive(viz::PrimitiveTopology::POINT_LIST, 1, data.size(), data.data()),
               std::runtime_error);
  EXPECT_THROW(viz::Text(0.5f, 0.5f, 0.1f, "Text"), std::runtime_error);
  EXPECT_NO_THROW(viz::EndLayer());

  EXPECT_NO_THROW(viz::BeginGeometryLayer());

  struct {
    viz::PrimitiveTopology topology;
    uint32_t values;
  } required[] = {
      {viz::PrimitiveTopology::POINT_LIST, 2},
      {viz::PrimitiveTopology::LINE_LIST, 4},
      {viz::PrimitiveTopology::LINE_STRIP, 4},
      {viz::PrimitiveTopology::TRIANGLE_LIST, 6},
      {viz::PrimitiveTopology::CROSS_LIST, 3},
      {viz::PrimitiveTopology::RECTANGLE_LIST, 4},
      {viz::PrimitiveTopology::OVAL_LIST, 4},
      {viz::PrimitiveTopology::POINT_LIST_3D, 3},
      {viz::PrimitiveTopology::LINE_LIST_3D, 6},
      {viz::PrimitiveTopology::LINE_STRIP_3D, 6},
      {viz::PrimitiveTopology::TRIANGLE_LIST_3D, 9},
  };

  for (auto&& cur : required) {
    std::vector<float> data(cur.values, 0.f);
    // Primitive function errors, first call the passing function
    EXPECT_NO_THROW(viz::Primitive(cur.topology, 1, data.size(), data.data()));
    // it's an error to call Primitive with a data size which is too small for the primitive count
    EXPECT_THROW(viz::Primitive(cur.topology, 1, data.size() - 1, data.data()), std::runtime_error);
  }

  // it's an error to call Primitive with a primitive count of zero
  EXPECT_THROW(viz::Primitive(viz::PrimitiveTopology::POINT_LIST, 0, data.size(), data.data()),
               std::invalid_argument);
  // it's an error to call Primitive with a data size of zero
  EXPECT_THROW(viz::Primitive(viz::PrimitiveTopology::POINT_LIST, 1, 0, data.data()),
               std::invalid_argument);
  // it's an error to call Primitive with a null data pointer
  EXPECT_THROW(viz::Primitive(viz::PrimitiveTopology::POINT_LIST, 1, data.size(), nullptr),
               std::invalid_argument);

  // Text function errors, first call the passing function
  EXPECT_NO_THROW(viz::Text(0.5f, 0.5f, 0.1f, "Text"));
  // it's an error to call Text with a size of zero
  EXPECT_THROW(viz::Text(0.5f, 0.5f, 0.0f, "Text"), std::invalid_argument);
  // it's an error to call Text with null text pointer
  EXPECT_THROW(viz::Text(0.5f, 0.5f, 0.1f, nullptr), std::invalid_argument);

  // Depth map function errors, first call the passing function
  const uint32_t map_width = 8;
  const uint32_t map_height = 8;

  // allocate device memory
  viz::CudaService cuda_service(0);
  viz::CudaService::ScopedPush cuda_context = cuda_service.PushContext();
  viz::UniqueCUdeviceptr depth_ptr;
  depth_ptr.reset([this] {
    CUdeviceptr device_ptr;
    EXPECT_EQ(cuMemAlloc(&device_ptr, map_width * map_height * sizeof(uint8_t)), CUDA_SUCCESS);
    return device_ptr;
  }());
  viz::UniqueCUdeviceptr color_ptr;
  color_ptr.reset([this] {
    CUdeviceptr device_ptr;
    EXPECT_EQ(cuMemAlloc(&device_ptr, map_width * map_height * sizeof(uint32_t)), CUDA_SUCCESS);
    return device_ptr;
  }());

  // First call the passing function
  EXPECT_NO_THROW(viz::DepthMap(viz::DepthMapRenderMode::POINTS,
                                map_width,
                                map_height,
                                viz::ImageFormat::R8_UNORM,
                                depth_ptr.get(),
                                viz::ImageFormat::R8G8B8A8_UNORM,
                                color_ptr.get()));
  // it's an error to call DepthMap with a width of zero
  EXPECT_THROW(viz::DepthMap(viz::DepthMapRenderMode::POINTS,
                             0,
                             map_height,
                             viz::ImageFormat::R8_UNORM,
                             depth_ptr.get(),
                             viz::ImageFormat::R8G8B8A8_UNORM,
                             color_ptr.get()),
               std::invalid_argument);
  // it's an error to call DepthMap with a width of zero
  EXPECT_THROW(viz::DepthMap(viz::DepthMapRenderMode::POINTS,
                             map_width,
                             0,
                             viz::ImageFormat::R8_UNORM,
                             depth_ptr.get(),
                             viz::ImageFormat::R8G8B8A8_UNORM,
                             color_ptr.get()),
               std::invalid_argument);
  // it's an error to call DepthMap with a depth format other than viz::ImageFormat::R8_UINT
  EXPECT_THROW(viz::DepthMap(viz::DepthMapRenderMode::POINTS,
                             map_width,
                             map_height,
                             viz::ImageFormat::R16_UNORM,
                             depth_ptr.get(),
                             viz::ImageFormat::R8G8B8A8_UNORM,
                             color_ptr.get()),
               std::invalid_argument);
  // it's an error to call DepthMap with no depth map pointer
  EXPECT_THROW(viz::DepthMap(viz::DepthMapRenderMode::POINTS,
                             map_width,
                             map_height,
                             viz::ImageFormat::R8_UNORM,
                             0,
                             viz::ImageFormat::R8G8B8A8_UNORM,
                             color_ptr.get()),
               std::invalid_argument);

  EXPECT_NO_THROW(viz::EndLayer());

  EXPECT_NO_THROW(viz::End());
}
