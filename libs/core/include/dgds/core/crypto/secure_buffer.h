#pragma once

#include <dgds/core/models/content.h>

#include <cstddef>
#include <string>

namespace dgds::core {

class SecureBuffer {
public:
  explicit SecureBuffer(std::size_t size);
  SecureBuffer(SecureBuffer &&other) noexcept;
  SecureBuffer &operator=(SecureBuffer &&other) noexcept;
  SecureBuffer(const SecureBuffer &) = delete;
  SecureBuffer &operator=(const SecureBuffer &) = delete;
  ~SecureBuffer();

  void wipe();

  [[nodiscard]] unsigned char *data();
  [[nodiscard]] const unsigned char *data() const;
  [[nodiscard]] Content view() const;
  [[nodiscard]] std::size_t size() const;
  [[nodiscard]] bool empty() const;

private:
  void protect();
  void release();

  std::string m_data;
};

} // namespace dgds::core
