#include "server_process.h"

#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <poll.h>
#include <string>
#include <sys/wait.h>
#include <system_error>
#include <unistd.h>

namespace dgds::test {
namespace {

std::atomic<unsigned> root_counter{0};

} // namespace

std::filesystem::path ServerProcess::temporary_root(std::string_view prefix) {
  return std::filesystem::temp_directory_path() /
         (std::string(prefix) + "-" + std::to_string(::getpid()) + "-" + std::to_string(root_counter++));
}

ServerProcess::~ServerProcess() { stop(); }

bool ServerProcess::start(const std::filesystem::path &root) {
  stop();

  std::error_code status;
  std::filesystem::create_directories(root, status);

  if (status) {
    return false;
  }

  int pipes[2] = {-1, -1};

  if (pipe(pipes) != 0) {
    return false;
  }

  const pid_t child = fork();

  if (child < 0) {
    close(pipes[0]);
    close(pipes[1]);
    return false;
  }

  if (child == 0) {
    dup2(pipes[1], STDOUT_FILENO);
    close(pipes[0]);
    close(pipes[1]);
    execl(DGDS_SERVER_PATH, DGDS_SERVER_PATH, "--root", root.c_str(), "--port", "0", nullptr);
    _exit(127);
  }

  close(pipes[1]);

  m_child = child;
  m_root = root;
  m_port = 0;

  FILE *output = fdopen(pipes[0], "r");

  if (output == nullptr) {
    stop();
    return false;
  }

  pollfd readable{.fd = pipes[0], .events = POLLIN, .revents = 0};
  const bool announced = poll(&readable, 1, k_server_start_timeout_ms) > 0;

  char buffer[512] = {};

  if (announced) {
    while (fgets(buffer, sizeof(buffer), output) != nullptr) {
      const std::string line{buffer};
      const std::size_t marker = line.find(k_server_listen_marker);

      m_banner += line;

      if (marker == std::string::npos) {
        continue;
      }

      m_port = std::atoi(line.substr(marker + k_server_listen_marker.size()).c_str());
      break;
    }
  }

  fclose(output);

  if (m_port <= 0) {
    stop();
    return false;
  }

  return true;
}

void ServerProcess::stop() {
  if (m_child <= 0) {
    return;
  }

  kill(m_child, SIGTERM);

  int status = 0;
  waitpid(m_child, &status, 0);

  m_child = -1;
  m_port = 0;
}

} // namespace dgds::test
