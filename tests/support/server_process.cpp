#include "server_process.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <poll.h>
#include <string>
#include <sys/wait.h>
#include <system_error>
#include <thread>
#include <unistd.h>

namespace dgds::test {
namespace {

std::atomic<unsigned> root_counter{0};

// Ожидание завершения потомка с дедлайном: true — процесс завершился (или его уже нет).
bool wait_for_child(pid_t child, int timeout_ms, int &status) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds{timeout_ms};

  while (std::chrono::steady_clock::now() < deadline) {
    const pid_t reaped = waitpid(child, &status, WNOHANG);

    if (reaped == child || reaped < 0) {
      return true;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds{10});
  }

  return false;
}

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
  m_banner.clear();

  const bool announced = read_banner(pipes[0]);
  close(pipes[0]);

  if (!announced) {
    stop();
    return false;
  }

  return true;
}

// Баннер читается с общим дедлайном: сервер может замолчать на середине вывода, и тогда
// ожидание адреса обязано закончиться неудачей, а не остановить весь тест.
bool ServerProcess::read_banner(int fd) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds{k_server_start_timeout_ms};

  while (true) {
    const auto left =
        std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();

    if (left <= 0) {
      return false;
    }

    pollfd readable{.fd = fd, .events = POLLIN, .revents = 0};

    if (poll(&readable, 1, static_cast<int>(left)) <= 0) {
      return false;
    }

    char chunk[512] = {};
    const ssize_t count = read(fd, chunk, sizeof(chunk));

    if (count <= 0) {
      return false;
    }

    m_banner.append(chunk, static_cast<std::size_t>(count));

    const std::size_t marker = m_banner.find(k_server_listen_marker);

    if (marker == std::string::npos) {
      continue;
    }

    const std::size_t digits = marker + k_server_listen_marker.size();

    // Порт разбирается только из дописанной строки: иначе часть цифр может остаться в
    // следующем чтении, и в порт попадёт обрезанное число.
    if (m_banner.find('\n', digits) == std::string::npos) {
      continue;
    }

    m_port = std::atoi(m_banner.c_str() + digits);

    return m_port > 0;
  }
}

void ServerProcess::stop() {
  if (m_child <= 0) {
    return;
  }

  kill(m_child, SIGTERM);

  int status = 0;

  // Потомок может не отреагировать на вежливый сигнал (например, застряв в рантайме
  // санитайзера), поэтому ожидание ограничено, а после дедлайна идёт безусловный сигнал.
  if (!wait_for_child(m_child, k_server_stop_timeout_ms, status)) {
    kill(m_child, SIGKILL);
    // Если и после безусловного сигнала потомок не исчез, уборка не ждёт его дальше:
    // тест не должен удерживаться чужим зависанием.
    wait_for_child(m_child, k_server_stop_timeout_ms, status);
  }

  m_child = -1;
  m_port = 0;
}

} // namespace dgds::test
