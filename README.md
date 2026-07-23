# ESP32 Autonomous Mecanum Robot Platform

An ESP32-based autonomous robotics platform featuring a four-wheel mecanum drivetrain, FreeRTOS task architecture, sensor-driven navigation, and stepper-based actuator control.

The system uses a modular embedded software architecture where motion control, sensor processing, and actuator management execute as independent real-time tasks. Communication between subsystems is handled through FreeRTOS queues, allowing scalable expansion of the control system.

---

# Overview

This project was developed as an embedded robotics platform demonstrating real-time multitasking, autonomous decision making, and hardware abstraction on the ESP32.

Unlike a traditional Arduino `loop()`-based program, the system uses the ESP32's dual-core FreeRTOS environment to separate functionality into independent tasks:

- **Drive Task** — Controls the mecanum drivetrain and executes movement commands.
- **Sensor Task** — Runs the autonomous navigation state machine and processes sensor inputs.
- **Arm Task** — Controls the auxiliary stepper-driven mechanism.

The architecture allows each subsystem to operate independently while maintaining deterministic communication through RTOS queues.

---

# Features

- ESP32 dual-core FreeRTOS architecture
- Four-wheel mecanum omnidirectional drive
- Five independent stepper motor controllers
- Queue-based task communication
- Autonomous navigation state machine
- Infrared distance sensing
- MPU6050 IMU integration
- External STEP/DIR motor driver interface
- Solenoid actuator control
- Start button and status indicators
- Modular embedded software design

---

# Hardware

## Controller

- ESP32 development board

## Actuation

The ESP32 generates STEP/DIR signals for external stepper motor drivers.

### Motors

- 4 × Stepper motors for mecanum drivetrain
- 1 × Stepper motor for arm mechanism

The project uses the **AccelStepper** library for motion generation and acceleration control.

Compatible drivers:

- A4988
- DRV8825
- TMC-series drivers operating in STEP/DIR mode

---

## Sensors

- MPU6050 6-axis IMU (I²C)
- Long-range infrared distance sensor
- Short-range infrared distance sensor
- Mechanical limit switch

---

## Outputs

- Solenoid actuator
- Ready LED
- Finished LED

---

# Pin Assignment

| Function | GPIO |
|---|---:|
| LF Step | 27 |
| LF Direction | 26 |
| LR Step | 18 |
| LR Direction | 32 |
| RF Step | 2 |
| RF Direction | 15 |
| RR Step | 19 |
| RR Direction | 3 |
| Arm Step | 4 |
| Arm Direction | 5 |
| Gyro Power | 13 |
| Ready LED | 22 |
| Finished LED | 12 |
| Solenoid | 33 |
| Start Button | 25 |
| Long IR Sensor | 39 |
| Short IR Sensor | 14 |
| Limit Switch | 34 |
| Battery Monitor | 36 |

---

# Software Architecture

The software is structured around three independent FreeRTOS tasks:

Sensor Task
    |
    | Drive Commands
    v
Drive Queue
    |
    v
Drive Task


Sensor Task
    |
    | Arm Trigger
    v
Arm Queue
    |
    v
Arm Task

---

# FreeRTOS Task Design

## Drive Task

**Purpose:** Real-time drivetrain control.

Responsibilities:

- Receive movement commands
- Generate mecanum wheel motion
- Continuously execute `AccelStepper::run()`
- Control individual wheel directions
- Handle stopping and movement transitions

Configuration:

    Priority: 2
    Core: 0

---

## Sensor Task

**Purpose:** Autonomous mission controller.

Responsibilities:

- Wait for start command
- Process IR sensor readings
- Monitor limit switches
- Execute navigation state machine
- Trigger arm operation
- Signal completion

Configuration:

    Priority: 3
    Core: 1

---

## Arm Task

**Purpose:** Auxiliary mechanism control.

Responsibilities:

- Wait for arm trigger
- Move arm stepper to target position
- Monitor completion

Configuration:

    Priority: 1
    Core: 1

---

