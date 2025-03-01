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

#include "core.hpp"

#include <NvInferPlugin.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace holoscan {
namespace inference {

TrtInfer::TrtInfer(const std::string& model_path, const std::string& model_name, int device_id,
                   bool enable_fp16, bool is_engine_path, bool cuda_buf_in, bool cuda_buf_out)
    : model_path_(model_path),
      model_name_(model_name),
      device_id_(device_id),
      enable_fp16_(enable_fp16),
      is_engine_path_(is_engine_path),
      cuda_buf_in_(cuda_buf_in),
      cuda_buf_out_(cuda_buf_out) {
  // Set the device index
  network_options_.device_index = device_id_;

  network_options_.use_fp16 = enable_fp16_;
  initLibNvInferPlugins(nullptr, "");

  if (!is_engine_path_) {
    HOLOSCAN_LOG_INFO("TRT Inference: converting ONNX model at {}", model_path_);

    bool status = generate_engine_path(network_options_, model_path_, engine_path_);
    if (!status) { throw std::runtime_error("TRT Inference: could not generate TRT engine path."); }

    status = build_engine(model_path_, engine_path_, network_options_, logger_);
    if (!status) {
      HOLOSCAN_LOG_ERROR("Engine file creation failed for {}", model_path_);
      HOLOSCAN_LOG_INFO(
          "If the input path {} is an engine file, set 'is_engine_path' parameter to true in "
          "the inference settings in the application config.",
          model_path_);
      throw std::runtime_error("TRT Inference: failed to build TRT engine file.");
    }
  } else {
    engine_path_ = model_path_;
  }

  bool status = load_engine();
  if (!status) { throw std::runtime_error("TRT Inference: failed to load TRT engine file."); }

  status = initialize_parameters();
  if (!status) { throw std::runtime_error("TRT Inference: Initialization error."); }
}

TrtInfer::~TrtInfer() {
  if (context_) { context_.reset(); }
  if (engine_) { engine_.reset(); }
  if (cuda_stream_) { cudaStreamDestroy(cuda_stream_); }
  if (infer_runtime_) { infer_runtime_.reset(); }
}

bool TrtInfer::load_engine() {
  HOLOSCAN_LOG_INFO("Loading Engine: {}", engine_path_);
  std::ifstream file(engine_path_, std::ios::binary | std::ios::ate);
  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);

  std::vector<char> buffer(size);
  if (!file.read(buffer.data(), size)) {
    HOLOSCAN_LOG_ERROR("Load Engine: File read error: {}", engine_path_);
    return false;
  }

  infer_runtime_.reset(nvinfer1::createInferRuntime(logger_));
  if (!infer_runtime_) {
    HOLOSCAN_LOG_ERROR("Load Engine: Error in creating inference runtime.");
    return false;
  }

  // Set the device index
  auto status = cudaSetDevice(network_options_.device_index);
  if (status != 0) {
    HOLOSCAN_LOG_ERROR("Load Engine: Setting cuda device failed.");
    throw std::runtime_error("Error setting cuda device in load engine.");
  }

  engine_ = std::unique_ptr<nvinfer1::ICudaEngine>(
      infer_runtime_->deserializeCudaEngine(buffer.data(), buffer.size()));
  if (!engine_) {
    HOLOSCAN_LOG_ERROR("Load Engine: Error in deserializing cuda engine.");
    return false;
  }

  context_ = std::unique_ptr<nvinfer1::IExecutionContext>(engine_->createExecutionContext());
  if (!context_) {
    HOLOSCAN_LOG_ERROR("Load Engine: Error in creating execution context.");
    return false;
  }

  status = cudaStreamCreate(&cuda_stream_);
  if (status != 0) {
    HOLOSCAN_LOG_ERROR("Load Engine: Cuda stream creation failed.");
    throw std::runtime_error("Unable to create cuda stream");
  }

  HOLOSCAN_LOG_INFO("Engine loaded: {}", engine_path_);
  return true;
}

std::vector<std::vector<int64_t>> TrtInfer::get_input_dims() const {
  return input_dims_;
}

std::vector<std::vector<int64_t>> TrtInfer::get_output_dims() const {
  return output_dims_;
}

std::vector<holoinfer_datatype> TrtInfer::get_input_datatype() const {
  return in_data_types_;
}

std::vector<holoinfer_datatype> TrtInfer::get_output_datatype() const {
  return out_data_types_;
}

