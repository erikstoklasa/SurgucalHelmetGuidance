# Sumilation for surgical helmet

## Installation

### Linux

install c++ tools

```sh
sudo apt update

sudo apt install -y \
  build-essential cmake ninja-build \
  git curl zip unzip tar pkg-config gdb
```

install openGL libraries:

```sh
sudo apt install -y \
  libgl1-mesa-dev libegl1-mesa-dev \
  libx11-dev libxext-dev libxrandr-dev \
  libxcursor-dev libxi-dev libxfixes-dev \
  libxkbcommon-dev libwayland-dev \
  libasound2-dev libpulse-dev \
  python3-jinja2
```

### Windows

TBD

### Mac

TBD

## Build the application

### Linux

Once at the beginning of the project:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
```

after every change before running the simulator:

```sh
cmake --build build
```

#### Mac/Windows

TBD

## Further Info

### Folder Structure

#### src

contains the main code of the programm

#### core

code used in more than one space can be put here

#### scenarios

different scenarios for the programm

#### assets

3d bodys, etc.
