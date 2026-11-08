// Tests fuer die Engine (ohne Qt/OpenCV):
//   g++ -O2 -std=c++20 -Iinclude tests/engine_test.cpp src/engine.cpp -o engine_test
#include "engine.h"
#include <cstdio>
#include <random>
#include <functional>

using namespace engine;
using bb::u64;

static int brute(u64 P, u64 O, bool passed) // Minimax ohne Schnitte, Wertung wie Engine-Terminal
{
	u64 mv = bb::moves(P, O);
	if (!mv)
	{
		if (passed)
		{
			int p = bb::popcount(P), o = bb::popcount(O), e = 64 - p - o;
			return p > o ? WIN + p - o + e : (p < o ? -WIN + p - o - e : 0);
		}
		return -brute(O, P, true);
	}
	int best = -WIN - 100;
	while (mv)
	{
		u64 m = mv & (0 - mv); mv &= mv - 1;
		u64 f = bb::flips(P, O, m);
		best = std::max(best, -brute(O & ~f, P | f | m, false));
	}
	return best;
}

static void start(u64& B, u64& W) { B = bb::bit(3, 4) | bb::bit(4, 3); W = bb::bit(3, 3) | bb::bit(4, 4); }

// Spielt eine Partie; spieler(P,O) liefert Feldindex. Rueckgabe: Schwarz - Weiss.
static int play(std::function<int(u64, u64)> black, std::function<int(u64, u64)> white)
{
	u64 B, W; start(B, W);
	bool blackToMove = true;
	int passes = 0;
	while (passes < 2)
	{
		u64& P = blackToMove ? B : W;
		u64& O = blackToMove ? W : B;
		if (!bb::moves(P, O)) { passes++; blackToMove = !blackToMove; continue; }
		passes = 0;
		int s = blackToMove ? black(P, O) : white(P, O);
		u64 m = 1ULL << s;
		if (!(bb::moves(P, O) & m)) { printf("ILLEGALER ZUG\n"); return -999; }
		u64 f = bb::flips(P, O, m);
		P |= f | m; O &= ~f;
		blackToMove = !blackToMove;
	}
	return bb::popcount(B) - bb::popcount(W);
}

