// Vergleichstest + Benchmark: Original-Zuggenerator (tests/reference_impl.h) gegen Bitboard-Kern (include/reversi_core.h).
// Bauen (ohne Qt/OpenCV):  g++ -O2 -std=c++20 -Iinclude tests/movegen_test.cpp -o movegen_test
#include "reversi_core.h"
#include "reference_impl.h"
#include <cstdio>
#include <random>
#include <map>
#include <set>
#include <chrono>

static ref::board toRef(const board& b)
{
	ref::board r(8, std::vector<unsigned char>(8));
	for (int i = 0; i < 8; i++) for (int j = 0; j < 8; j++) r[i][j] = b[i][j];
	return r;
}
static bool same(const board& b, const ref::board& r)
{
	for (int i = 0; i < 8; i++) for (int j = 0; j < 8; j++) if (b[i][j] != r[i][j]) return false;
	return true;
}
static std::set<int> sqset(const std::vector<field>& v)
{
	std::set<int> s;
	for (auto& f : v) s.insert(f.x * 8 + f.y);
	return s;
}
static long errors = 0;
#define CHECK(c, msg) do { if (!(c)) { if (errors < 20) printf("FEHLER: %s\n", msg); errors++; } } while (0)

static void comparePosition(ref::calc& rc, const board& pos, int8_t color, long& nMoves)
{
	// findMove
	board N = pos; ref::board R = toRef(pos);
	findOut fn = bb::findMove(N, color);
	findOut fr = rc.findMove(R, color);
	CHECK(same(N, R), "findMove: Brett/Marker verschieden");
	CHECK(sqset(fn.Ges) == sqset(fr.Ges), "findMove: Zugmenge verschieden");
	CHECK(fn.Ges.size() == fr.Ges.size(), "findMove: Anzahl verschieden");
	CHECK(sqset(fn.Normal) == sqset(fr.Normal), "findMove: Normal");
	CHECK(sqset(fn.sField) == sqset(fr.sField), "findMove: sField");
	CHECK(sqset(fn.cField) == sqset(fr.cField), "findMove: cField");
	CHECK(sqset(fn.xField) == sqset(fr.xField), "findMove: xField");
	CHECK(sqset(fn.eField) == sqset(fr.eField), "findMove: eField");
	std::map<int, int> sn, sr;
	for (size_t i = 0; i < fn.Ges.size(); i++) sn[fn.Ges[i].x * 8 + fn.Ges[i].y] = fn.status[i];
	for (size_t i = 0; i < fr.Ges.size(); i++) sr[fr.Ges[i].x * 8 + fr.Ges[i].y] = fr.status[i];
	CHECK(sn == sr, "findMove: status verschieden");

	// fastfind
	{
		board N2 = pos; ref::board R2 = toRef(pos);
		auto a = bb::fastfind(N2, color);
		auto b = rc.fastfind(R2, color);
		CHECK(same(N2, R2), "fastfind: Brett/Marker verschieden");
		CHECK(sqset(a) == sqset(b) && a.size() == b.size(), "fastfind: Zuege verschieden");
	}

	// doMove / fastMove fuer jeden legalen Zug (auf dem Brett mit Markern, wie im Spiel)
	for (auto& mv : fr.Ges)
	{
		nMoves++;
		board N3 = N; ref::board R3 = R;
		doOut dn = bb::doMove(N3, mv, color);
		doOut dr = rc.doMove(R3, mv, color);
		CHECK(same(N3, R3), "doMove: Brett verschieden");
		CHECK(dn.In == dr.In && dn.Half == dr.Half && dn.Out == dr.Out, "doMove: In/Half/Out verschieden");
		board N4 = N; ref::board R4 = R;
		bb::fastMove(N4, mv, color);
		rc.fastMove(R4, mv, color);
		CHECK(same(N4, R4), "fastMove: Brett verschieden");
	}

	int8_t b, w;
	bb::countChips(pos, b, w);
	rc.countChips(toRef(pos));
	CHECK(b == rc.BlackChips && w == rc.WhiteChips, "countChips");
}

