#pragma once

// Reading the game's input-context stack (ControlMap::contextPriorityStack) without trusting the header's layout.
//
// WHY (1.7.1, a player's crash on Skyrim AE 1.6.1170, crash-2026-11-12-04-45-32.log): InputInject's Splice read the
// stack as the plain member of the pinned CommonLibSSE-NG checkout (b93280e), which lays ControlMap out the SE way -
// the stack at +0x100. Skyrim 1.6.1130 added an input context (Marketplace), so ControlMap's context table grew by one
// pointer and every member after it moved down 8 bytes: on 1.6.1130 and later the stack is at +0x108. Read at +0x100 on
// 1.6.1170, the "data pointer" was the linked-mappings COUNT (0x43) and the "size" was the real stack's capacity (10);
// the depth <= 64 guard passed and stack[9] read address 0x67. Newer CommonLibSSE-NG (3.7, 7.2) has a versioned
// GetRuntimeData() for exactly this (base 0xE8 on SE and AE before 1.6.1130, 0xF0 from 1.6.1130, 0x108 on VR); this
// checkout has none, so the offset is chosen here from the running game's version, and the array is checked to look
// like an array BEFORE it is indexed - a pointer below 64 KiB, a size above its capacity or a depth past 64 is a
// misread, and the caller falls back to the plain gameplay lookup.
//
// Header-only and free of CommonLib so tests/context_stack_read_test.cpp can feed it the crash's own bytes.

#include <cstddef>
#include <cstdint>
#include <cstring>

#if defined(_MSC_VER)
#	include <excpt.h>
#endif

namespace InputInject::ContextStack
{
	// BSTArray<InputContextID>: data pointer +0x00, capacity (u32) +0x08, size (u32) +0x10.
	inline constexpr std::size_t kDataAt = 0x00;
	inline constexpr std::size_t kCapacityAt = 0x08;
	inline constexpr std::size_t kSizeAt = 0x10;

	// Where contextPriorityStack sits inside ControlMap (RUNTIME_DATA base + 0x18).
	inline constexpr std::size_t kOffsetSE = 0x100;      // SE 1.5.x, and AE before 1.6.1130 (RUNTIME_DATA at 0xE8)
	inline constexpr std::size_t kOffsetAE1130 = 0x108;  // AE 1.6.1130 and later, 1.7.x included (RUNTIME_DATA at 0xF0)
	inline constexpr std::size_t kOffsetVR = 0x120;      // VR (RUNTIME_DATA at 0x108)

	inline constexpr std::uint32_t kMaxDepth = 64;
	inline constexpr std::uintptr_t kLowestValidPointer = 0x10000;        // Windows never maps the first 64 KiB
	inline constexpr std::uintptr_t kHighestUserPointer = 0x7FFFFFFFFFFF;  // x64 user space

	// a_major.a_minor.a_patch is the running game (REL::Module::get().version()).
	[[nodiscard]] constexpr std::size_t OffsetFor(bool a_isVR, std::uint32_t a_major, std::uint32_t a_minor, std::uint32_t a_patch) noexcept
	{
		if (a_isVR) {
			return kOffsetVR;
		}
		if (a_major > 1 || (a_major == 1 && (a_minor > 6 || (a_minor == 6 && a_patch >= 1130)))) {
			return kOffsetAE1130;
		}
		return kOffsetSE;
	}

	struct Result
	{
		std::uint32_t count = 0;            // ids copied, bottom of the stack first
		std::uint32_t ids[kMaxDepth]{};
		const char*   refused = nullptr;    // why the array was not read; nullptr when it was
	};

	namespace detail
	{
		// Copies a_bytes from a_src, or returns false if the read faults. Plain data only, so SEH is allowed here.
		inline bool GuardedCopy(void* a_dst, const void* a_src, std::size_t a_bytes) noexcept
		{
#if defined(_MSC_VER)
			__try {
				std::memcpy(a_dst, a_src, a_bytes);
				return true;
			} __except (EXCEPTION_EXECUTE_HANDLER) {
				return false;
			}
#else
			std::memcpy(a_dst, a_src, a_bytes);
			return true;
#endif
		}
	}

	// Reads the stack at a_controlMap + a_offset. Context ids at or past a_contextTotal are dropped (not ours to index).
	[[nodiscard]] inline Result Read(const void* a_controlMap, std::size_t a_offset, std::uint32_t a_contextTotal) noexcept
	{
		Result r{};
		if (!a_controlMap) {
			r.refused = "no ControlMap";
			return r;
		}
		const auto* base = static_cast<const std::uint8_t*>(a_controlMap) + a_offset;
		std::uintptr_t data = 0;
		std::uint32_t  capacity = 0;
		std::uint32_t  size = 0;
		if (!detail::GuardedCopy(&data, base + kDataAt, sizeof(data)) ||
			!detail::GuardedCopy(&capacity, base + kCapacityAt, sizeof(capacity)) ||
			!detail::GuardedCopy(&size, base + kSizeAt, sizeof(size))) {
			r.refused = "header unreadable";
			return r;
		}
		if (size == 0) {
			return r;   // an empty stack is a real answer
		}
		if (data < kLowestValidPointer || data > kHighestUserPointer || (data % alignof(std::uint32_t)) != 0) {
			r.refused = "data pointer is not a heap pointer";
			return r;
		}
		if (size > capacity) {
			r.refused = "size above capacity";
			return r;
		}
		if (size > kMaxDepth) {
			r.refused = "deeper than 64";
			return r;
		}
		std::uint32_t raw[kMaxDepth]{};
		if (!detail::GuardedCopy(raw, reinterpret_cast<const void*>(data), size * sizeof(std::uint32_t))) {
			r.refused = "array unreadable";
			return r;
		}
		for (std::uint32_t i = 0; i < size; ++i) {
			if (raw[i] < a_contextTotal) {
				r.ids[r.count++] = raw[i];
			}
		}
		return r;
	}
}
