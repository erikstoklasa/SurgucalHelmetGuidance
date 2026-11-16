# Simulation for Surgical Helmet

A C++ application for visualizing and simulating rigid-body tracking data for a surgical helmet guidance system.

The simulator supports mock tracking, recorded CSV data, and experimental live OptiTrack tracking through NatNet.

It provides a 3D scene, tracker visualization, playback controls, movement trails, tracking diagnostics, and basic scenario configuration.

## Installation

### Linux / WSL (Ubuntu)

#### 1. Install C++ development tools

```sh
sudo apt update

sudo apt install -y \
  build-essential cmake ninja-build \
  git curl zip unzip tar pkg-config gdb
```

#### 2. Install OpenGL and windowing dependencies

```sh
sudo apt install -y \
  libgl1-mesa-dev libegl1-mesa-dev \
  libx11-dev libxext-dev libxrandr-dev \
  libxcursor-dev libxi-dev libxfixes-dev \
  libxkbcommon-dev libwayland-dev \
  libasound2-dev libpulse-dev \
  libxss-dev python3-jinja2
```

These libraries are needed for OpenGL rendering, SDL3, and other graphical dependencies.

#### 3. Install the 3D model importer

The simulator uses Assimp to import 3D models.

```sh
sudo apt install -y libassimp-dev
```

#### 4. Install the OptiTrack NatNet SDK

The NatNet SDK is required by the current CMake configuration for live OptiTrack integration.

Download the Linux/Ubuntu SDK from:

https://www.optitrack.com/support/downloads?cat=developer-tools

Extract it into:

```text
~/optitrack/natnet/
```

The directory should contain at least:

```text
natnet/
├── include/
│   └── NatNetClient.h
├── lib/
│   └── libNatNet.so
└── samples/
```

You can verify the installation with:

```sh
find ~/optitrack/natnet -name 'libNatNet.so'
find ~/optitrack/natnet -name 'NatNetClient.h'
```

NatNet can be compiled and linked without an active Motive server. Live tracking requires a running and reachable Motive instance.

### Windows

TBD

### macOS

TBD

## Build the Application

### Linux / WSL

#### 1. Configure the project