bool TrtInfer::initialize_parameters() {
  if (!engine_) {
    HOLOSCAN_LOG_ERROR("Engine is Null.");
    return false;
  }
  const int num_bindings = engine_->getNbBindings();

  for (int i = 0; i < num_bindings; i++) {
    auto dims = engine_->getBindingDimensions(i);
    auto data_type = engine_->getBindingDataType(i);

    holoinfer_datatype holoinfer_type = holoinfer_datatype::h_Float32;
    switch (data_type) {
      case nvinfer1::DataType::kFLOAT: {
        holoinfer_type = holoinfer_datatype::h_Float32;
        break;
      }
      case nvinfer1::DataType::kINT32: {
        holoinfer_type = holoinfer_datatype::h_Int32;
        break;
      }
      case nvinfer1::DataType::kINT8: {
        holoinfer_type = holoinfer_datatype::h_Int8;
        break;
      }
      default: {
        HOLOSCAN_LOG_ERROR("Data type not supported.");
        return false;
      }
    }

    if (engine_->bindingIsInput(i)) {
      switch (dims.nbDims) {
        case 2: {
          nvinfer1::Dims2 in_dimensions = {dims.d[0], dims.d[1]};
          context_->setBindingDimensions(i, in_dimensions);
        } break;
        case 3: {
          nvinfer1::Dims3 in_dimensions = {dims.d[0], dims.d[1], dims.d[2]};
          context_->setBindingDimensions(i, in_dimensions);
        } break;
        case 4: {
          nvinfer1::Dims4 in_dimensions = {dims.d[0], dims.d[1], dims.d[2], dims.d[3]};
          context_->setBindingDimensions(i, in_dimensions);
        } break;
        default: {
          throw std::runtime_error("Dimension size not supported: " + std::to_string(dims.nbDims));
        }
      }

      std::vector<int64_t> indim;
      for (size_t in = 0; in < dims.nbDims; in++) { indim.push_back(dims.d[in]); }
      input_dims_.push_back(std::move(indim));

      in_data_types_.push_back(holoinfer_type);
    } else {
      std::vector<int64_t> outdim;
      for (size_t in = 0; in < dims.nbDims; in++) { outdim.push_back(dims.d[in]); }

      output_dims_.push_back(outdim);
      out_data_types_.push_back(holoinfer_type);
    }
  }

  if (!context_->allInputDimensionsSpecified()) {
    throw std::runtime_error("Error, not all input dimensions specified.");
  }

  return true;
}

InferStatus TrtInfer::do_inference(const std::vector<std::shared_ptr<DataBuffer>>& input_buffers,
                                   std::vector<std::shared_ptr<DataBuffer>>& output_buffers) {
  InferStatus status = InferStatus(holoinfer_code::H_ERROR);
  std::vector<void*> cuda_buffers;

  for (auto& input_buffer : input_buffers) {
    if (input_buffer->device_buffer == nullptr) {
      status.set_message(" TRT inference core: Input Device buffer is null.");
      return status;
    }

    if (input_buffer->device_buffer->data() == nullptr) {
      status.set_message(" TRT inference core: Data in Input Device buffer is null.");
      return status;
    }

    //  Host to Device transfer
    if (!cuda_buf_in_) {
      if (input_buffer->host_buffer.size() == 0) {
        status.set_message(" TRT inference core: Empty input host buffer.");
        return status;
      }
      if (input_buffer->device_buffer->size() != input_buffer->host_buffer.size()) {
        status.set_message(" TRT inference core: Input Host and Device buffer size mismatch.");
        return status;
      }
      auto cstatus = cudaMemcpyAsync(input_buffer->device_buffer->data(),
                                     input_buffer->host_buffer.data(),
                                     input_buffer->device_buffer->get_bytes(),
                                     cudaMemcpyHostToDevice,
                                     cuda_stream_);
      if (cstatus != cudaSuccess) {
        status.set_message(" TRT inference core: Host to device transfer failed.");
        return status;
      }
      cstatus = cudaStreamSynchronize(cuda_stream_);
      if (cstatus != cudaSuccess) {
        status.set_message(" TRT: Cuda stream synchronization failed");
        return status;
      }
      if (input_buffer->device_buffer->size() == 0) {
        status.set_message(" TRT inference core: Input Device buffer size is 0.");
        return status;
      }
    }
    cuda_buffers.push_back(input_buffer->device_buffer->data());
  }

  for (auto& output_buffer : output_buffers) {
    if (output_buffer->device_buffer == nullptr) {
      status.set_message(" TRT inference core: Output Device buffer is null.");
      return status;
    }
    if (output_buffer->device_buffer->data() == nullptr) {
      status.set_message(" TRT inference core: Data in Output Device buffer is null.");
      return status;
    }

    if (output_buffer->device_buffer->size() == 0) {
      status.set_message(" TRT inference core: Output Device buffer size is 0.");
      return status;
    }
    cuda_buffers.push_back(output_buffer->device_buffer->data());
  }

  auto inference_bindings = std::make_shared<std::vector<void*>>(cuda_buffers);
  bool infer_status = false;
  if (!engine_->hasImplicitBatchDimension()) {
    infer_status = context_->enqueueV2(inference_bindings->data(), cuda_stream_, nullptr);
  } else {
    infer_status = context_->enqueue(1, inference_bindings->data(), cuda_stream_, nullptr);
  }

  if (!infer_status) {
    status.set_message(" TRT inference core: Inference failure.");
    return status;
  }

  if (!cuda_buf_out_) {
    for (auto& output_buffer : output_buffers) {
      if (output_buffer->host_buffer.size() == 0) {
        status.set_message(" TRT inference core: Empty output host buffer.");
        return status;
      }
      if (output_buffer->device_buffer->size() != output_buffer->host_buffer.size()) {
        status.set_message(" TRT inference core: Output Host and Device buffer size mismatch.");
        return status;
      }
      // Copy the results back to CPU memory
      auto cstatus = cudaMemcpyAsync(output_buffer->host_buffer.data(),
                                     output_buffer->device_buffer->data(),
                                     output_buffer->device_buffer->get_bytes(),
                                     cudaMemcpyDeviceToHost,
                                     cuda_stream_);
      if (cstatus != cudaSuccess) {
        status.set_message(" TRT: Device to host transfer failed");
        return status;
      }
      cstatus = cudaStreamSynchronize(cuda_stream_);
      if (cstatus != cudaSuccess) {
        status.set_message(" TRT: Cuda stream synchronization failed");
        return status;
      }
    }
  }

  auto cstatus = cudaStreamSynchronize(cuda_stream_);
  if (cstatus != cudaSuccess) {
    status.set_message(" TRT: Cuda stream synchronization failed");
    return status;
  }
  return InferStatus();
}

}  // namespace inference
}  // namespace holoscan
