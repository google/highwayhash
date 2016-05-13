#ifndef HIGHWAYHASH_STATE_H_
#define HIGHWAYHASH_STATE_H_

// Helper functions to split inputs into packets and call State::Update on each.

#include "code_annotation.h"

// Copies the remaining bytes to a zero-padded buffer, sets the upper byte to
// size % 256 (always possible because this should only be called if the
// total size is not a multiple of the packet size) and updates hash state.
// "remaining_size" is the number of accessible/remaining bytes (size % 32).
//
// Primary template; the specialization for AVX-2 is faster. Intended as an
// implementation detail, do not call directly.
template <class State>
INLINE void PaddedUpdate(const uint64_t size, const uint8_t* remaining_bytes,
                         const uint64_t remaining_size, State* state) {
  // Copy to avoid overrunning the input buffer.
  uint8_t final_packet[State::kPacketSize] = {0};
  memcpy(final_packet, remaining_bytes, remaining_size);
  final_packet[State::kPacketSize - 1] = static_cast<uint8_t>(size & 0xFF);
  state->Update(final_packet);
}

// Updates hash state for every whole packet, and once more for the final
// padded packet.
template <class State>
INLINE void UpdateState(const uint8_t* bytes, const uint64_t size,
                        State* state) {
  // Feed entire packets.
  const int kPacketSize = State::kPacketSize;
  static_assert((kPacketSize & (kPacketSize - 1)) == 0, "Size must be 2^i.");
  const size_t remainder = size & (kPacketSize - 1);
  const size_t truncated_size = size - remainder;
  for (size_t i = 0; i < truncated_size; i += kPacketSize) {
    state->Update(bytes + i);
  }

  PaddedUpdate(size, bytes + truncated_size, remainder, state);
}

// Convenience function for hashing strings.
template <class String, class State>
INLINE void UpdateState(const String& s, State* state) {
  const uint8_t* bytes = reinterpret_cast<const uint8_t*>(s.data());
  const size_t size = s.length() * sizeof(typename String::value_type);
  UpdateState(bytes, size, state);
}

#endif  // HIGHWAYHASH_STATE_H_
