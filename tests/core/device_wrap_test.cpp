#include <dgds/core/envelope/device_wrap.h>

#include <dgds/core/envelope/keys.h>

#include <gtest/gtest.h>

namespace {

using dgds::core::CoreError;
using dgds::core::derive_device_public_key;
using dgds::core::generate_device_key;
using dgds::core::generate_key;
using dgds::core::open_for_device;
using dgds::core::seal_for_device;

constexpr const char k_associated_data[] = "идентификатор покупки";

TEST(DeviceWrap, DerivesSamePublicKey) {
  const auto device = generate_device_key();
  ASSERT_TRUE(device.has_value());

  const auto derived = derive_device_public_key(device->private_key);

  ASSERT_TRUE(derived.has_value());
  EXPECT_EQ(derived.value(), device->public_key);
}

TEST(DeviceWrap, UnsealsWithOwnDevice) {
  const auto key = generate_key();
  const auto device = generate_device_key();
  ASSERT_TRUE(key.has_value());
  ASSERT_TRUE(device.has_value());

  const auto envelope = seal_for_device(key.value(), device->public_key, k_associated_data);
  ASSERT_TRUE(envelope.has_value());

  const auto opened = open_for_device(envelope.value(), device->private_key, k_associated_data);

  ASSERT_TRUE(opened.has_value());
  EXPECT_TRUE(opened->equals(key.value()));
}

TEST(DeviceWrap, RejectsForeignDevice) {
  const auto key = generate_key();
  const auto device = generate_device_key();
  const auto foreign = generate_device_key();
  ASSERT_TRUE(key.has_value());
  ASSERT_TRUE(device.has_value());
  ASSERT_TRUE(foreign.has_value());

  const auto envelope = seal_for_device(key.value(), device->public_key, k_associated_data);
  ASSERT_TRUE(envelope.has_value());

  const auto opened = open_for_device(envelope.value(), foreign->private_key, k_associated_data);

  ASSERT_FALSE(opened.has_value());
  EXPECT_EQ(opened.error(), CoreError::authentication_failed);
}

TEST(DeviceWrap, UsesFreshEphemeralKey) {
  const auto key = generate_key();
  const auto device = generate_device_key();
  ASSERT_TRUE(key.has_value());
  ASSERT_TRUE(device.has_value());

  const auto first = seal_for_device(key.value(), device->public_key, k_associated_data);
  const auto second = seal_for_device(key.value(), device->public_key, k_associated_data);

  ASSERT_TRUE(first.has_value());
  ASSERT_TRUE(second.has_value());

  EXPECT_NE(first->ephemeral_key, second->ephemeral_key);
  EXPECT_NE(first->wrapped.nonce, second->wrapped.nonce);
}

} // namespace
