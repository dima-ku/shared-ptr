//
// Created by dmitryk on 04.10.23.
//

#include "shared-ptr.h"

namespace details {

control_block_base::~control_block_base() = default;

void control_block_base::add_strong_ref() noexcept {
  strong_refs_++;
}

void control_block_base::release_strong_ref() noexcept {
  if (--strong_refs_ == 0) {
    delete_data();
    release_weak_ref();
  }
}

void control_block_base::add_weak_ref() noexcept {
  weak_refs_++;
}

void control_block_base::release_weak_ref() noexcept {
  if (--weak_refs_ == 0) {
    delete this;
  }
}

std::size_t control_block_base::get_strong_refs() const noexcept {
  return strong_refs_;
}

} // namespace details
