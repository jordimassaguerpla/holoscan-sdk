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

#include "holoscan/core/app_driver.hpp"

#include <stdlib.h>  // POSIX setenv

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <deque>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "gxf/std/system.hpp"  // for stopping the GXF scheduler
#include "holoscan/core/app_worker.hpp"
#include "holoscan/core/application.hpp"
#include "holoscan/core/cli_options.hpp"
#include "holoscan/core/executors/gxf/gxf_executor.hpp"
#include "holoscan/core/fragment.hpp"
#include "holoscan/core/graph.hpp"  // for FragmentNodeType
#include "holoscan/core/gxf/gxf_resource.hpp"
#include "holoscan/core/network_contexts/gxf/ucx_context.hpp"
#include "holoscan/core/schedulers/greedy_fragment_allocation.hpp"
#include "holoscan/core/schedulers/gxf/greedy_scheduler.hpp"
#include "holoscan/core/schedulers/gxf/multithread_scheduler.hpp"
#include "holoscan/core/services/app_driver/server.hpp"
#include "holoscan/core/services/app_worker/server.hpp"
#include "holoscan/core/services/common/network_constants.hpp"
#include "holoscan/core/signal_handler.hpp"
#include "holoscan/core/system/network_utils.hpp"
#include <holoscan/core/system/system_resource_manager.hpp>
#include "services/app_worker/client.hpp"

#include "holoscan/logger/logger.hpp"

