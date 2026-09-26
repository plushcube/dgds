#include <httplib.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;
using Json = nlohmann::json;

struct Options {
  std::string host = "127.0.0.1";
  int port = 0;
  std::string path = "/catalog";
  std::string certificate;
  std::string body = R"({"version":1})";
  std::string login;
  int threads = 8;
  int seconds = 5;
  bool keep_alive = true;
};

void usage() {
  std::cout << "dgds-loadgen [--host <адрес>] [--port <номер>] --certificate <путь> [--path <путь>]\n"
            << "            [--body <json>] [--login <имя>] [--threads <число>] [--seconds <число>]\n"
            << "            [--no-keep-alive] [--help]\n"
            << "  в теле запроса допустимы подстановки %credentials% и %purchase%\n";
}

Options parse(int argc, char *argv[]) {
  Options options;

  if (argc == 1) {
    usage();
    std::exit(2);
  }

  for (int index = 1; index < argc; ++index) {
    const std::string key = argv[index];

    if (key == "--no-keep-alive") {
      options.keep_alive = false;
      continue;
    }

    if (key == "--help") {
      usage();
      std::exit(0);
    }

    const auto value = [&]() -> std::string {
      if (index + 1 < argc) {
        return argv[++index];
      }

      std::cerr << "нет значения для " << key << '\n';
      std::exit(2);
    }();

    if (key == "--host") {
      options.host = value;
    } else if (key == "--port") {
      options.port = std::atoi(value.c_str());
    } else if (key == "--path") {
      options.path = value;
    } else if (key == "--certificate") {
      options.certificate = value;
    } else if (key == "--body") {
      options.body = value;
    } else if (key == "--login") {
      options.login = value;
    } else if (key == "--threads") {
      options.threads = std::atoi(value.c_str());
    } else if (key == "--seconds") {
      options.seconds = std::atoi(value.c_str());
    } else {
      std::cerr << "неизвестный аргумент " << key << '\n';
      std::exit(2);
    }
  }

  return options;
}

std::unique_ptr<httplib::SSLClient> make_client(const Options &options) {
  auto client = std::make_unique<httplib::SSLClient>(options.host, options.port);
  client->enable_server_certificate_verification(true);
  client->set_ca_cert_path(options.certificate);
  client->set_connection_timeout(10);
  client->set_read_timeout(60);
  client->set_write_timeout(60);
  client->set_keep_alive(options.keep_alive);
  client->set_tcp_nodelay(true);

  return client;
}

std::string request(const Options &options, const std::string &path, const std::string &body) {
  const auto client = make_client(options);
  const auto response = client->Post(path, body, "application/json");

  if (!response) {
    std::cerr << "запрос не выполнен: " << httplib::to_string(response.error()) << '\n';
    std::exit(3);
  }

  return response->body;
}

std::string find_purchase_id(const Json &node) {
  if (node.is_object()) {
    for (const auto &[key, value] : node.items()) {
      if (key == "purchase_id" && value.is_string()) {
        return value.get<std::string>();
      }

      const std::string nested = find_purchase_id(value);

      if (!nested.empty()) {
        return nested;
      }
    }
  }

  if (node.is_array()) {
    for (const auto &item : node) {
      const std::string nested = find_purchase_id(item);

      if (!nested.empty()) {
        return nested;
      }
    }
  }

  return {};
}

std::string prepare_body(const Options &options) {
  std::string body = options.body;

  if (options.login.empty()) {
    return body;
  }

  const std::string authorized = request(options, "/login", Json{{"version", 1}, {"name", options.login}}.dump());
  const Json parsed = Json::parse(authorized, nullptr, false);

  if (parsed.is_discarded() || !parsed.contains("data")) {
    std::cerr << "вход не удался: " << authorized << '\n';
    std::exit(3);
  }

  const std::string credentials = parsed.at("data").dump();

  if (body.find("%credentials%") != std::string::npos) {
    body.replace(body.find("%credentials%"), std::strlen("%credentials%"), credentials);
  }

  if (body.find("%purchase%") != std::string::npos) {
    const std::string listed =
        request(options, "/purchases", Json{{"version", 1}, {"credentials", parsed.at("data")}}.dump());
    const Json purchases = Json::parse(listed, nullptr, false);
    const std::string purchase = find_purchase_id(purchases);

    if (purchase.empty()) {
      std::cerr << "покупок не найдено: " << listed << '\n';
      std::exit(3);
    }

    body.replace(body.find("%purchase%"), std::strlen("%purchase%"), purchase);
  }

  return body;
}

double percentile(std::vector<double> &values, double share) {
  if (values.empty()) {
    return 0.0;
  }

  const std::size_t index = std::min(values.size() - 1, static_cast<std::size_t>(values.size() * share));
  std::nth_element(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(index), values.end());

  return values[index];
}

} // namespace

int main(int argc, char *argv[]) {
  const Options options = parse(argc, argv);
  const std::string body = prepare_body(options);

  std::mutex reported;
  std::atomic<bool> stop{false};
  std::atomic<long> completed{0};
  std::atomic<long> failed{0};
  std::vector<std::vector<double>> samples(static_cast<std::size_t>(options.threads));

  const Clock::time_point started = Clock::now();

  std::vector<std::thread> workers;

  for (int index = 0; index < options.threads; ++index) {
    workers.emplace_back([&, index] {
      const auto client = make_client(options);
      std::vector<double> &mine = samples[static_cast<std::size_t>(index)];

      while (!stop.load(std::memory_order_relaxed)) {
        const Clock::time_point begin = Clock::now();
        const auto response = client->Post(options.path, body, "application/json");
        const Clock::time_point end = Clock::now();

        if (!response || response->status != 200) {
          if (failed.fetch_add(1, std::memory_order_relaxed) == 0) {
            std::lock_guard<std::mutex> lock(reported);
            std::cerr << "первый отказ: " << (response ? std::to_string(response->status) : "нет ответа") << " "
                      << (response ? response->body.substr(0, 300) : "") << '\n';
          }

          continue;
        }

        mine.push_back(std::chrono::duration<double, std::milli>(end - begin).count());
        completed.fetch_add(1, std::memory_order_relaxed);
      }
    });
  }

  std::this_thread::sleep_for(std::chrono::seconds(options.seconds));
  stop.store(true);

  for (auto &worker : workers) {
    worker.join();
  }

  const double elapsed = std::chrono::duration<double>(Clock::now() - started).count();

  std::vector<double> all;
  for (auto &mine : samples) {
    all.insert(all.end(), mine.begin(), mine.end());
  }

  const double total = static_cast<double>(completed.load());
  const double rps = total / elapsed;

  std::printf("%s: потоков %d, %.1f с, успешных %ld, ошибок %ld, RPS %.0f, "
              "задержка p50 %.1f мс, p95 %.1f мс, p99 %.1f мс\n",
              options.path.c_str(), options.threads, elapsed, completed.load(), failed.load(), rps,
              percentile(all, 0.50), percentile(all, 0.95), percentile(all, 0.99));

  return failed.load() == 0 ? 0 : 1;
}
