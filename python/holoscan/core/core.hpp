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

#ifndef PYBIND11_CORE_CORE_HPP
#define PYBIND11_CORE_CORE_HPP

#include <pybind11/pybind11.h>

#include <memory>
#include <unordered_map>
#include <string>
#include <vector>

#include "holoscan/core/arg.hpp"
#include "holoscan/core/domain/tensor.hpp"
#include "holoscan/core/io_spec.hpp"

#define STRINGIFY(x) #x
#define MACRO_STRINGIFY(x) STRINGIFY(x)

namespace py = pybind11;

namespace holoscan {

static const std::unordered_map<ArgElementType, const char*> element_type_namemap{
    {ArgElementType::kCustom, "CUSTOM"},
    {ArgElementType::kBoolean, "BOOLEAN"},
    {ArgElementType::kInt8, "INT8"},
    {ArgElementType::kUnsigned8, "UNSIGNED8"},
    {ArgElementType::kInt16, "INT16"},
    {ArgElementType::kUnsigned16, "UNSIGNED16"},
    {ArgElementType::kInt32, "INT32"},
    {ArgElementType::kUnsigned32, "UNSIGNED32"},
    {ArgElementType::kInt64, "INT64"},
    {ArgElementType::kUnsigned64, "UNSIGNED64"},
    {ArgElementType::kFloat32, "FLOAT32"},
    {ArgElementType::kFloat64, "FLOAT64"},
    {ArgElementType::kString, "STRING"},
    {ArgElementType::kHandle, "HANDLE"},
    {ArgElementType::kYAMLNode, "YAML_NODE"},
    {ArgElementType::kIOSpec, "IO_SPEC"},
    {ArgElementType::kCondition, "CONDITION"},
    {ArgElementType::kResource, "RESOURCE"},
};

static const std::unordered_map<ArgContainerType, const char*> container_type_namemap{
    {ArgContainerType::kNative, "NATIVE"},
    {ArgContainerType::kVector, "VECTOR"},
    {ArgContainerType::kArray, "ARRAY"},
};

static const std::unordered_map<IOSpec::IOType, const char*> io_type_namemap{
    {IOSpec::IOType::kInput, "INPUT"},
    {IOSpec::IOType::kOutput, "OUTPUT"},
};

static const std::unordered_map<IOSpec::ConnectorType, const char*> connector_type_namemap{
    {IOSpec::ConnectorType::kDefault, "DEFAULT"},
    {IOSpec::ConnectorType::kDoubleBuffer, "DOUBLE_BUFFER"},
    {IOSpec::ConnectorType::kUCX, "UCX"},
};

static const std::unordered_map<DLDataTypeCode, const char*> dldatatypecode_namemap{
    {kDLInt, "DLINT"},
    {kDLUInt, "DLUINT"},
    {kDLFloat, "DLFLOAT"},
    {kDLOpaqueHandle, "DLOPAQUEHANDLE"},
    {kDLBfloat, "DLBFLOAT"},
    {kDLComplex, "DLCOMPLEX"},
};

/**
 * @brief Class to wrap the deleter of a DLManagedTensor in Python.
 *
 * This class is used with DLManagedTensorCtx class to wrap the DLManagedTensor.
 *
 * A shared pointer to this class in DLManagedTensorCtx class is used as the deleter of the
 * DLManagedTensorCtx::memory_ref
 *
 * When the last reference to the DLManagedTensorCtx object is released,
 * DLManagedTensorCtx::memory_ref will also be destroyed, which will call the deleter function
 * of the DLManagedTensor object.
 *
 * Compared to the C++ version (DLManagedMemoryBuffer), this class is used to acquire the GIL
 * before calling the deleter function.
 *
 */
class PyDLManagedMemoryBuffer {
 public:
  explicit PyDLManagedMemoryBuffer(DLManagedTensor* self);
  ~PyDLManagedMemoryBuffer();

 private:
  DLManagedTensor* self_ = nullptr;
};

class PyTensor : public Tensor {
 public:
  /**
   * @brief Construct a new Tensor from an existing DLManagedTensorCtx.
   *
   * @param ctx A shared pointer to the DLManagedTensorCtx to be used in Tensor construction.
   */
  explicit PyTensor(std::shared_ptr<DLManagedTensorCtx>& ctx);

  /**
   * @brief Construct a new Tensor from an existing DLManagedTensor pointer.
   *
   * @param ctx A pointer to the DLManagedTensor to be used in Tensor construction.
   */
  explicit PyTensor(DLManagedTensor* dl_managed_tensor_ptr);

  PyTensor() = default;

  /**
   * @brief Create a new Tensor object from a py::object
   *
   * The given py::object must support the array interface protocol or dlpack protocol.
   *
   * @param obj A py::object that can be converted to a Tensor
   * @return A new Tensor object
   */
  static py::object as_tensor(const py::object& obj);
  static std::shared_ptr<PyTensor> from_array_interface(const py::object& obj);
  static std::shared_ptr<PyTensor> from_cuda_array_interface(const py::object& obj);
  static std::shared_ptr<PyTensor> from_dlpack(const py::object& obj);
  static py::capsule dlpack(const py::object& obj, py::object stream);
  static py::tuple dlpack_device(const py::object& obj);
};

template <typename ObjT>
std::vector<std::string> get_names_from_map(ObjT& map_obj) {
  std::vector<std::string> names;
  names.reserve(map_obj.size());
  for (auto& i : map_obj) { names.push_back(i.first); }
  return names;
}

}  // namespace holoscan

#endif /* PYBIND11_CORE_CORE_HPP */
