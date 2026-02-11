# raft-nng

## Getting Started on Raspberry Pi

### Prerequisites

Before you begin, ensure your Raspberry Pi has the following installed:

```bash
# Update package list
sudo apt update

# Install git
sudo apt install -y git

# Install build tools
sudo apt install -y build-essential cmake
```

### Downloading the Code

1. **Clone the repository with submodules** (NNG is included as a submodule):

```bash
git clone --recursive https://github.com/nrodd/raft-nng.git
cd raft-nng
```

If you've already cloned without the `--recursive` flag, initialize the submodules:

```bash
git submodule update --init --recursive
```

### Building the Project

2. **Create a build directory and compile**:

```bash
# Create build directory
mkdir build
cd build

# Configure with CMake
cmake ..

# Build (use -j4 for parallel compilation on Pi 4, or -j2 for older models)
make -j4
```

The build process will compile both the NNG library and your application.

### Running the Application

3. **Run the compiled executable**:

The application requires two arguments:
- **mode**: either `listen` or `dial`  
- **url**: the connection URL (e.g., `tcp://0.0.0.0:5555` or `ipc:///tmp/pair.ipc`)

**Example - On the first Raspberry Pi (listener)**:
```bash
./my_app listen tcp://0.0.0.0:5555
```

**Example - On the second Raspberry Pi (dialer)**:
```bash
./my_app dial tcp://192.168.1.100:5555
```
*(Replace `192.168.1.100` with the IP address of the first Pi)*

**Local testing on a single Pi using IPC**:
```bash
# Terminal 1
./my_app listen ipc:///tmp/pair.ipc

# Terminal 2  
./my_app dial ipc:///tmp/pair.ipc
```

## License

This project uses the NNG library, which is licensed under the MIT License. See the [NNG LICENSE](external/nng/LICENSE.txt) for details.

