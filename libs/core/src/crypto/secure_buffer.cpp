#include <dgds/core/crypto/secure_buffer.h>

#include <openssl/crypto.h>

#include <sys/mman.h>

#include <cstddef>
#include <utility>

namespace dgds::core {

SecureBuffer::SecureBuffer(std::size_t size) : m_data(size, '\0') { protect(); }

SecureBuffer::SecureBuffer(SecureBuffer &&other) noexcept : m_data(std::move(other.m_data)) {
  protect();
  other.release();
}

SecureBuffer &SecureBuffer::operator=(SecureBuffer &&other) noexcept {
  if (this != &other) {
    release();
    m_data = std::move(other.m_data);
    protect();
    other.release();
  }

  return *this;
}

SecureBuffer::~SecureBuffer() { release(); }

void SecureBuffer::wipe() {
  if (!m_data.empty()) {
    OPENSSL_cleanse(m_data.data(), m_data.size());
  }
}

unsigned char *SecureBuffer::data() { return reinterpret_cast<unsigned char *>(m_data.data()); }

const unsigned char *SecureBuffer::data() const { return reinterpret_cast<const unsigned char *>(m_data.data()); }

Content SecureBuffer::view() const { return Content(m_data.data(), m_data.size()); }

std::size_t SecureBuffer::size() const { return m_data.size(); }

bool SecureBuffer::empty() const { return m_data.empty(); }

void SecureBuffer::protect() {
  if (m_data.empty()) {
    return;
  }

  ::mlock(m_data.data(), m_data.size());

#if defined(MADV_DONTDUMP)
  ::madvise(m_data.data(), m_data.size(), MADV_DONTDUMP);
#endif
}

void SecureBuffer::release() {
  wipe();

  if (!m_data.empty()) {
    ::munlock(m_data.data(), m_data.size());
  }

  m_data.clear();
}

} // namespace dgds::core
