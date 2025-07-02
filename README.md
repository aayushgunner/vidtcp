# TCP Video Streaming with OpenCV in C++

This project demonstrates real-time video streaming over TCP using OpenCV in C++. One machine acts as the sender (camera source), and another as the receiver (display client). It mimics the behavior of a client-server model for transmitting raw video frames.

---

## 🛠 Requirements

- C++17 or later
- OpenCV 4.x
- CMake ≥ 3.10
- Linux (tested on Fedora/Ubuntu)
- POSIX Sockets (`sys/socket.h`, etc.)

Install OpenCV:

```bash
# Ubuntu / Debian
sudo apt install libopencv-dev

# Fedora
sudo dnf install opencv opencv-devel
```

---

## 🚀 Build Instructions

```bash
git clone https://github.com/aayushgunner/vidtcp.git
cd vidtcp

mkdir build
cd build
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..
make

# (Optional) Symlink compile_commands.json for clangd / Neovim
ln -s build/compile_commands.json .
```

---

## 🧪 Usage

Open two terminals or two devices on the same network.

### Terminal 1 (Receiver):

```bash
./app receive 127.0.0.1 8080
```

### Terminal 2 (Sender):

```bash
./app send 127.0.0.1 8080
```

> Replace `127.0.0.1` with the IP address of the receiver if running across two machines.

Press `q` in the receiver window to quit.

---

## 🧠 Project Structure

```
.
├── main.cpp             # Entry point, contains both sender and receiver
├── CMakeLists.txt       # Build configuration
└── README.md            # You're here!
```

---

## 💡 Future Improvements

- Compress frames using JPEG before sending
- Add FPS control or time sync
- Support multiple clients
- Add TLS encryption for secure transfer
