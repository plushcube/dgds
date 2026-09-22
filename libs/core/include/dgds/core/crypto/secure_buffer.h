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

  [[nodiscard]] unsigned char *data() { return reinterpret_cast<unsigned char *>(m_data.data()); }
  [[nodiscard]] const unsigned char *data() const { return reinterpret_cast<const unsigned char *>(m_data.data()); }
  [[nodiscard]] Content view() const { return Content(m_data.data(), m_data.size()); }
  [[nodiscard]] std::size_t size() const { return m_data.size(); }
  [[nodiscard]] bool empty() const { return m_data.empty(); }

private:
  void protect();
  void release();

  std::string m_data;
};

} // namespace dgds::core
