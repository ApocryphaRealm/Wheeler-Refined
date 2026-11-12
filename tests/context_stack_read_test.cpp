// Offline check for the 1.7.1 fix: InputInject's read of the game's input-context stack.
//
// It rebuilds the bytes a player's Skyrim AE 1.6.1170 ControlMap held when 1.7.0 crashed
// (crash-2026-11-12-04-45-32.log: AV reading 0x67 at wheeler.dll+0095250, rax=0x43, rbx=9) and runs two readers over them:
//
//   default   - the 1.7.1 reader (src/bin/DevBench/ContextStackRead.h): must pick +0x108 on 1.6.1170, read the real stack
//               there, and REFUSE (not fault) when pointed at the old +0x100;
//   --legacy  - the 1.7.0 algorithm, line for line: member at +0x100, depth <= 64 checked, pointer never checked. It faults
//               reading 0x67 on these bytes, exactly as the player's game did, and the run FAILS (exit 1).
//
// Build and run: tests\run-tests.bat (exit 0 = pass).

#include "../src/bin/DevBench/ContextStackRead.h"

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <vector>

namespace CS = InputInject::ContextStack;

static int g_failures = 0;
#define CHECK(cond)                                                          \
	do {                                                                     \
		if (cond) {                                                          \
			std::printf("  ok    %s\n", #cond);                              \
		} else {                                                             \
			std::printf("  FAIL  %s  (line %d)\n", #cond, __LINE__);         \
			++g_failures;                                                    \
		}                                                                    \
	} while (0)

template <class T>
static void Put(std::vector<std::uint8_t>& a_blob, std::size_t a_at, T a_value)
{
	std::memcpy(a_blob.data() + a_at, &a_value, sizeof(T));
}

// ControlMap as laid out on Skyrim 1.6.1130+ (0x130 bytes): linkedMappings at 0xF0, contextPriorityStack at 0x108.
static std::vector<std::uint8_t> MakeAE1170(const std::uint32_t* a_stackIds, std::uint32_t a_count)
{
	std::vector<std::uint8_t> blob(0x130, 0);
	static std::uint8_t linkedStorage[0x43 * 0x20]{};
	Put<std::uintptr_t>(blob, 0xF0, reinterpret_cast<std::uintptr_t>(linkedStorage));  // linkedMappings.data
	Put<std::uint32_t>(blob, 0xF8, 0x50);                                               // linkedMappings.capacity
	Put<std::uint32_t>(blob, 0x100, 0x43);                                              // linkedMappings.size = 67 -> the "rax" of the crash
	Put<std::uintptr_t>(blob, 0x108, reinterpret_cast<std::uintptr_t>(a_stackIds));    // contextPriorityStack.data
	Put<std::uint32_t>(blob, 0x110, 10);                                                // .capacity = 10 -> rbx = 10 - 1 = 9
	Put<std::uint32_t>(blob, 0x118, a_count);                                           // .size
	return blob;
}

// ControlMap as laid out on SE 1.5.97 (and AE before 1.6.1130): contextPriorityStack at 0x100.
static std::vector<std::uint8_t> MakeSE(const std::uint32_t* a_stackIds, std::uint32_t a_count)
{
	std::vector<std::uint8_t> blob(0x128, 0);
	Put<std::uintptr_t>(blob, 0x100, reinterpret_cast<std::uintptr_t>(a_stackIds));
	Put<std::uint32_t>(blob, 0x108, 8);
	Put<std::uint32_t>(blob, 0x110, a_count);
	return blob;
}

// The 1.7.0 Splice loop, as compiled (wheeler.dll 1.7.0 Line-SE-AE16: size from +0x110, data from +0x100).
static std::uint32_t LegacyTopContext(const std::uint8_t* a_controlMap, std::uintptr_t* a_faultAt)
{
	const auto* data = *reinterpret_cast<const std::uint32_t* const*>(a_controlMap + 0x100);
	const std::uint32_t size = *reinterpret_cast<const std::uint32_t*>(a_controlMap + 0x110);
	constexpr std::uint32_t kMaxContextDepth = 64;
	*a_faultAt = reinterpret_cast<std::uintptr_t>(data) + (size ? (size - 1) * 4 : 0);
	if (size <= kMaxContextDepth) {
		for (std::uint32_t i = size; i > 0; --i) {
			const auto context = data[i - 1];   // InputInject.cpp:42 in 1.7.0
			if (context >= 17) { continue; }
			return context;
		}
	}
	return 0xFFFFFFFF;
}

static int RunLegacyOn(const std::uint8_t* a_blob)
{
	std::uintptr_t faultAt = 0;
	__try {
		const auto top = LegacyTopContext(a_blob, &faultAt);
		std::printf("legacy reader returned context %u without faulting\n", top);
		return 0;
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		std::printf("FAIL  legacy (1.7.0) reader: access violation reading 0x%llX - the player's crash, reproduced\n",
			static_cast<unsigned long long>(faultAt));
		return 1;
	}
}

static int RunLegacy()
{
	static const std::uint32_t ids[] = { 0, 1, 4 };
	const auto blob = MakeAE1170(ids, 3);
	return RunLegacyOn(blob.data());
}

int main(int argc, char** argv)
{
	if (argc > 1 && std::strcmp(argv[1], "--legacy") == 0) {
		return RunLegacy();
	}

	std::printf("offset choice\n");
	CHECK(CS::OffsetFor(false, 1, 5, 97) == 0x100);
	CHECK(CS::OffsetFor(false, 1, 6, 640) == 0x100);
	CHECK(CS::OffsetFor(false, 1, 6, 1129) == 0x100);
	CHECK(CS::OffsetFor(false, 1, 6, 1130) == 0x108);
	CHECK(CS::OffsetFor(false, 1, 6, 1170) == 0x108);
	CHECK(CS::OffsetFor(false, 1, 7, 0) == 0x108);
	CHECK(CS::OffsetFor(true, 1, 4, 15) == 0x120);

	std::printf("AE 1.6.1170 bytes from the crash\n");
	static const std::uint32_t aeIds[] = { 0, 1, 4 };   // gameplay, menu mode, inventory
	const auto ae = MakeAE1170(aeIds, 3);
	const auto good = CS::Read(ae.data(), CS::OffsetFor(false, 1, 6, 1170), 18);
	CHECK(good.refused == nullptr);
	CHECK(good.count == 3);
	CHECK(good.count == 3 && good.ids[2] == 4);
	const auto old = CS::Read(ae.data(), 0x100, 17);   // the 1.7.0 offset on the same bytes: refused, no fault
	CHECK(old.refused != nullptr);
	CHECK(old.count == 0);

	std::printf("SE 1.5.97 bytes\n");
	static const std::uint32_t seIds[] = { 0, 6 };   // gameplay, favorites
	const auto se = MakeSE(seIds, 2);
	const auto seRead = CS::Read(se.data(), CS::OffsetFor(false, 1, 5, 97), 17);
	CHECK(seRead.refused == nullptr);
	CHECK(seRead.count == 2 && seRead.ids[1] == 6);

	std::printf("misreads that must be refused\n");
	auto bad = MakeAE1170(aeIds, 3);
	Put<std::uint32_t>(bad, 0x118, 11);   // size above capacity
	CHECK(CS::Read(bad.data(), 0x108, 18).refused != nullptr);
	auto deep = MakeAE1170(aeIds, 3);
	Put<std::uint32_t>(deep, 0x110, 500);
	Put<std::uint32_t>(deep, 0x118, 100);   // deeper than 64
	CHECK(CS::Read(deep.data(), 0x108, 18).refused != nullptr);
	auto unmapped = MakeAE1170(aeIds, 3);
	void* released = VirtualAlloc(nullptr, 0x10000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	VirtualFree(released, 0, MEM_RELEASE);   // a real heap-looking address, now unmapped: the guarded copy must catch it
	Put<std::uintptr_t>(unmapped, 0x108, reinterpret_cast<std::uintptr_t>(released));
	CHECK(CS::Read(unmapped.data(), 0x108, 18).refused != nullptr);
	CHECK(CS::Read(nullptr, 0x108, 18).refused != nullptr);
	static const std::uint32_t foreignIds[] = { 0, 99 };   // an id past the context table is dropped, never indexed
	const auto foreign = MakeAE1170(foreignIds, 2);
	const auto f = CS::Read(foreign.data(), 0x108, 18);
	CHECK(f.refused == nullptr && f.count == 1 && f.ids[0] == 0);

	std::printf(g_failures ? "FAILED (%d)\n" : "PASSED\n", g_failures);
	return g_failures ? 1 : 0;
}
