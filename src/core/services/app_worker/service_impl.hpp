/*
 * SPDX-FileCopyrightText: Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#ifndef CORE_SERVICES_APP_WORKER_SERVICE_IMPL_HPP
#define CORE_SERVICES_APP_WORKER_SERVICE_IMPL_HPP

#include <grpcpp/grpcpp.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "../generated/app_worker.grpc.pb.h"

namespace holoscan {

// Forward declarations
class AppWorker;

namespace service {

class AppWorkerServiceImpl final : public AppWorkerService::Service {
 public:
  explicit AppWorkerServiceImpl(holoscan::AppWorker* app_worker);

  grpc::Status GetAvailablePorts(grpc::ServerContext* context,
                              const holoscan::service::AvailablePortsRequest* request,
                              holoscan::service::AvailablePortsResponse* response) override;

  grpc::Status ExecuteFragments(grpc::ServerContext* context,
                                 const holoscan::service::FragmentExecutionRequest* request,
                                 holoscan::service::FragmentExecutionResponse* response) override;

  grpc::Status TerminateWorker(grpc::ServerContext* context,
                               const holoscan::service::TerminateWorkerRequest* request,
                               holoscan::service::TerminateWorkerResponse* response) override;

 private:
  holoscan::AppWorker* app_worker_ = nullptr;
};

}  // namespace service
}  // namespace holoscan

#endif /* CORE_SERVICES_APP_WORKER_SERVICE_IMPL_HPP */