Run this once from the project root directory:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
```

CMake will configure the project and prepare its dependencies.

Run the configuration command again after changing CMake settings or adding new source files that need to be included in the build.

#### 2. Build

Run this after changing the source code:

```sh
cmake --build build
```

Alternatively, build using multiple CPU cores:

```sh
cmake --build build -j$(nproc)
```

#### 3. Run the simulator

From the project root directory:

```sh
./build/surgicalhelmet-simulator
```

Running from the project root is important because the default CSV path is relative to this directory.

### Windows / macOS

TBD

## Using the Simulator

### Scene Manager

The Scene Manager is the main control panel of the simulator.

It allows you to:

- View and select rigid bodies
- Add or remove bodies in mock mode
- Rename trackers
- Assign scenario roles
- Select a tracking source
- Control CSV playback
- Inspect tracking information
- Configure the visualization
- Adjust the scenario reference direction

### Tracking Sources

The simulator currently supports three tracking sources.

#### Mock Tracking

Generates simulated positions and rotations.

Useful for testing the application without recorded data or OptiTrack hardware.

#### CSV Playback

Loads prerecorded tracking frames from a CSV file.

The default recording is:

```text
data/sample_tracking.csv
```

Playback controls include:

- Play / Pause
- Playback speed
- Timeline position
- Restart

The CSV file uses the following columns:

```csv
frame,time,id,x,y,z,qx,qy,qz,qw,mean_error,valid
```

Positions are expressed in meters.

#### Live OptiTrack (NatNet)

The simulator contains an experimental NatNet input source.

It can connect to an OptiTrack Motive server and receive rigid-body tracking data.

The current implementation uses a direct NatNet connection with server and local IP addresses.

This connection workflow may change later if tracking is handled by a separate application.

### Camera Controls

The 3D viewport supports mouse navigation.

| Input | Action |
|---|---|
| Left mouse drag | Rotate / orbit camera |
| Middle mouse drag | Move / pan camera |
| Mouse wheel | Zoom in or out |
| Reset Camera | Restore default camera position |

Camera interaction starts outside the ImGui control panels.

### Visualization

Rigid bodies are displayed in a 3D coordinate system.

Available visualization features include:

- 3D ground and grid
- World coordinate axes
- Selected-body highlighting
- Local XYZ axes
- Movement trails
- Imported 3D models for scenario objects

Local body axes can be configured to show:

- Always
- Selected only
- Hidden

Movement trails can be enabled or disabled, and their history length can be adjusted.

### Tracking Diagnostics

The Body Inspector displays tracking information for the selected rigid body.

This includes:

- Tracking ID
- Tracking validity
- Mean tracking error
- Position
- Quaternion rotation
- Euler angles
- Movement speed
- Recent speed history

The application also displays its rendering FPS and an estimated tracking frame rate.

## Scenarios

Scenarios describe what the tracked objects represent and how they are used in a simulation.

### Tracker Roles

Trackers can currently be assigned the following roles:

| Role | Description |
|---|---|
| Unassigned | Generic tracked rigid body |
| Head | Represents the tracked head |
| Drill | Represents the tracked drilling instrument |
| Target | Represents the tracked target |

Each role can be assigned to one tracker at a time.

Tracker names and roles are independent of OptiTrack tracking IDs.

### Sample Scenario

The **Load Sample Scenario** button creates a basic demonstration setup.

It assigns the first three visible trackers to:

1. Head
2. Drill
3. Target

It also resets and enables the reference direction.

The sample scenario can be used with simulated or recorded tracking data.

### Manual Reference

The reference is **not an OptiTrack tracker**.

It represents a manually specified desired direction, attached to the Target.

Its position is automatically determined by the Target's tracked position, and its direction follows the Target's orientation.

The reference can be adjusted using:

- **Yaw:** −180° to +180°
- **Elevation:** −90° to +90°

Both angles are defined relative to the Target's local coordinate system.

A yellow arrow visualizes the resulting reference direction in the 3D scene.

The reference direction is currently used for visualization. Calibrated drill-to-reference guidance calculations are planned for later development.

## 3D Models

The simulator uses external 3D model files rather than hardcoded geometry for scenario objects.

The current models are simple representations:

- **Head:** Oval head model
- **Drill:** Pen-shaped drilling tool
- **Target:** Bullseye-shaped target

Models are stored in:

```text
assets/models/
```

Currently used files:

```text
head.glb
drill_pen.glb
target.glb
```

Models are loaded using Assimp and rendered with OpenGL.

The position and rotation of each model follow the corresponding tracked rigid body.

**Important:** Visual models do not yet include calibrated offsets between the OptiTrack tracker, the physical object, and important points such as the drill tip.

## Further Information

### Technologies

The project uses:

| Technology | Purpose |
|---|---|
| C++20 | Main programming language |
| CMake | Build system |
| SDL3 | Windows and input handling |
| OpenGL 3.3 | 3D rendering |
| GLAD | OpenGL function loading |
| GLM | Vector, matrix and quaternion mathematics |
| Dear ImGui | User interface |
| Assimp | 3D model importing |
| OptiTrack NatNet | Live tracking integration |

### Folder Structure

```text
project/
├── assets/
│   └── models/
├── core/
├── data/
├── scenarios/
├── sources/
├── src/
├── tools/
├── CMakeLists.txt
└── README.md
```

#### `src/`

Contains the main application code, user interface, viewport rendering and OpenGL setup.

Important files include:

- `main.cpp` — Application initialization and main loop
- `viewport.hpp` / `viewport.cpp` — 3D scene rendering and camera

#### `core/`

Contains shared data structures and application logic.

Examples:

- Rigid bodies
- Scene management
- Tracking frames
- Movement trails
- Tracking diagnostics
- Scenario reference

#### `sources/`

Contains the different tracking data sources.

Currently includes:

- `mock_tracking.hpp`
- `csv_tracking.hpp`
- `csv_playback.hpp`
- `natnet_tracking.hpp`
- `natnet_tracking.cpp`

Each source provides tracking information that can be converted into the application's common `TrackingFrame` structure.

#### `scenarios/`

Intended for saved simulation configurations.

Future scenario files may store tracker role assignments, reference angles, object offsets, tolerances and other simulation settings.

Scenario file persistence is not yet implemented.

#### `assets/`

Contains external resources used by the simulator.

Currently mainly 3D models in GLB format.

#### `data/`

Contains recorded and generated tracking data.

The default CSV recording is stored here.

#### `tools/`

Contains helper scripts and development utilities, such as the sample tracking data generator.

## Planned Features

The following features are planned for future development:

- Save and load scenario configurations
- Calibrate the drill tip and drilling axis
- Calculate drill-to-target distance and angular deviation
- Compare drill orientation with the reference direction
- Improve live OptiTrack connectivity and source management
- Simulate the LEDs installed on the surgical helmet
- Connect guidance calculations to LED feedback
- Improve model positioning, scaling and calibration
- Improve tracking-loss visualization
- Add further scenario and visualization options
