#pragma once
// Hilfsfunktionen fuer Tests und Benchmark: Spieler-Typen, Partien, Zufallsstellungen.
#include "engine.h"
#include <functional>
#include <random>
#include <vector>

namespace testutil
{
	using bb::u64;
	using Player = std::function<int(u64 P, u64 O)>; // liefert einen legalen Zug (Feldindex)

	inline std::mt19937& rng()
	{
		static std::mt19937 r(12345);
		return r;
	}

	inline int randomMove(u64 P, u64 O)
	{
		u64 mv = bb::moves(P, O);
		for (int k = rng()() % bb::popcount(mv); k > 0; k--)
			mv &= mv - 1;
		return bb::lsb(mv);
	}

	// Nimmt den Zug mit den meisten umgedrehten Steinen (typische Anfaenger-Strategie).
	inline int greedyMove(u64 P, u64 O)
	{
		int best = -1, bestSq = -1;
		for (u64 mv = bb::moves(P, O); mv; mv &= mv - 1)
		{
			const int s = bb::lsb(mv);
			const int v = bb::popcount(bb::flips(P, O, 1ULL << s));
			if (v > best || (v == best && rng()() % 2)) { best = v; bestSq = s; }
		}
		return bestSq;
	}

	inline Player enginePlayer(engine::Engine& e, engine::Settings s)
	{
		return [&e, s](u64 P, u64 O) { return e.search(P, O, s).move; };
	}

	// Spielt ab (B, W) bis zum Ende; Rueckgabe: Steine Schwarz - Weiss.
	inline int playGame(const Player& black, const Player& white, u64 B, u64 W, bool blackToMove)
	{
		for (int passes = 0; passes < 2;)
		{
			u64& P = blackToMove ? B : W;
			u64& O = blackToMove ? W : B;
			if (!bb::moves(P, O)) { passes++; blackToMove = !blackToMove; continue; }
			passes = 0;
			const bb::Position next = bb::play(P, O, 1ULL << (blackToMove ? black : white)(P, O));
			O = next.P;
			P = next.O;
			blackToMove = !blackToMove;
		}
		return bb::popcount(B) - bb::popcount(W);
	}

	// Punkte von a gegen b (Sieg 1, Remis 0.5); je Eroeffnung zwei Partien mit Farbwechsel.
	inline double match(const Player& a, const Player& b, int games, int openingPlies = 4)
	{
		double points = 0;
		for (int g = 0; g < games; g += 2)
		{
			u64 B = bb::START_BLACK, W = bb::START_WHITE;
			bool blackToMove = true;
			for (int i = 0; i < openingPlies; i++)
			{
				u64& P = blackToMove ? B : W;
				u64& O = blackToMove ? W : B;
				const bb::Position next = bb::play(P, O, 1ULL << randomMove(P, O));
				O = next.P; P = next.O;
				blackToMove = !blackToMove;
			}
			const int d1 = playGame(a, b, B, W, blackToMove);
			const int d2 = -playGame(b, a, B, W, blackToMove);
			points += (d1 > 0) + 0.5 * (d1 == 0) + (d2 > 0) + 0.5 * (d2 == 0);
		}
		return points;
	}

	// Stellungen mit 'discs' Steinen aus halbzufaelligen Partien (deterministisch je Seed).
	inline std::vector<bb::Position> samplePositions(int discs, int count, unsigned seed)
	{
		std::mt19937 r(seed);
		engine::Engine e(16);
		engine::Settings quick;
		quick.maxDepth = 2; quick.threads = 1; quick.endgameEmpties = 0; quick.timeMs = 100000;
		std::vector<bb::Position> out;
		while ((int)out.size() < count)
		{
			u64 P = bb::START_BLACK, O = bb::START_WHITE;
			bool ok = true, passed = false;
			for (int ply = 0; bb::popcount(P | O) < discs; ply++)
			{
				u64 mv = bb::moves(P, O);
				if (!mv) { if (passed) { ok = false; break; } passed = true; std::swap(P, O); continue; }
				passed = false;
				int s;
				if (ply < 6 || r() % 3 == 0) { for (int k = r() % bb::popcount(mv); k > 0; k--) mv &= mv - 1; s = bb::lsb(mv); }
				else s = e.search(P, O, quick).move;
				const bb::Position next = bb::play(P, O, 1ULL << s);
				P = next.P; O = next.O;
			}
			if (ok && bb::popcount(bb::moves(P, O)) > 1)
				out.push_back({ P, O });
		}
		return out;
	}
}