int main(int argc, char** argv)
{
	const int egThreads = argc > 1 ? atoi(argv[1]) : 0;
	const bool onlyEndgame = argc > 2;
	std::mt19937 rng(7);
	int errors = 0;

	// 1) Bewertung antisymmetrisch, 2) Endspiel exakt gegen Brute-Force
	int checked = 0;
	Engine eng(18);
	for (int g = 0; g < 300; g++)
	{
		u64 P, O; start(P, O);
		bool passed = false;
		for (;;)
		{
			if (evaluate(P, O) != -evaluate(O, P)) errors++;
			int empties = bb::popcount(~(P | O));
			if (empties <= 9 && bb::popcount(bb::moves(P, O)) > 1)
			{
				Settings s; s.timeMs = 100000; s.threads = egThreads;
				Result r = eng.search(P, O, s);
				int b = brute(P, O, false);
				if (r.score != b || !r.exact) { if (errors < 10) printf("Endspiel-Fehler: engine %d brute %d\n", r.score, b); errors++; }
				// Zug muss den optimalen Wert erreichen
				u64 m = 1ULL << r.move; u64 f = bb::flips(P, O, m);
				if (-brute(O & ~f, P | f | m, false) != b) { errors++; printf("Zug nicht optimal\n"); }
				checked++;
				break;
			}
			u64 mv = bb::moves(P, O);
			if (!mv) { if (passed) break; passed = true; std::swap(P, O); continue; }
			passed = false;
			int k = rng() % bb::popcount(mv);
			while (k--) mv &= mv - 1;
			u64 m = mv & (0 - mv), f = bb::flips(P, O, m);
			u64 nP = O & ~f, nO = P | f | m; P = nP; O = nO;
		}
	}
	printf("Endspiel exakt geprueft: %d Stellungen, Fehler: %d\n", checked, errors);
	if (onlyEndgame) return errors ? 1 : 0;

	// 3) Spielstaerke: Engine (50 ms) gegen Zufall und gegen 1-Zug-Gierig
	auto randomPlayer = [&](u64 P, u64 O) { u64 mv = bb::moves(P, O); int k = rng() % bb::popcount(mv); while (k--) mv &= mv - 1; return bb::lsb(mv); };
	auto greedy = [&](u64 P, u64 O) { // maximiert Steine nach dem Zug, Zufall bei Gleichstand
		u64 mv = bb::moves(P, O); int best = -1, bs = -1, cnt = 0;
		while (mv) { int s = bb::lsb(mv); mv &= mv - 1; int v = bb::popcount(bb::flips(P, O, 1ULL << s));
			if (v > best) { best = v; bs = s; cnt = 1; } else if (v == best && rng() % (++cnt) == 0) bs = s; }
		return bs; };
	Engine e2(20);
	Settings fast; fast.timeMs = 50;
	auto eng50 = [&](u64 P, u64 O) { return e2.search(P, O, fast).move; };
	int w = 0, games = 0;
	for (int g = 0; g < 20; g++) { int d = g % 2 ? -play(randomPlayer, eng50) : play(eng50, randomPlayer); w += d > 0; games++; }
	printf("Engine(50ms) vs Zufall : %d/%d gewonnen\n", w, games);
	w = 0; games = 0;
	for (int g = 0; g < 20; g++) { int d = g % 2 ? -play(greedy, eng50) : play(eng50, greedy); w += d > 0; games++; }
	printf("Engine(50ms) vs Gierig : %d/%d gewonnen\n", w, games);

	// 4) Geschwindigkeit: Startstellung nach 4 Zuegen, 2 s
	{
		u64 P, O; start(P, O);
		Engine e3(22);
		Settings s; s.timeMs = 2000;
		Result r = e3.search(P, O, s);
		printf("Startstellung 2 s: Tiefe %d, %lld Knoten, %.2f s, %.1f Mio Knoten/s, Zug %d,%d\n",
			r.depth, r.nodes, r.seconds, r.nodes / r.seconds / 1e6, r.move / 8, r.move % 8);
	}
	// 4b) Parallelisierung: Zeit bis Tiefe 13 mit 1 Thread und mit allen Kernen
	for (int th : {1, 0})
	{
		u64 P, O; start(P, O);
		Engine e5(22);
		Settings s; s.timeMs = 600000; s.maxDepth = 13; s.threads = th;
		Result r = e5.search(P, O, s);
		printf("Tiefe 13, %d Thread(s): %.2f s, %lld Knoten, Zug %d,%d Wert %d\n", r.threads, r.seconds, r.nodes, r.move / 8, r.move % 8, r.score);
	}
	// 5) Endspiel mit 20 freien Feldern: wie lange dauert exakt?
	{
		u64 P = 0, O = 0; int found = 0;
		std::mt19937 r2(99);
		while (!found)
		{
			start(P, O); bool passed = false; bool ok = true;
			while (bb::popcount(~(P | O)) > 20)
			{
				u64 mv = bb::moves(P, O);
				if (!mv) { if (passed) { ok = false; break; } passed = true; std::swap(P, O); continue; }
				passed = false; int k = r2() % bb::popcount(mv); while (k--) mv &= mv - 1;
				u64 m = mv & (0 - mv), f = bb::flips(P, O, m); u64 nP = O & ~f, nO = P | f | m; P = nP; O = nO;
			}
			found = ok && bb::moves(P, O);
		}
		Engine e4(22);
		Settings s; s.timeMs = 30000;
		Result r = e4.search(P, O, s);
		printf("20 Leerfelder: exakt=%d wld=%d Wert %d, %.2f s, %lld Knoten\n", r.exact, r.wld, r.score, r.seconds, r.nodes);
	}
	return errors ? 1 : 0;
}
