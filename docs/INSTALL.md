<!--!
\defgroup install Installation
\ingroup docs
\hidegroupgraph
[TOC]
-->

# Install

## Install from source

### Dependencies

 - gcc compiler
 - cmake
 - pthread
 - BSD sockets

#### Packages

 - debian: `build-essential`, `cmake`

### Building

```
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Running

For debug

`./tripd ../tripd.conf`

