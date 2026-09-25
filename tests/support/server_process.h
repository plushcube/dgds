#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <sys/types.h>

namespace dgds::test {

inline constexpr int k_server_start_timeout_ms = 10000;
inline constexpr std::string_view k_server_listen_marker = "Слушаю https://127.0.0.1:";

class ServerProcess {
public:
  ServerProcess() = default;
  ServerProcess(const ServerProcess &) = delete;
  ServerProcess &operator=(const ServerProcess &) = delete;
  ServerProcess(ServerProcess &&) = delete;
  ServerProcess &operator=(ServerProcess &&) = delete;
  ~ServerProcess();

  [[nodiscard]] static std::filesystem::path temporary_root(std::string_view prefix);

  [[nodiscard]] bool start(const std::filesystem::path &root);
  void stop();

  [[nodiscard]] int port() const { return m_port; }
  [[nodiscard]] const std::string &banner() const { return m_banner; }
  [[nodiscard]] const std::filesystem::path &root() const { return m_root; }
  [[nodiscard]] std::filesystem::path certificate() const { return m_root / "tls" / "server.crt"; }

private:
  std::filesystem::path m_root;
  std::string m_banner;
  pid_t m_child = -1;
  int m_port = 0;
};

} // namespace dgds::test
