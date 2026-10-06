#pragma once

#include <dgds/core/crypto/memory_protection.h>
#include <dgds/core/models/content.h>

#include <cstddef>

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

  [[nodiscard]] MemoryLock close();

  [[nodiscard]] MemoryProtection protection() const { return m_protection; }
  [[nodiscard]] unsigned char *data() { return m_data; }
  [[nodiscard]] const unsigned char *data() const { return m_data; }
  [[nodiscard]] Content view() const { return Content(reinterpret_cast<const char *>(m_data), m_size); }
  [[nodiscard]] std::size_t size() const { return m_size; }
  [[nodiscard]] bool empty() const { return m_size == 0; }

private:
  void release();

  unsigned char *m_data = nullptr;
  std::size_t m_size = 0;
  std::size_t m_protected = 0;
  MemoryProtection m_protection{};
  MemoryLock m_release{MemoryLock::locked};
};

} // namespace dgds::core
