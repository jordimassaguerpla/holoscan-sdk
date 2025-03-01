# Ping Distributed

This example demonstrates a distributed ping application with two operators connected using add_flow().

There are two operators involved in this example:
  1. a transmitter in Fragment 1 (`fragment1`), set to transmit a sequence of integers from 1-10 to it's 'out' port
  2. a receiver in Fragment 2 (`fragment2`) that prints the received values to the terminal

*Visit the [SDK User Guide](https://docs.nvidia.com/holoscan/sdk-user-guide/holoscan_create_distributed_app.html) to learn more about distributed applications.*

## C++ Run instructions

Please refer to the [user guide](https://docs.nvidia.com/holoscan/sdk-user-guide/holoscan_create_distributed_app.html#building-and-running-a-distributed-application) for instructions on how to run the application in a distributed manner.

### Prerequisites

* **using deb package install**:
  ```bash
  # Set the application folder
  APP_DIR=/opt/nvidia/holoscan/examples/ping_distributed/cpp
  ```

* **from NGC container**:
  ```bash
  # Set the application folder
  APP_DIR=/opt/nvidia/holoscan/examples/ping_distributed/cpp
  ```
* **source (dev container)**:
  ```bash
  ./run launch # optional: append `install` for install tree (default: `build`)

  # Set the application folder
  APP_DIR=./examples/ping_distributed/cpp
  ```
* **source (local env)**:
  ```bash
  # Set the application folder
  APP_DIR=${BUILD_OR_INSTALL_DIR}/examples/ping_distributed/cpp
  ```

### Run the application

```bash
# 1. The following commands will start a driver and one worker in one machine
#    (e.g. IP address `10.2.34.56`) using the port number `10000`,
#    and another worker in another machine.
#    If `--fragments` is not specified, any fragment in the application will be chosen to run.
# 1a. In the first machine (e.g. `10.2.34.56`):
${APP_DIR}/ping_distributed --driver --worker --address 10.2.34.56:10000
# 1b. In the second machine:
${APP_DIR}/ping_distributed --worker --address 10.2.34.56:10000

# 2. The following command will start the distributed app in a single process
${APP_DIR}/ping_distributed
```

## Python Run instructions

Please refer to the [user guide](https://docs.nvidia.com/holoscan/sdk-user-guide/holoscan_create_distributed_app.html#building-and-running-a-distributed-application) for instructions on how to run the application in a distributed manner.

### Prerequisites

* **using python wheel**:
  ```bash
  # [Prerequisite] Download NGC dataset above to `DATA_DIR`
  export HOLOSCAN_INPUT_PATH=<DATA_DIR>
  # [Prerequisite] Download example .py file below to `APP_DIR`
  # [Optional] Start the virtualenv where holoscan is installed

  # Set the application folder
  APP_DIR=<APP_DIR>
  ```
* **using deb package install**:
  ```bash
  # [Prerequisite] Download NGC dataset above to `DATA_DIR` (e.g., `/opt/nvidia/data`)
  export HOLOSCAN_INPUT_PATH=<DATA_DIR>
  export PYTHONPATH=/opt/nvidia/holoscan/python/lib

  # Set the application folder
  APP_DIR=/opt/nvidia/holoscan/examples/ping_distributed/python
  ```
* **from NGC container**:
  ```bash
  # HOLOSCAN_INPUT_PATH is set to /opt/nvidia/data by default

  # Set the application folder
  APP_DIR=/opt/nvidia/holoscan/examples/ping_distributed/python
  ```
* **source (dev container)**:
  ```bash
  ./run launch # optional: append `install` for install tree (default: `build`)

  # Set the application folder
  APP_DIR=./examples/ping_distributed/python
  ```
* **source (local env)**:
  ```bash
  export HOLOSCAN_INPUT_PATH=${SRC_DIR}/data
  export PYTHONPATH=${BUILD_OR_INSTALL_DIR}/python/lib

  # Set the application folder
  APP_DIR=${BUILD_OR_INSTALL_DIR}/examples/ping_distributed/python
  ```

### Run the application

```bash
# 1. The following commands will start a driver and one worker in one machine
#    (e.g. IP address `10.2.34.56`) using the port number `10000`,
#    and another worker in another machine.
#    If `--fragments` is not specified, any fragment in the application will be chosen to run.
# 1a. In the first machine (e.g. `10.2.34.56`):
python3 ${APP_DIR}/ping_distributed.py --driver --worker --address 10.2.34.56:10000
# 1b. In the second machine:
python3 ${APP_DIR}/ping_distributed.py --worker --address 10.2.34.56:10000

# 2. The following command will start the distributed app in a single process
python3 ${APP_DIR}/ping_distributed.py
```
