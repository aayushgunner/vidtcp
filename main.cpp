#include <arpa/inet.h>
#include <chrono>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <opencv2/opencv.hpp>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

const size_t CHUNK_SIZE = 1024;
const int JPEG_QUALITY = 80;

bool is_ipv6(const std::string &addr) {
  return addr.find(':') != std::string::npos;
}

bool sender_fn(const std::string &addr, int port) {
  int sock;
  bool ipv6 = is_ipv6(addr);

  sock = socket(ipv6 ? AF_INET6 : AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    perror("Socket creation failed");
    return false;
  }

  if (ipv6) {
    sockaddr_in6 serv_addr{};
    serv_addr.sin6_family = AF_INET6;
    serv_addr.sin6_port = htons(port);

    if (inet_pton(AF_INET6, addr.c_str(), &serv_addr.sin6_addr) <= 0) {
      std::cerr << "Invalid IPv6 address\n";
      close(sock);
      return false;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
      perror("Connection Failed");
      close(sock);
      return false;
    }
  } else {
    sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, addr.c_str(), &serv_addr.sin_addr) <= 0) {
      std::cerr << "Invalid IPv4 address\n";
      close(sock);
      return false;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
      perror("Connection Failed");
      close(sock);
      return false;
    }
  }

  cv::VideoCapture cam(0);
  if (!cam.isOpened()) {
    std::cerr << "Error: Could not open camera\n";
    close(sock);
    return false;
  }

  cam.set(cv::CAP_PROP_FRAME_WIDTH, 640);
  cam.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
  cam.set(cv::CAP_PROP_FPS, 30);

  cv::Mat frame;
  std::vector<uchar> compressed_data;
  std::vector<int> compression_params = {cv::IMWRITE_JPEG_QUALITY,
                                         JPEG_QUALITY};

  std::cout << "Starting video transmission to " << addr << ":" << port
            << std::endl;

  while (true) {
    cam >> frame;
    if (frame.empty()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      continue;
    }

    if (!cv::imencode(".jpg", frame, compressed_data, compression_params)) {
      std::cerr << "Failed to encode frame\n";
      continue;
    }

    uint32_t frame_len = compressed_data.size();
    uint32_t width = frame.cols;
    uint32_t height = frame.rows;

    uint32_t width_le = htole32(width);
    uint32_t height_le = htole32(height);
    uint32_t len_le = htole32(frame_len);

    if (write(sock, &width_le, sizeof(width_le)) < 0 ||
        write(sock, &height_le, sizeof(height_le)) < 0 ||
        write(sock, &len_le, sizeof(len_le)) < 0) {
      perror("Write metadata failed");
      break;
    }

    size_t sent = 0;
    while (sent < compressed_data.size()) {
      size_t chunk_size = std::min(CHUNK_SIZE, compressed_data.size() - sent);
      ssize_t bytes_sent = write(sock, &compressed_data[sent], chunk_size);
      if (bytes_sent < 0) {
        perror("Write chunk failed");
        goto cleanup;
      }
      sent += bytes_sent;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(33)); // ~30 FPS
  }

cleanup:
  close(sock);
  return true;
}

bool receiver_fn(const std::string &addr, int port) {
  int server_fd;
  bool ipv6 = is_ipv6(addr);

  server_fd = socket(ipv6 ? AF_INET6 : AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
    perror("Socket creation failed");
    return false;
  }

  int opt = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
    perror("setsockopt failed");
  }

  if (ipv6) {
    sockaddr_in6 address{};
    address.sin6_family = AF_INET6;
    address.sin6_port = htons(port);

    if (addr == "::") {
      address.sin6_addr = in6addr_any;
    } else {
      if (inet_pton(AF_INET6, addr.c_str(), &address.sin6_addr) <= 0) {
        std::cerr << "Invalid IPv6 address\n";
        close(server_fd);
        return false;
      }
    }

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
      perror("Bind failed");
      close(server_fd);
      return false;
    }
  } else {
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);

    if (addr == "0.0.0.0") {
      address.sin_addr.s_addr = INADDR_ANY;
    } else {
      if (inet_pton(AF_INET, addr.c_str(), &address.sin_addr) <= 0) {
        std::cerr << "Invalid IPv4 address\n";
        close(server_fd);
        return false;
      }
    }

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
      perror("Bind failed");
      close(server_fd);
      return false;
    }
  }

  if (listen(server_fd, 1) < 0) {
    perror("Listen failed");
    close(server_fd);
    return false;
  }

  std::cout << "Waiting for connection on " << addr << ":" << port << std::endl;

  int client_sock = accept(server_fd, nullptr, nullptr);
  if (client_sock < 0) {
    perror("Accept failed");
    close(server_fd);
    return false;
  }

  std::cout << "Client connected!" << std::endl;

  while (true) {
    uint32_t width_le, height_le, len_le;

    if (read(client_sock, &width_le, sizeof(width_le)) != sizeof(width_le) ||
        read(client_sock, &height_le, sizeof(height_le)) != sizeof(height_le) ||
        read(client_sock, &len_le, sizeof(len_le)) != sizeof(len_le)) {
      std::cout << "Connection closed or metadata read failed\n";
      break;
    }

    uint32_t width = le32toh(width_le);
    uint32_t height = le32toh(height_le);
    size_t frame_len = le32toh(len_le);

    if (frame_len > 10 * 1024 * 1024) { // 10MB limit
      std::cerr << "Frame too large: " << frame_len << " bytes\n";
      break;
    }

    std::vector<uchar> buffer(frame_len);
    size_t received = 0;
    while (received < frame_len) {
      ssize_t bytes_read = read(client_sock, buffer.data() + received,
                                std::min(CHUNK_SIZE, frame_len - received));
      if (bytes_read <= 0) {
        std::cout << "Connection closed during frame read\n";
        goto cleanup_receiver;
      }
      received += bytes_read;
    }

    cv::Mat frame = cv::imdecode(buffer, cv::IMREAD_COLOR);
    if (frame.empty()) {
      std::cerr << "Failed to decode frame\n";
      continue;
    }

    cv::imshow("Video Stream", frame);
    int key = cv::waitKey(1) & 0xFF;
    if (key == 'q' || key == 27) { // 'q' or ESC
      break;
    }
  }

cleanup_receiver:
  close(client_sock);
  close(server_fd);
  cv::destroyAllWindows();
  return true;
}

int main(int argc, char **argv) {
  if (argc != 4) {
    std::cerr << "Usage: " << argv[0] << " send|receive <ip> <port>\n";
    std::cerr << "Examples:\n";
    std::cerr << "  IPv4: " << argv[0] << " send 192.168.1.100 8080\n";
    std::cerr << "  IPv4: " << argv[0] << " receive 0.0.0.0 8080\n";
    std::cerr << "  IPv6: " << argv[0] << " send 2001:db8::1 8080\n";
    std::cerr << "  IPv6: " << argv[0] << " receive :: 8080\n";
    return 1;
  }

  std::string mode = argv[1];
  std::string ip = argv[2];
  int port = std::stoi(argv[3]);

  std::cout << "Mode: " << mode << ", Address: " << ip << ":" << port << " ("
            << (is_ipv6(ip) ? "IPv6" : "IPv4") << ")" << std::endl;

  if (mode == "send") {
    return sender_fn(ip, port) ? 0 : 1;
  } else if (mode == "receive") {
    return receiver_fn(ip, port) ? 0 : 1;
  } else {
    std::cerr << "Unknown mode: " << mode << std::endl;
    std::cerr << "Use 'send' or 'receive'\n";
    return 1;
  }
}