// Suchbaum wie silasindustries: findMove, Brett kopieren, doMove, rekursiv.
template<class Board, class FindFn, class DoFn>
static long long perft(Board& m, int depth, int8_t color, bool passed, FindFn& find, DoFn& doit)
{
	if (depth == 0) return 1;
	findOut a = find(m, color);
	if (a.Ges.empty())
	{
		if (passed) return 1;
		return perft(m, depth - 1, (int8_t)(3 - color), true, find, doit);
	}
	long long n = 0;
	for (size_t i = 0; i < a.Ges.size(); i++)
	{
		Board t = m;
		doit(t, a.Ges[i], color);
		n += perft(t, depth - 1, (int8_t)(3 - color), false, find, doit);
	}
	return n;
}

// Reine Bitboard-Suche ohne findOut/board - zeigt das Potenzial fuer die neue Suche.
static long long perftBB(bb::u64 P, bb::u64 O, int depth, bool passed)
{
	if (depth == 0) return 1;
	bb::u64 mv = bb::moves(P, O);
	if (!mv) return passed ? 1 : perftBB(O, P, depth - 1, true);
	long long n = 0;
	while (mv)
	{
		bb::u64 m = mv & (0 - mv);
		mv &= mv - 1;
		bb::u64 f = bb::flips(P, O, m);
		n += perftBB(O & ~f, P | f | m, depth - 1, false);
	}
	return n;
}

int main(int argc, char** argv)
{
	const int games = argc > 1 ? atoi(argv[1]) : 3000;
	std::mt19937 rng(12345);
	ref::calc rc;
	long positions = 0, nMoves = 0;

	for (int g = 0; g < games; g++)
	{
		board pos;
		pos[3][4] = 1; pos[4][3] = 1; pos[3][3] = 2; pos[4][4] = 2;
		int8_t color = 1;
		int passes = 0;
		while (passes < 2)
		{
			// zufaellige Alt-Marker einstreuen (wie sie im Spiel auf dem Brett liegen)
			board noisy = pos;
			for (int k = 0; k < 64; k++)
				if (noisy.c[k / 8][k % 8] == 0 && rng() % 5 == 0) noisy.c[k / 8][k % 8] = (unsigned char)(3 + rng() % 2);
			comparePosition(rc, noisy, 1, nMoves);
			comparePosition(rc, noisy, 2, nMoves);
			positions++;

			board tmp = pos;
			auto mv = bb::fastfind(tmp, color);
			if (mv.empty()) { passes++; color = (int8_t)(3 - color); continue; }
			passes = 0;
			bb::fastMove(pos, mv[rng() % mv.size()], color);
			bb::clearMarkers(pos);
			color = (int8_t)(3 - color);
		}
	}
	printf("Vergleich: %ld Stellungen, %ld Zuege geprueft, %ld Fehler\n", positions, nMoves, errors);

	// Benchmark
	const int depth = argc > 2 ? atoi(argv[2]) : 7;
	using clk = std::chrono::steady_clock;
	{
		board s; s[3][4] = 1; s[4][3] = 1; s[3][3] = 2; s[4][4] = 2;
		auto f = [](board& m, int8_t c) { return bb::findMove(m, c); };
		auto d = [](board& m, field k, int8_t c) { return bb::doMove(m, k, c); };
		auto t0 = clk::now();
		long long n = perft(s, depth, 1, false, f, d);
		double sec = std::chrono::duration<double>(clk::now() - t0).count();
		printf("NEU : perft(%d) = %lld Blaetter in %.3f s\n", depth, n, sec);
		ref::board r = toRef(s);
		auto fr = [&rc](ref::board& m, int8_t c) { return rc.findMove(m, c); };
		auto dr = [&rc](ref::board& m, field k, int8_t c) { return rc.doMove(m, k, c); };
		t0 = clk::now();
		long long n2 = perft(r, depth, 1, false, fr, dr);
		double sec2 = std::chrono::duration<double>(clk::now() - t0).count();
		t0 = clk::now();
		long long n3 = perftBB(bb::bit(3, 4) | bb::bit(4, 3), bb::bit(3, 3) | bb::bit(4, 4), depth, false);
		double sec3 = std::chrono::duration<double>(clk::now() - t0).count();
		printf("BB  : perft(%d) = %lld Blaetter in %.3f s  (reine Bitboards)\n", depth, n3, sec3);
		printf("ALT : perft(%d) = %lld Blaetter in %.3f s  -> Faktor %.1f\n", depth, n2, sec2, sec2 / sec);
	}
	return errors ? 1 : 0;
}