# Autonomous Operation

The programmed sequence is:

1. System waits for start button input.
2. Arm mechanism is activated.
3. Robot drives forward until the long-range IR sensor detects the boundary.
4. Robot performs a small alignment correction.
5. Robot strafes sideways while monitoring distance.
6. Robot moves diagonally backwards.
7. Motion continues until:
   - the limit switch activates, or
   - the target sensor condition is reached.
8. Robot stops.
9. Solenoid activates.
10. Completion LED illuminates.

---

# Mecanum Drive Commands

| Command | Motion |
|---|---|
| FORWARD | Forward movement |
| BACKWARD | Reverse movement |
| LEFT | Left strafe |
| LEFT_5_DEGREES | Alignment correction |
| DIAGONAL_BACK_LEFT | Rear-left diagonal movement |
| STOP | Stop |

---

# Inter-Task Communication

The system uses FreeRTOS queues to prevent direct coupling between subsystems.

## Drive Queue

Transfers movement commands from the Sensor Task to the Drive Task using the `Command` enumeration.

Flow:

    Sensor Task
          |
          v
    Drive Queue
          |
          v
    Drive Task

---

## Arm Queue

Provides a one-time trigger for arm deployment.

Flow:

    Sensor Task
          |
          v
    Arm Queue
          |
          v
    Arm Task

---

# Sensor Processing

## Infrared Distance Sensors

Distance is estimated from analog voltage using an experimentally calibrated model:

    distance = 9.5 × voltage^-0.7225

Invalid measurements return an out-of-range value to prevent false navigation decisions.

---

## MPU6050 IMU

The MPU6050 is configured through I²C during startup.

Current functionality:

- IMU initialization
- Gyroscope configuration
- Angular velocity acquisition

The IMU interface provides a foundation for future improvements including:

- Heading stabilisation
- Gyroscope feedback control
- Sensor fusion

---

# Development Environment

Developed using:

- PlatformIO
- ESP32 Arduino Framework
- C++17

## Dependencies

- **AccelStepper** — Stepper motor control
- **Wire** — I²C communication
- **FreeRTOS** — Real-time scheduling included with ESP32 Arduino framework

---

# Project Structure

    ESP32-Mecanum-Robot/
    |
    ├── src/
    │   └── main.cpp              # Main application source
    |
    ├── platformio.ini            # PlatformIO configuration
    |
    ├── README.md                 # Documentation
    |
    └── LICENSE                   # MIT License   

---

# Building and Uploading

Clone the repository and open it in PlatformIO.

Build:

    pio run

Upload:

    pio run --target upload

Serial monitor:

    pio device monitor

Default baud rate:

    115200

---

# PlatformIO Configuration

The project configuration is stored in:

    platformio.ini

This defines:

- ESP32 target board
- Arduino framework
- Build configuration
- Library dependencies
- Upload settings
- Serial monitor settings

---

# Running the Robot

After uploading:

1. Power the ESP32 and motor drivers.
2. Ensure all sensors and actuators are connected according to the pin map.
3. Wait for the ready LED to illuminate.
4. Press the start button.
5. The autonomous sequence executes automatically.

The ESP32 uses FreeRTOS tasks rather than a traditional Arduino loop-based architecture. Blocking operations are isolated within individual tasks, allowing independent subsystems to execute concurrently.

---

# Future Improvements

Potential extensions:

- Closed-loop heading control using IMU feedback
- Wheel odometry
- Encoder integration
- EKF sensor fusion
- Velocity control
- Motion profiling
- Improved obstacle avoidance
- Wireless telemetry
- ROS integration

---

# Engineering Summary

This project demonstrates:

- Real-time embedded software architecture
- FreeRTOS task scheduling
- Queue-based subsystem communication
- Autonomous robot control
- Mecanum drive kinematics
- Sensor integration
- Stepper motor control
- Modular C++ embedded design

The focus of this project is scalable embedded architecture rather than a single-purpose Arduino sketch, providing a foundation for future autonomous robotics development.

---

# License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.
