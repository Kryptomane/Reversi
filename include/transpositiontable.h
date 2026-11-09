#pragma once
// Transpositionstabelle: merkt sich Wert, Tiefe und besten Zug bereits untersuchter Stellungen.
// Lock-frei fuer mehrere Threads: gespeichert wird (key ^ data, data); ein halb geschriebener
// Eintrag passt dann nicht mehr zum Schluessel und wird ignoriert.
#include "bitboard.h"
#include <atomic>
#include <memory>

namespace engine
{
	using bb::u64;

	enum class Bound : uint8_t { None = 0, Exact = 1, Lower = 2, Upper = 3 };

	struct TTEntry
	{
		int score = 0;
		int depth = 0;
		Bound bound = Bound::None;
		int move = -1;
	};

	class TranspositionTable
	{
	public:
		explicit TranspositionTable(int bits) : entries(new Slot[size_t(1) << bits]), mask((u64(1) << bits) - 1) {}

		static u64 hash(u64 P, u64 O)
		{
			return mix(P) ^ mix(O ^ 0x5851F42D4C957F2DULL);
		}

		bool probe(u64 key, TTEntry& out) const
		{
			const u64 i = bucket(key);
			return read(entries[i], key, out) || read(entries[i + 1], key, out);
		}

		// Je Bucket zwei Plaetze: [0] behaelt den tiefsten Eintrag, [1] nimmt immer den neuesten.
		void store(u64 key, const TTEntry& e)
		{
			const u64 i = bucket(key);
			Slot& deep = entries[i];
			const u64 old = deep.data.load(std::memory_order_relaxed);
			const bool sameKey = (deep.keyx.load(std::memory_order_relaxed) ^ old) == key;
			Slot& target = (old == 0 || sameKey || e.depth >= unpack(old).depth) ? deep : entries[i + 1];
			const u64 data = pack(e);
			target.data.store(data, std::memory_order_relaxed);
			target.keyx.store(key ^ data, std::memory_order_relaxed);
		}

		void clear()
		{
			for (u64 i = 0; i <= mask; i++)
			{
				entries[i].keyx.store(0, std::memory_order_relaxed);
				entries[i].data.store(0, std::memory_order_relaxed);
			}
		}

	private:
		struct Slot { std::atomic<u64> keyx{ 0 }; std::atomic<u64> data{ 0 }; };

		u64 bucket(u64 key) const { return key & mask & ~1ULL; }

		static u64 mix(u64 z) // splitmix64
		{
			z += 0x9E3779B97F4A7C15ULL;
			z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
			z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
			return z ^ (z >> 31);
		}

		static u64 pack(const TTEntry& e)
		{
			return (u64)(uint32_t)e.score | ((u64)(uint8_t)e.depth << 32) | ((u64)(uint8_t)e.bound << 40)
				| ((u64)(uint8_t)(int8_t)e.move << 48);
		}

		static TTEntry unpack(u64 d)
		{
			TTEntry e;
			e.score = (int)(int32_t)(uint32_t)(d & 0xFFFFFFFFULL);
			e.depth = (int)(int8_t)(uint8_t)(d >> 32);
			e.bound = (Bound)(uint8_t)(d >> 40);
			e.move = (int)(int8_t)(uint8_t)(d >> 48);
			return e;
		}

		static bool read(const Slot& s, u64 key, TTEntry& out)
		{
			const u64 data = s.data.load(std::memory_order_relaxed);
			const u64 keyx = s.keyx.load(std::memory_order_relaxed);
			if (data == 0 || (keyx ^ data) != key)
				return false;
			out = unpack(data);
			return out.bound != Bound::None;
		}

		std::unique_ptr<Slot[]> entries;
		u64 mask;
	};
}
