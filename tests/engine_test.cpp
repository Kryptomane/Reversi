// Tests fuer Zuggenerator, Spielablauf, Bewertung und Engine (ohne Qt). Rueckgabe 0 = alles gruen.
//   g++ -O2 -std=c++20 -pthread -Iinclude -Itests tests/engine_test.cpp src/engine.cpp src/evaluation.cpp src/game.cpp
#include "testutil.h"
#include "evaluation.h"
#include "game.h"
#include <cstdio>

using namespace testutil;
static int failures = 0;

static void check(bool ok, const char* what)
{
	printf("%-58s %s\n", what, ok ? "ok" : "FEHLER");
	if (!ok) failures++;
}

static long long perft(u64 P, u64 O, int depth, bool passed)
{
	if (depth == 0) return 1;
	u64 mv = bb::moves(P, O);
	if (!mv) return passed ? 1 : perft(O, P, depth - 1, true);
	long long n = 0;
	for (; mv; mv &= mv - 1)
	{
		const bb::Position next = bb::play(P, O, mv & (0 - mv));
		n += perft(next.P, next.O, depth - 1, false);
	}
	return n;
}

// Minimax ohne Schnitte bis zum Spielende (Referenz fuer die Endspielrechnung).
static int bruteForce(u64 P, u64 O, bool passed)
{
	u64 mv = bb::moves(P, O);
	if (!mv)
	{
		if (!passed) return -bruteForce(O, P, true);
		const int d = bb::popcount(P) - bb::popcount(O), e = bb::popcount(~(P | O));
		return d > 0 ? engine::WIN + d + e : (d < 0 ? -engine::WIN + d - e : 0);
	}
	int best = -engine::INF;
	for (; mv; mv &= mv - 1)
	{
		const bb::Position next = bb::play(P, O, mv & (0 - mv));
		best = std::max(best, -bruteForce(next.P, next.O, false));
	}
	return best;
}

static void testPerft()
{
	const long long expected[] = { 4, 12, 56, 244, 1396, 8200, 55092, 390216, 3005288 };
	bool ok = true;
	for (int d = 1; d <= 9; d++)
		ok &= perft(bb::START_BLACK, bb::START_WHITE, d, false) == expected[d - 1];
	check(ok, "Zuggenerator: perft 1..9 = bekannte Werte");
}

static void testGameFlow()
{
	bool ok = true;
	for (int g = 0; g < 2000 && ok; g++)
	{
		Game game;
		u64 B = bb::START_BLACK, W = bb::START_WHITE;
		bool blackToMove = true;
		while (!game.isOver() && ok)
		{
			u64& P = blackToMove ? B : W;
			u64& O = blackToMove ? W : B;
			ok &= game.legalMoves() == bb::moves(P, O) && (game.toMove() == Color::Black) == blackToMove;
			const int s = randomMove(P, O);
			const bb::Position next = bb::play(P, O, 1ULL << s);
			O = next.P; P = next.O;
			const Game::Turn t = game.play(s);
			const bool opponentCan = bb::moves(O, P) != 0, meCan = bb::moves(P, O) != 0;
			ok &= t == (opponentCan ? Game::Turn::Next : meCan ? Game::Turn::OpponentPasses : Game::Turn::Over);
			ok &= game.discs(Color::Black) == B && game.discs(Color::White) == W;
			if (opponentCan) blackToMove = !blackToMove;
		}
	}
	check(ok, "Spielablauf: Zugwechsel, Passen, Spielende (2000 Partien)");
}

static void testEvaluationSymmetry()
{
	bool ok = true;
	for (const bb::Position& p : samplePositions(30, 200, 1))
		ok &= eval::evaluate(p.P, p.O) == -eval::evaluate(p.O, p.P);
	check(ok, "Bewertung: evaluate(P,O) == -evaluate(O,P)");
}

static void testEndgameExact(int threads)
{
	engine::Engine e(18);
	engine::Settings s;
	s.threads = threads;
	s.timeMs = 100000;
	bool ok = true;
	for (const bb::Position& p : samplePositions(55, 150, 2)) // 9 freie Felder
	{
		const engine::Result r = e.search(p.P, p.O, s);
		const int truth = bruteForce(p.P, p.O, false);
		const bb::Position next = bb::play(p.P, p.O, 1ULL << r.move);
		ok &= r.exact && r.score == truth && -bruteForce(next.P, next.O, false) == truth;
	}
	char what[80];
	snprintf(what, sizeof what, "Endspiel exakt = Brute-Force, Zug optimal (%d Thread(s))", threads);
	check(ok, what);
}

static void testLevelsAreOrdered()
{
	engine::Engine a(18), b(18);
	bool ok = true;
	for (int level = 1; level < 8; level++)
	{
		engine::Settings stronger = engine::levelSettings(level + 1), weaker = engine::levelSettings(level);
		stronger.threads = weaker.threads = 1;
		const double pts = match(enginePlayer(a, stronger), enginePlayer(b, weaker), 20);
		printf("    Stufe %d gegen Stufe %d: %.1f/20\n", level + 1, level, pts);
		ok &= pts > 10;
	}
	check(ok, "Stufen: jede hoehere schlaegt die niedrigere (1..8)");
}

// Der Tipp meldet Zwischenstaende mit wachsender Tiefe und laesst sich jederzeit stoppen.
static void testProgressAndStop()
{
	engine::Engine e(20);
	engine::Settings s = engine::hintSettings();
	std::vector<int> depths;
	s.onProgress = [&](const engine::Result& r) { depths.push_back(r.depth); if (r.depth >= 10) e.stop(); };
	e.resetStop();
	const engine::Result r = e.search(bb::START_BLACK, bb::START_WHITE, s);
	bool increasing = depths.size() >= 10;
	for (size_t i = 1; i < depths.size(); i++)
		increasing &= depths[i] == depths[i - 1] + 1;
	check(increasing && r.stopped && r.move >= 0 && r.seconds < 5, "Tipp: Zwischenstaende Tiefe 1,2,3,... und Abbruch per stop()");
}

static void testStrength()
{
	engine::Engine e(20);
	engine::Settings s;
	s.timeMs = 50;
	const double vsRandom = match(enginePlayer(e, s), randomMove, 20);
	const double vsGreedy = match(enginePlayer(e, s), greedyMove, 20);
	printf("    50 ms gegen Zufall %.1f/20, gegen Gierig %.1f/20\n", vsRandom, vsGreedy);
	check(vsRandom >= 19 && vsGreedy >= 19, "Spielstaerke: Engine (50 ms) schlaegt Zufall und Gierig");
}

int main()
{
	testPerft();
	testGameFlow();
	testEvaluationSymmetry();
	testEndgameExact(1);
	testEndgameExact(4);
	testLevelsAreOrdered();
	testProgressAndStop();
	testStrength();
	printf(failures ? "\n%d Test(s) fehlgeschlagen\n" : "\nAlle Tests gruen\n", failures);
	return failures ? 1 : 0;
}
