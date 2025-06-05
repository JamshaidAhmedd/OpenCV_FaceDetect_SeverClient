# Face Detection Server and Client - CSSE2310 Assignment 4

![Awesome Badge](https://cdn.rawgit.com/sindresorhus/awesome/d7305f38d29fed78fa85652e3a63e154dd8e8829/media/badge.svg) <a href="https://arbeitnow.com/?utm_source=awesome-github-profile-readme"><img src="https://img.shields.io/static/v1?label=&labelColor=505050&message=arbeitnow&color=%230076D6&style=flat&logo=google-chrome&logoColor=%230076D6" alt="website"/></a>

## Table of Contents:

* [About](#about)
* [Features](#features)
* [Installation](#installation)
* [Usage](#usage)
* [Project Structure](#project-structure)
* [Protocol Details](#protocol-details)
* [Client Usage](#client-usage)
* [Server Usage](#server-usage)
* [Contributing](#contributing)
* [License](#license)

## About

This project implements a **multithreaded TCP server** (`uqfacedetect`) and **client** (`uqfaceclient`) in **C++** for **face detection and face replacement** using **OpenCV** (4.x). The server uses OpenCV to detect faces and eyes in a given image or to replace detected faces with another image. The client communicates with the server to send image data and handle responses.

The project is built with **C++11/14**, **OpenCV 4.x**, **POSIX Sockets**, and employs **multithreading** and **semaphores** for concurrency and resource management.

## Features

* **Multithreaded Server**: Handles multiple client connections concurrently.
* **OpenCV Integration**: Uses Haar cascades for face and eye detection, along with image manipulation for face replacement.
* **Custom Protocol**: Implements a custom binary communication protocol for sending/receiving data between the client and server.
* **Robust Error Handling**: Handles invalid image data, connection errors, and other exceptions gracefully.

## Installation

### Dependencies:

* **C++11/14 compliant compiler**
* **CMake** for building
* **OpenCV 4.x**

To install dependencies on **WSL2 Ubuntu**, run:

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake git pkg-config libgtk2.0-dev \
libjpeg-dev libpng-dev libtiff-dev libavcodec-dev libavformat-dev libswscale-dev libopencv-dev
```

### Building the Project

1. Clone this repository:

   ```bash
   git clone https://github.com/yourusername/OpenCV_FaceDetect_SeverClient.git
   ```
2. Create a build directory:

   ```bash
   mkdir build && cd build
   ```
3. Run CMake and compile the project:

   ```bash
   cmake ..
   make
   ```

### Running the Project

#### Server

To run the server, use the following command:

```bash
./uqfacedetect <connectionlimit> <maxsize> [portnum]
```

Example:

```bash
./uqfacedetect 5 10000000 2310
```

This allows up to 5 client connections, with a maximum image size of 10MB, listening on port `2310`.

#### Client (Face Detection)

To run the client for face detection:

```bash
./uqfaceclient 2310 --detect face.jpg --outputimage result.jpg
```

#### Client (Face Replacement)

To run the client for face replacement:

```bash
./uqfaceclient 2310 --detect group.jpg --replacefilename overlay.png --outputimage out.jpg
```

### Project Structure

```
face_detect_project/
├── CMakeLists.txt        # Root CMake build file
└── src/
    ├── CMakeLists.txt    # CMake for source
    ├── protocol.h        # Protocol constants and function prototypes
    ├── protocol.cpp      # Protocol utility functions
    ├── uqfacedetect.cpp  # Server implementation
    └── uqfaceclient.cpp  # Client implementation
```

### Protocol Details

The communication between the server and client follows a custom binary protocol. Each message consists of the following parts:

1. **4-byte prefix**: `0x23107231` (little-endian)
2. **1-byte operation code**:

   * `0` = face detect request
   * `1` = face replace request
   * `2` = output image response
   * `3` = error message response
3. **4-byte size of the image**:
4. **Image data**: The actual image data.

The protocol ensures reliable transmission of all data and error handling via `send_all()` and `recv_all()` functions.

### Client Usage

To use the client, run the following command:

```bash
./uqfaceclient <port> [--outputimage filename] [--replacefilename filename] [--detect filename]
```

* **--detect**: Specifies the image for detection.
* **--replacefilename**: Specifies the image for face replacement.
* **--outputimage**: Specifies the output filename.

### Server Usage

The server listens for incoming connections on the specified port. It handles face detection or replacement requests based on the operation code. It also prints statistics when the `SIGHUP` signal is sent to it.

### Contributing

Contributions are always welcome! Feel free to open issues, fork the repository, and submit pull requests. Ensure your code adheres to the existing style and includes adequate comments.

### License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

---
