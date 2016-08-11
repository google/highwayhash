#ifndef HIGHWAYHASH_HIGHWAYHASH_STATE_H_
#define HIGHWAYHASH_HIGHWAYHASH_STATE_H_

// Helper functions to split inputs into packets and call State::Update on each.

#include <cstddef>
#include <cstring>
#include <memory>

#include "highwayhash/code_annotation.h"
#include "highwayhash/types.h"

namespace highwayhash {

// Copies the remaining bytes to a zero-padded buffer, sets the upper byte to
// size % 256 (always possible because this should only be called if the
// total size is not a multiple of the packet size) and updates hash state.
//
// The padding scheme is essentially from SipHash, but permuted for the
// convenience of AVX-2 masked loads. This function must use the same layout so
// that the vector and scalar HighwayTreeHash have the same result.
//
// "remaining_size" is the number of accessible/remaining bytes
// (size % kPacketSize).
//
// Primary template; the specialization for AVX-2 is faster. Intended as an
// implementation detail, do not call directly.
template <class State>
INLINE void PaddedUpdate(const uint64 size, const char* remaining_bytes,
                         const uint64 remaining_size, State* state) {
  alignas(32) char final_packet[State::kPacketSize] = {0};

  // Unusual layout matches the AVX-2 specialization in highway_tree_hash.h.
  const size_t remainder_mod4 = remaining_size & 3;
  uint32 packet4 = static_cast<uint32>(size) << 24;
  const char* final_bytes = remaining_bytes + remaining_size - remainder_mod4;
  for (size_t i = 0; i < remainder_mod4; ++i) {
    const uint32 byte = static_cast<unsigned char>(final_bytes[i]);
    packet4 += byte << (i * 8);
  }

  memcpy(final_packet, remaining_bytes, remaining_size - remainder_mod4);
  memcpy(final_packet + State::kPacketSize - 4, &packet4, sizeof(packet4));

  state->Update(final_packet);
}

// Updates hash state for every whole packet, and once more for the final
// padded packet.
template <class State>
INLINE void UpdateState(const char* bytes, const uint64 size, State* state) {
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

// Convenience function for updating with the bytes of a string.
template <class String, class State>
INLINE void UpdateState(const String& s, State* state) {
  const char* bytes = reinterpret_cast<const char*>(s.data());
  const size_t size = s.length() * sizeof(typename String::value_type);
  UpdateState(bytes, size, state);
}

// Computes a hash of a byte array using the given hash State class.
//
// Example: const SipHashState::Key key = { 1, 2 }; char data[4];
// ComputeHash<SipHashState>(key, data, sizeof(data));
//
// This function avoids duplicating Update/Finalize in every call site.
// Callers wanting to combine multiple hashes should repeatedly UpdateState()
// and only call State::Finalize once.
template <class State>
uint64 ComputeHash(const typename State::Key& key, const char* bytes,
                   const uint64 size) {
  State state(key);
  UpdateState(bytes, size, &state);
  return state.Finalize();
}

// Computes a hash of a string's bytes using the given hash State class.
//
// Example: const SipHashState::Key key = { 1, 2 };
// StringHasher<SipHashState>()(key, std::u16string(u"abc"));
//
// A struct with nested function template enables deduction of the String type.
template <class State>
struct StringHasher {
  template <class String>
  uint64 operator()(const typename State::Key& key, const String& s) {
    State state(key);
    UpdateState(s, &state);
    return state.Finalize();
  }
};

}  // namespace highwayhash

#endif  // HIGHWAYHASH_HIGHWAYHASH_STATE_H_
