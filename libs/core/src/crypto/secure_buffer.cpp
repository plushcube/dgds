#include <dgds/core/crypto/secure_buffer.h>

#include <openssl/crypto.h>

#include <unistd.h>

#include <cstddef>
#include <new>

namespace dgds::core {
namespace {

std::size_t page_size() {
  static const std::size_t k_page = static_cast<std::size_t>(::sysconf(_SC_PAGESIZE));

  return k_page;
}

std::size_t round_up(std::size_t size, std::size_t page) { return ((size + page - 1) / page) * page; }

} // namespace

SecureBuffer::SecureBuffer(std::size_t size) : m_size(size), m_protected(round_up(size, page_size())) {
  if (m_protected != 0) {
    m_data = static_cast<unsigned char *>(::operator new(m_protected, std::align_val_t{page_size()}));
  }

  m_protection = protect_memory(m_data, m_protected);
}

SecureBuffer::SecureBuffer(SecureBuffer &&other) noexcept
    : m_data(other.m_data), m_size(other.m_size), m_protected(other.m_protected), m_protection(other.m_protection),
      m_release(other.m_release) {
  other.m_data = nullptr;
  other.m_size = 0;
  other.m_protected = 0;
  other.m_release = MemoryLock::locked;
}

SecureBuffer &SecureBuffer::operator=(SecureBuffer &&other) noexcept {
  if (this != &other) {
    release();

    m_data = other.m_data;
    m_size = other.m_size;
    m_protected = other.m_protected;
    m_protection = other.m_protection;
    m_release = other.m_release;

    other.m_data = nullptr;
    other.m_size = 0;
    other.m_protected = 0;
    other.m_release = MemoryLock::locked;
  }

  return *this;
}

SecureBuffer::~SecureBuffer() { release(); }

void SecureBuffer::wipe() {
  if (m_data != nullptr) {
    OPENSSL_cleanse(m_data, m_protected);
  }
}

MemoryLock SecureBuffer::close() {
  release();

  return m_release;
}

void SecureBuffer::release() {
  if (m_data == nullptr) {
    return;
  }

  wipe();
  m_release = release_memory(m_data, m_protected);
  ::operator delete(m_data, std::align_val_t{page_size()});
  m_data = nullptr;
  m_size = 0;
  m_protected = 0;
}

} // namespace dgds::core