namespace holoscan {

bool AppDriver::get_bool_env_var(const char* name, bool default_value) {
  const char* env_value = std::getenv(name);

  if (env_value) {
    std::string value(env_value);
    std::transform(
        value.begin(), value.end(), value.begin(), [](unsigned char c) { return std::tolower(c); });

    if (value == "true" || value == "1" || value == "on") { return true; }
  }

  return default_value;
}

int64_t AppDriver::get_int_env_var(const char* name, int64_t default_value) {
  const char* env_value = std::getenv(name);

  if (env_value) {
    try {
      return std::stoll(env_value);
    } catch (std::exception& e) {
      HOLOSCAN_LOG_WARN("Unable to interpret environment variable '{}': '{}'", name, e.what());
    }
  }

  return default_value;
}

void AppDriver::set_ucx_to_exclude_cuda_ipc() {
  const char* env_value = std::getenv("UCX_TLS");
  if (env_value) {
    // warn if cuda_ipc is allowed
    std::string tls_str{env_value};
    bool is_deny_list = tls_str.at(0) == '^';
    bool cuda_ipc_found = tls_str.find("cuda_ipc") != std::string::npos;
    // Expect either a deny list containing cuda_ipc or an allow list without cuda_ipc.
    // If this is not the case, warn.
    if (!((is_deny_list && cuda_ipc_found) || (!is_deny_list && !cuda_ipc_found))) {
      HOLOSCAN_LOG_WARN(
          "UCX_TLS already defined. To use UCX for GPU transport on iGPU systems, cuda_ipc "
          "transport should be disabled (e.g. UCX_TLS=^cuda_ipc)");
    }
  } else {
    // set cuda_ipc as disallowed
    setenv("UCX_TLS", "^cuda_ipc", 0);
  }
}

void AppDriver::exclude_cuda_ipc_transport_on_igpu() {
  holoscan::GPUResourceMonitor gpu_resource_monitor;
  gpu_resource_monitor.update();
  bool is_integrated =
      (gpu_resource_monitor.num_gpus() > 0) && gpu_resource_monitor.is_integrated_gpu(0);
  if (is_integrated) { set_ucx_to_exclude_cuda_ipc(); }
}

uint64_t AppDriver::parse_memory_size(const std::string& size_str) {
  try {
    std::size_t dot_pivot = size_str.find_first_of(".");
    std::size_t unit_pivot = size_str.find_first_not_of("0123456789.");
    std::size_t pivot = size_str.find_first_of("MmGg", unit_pivot);
    std::size_t pivot2 = size_str.find_first_of("Ii", pivot + 1);

    if ((pivot != std::string::npos &&
         (pivot2 != pivot + 1 || size_str.size() >= pivot2 + 2 || unit_pivot != pivot)) ||
        (pivot == std::string::npos &&
         (unit_pivot != std::string::npos && unit_pivot != size_str.size())) ||
        (dot_pivot != std::string::npos &&
         (unit_pivot == std::string::npos ||
          (unit_pivot != std::string::npos && dot_pivot + 2 != unit_pivot)))) {
      HOLOSCAN_LOG_WARN(
          "Unable to interpret memory size (the unit should be Mi or Gi, and the value should be a "
          "non-negative decimal with an optional precision of one decimal place): '{}'",
          size_str);
      return 0;
    }
    uint64_t size = 0;
    // Gi to Mi, Mi to Ki
    if (dot_pivot != std::string::npos) {
      size = std::stod(size_str.substr(0, unit_pivot)) * 1024;
    } else {
      size = std::stoull(size_str.substr(0, unit_pivot)) * 1024;
    }

    if (pivot != std::string::npos && std::tolower(size_str[pivot]) == 'g') {
      size *= 1024 * 1024;  // Mi to bytes
    } else {
      size *= 1024;  // Ki to bytes
    }
    return static_cast<uint64_t>(size);
  } catch (std::exception& e) {
    HOLOSCAN_LOG_WARN("Unable to interpret memory size: '{}'", size_str);
    return 0;
  }
}

AppDriver::AppDriver(Application* app) : app_(app) {
  if (app_) {
    app_status_ = AppStatus::kNotStarted;
    options_ = &app_->cli_parser_.options();
  }
}

AppDriver::~AppDriver() = default;

void AppDriver::run() {
  // Compose graph and setup configuration
  if (!check_configuration()) {
    HOLOSCAN_LOG_ERROR("Application configuration is invalid");
    return;
  }

  launch_app_driver();

  auto& fragment_graph = app_->fragment_graph();

  // If there are no separate fragments to run, we run the entire graph.
  if (fragment_graph.is_empty()) {
    // Application's graph is already composed in check_configuration() so we can run it directly.
    app_->executor().run(app_->graph());
    // Stop the driver server if it is running.
    if (driver_server_) {
      driver_server_->stop();
      driver_server_->wait();
      driver_server_ = nullptr;
    }
    return;
  }

  // If the application is not running in driver/worker mode, we run the application graph directly.
  if (is_local_) {
    auto target_fragments = fragment_graph.get_nodes();
    auto future = launch_fragments_async(target_fragments);
    future.wait();
    return;
  } else {
    if (need_worker_) {
      launch_app_worker();
      auto worker_server = app_->worker().server();

      // Get parameters for connecting to the driver
      auto max_connection_retry_count = get_int_env_var("HOLOSCAN_MAX_CONNECTION_RETRY_COUNT",
                                                        service::kDefaultMaxConnectionRetryCount);
      auto connection_retry_interval_ms = get_int_env_var(
          "HOLOSCAN_CONNECTION_RETRY_INTERVAL_MS", service::kDefaultConnectionRetryIntervalMs);

      // Connect to the driver
      bool connection_result = worker_server->connect_to_driver(max_connection_retry_count,
                                                                connection_retry_interval_ms);
      if (connection_result) {
        HOLOSCAN_LOG_INFO("Connected to driver");
        // Install signal handler for app worker
        auto sig_handler = [this](void* context, int signum) {
          (void)signum;
          (void)context;

          HOLOSCAN_LOG_ERROR("Interrupted by user for app worker");

          auto worker_server = app_->worker().server();
          // Notify the driver that the worker is cancelled
          worker_server->notify_worker_execution_finished(AppWorkerTerminationCode::kCancelled);
          // Stop the worker server
          worker_server->stop();
          // Do not wait for the worker server to stop because it will block the main thread
        };
        SignalHandler::register_signal_handler(app_->executor().context(), SIGINT, sig_handler);
        SignalHandler::register_signal_handler(app_->executor().context(), SIGTERM, sig_handler);

        worker_server->wait();
      } else {
        HOLOSCAN_LOG_ERROR("Failed to connect to driver");
        worker_server->stop();
        worker_server->wait();
        // Stop the driver server if it is running because the worker cannot connect to the driver
        // and the driver server will block the main thread.
        if (driver_server_) { driver_server_->stop(); }
      }
      if (!need_driver_ && driver_server_) {
        // Stop the driver server if it is running as a health checking service.
        driver_server_->stop();
        driver_server_->wait();
      }
    } else {
      // Install signal handler for app driver
      auto sig_handler = [this](void* context, int signum) {
        (void)signum;
        (void)context;

        HOLOSCAN_LOG_ERROR("Interrupted by user for app driver");

        // If the app is already in error state, we set global signal handler.
        if (app_status_ == AppStatus::kError) {
          HOLOSCAN_LOG_ERROR("Send interrupt once more to terminate immediately");
          SignalHandler::unregister_signal_handler(context, signum);
          // Register the global signal handler.
          SignalHandler::register_global_signal_handler(signum, [](int signum) {
            (void)signum;
            HOLOSCAN_LOG_ERROR("Interrupted by user (global signal handler)");
            exit(1);
          });

          return;
        }

        // Terminate all worker and close all worker connections
        terminate_all_workers(AppWorkerTerminationCode::kCancelled);

        // Set app status to error
        app_status_ = AppStatus::kError;

        // Stop the driver server
        driver_server_->stop();
        // Do not wait for the driver server to stop because it will block the main thread
      };
      SignalHandler::register_signal_handler(app_->executor().context(), SIGINT, sig_handler);
      SignalHandler::register_signal_handler(app_->executor().context(), SIGTERM, sig_handler);
    }
    driver_server_->wait();
  }
}

std::future<void> AppDriver::run_async() {
  if (!check_configuration()) {
    HOLOSCAN_LOG_ERROR("Application configuration is invalid");
    return std::async(std::launch::async, []() {});
  }

  launch_app_driver();

  auto& fragment_graph = app_->fragment_graph();

  // If the application is not running in driver/worker mode, we run the application graph directly.
  if (is_local_) {
    // If there are no separate fragments to run, we run the entire graph.
    if (fragment_graph.is_empty()) {
      if (driver_server_) {
        auto executor_future = app_->executor().run_async(app_->graph());
        auto future =
            std::async(std::launch::async,
                       [future = std::move(executor_future), &driver_server = driver_server_]() {
                         future.wait();
                         if (driver_server) {
                           driver_server->stop();
                           driver_server->wait();
                           driver_server = nullptr;
                         }
                       });
        return future;
      }
      // Application's graph is already composed in check_configuration() so we can run it directly.
      return app_->executor().run_async(app_->graph());
    }

    auto target_fragments = fragment_graph.get_nodes();
    auto future = launch_fragments_async(target_fragments);
    return future;
  }

  return std::async(std::launch::async, []() {});
}

AppDriver::AppStatus AppDriver::status() {
  return app_status_;
}

CLIOptions* AppDriver::options() {
  if (app_ == nullptr) { return nullptr; }
  return options_;
}

FragmentScheduler* AppDriver::fragment_scheduler() {
  if (fragment_scheduler_) { return fragment_scheduler_.get(); }
  return nullptr;
}

void AppDriver::submit_message(DriverMessage&& message) {
  std::lock_guard<std::mutex> lock(message_mutex_);
  message_queue_.push(std::move(message));

  // Notify the driver server to process the message queue
  driver_server_->notify();
}

void AppDriver::process_message_queue() {
  std::lock_guard<std::mutex> lock(message_mutex_);

  while (!message_queue_.empty()) {
    auto message = std::move(message_queue_.front());
    message_queue_.pop();

    // Process message based on message code
    auto message_code = message.code;
    switch (message_code) {
      case DriverMessageCode::kCheckFragmentSchedule:
        try {
          std::string worker_address = std::any_cast<std::string>(message.data);
          check_fragment_schedule(worker_address);
        } catch (std::bad_any_cast& e) {
          HOLOSCAN_LOG_ERROR("Failed to cast message data to std::string: {}", e.what());
        }
        break;
      case DriverMessageCode::kWorkerExecutionFinished:
        try {
          check_worker_execution(std::any_cast<AppWorkerTerminationStatus>(message.data));
        } catch (std::bad_any_cast& e) {
          HOLOSCAN_LOG_ERROR("Failed to cast message data to AppWorkerTerminationStatus: {}",
                             e.what());
        }
        break;
      case DriverMessageCode::kTerminateServer:
        HOLOSCAN_LOG_DEBUG(
            "Terminating the driver server (DriverMessageCode::kTerminateServer received)");
        driver_server_->stop();
        // Do not call 'driver_server_->wait()' as current thread is the driver server thread
        break;
      default:
        HOLOSCAN_LOG_WARN("Unknown message code: {}", static_cast<int>(message_code));
        break;
    }
  }
}

bool AppDriver::update_port_names(
    holoscan::FragmentNodeType src_frag, holoscan::FragmentNodeType target_frag,
    std::shared_ptr<holoscan::FragmentEdgeDataElementType>& port_map) {
  auto& src_frag_graph = src_frag->graph();
  auto& target_frag_graph = target_frag->graph();

  std::vector<std::pair<std::string, std::string>> corrected_name_pair;
  // Collect corrected port names
  for (auto& [source_op_port, target_op_ports] : *port_map) {
    auto [src_operator_name, src_port_name] = Operator::parse_port_name(source_op_port);
    auto& src_op_name = src_operator_name;

    auto src_op = src_frag_graph.find_node(src_op_name);
    if (src_op == nullptr) {
      HOLOSCAN_LOG_ERROR(
          "Cannot find operator '{}' from fragment '{}'", src_op_name, src_frag->name());
      return false;
    }
    OperatorSpec* src_op_spec = src_op->spec();

    auto& src_op_io_spec_outputs = src_op_spec->outputs();

    if (src_op_io_spec_outputs.find(src_port_name) == src_op_io_spec_outputs.end()) {
      if (src_op_io_spec_outputs.size() == 1 && (src_port_name.empty() || src_port_name == "")) {
        src_port_name = src_op_io_spec_outputs.begin()->first;
        // Correct source port name
        corrected_name_pair.push_back(
            std::make_pair(source_op_port, fmt::format("{}.{}", source_op_port, src_port_name)));
        continue;
      }
      if (src_op_io_spec_outputs.empty()) {
        HOLOSCAN_LOG_ERROR("Source operator '{}' has no output ports in fragment '{}'",
                           src_op_name,
                           src_frag->name());
        return false;
      }

      auto msg_buf = fmt::memory_buffer();
      for (const auto& [label, _] : src_op_io_spec_outputs) {
        if (&label == &(src_op_io_spec_outputs.begin()->first)) {
          fmt::format_to(std::back_inserter(msg_buf), "{}", label);
        } else {
          fmt::format_to(std::back_inserter(msg_buf), ", {}", label);
        }
      }

      HOLOSCAN_LOG_ERROR(
          "Source operator {} has multiple ports, but no port name is specified in fragment "
          "'{}'. It should be one of ({:.{}})",
          src_op_name,
          src_frag->name(),
          msg_buf.data(),
          msg_buf.size());
      return false;
    }
  }

  // Correct source port names
  for (auto& [original_name, corrected_name] : corrected_name_pair) {
    (*port_map)[corrected_name] = std::move((*port_map)[original_name]);
    (*port_map).erase(original_name);
  }

  // Correct target port names
  for (auto& [source_op_port, target_op_ports] : *port_map) {
    // Reuse corrected_name_pair to avoid creating a new vector
    corrected_name_pair.clear();
    for (const auto& target_op_port : target_op_ports) {
      auto [target_operator_name, target_port_name] = Operator::parse_port_name(target_op_port);
      auto& target_op_name = target_operator_name;

      auto target_op = target_frag_graph.find_node(target_op_name);
      OperatorSpec* target_op_spec = target_op->spec();

      auto& target_op_io_spec_inputs = target_op_spec->inputs();

      if (target_op_io_spec_inputs.find(target_port_name) == target_op_io_spec_inputs.end()) {
        if (target_op_io_spec_inputs.size() == 1 &&
            (target_port_name.empty() || target_port_name == "")) {
          target_port_name = target_op_io_spec_inputs.begin()->first;
          // Correct target port name
          corrected_name_pair.push_back(std::make_pair(
              target_op_port, fmt::format("{}.{}", target_op_name, target_port_name)));
          continue;
        }

        bool is_receivers = false;
        // Support for the case where the destination input port label points to the
        // parameter name of the downstream operator, and the parameter type is
        // 'std::vector<holoscan::IOSpec*>'.
        auto& target_op_params = target_op_spec->params();
        if (target_op_params.find(target_port_name) != target_op_params.end()) {
          auto& target_op_param = target_op_params.at(target_port_name);
          if (std::type_index(target_op_param.type()) ==
              std::type_index(typeid(std::vector<holoscan::IOSpec*>))) {
            is_receivers = true;
          }
        }
        if (!is_receivers) {
          if (target_op_io_spec_inputs.empty()) {
            HOLOSCAN_LOG_ERROR("Target operator '{}' has no input ports in fragment '{}'",
                               target_op_name,
                               target_frag->name());
            return false;
          }

          auto msg_buf = fmt::memory_buffer();
          for (const auto& [label, _] : target_op_io_spec_inputs) {
            if (&label == &(target_op_io_spec_inputs.begin()->first)) {
              fmt::format_to(std::back_inserter(msg_buf), "{}", label);
            } else {
              fmt::format_to(std::back_inserter(msg_buf), ", {}", label);
            }
          }

          HOLOSCAN_LOG_ERROR(
              "Target operator {} has multiple ports, but no port name is specified in "
              "fragment "
              "'{}'. It should be one of ({:.{}})",
              target_op_name,
              target_frag->name(),
              msg_buf.data(),
              msg_buf.size());
          return false;
        }
      }
    }
    // Correct target port names
    for (auto& [original_name, corrected_name] : corrected_name_pair) {
      target_op_ports.erase(original_name);
      target_op_ports.insert(corrected_name);
    }
  }
  return true;
}

/**
 * @brief Collect all the connections between fragments.
 *
 * collected connections are stored in the following field in AppDriver
 * ```
 * std::unordered_map<std::shared_ptr<Fragment>, std::vector<std::shared_ptr<ConnectionItem>>>
 *     connection_map_;
 * ```
 *
 * @param fragment_graph The fragment graph to collect connections from.
 * @return true if all connections are collected successfully.
 */
bool AppDriver::collect_connections(holoscan::FragmentGraph& fragment_graph) {
  if (!connection_map_.empty()) {
    HOLOSCAN_LOG_DEBUG("Connections are already collected");
    return true;
  }
  auto fragments = fragment_graph.get_nodes();

  // Create a list of nodes in the graph to iterate in topological order
  std::deque<holoscan::FragmentGraph::NodeType> worklist;
  // Create a list of the indegrees of all the nodes in the graph
  std::unordered_map<holoscan::FragmentGraph::NodeType, int> indegrees;
  // Create a set of visited nodes to avoid visiting the same node more than once.
  std::unordered_set<holoscan::FragmentGraph::NodeType> visited_nodes;
  visited_nodes.reserve(fragments.size());

  // Initialize the indegrees of all nodes in the graph and add root fragments to the worklist.
  for (auto& node : fragments) {
    indegrees[node] = fragment_graph.get_previous_nodes(node).size();
    if (indegrees[node] == 0) {
      // Insert a root node as indegree is 0
      // node is not moved with std::move because fragments may be used later
      worklist.push_back(node);
    }
  }

  int32_t port_index = 0;
  std::string ucx_rx_ip{"0.0.0.0"};

  while (true) {
    if (worklist.empty()) {
      // If the worklist is empty, we check if we have visited all nodes.
      if (visited_nodes.size() == fragments.size()) {
        // If we have visited all nodes, we are done.
        break;
      } else {
        HOLOSCAN_LOG_TRACE(
            "Worklist is empty, but not all nodes have been visited. There is a cycle.");
        // If we have not visited all nodes, we have a cycle in the graph.
        // Add unvisited nodes to the worklist.
        for (auto& node : fragments) {
          if (indegrees[node]) {
            // More confirmation of a cycle as the node has not been added to the
            // worklist, and still has positive in degree
            HOLOSCAN_LOG_TRACE("Adding node {} to worklist", node->name());
            HOLOSCAN_LOG_DEBUG("Fragment {} has indegree of {}", node->name(), indegrees[node]);
            indegrees[node] = 0;  // Implicitly make the indegree 0 as we are breaking a cycle
            // node is not moved with std::move because fragments maybe used later
            worklist.push_back(node);
          }
        }
      }
    }
    const auto& frag = worklist.front();
    const auto& frag_name = frag->name();
    worklist.pop_front();

    // Check if we have already visited this node
    if (visited_nodes.find(frag) != visited_nodes.end()) { continue; }
    visited_nodes.insert(frag);

    // Add the connections from the previous operator to the current operator, for both direct
    // and Broadcast connections.
    auto prev_fragments = fragment_graph.get_previous_nodes(frag);

    for (auto& prev_frag : prev_fragments) {
      const auto& prev_frag_name = prev_frag->name();
      auto input_op_port_map = fragment_graph.get_port_map(prev_frag, frag);
      if (!input_op_port_map.has_value()) {
        HOLOSCAN_LOG_ERROR(
            "Could not find operator/port map for fragment {} -> {}", prev_frag_name, frag_name);
        return false;
      }
      auto& input_op_port_map_val = input_op_port_map.value();

      // Correct port names for each connection item
      if (!update_port_names(prev_frag, frag, input_op_port_map_val)) {
        HOLOSCAN_LOG_ERROR("Could not update operator/port names for fragment {} -> {}",
                           prev_frag_name,
                           frag_name);
        return false;
      }

      for (const auto& [source_op_port, target_op_ports] : *input_op_port_map_val) {
        // Find the target operator and port name
        for (const auto& target_op_port : target_op_ports) {
          // Create a connection item
          auto source_connection_item = std::make_shared<ConnectionItem>(
              source_op_port,
              IOSpec::IOType::kOutput,
              IOSpec::ConnectorType::kUCX,
              ArgList({Arg("receiver_address", ucx_rx_ip),
                       Arg("port", static_cast<int32_t>(port_index))}));

          auto target_connection_item = std::make_shared<ConnectionItem>(
              target_op_port,
              IOSpec::IOType::kInput,
              IOSpec::ConnectorType::kUCX,
              ArgList({Arg("address", ucx_rx_ip), Arg("port", static_cast<int32_t>(port_index))}));

          // Initialize map for the port index
          if (receiver_port_map_.find(frag_name) == receiver_port_map_.end()) {
            receiver_port_map_[frag_name] =
                std::vector<std::pair<int32_t, int32_t>>{{port_index, -1}};
          } else {
            std::vector<std::pair<int32_t, int32_t>>& receiver_port_vector =
                receiver_port_map_[frag_name];
            receiver_port_vector.push_back({port_index, -1});
          }
          index_to_port_map_[port_index] = -1;
          index_to_ip_map_[port_index] = frag_name;

          // Add the connection item to the connection map
          connection_map_[prev_frag].push_back(source_connection_item);
          connection_map_[frag].push_back(target_connection_item);

          // Increment the port index
          ++port_index;
        }
      }
    }
  }

  return true;
}

void AppDriver::correct_connection_map() {
  for (auto& [fragment, connections] : connection_map_) {
    for (auto& connection : connections) {
      // Update port index
      int32_t port_index = -1;
      for (auto& arg : connection->args) {
        if (arg.name() == "port") {
          try {
            auto arg_port_index = std::any_cast<int32_t>(arg.value());
            port_index = arg_port_index;
            auto port_number = index_to_port_map_[arg_port_index];
            arg = port_number;
          } catch (const std::bad_any_cast& e) {
            HOLOSCAN_LOG_ERROR("Cannot cast the port number from fragment '{}' to int32_t",
                               fragment->name());
            return;
          }
        }
      }
      // Update IP address
      for (auto& arg : connection->args) {
        if (arg.name() == "address" || arg.name() == "receiver_address") {
          auto ip_address = index_to_ip_map_[port_index];
          arg = ip_address;
        }
      }
    }
  }
}

bool AppDriver::check_configuration() {
  if (app_ == nullptr) {
    HOLOSCAN_LOG_ERROR("Application is null");
    return false;
  }

  // Compose the graph
  app_->compose_graph();

  // Check fragment and operator graphs
  auto is_fragment_empty = app_->fragment_graph().is_empty();
  auto is_operator_empty = app_->graph().is_empty();

  if (!is_fragment_empty && !is_operator_empty) {
    HOLOSCAN_LOG_ERROR(
        "Both fragments and operators are added to the application graph. "
        "Please use either fragments or operators in the compose() method.");
    return false;
  }

  // If the environment variable HOLOSCAN_ENABLE_HEALTH_CHECK is set to true, we enable the
  // health check service.
  if (get_bool_env_var("HOLOSCAN_ENABLE_HEALTH_CHECK")) { need_health_check_ = true; }

  auto& app_options = *options_;

  // Or, if the driver or worker is running, we need to launch the health check service.
  if (app_options.run_driver || app_options.run_worker) { need_health_check_ = true; }

  // Check if the driver or worker service needs to be launched
  if (app_options.run_driver) { need_driver_ = true; }
  if (app_options.run_worker) { need_worker_ = true; }

  // If there are no driver or worker, we need to run the application graph directly.
  if (!need_driver_ && !need_worker_) { is_local_ = true; }

  // Set the default driver server address if not specified
  auto& server_address = options()->driver_address;

  if (server_address.empty() || server_address == "") {
    server_address.assign(fmt::format("0.0.0.0:{}", service::kDefaultAppDriverPort));
  }
  // Set default IP address and port if not specified
  auto split = server_address.find(':');
  if (split == std::string::npos) {
    std::string ip = server_address;
    if (server_address.empty() || server_address == "") { ip = "0.0.0.0"; }
    server_address.assign(fmt::format("{}:{}", ip, service::kDefaultAppDriverPort));
  } else if (split == 0) {
    std::string port = server_address.substr(1);
    server_address.assign(fmt::format("0.0.0.0:{}", port));
  }
  return true;
}

void AppDriver::collect_resource_requirements(const Config& app_config,
                                              holoscan::FragmentGraph& fragment_graph) {
  auto& yaml_nodes = app_config.yaml_nodes();

  // Create a set of fragment nodes from the vector of fragment nodes
  auto fragment_nodes = fragment_graph.get_nodes();
  std::unordered_set<FragmentNodeType> fragments_to_process(fragment_nodes.begin(),
                                                            fragment_nodes.end());

  SystemResourceRequirement app_resource_requirement{};

  HOLOSCAN_LOG_DEBUG("Checking YAML node for system resource requirements");
  for (const auto& yaml_node : yaml_nodes) {
    try {
      auto resources = yaml_node["resources"];
      // If 'resources' is a map, it is a system resource requirement for the application.
      if (resources.IsMap()) {
        app_resource_requirement = parse_resource_requirement(resources);
        HOLOSCAN_LOG_DEBUG("Found system resource requirement for the application");

        auto fragments_node = resources["fragments"];
        if (fragments_node.IsMap()) {
          for (const auto& fragment_node : fragments_node) {
            auto fragment_name = fragment_node.first.as<std::string>();

            auto fragment = fragment_graph.find_node(fragment_name);
            // If the fragment is found, we add the resource requirement to the fragment scheduler
            if (fragment) {
              auto fragment_resource_requirement = parse_resource_requirement(
                  fragment_name, fragment_node.second, app_resource_requirement);
              fragment_scheduler_->add_resource_requirement(fragment_resource_requirement);

              // Remove the fragment from the set of fragments to process
              fragments_to_process.erase(fragment);
              HOLOSCAN_LOG_DEBUG("Found system resource requirement for the fragment '{}'",
                                 fragment_name);
            }
          }
        }
      }
    } catch (std::exception& e) {}
  }

  // For the remaining fragments, we use the default resource requirement
  for (const auto& fragment : fragments_to_process) {
    auto fragment_resource_requirement =
        parse_resource_requirement(fragment->name(), app_resource_requirement);
    fragment_scheduler_->add_resource_requirement(fragment_resource_requirement);
  }
}

SystemResourceRequirement AppDriver::parse_resource_requirement(const YAML::Node& node) {
  SystemResourceRequirement req{};
  return parse_resource_requirement("", node, req);
}

SystemResourceRequirement AppDriver::parse_resource_requirement(
    const std::string& fragment_name, const YAML::Node& node,
    const SystemResourceRequirement& base_requirement) {
  SystemResourceRequirement req = base_requirement;
  req.fragment_name = fragment_name;
  req.cpu = node["cpu"].as<float>(req.cpu);
  req.cpu_limit = node["cpuLimit"].as<float>(req.cpu_limit);
  req.gpu = node["gpu"].as<float>(req.gpu);
  req.gpu_limit = node["gpuLimit"].as<float>(req.gpu_limit);
  if (node["memory"]) { req.memory = parse_memory_size(node["memory"].as<std::string>()); }
  if (node["memoryLimit"]) {
    req.memory_limit = parse_memory_size(node["memoryLimit"].as<std::string>());
  }
  if (node["sharedMemory"]) {
    req.shared_memory = parse_memory_size(node["sharedMemory"].as<std::string>());
  }
  if (node["sharedMemoryLimit"]) {
    req.shared_memory_limit = parse_memory_size(node["sharedMemoryLimit"].as<std::string>());
  }
  if (node["gpuMemory"]) {
    req.gpu_memory = parse_memory_size(node["gpuMemory"].as<std::string>());
  }
  if (node["gpuMemoryLimit"]) {
    req.gpu_memory_limit = parse_memory_size(node["gpuMemoryLimit"].as<std::string>());
  }
  return req;
}

SystemResourceRequirement AppDriver::parse_resource_requirement(
    const std::string& fragment_name, const SystemResourceRequirement& base_requirement) {
  SystemResourceRequirement req = base_requirement;
  req.fragment_name = fragment_name;
  return req;
}

void AppDriver::check_fragment_schedule(const std::string& worker_address) {
  // Create a client to communicate with the worker
  if (!worker_address.empty() && worker_address != "") {
    driver_server_->connect_to_worker(worker_address);
  }

  auto schedule_result = fragment_scheduler_->schedule();
  if (schedule_result) {
    // Keep the used ports for the IP address to avoid port conflicts
    std::unordered_map<std::string, std::vector<uint32_t>> used_ports_map;

    auto& schedule = schedule_result.value();
    HOLOSCAN_LOG_INFO("Fragment schedule is available:");
    for (auto& [fragment_name, worker_id] : schedule) {
      HOLOSCAN_LOG_INFO("  Fragment '{}' => Worker '{}'", fragment_name, worker_id);
    }

    // Collect any workers not participating and notify them to stop
    auto worker_addresses = driver_server_->get_worker_addresses();
    std::unordered_set<std::string> not_participated_workers(worker_addresses.begin(),
                                                             worker_addresses.end());
    for (auto& [fragment_name, worker_id] : schedule) { not_participated_workers.erase(worker_id); }
    for (const auto& worker_address : not_participated_workers) {
      HOLOSCAN_LOG_INFO("{}' does not participate in the schedule", worker_address);
      auto& worker_client = driver_server_->connect_to_worker(worker_address);
      worker_client->terminate_worker(AppWorkerTerminationCode::kCancelled);
      driver_server_->close_worker_connection(worker_address);
    }

    auto& fragment_graph = app_->fragment_graph();
    auto target_fragments = fragment_graph.get_nodes();

    // Compose fragment graphs
    for (auto& fragment : target_fragments) { fragment->compose_graph(); }

    // Collect connections
    if (!collect_connections(fragment_graph)) { HOLOSCAN_LOG_ERROR("Cannot collect connections"); }

    // Collect # of connectors for each fragment
    std::unordered_map<std::string, int> fragment_connector_count;
    for (const auto& fragment : target_fragments) {
      const auto& fragment_name = fragment->name();
      if (receiver_port_map_.find(fragment_name) != receiver_port_map_.end()) {
        fragment_connector_count[fragment_name] = receiver_port_map_[fragment_name].size();
      }
    }

    // Assign index_to_ip_map_ with the worker's IP address.
    for (auto& [port, fragment_name] : index_to_ip_map_) {
      auto& assigned_worker_id = schedule[fragment_name];
      auto& worker_client = driver_server_->connect_to_worker(assigned_worker_id);
      // Set worker's IP instead of the fragment name
      fragment_name = worker_client->ip_address();
    }

    // Construct worker_id to the vector of fragment name map
    std::unordered_map<std::string, std::vector<std::string>> worker_fragment_map;
    for (const auto& [fragment_name, worker_id] : schedule) {
      worker_fragment_map[worker_id].push_back(fragment_name);
    }

    for (const auto& [worker_id, fragment_names] : worker_fragment_map) {
      HOLOSCAN_LOG_DEBUG("Worker {} has {} fragments", worker_id, fragment_names.size());
      std::size_t total_port_count = 0;
      for (const auto& fragment_name : fragment_names) {
        total_port_count += fragment_connector_count[fragment_name];
      }
      HOLOSCAN_LOG_DEBUG("Worker {} needs {} ports", worker_id, total_port_count);

      auto& worker_client = driver_server_->connect_to_worker(worker_id);

      // Check available ports
      auto& ip_address = worker_client->ip_address();
      if (used_ports_map.find(ip_address) == used_ports_map.end()) {
        used_ports_map.emplace(ip_address, std::vector<uint32_t>());
      }
      auto& used_ports = used_ports_map[ip_address];
      auto available_ports = worker_client->available_ports(
          total_port_count, service::kMinNetworkPort, service::kMaxNetworkPort, used_ports);

      if (available_ports.size() != total_port_count) {
        HOLOSCAN_LOG_ERROR("Worker {} does not have enough ports (required: {}, available: {})",
                           worker_id,
                           total_port_count,
                           available_ports.size());
        // TODO(gbae): Handle this error (remove the worker and client from the schedule)
        return;
      }
      used_ports.insert(used_ports.end(), available_ports.begin(), available_ports.end());

      // Assign receiver_port_map_ and index_to_port_map_ with the real port number.
      int32_t worker_port_index = 0;
      for (const auto& fragment_name : fragment_names) {
        auto& fragment_ports = receiver_port_map_[fragment_name];
        for (auto& [port_index, real_port_number] : fragment_ports) {
          real_port_number = available_ports[worker_port_index];
          index_to_port_map_[port_index] = real_port_number;

          ++worker_port_index;
        }
      }
    }

    // Update connection_map_ with the real address and port numbers
    correct_connection_map();

    // Request worker to launch fragments
    for (const auto& [worker_id, fragment_names] : worker_fragment_map) {
      std::vector<std::shared_ptr<Fragment>> fragment_vector;
      fragment_vector.reserve(fragment_names.size());

      for (const auto& fragment_name : fragment_names) {
        auto fragment = fragment_graph.find_node(fragment_name);
        fragment_vector.push_back(fragment);
      }

      auto& worker_client = driver_server_->connect_to_worker(worker_id);
      bool result = worker_client->fragment_execution(fragment_vector, connection_map_);
      if (!result) {
        HOLOSCAN_LOG_ERROR("Cannot launch fragments on worker {}", worker_id);

        // Terminate all worker and close all worker connections
        terminate_all_workers(AppWorkerTerminationCode::kFailure);

        // Set app status to error
        app_status_ = AppStatus::kError;

        // Stop the driver server
        driver_server_->stop();
        // Do not call 'driver_server_->wait()' as current thread is the driver server thread
        return;
      }
    }

    // Update app status
    app_status_ = AppStatus::kRunning;

  } else {
    HOLOSCAN_LOG_INFO(schedule_result.error());
  }
}

void AppDriver::check_worker_execution(const AppWorkerTerminationStatus& termination_status) {
  auto& [worker_id, error_code] = termination_status;
  bool is_removed = driver_server_->close_worker_connection(worker_id);
  if (is_removed) {
    switch (error_code) {
      case AppWorkerTerminationCode::kSuccess: {
        auto num_worker_connections = driver_server_->num_worker_connections();
        HOLOSCAN_LOG_INFO(
            "Worker {} has finished execution. Remaining: {}", worker_id, num_worker_connections);
        if (num_worker_connections == 0) {
          HOLOSCAN_LOG_INFO("All workers have finished execution");
          // Set app status to finished
          if (app_status_ != AppStatus::kError) { app_status_ = AppStatus::kFinished; }
          // Stop the driver server
          driver_server_->stop();
          // Do not call 'driver_server_->wait()' as current thread is the driver server thread
        }
      } break;
      case AppWorkerTerminationCode::kCancelled:
      case AppWorkerTerminationCode::kFailure: {
        HOLOSCAN_LOG_ERROR("Worker {} execution has failed. Closing all worker connections ...",
                           worker_id);

        // Terminate all worker and close all worker connections
        terminate_all_workers(error_code);

        // Set app status to error
        app_status_ = AppStatus::kError;

        // Stop the driver server
        driver_server_->stop();
        // Do not call 'driver_server_->wait()' as current thread is the driver server thread
      } break;
    }
  }
}

void AppDriver::terminate_all_workers(AppWorkerTerminationCode error_code) {
  auto worker_addresses = driver_server_->get_worker_addresses();
  HOLOSCAN_LOG_DEBUG("Number of total workers: {}", worker_addresses.size());
  for (const auto& worker_address : worker_addresses) {
    auto& worker_client = driver_server_->connect_to_worker(worker_address);
    HOLOSCAN_LOG_DEBUG("Requesting worker {} to terminate", worker_address);
    worker_client->terminate_worker(error_code);
    driver_server_->close_worker_connection(worker_address);
  }
}

void AppDriver::launch_app_driver() {
  // Launch the driver service if need_driver_ is true.`
  // Even if need_driver_ is false, we still need to launch the driver service if
  // need_health_check_ is true.

  if (need_driver_ || need_health_check_) {
    HOLOSCAN_LOG_INFO("Launching the driver/health checking service");

    // Initialize fragment scheduler
    if (!fragment_scheduler_) {
      fragment_scheduler_ =
          std::make_unique<FragmentScheduler>(std::make_unique<GreedyFragmentAllocationStrategy>());
    }

    // Get the system resource requirements for each fragment
    const auto& app_config = app_->config();
    auto& fragment_graph = app_->fragment_graph();
    collect_resource_requirements(app_config, fragment_graph);

    driver_server_ =
        std::make_unique<service::AppDriverServer>(this, need_driver_, need_health_check_);
    driver_server_->start();
  }
}

void AppDriver::launch_app_worker() {
  HOLOSCAN_LOG_INFO("Launching the worker service");
  auto& app_worker = app_->worker();
  // Create and start server
  auto worker_server = app_worker.server(std::make_unique<service::AppWorkerServer>(&app_worker));
  worker_server->start();
}

std::future<void> AppDriver::launch_fragments_async(
    std::vector<FragmentNodeType>& target_fragments) {
  auto& fragment_graph = app_->fragment_graph();

  // Create GXFExecutor for Application
  app_->executor();

  // Compose each operator graph first before collecting connections
  for (auto& fragment : target_fragments) { fragment->compose_graph(); }

  if (!collect_connections(fragment_graph)) {
    HOLOSCAN_LOG_ERROR("Cannot collect connections");
    return std::async(std::launch::async, []() {});
  }
  // Correct connection_map_ with the real address and port numbers
  int32_t required_port_count = index_to_ip_map_.size();
  auto unused_ports = get_unused_network_ports(
      required_port_count, service::kMinNetworkPort, service::kMaxNetworkPort);

  for (int port_index = 0; port_index < required_port_count; ++port_index) {
    index_to_ip_map_[port_index] = "0.0.0.0";
    index_to_port_map_[port_index] = unused_ports[port_index];
  }
  correct_connection_map();

  // If UCX_PROTO_ENABLE is not already set, set it to enable UCX Protocols v2
  setenv("UCX_PROTO_ENABLE", "y", 0);
  // Reuse address
  // (see https://github.com/openucx/ucx/issues/8585 and https://github.com/rapidsai/ucxx#c-1)
  setenv("UCX_TCP_CM_REUSEADDR", "y", 0);

  // exclude CUDA Interprocess Communication if we are on iGPU
  exclude_cuda_ipc_transport_on_igpu();

  // Add the UCX network context
  for (auto& fragment : target_fragments) {
    auto network_context = fragment->make_network_context<holoscan::UcxContext>("ucx_context");
    fragment->network_context(network_context);
  }

  // Set scheduler for each fragment
  // Should be called before GXFExecutor::initialize_gxf_graph()
  Application::set_scheduler_for_fragments(target_fragments);

  // Initialize fragment graphs
  for (auto& fragment : target_fragments) {
    auto gxf_executor = dynamic_cast<gxf::GXFExecutor*>(&fragment->executor());
    if (gxf_executor == nullptr) {
      HOLOSCAN_LOG_ERROR("Cannot cast executor to GXFExecutor");
      return std::async(std::launch::async, []() {});
    }
    // Set the connection items
    if (connection_map_.find(fragment) != connection_map_.end()) {
      gxf_executor->connection_items(connection_map_[fragment]);
    }
    // Initialize the operator graph
    gxf_executor->initialize_gxf_graph(fragment->graph());
  }

  // Launch fragments
  std::vector<std::pair<holoscan::FragmentNodeType, std::future<void>>> futures;
  futures.reserve(target_fragments.size());
  for (auto& fragment : target_fragments) {
    futures.push_back(std::make_pair(fragment, fragment->executor().run_async(fragment->graph())));
  }

  auto future = std::async(std::launch::async,
                           [futures = std::move(futures), &driver_server = driver_server_]() {
                             bool any_fragment_finished = false;
                             while (!any_fragment_finished) {
                               // Check every 500 ms
                               std::this_thread::sleep_for(std::chrono::milliseconds(500));
                               for (auto& [fragment, future_obj] : futures) {
                                 const auto status = future_obj.wait_for(std::chrono::seconds(0));
                                 if (status == std::future_status::ready) {
                                   any_fragment_finished = true;
                                   break;
                                 }
                               }
                             }

                             // Stop driver server
                             if (driver_server) {
                               driver_server->stop();
                               driver_server->wait();
                               driver_server = nullptr;
                             }
                           });
  return future;
}

}  // namespace holoscan
