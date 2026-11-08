#include "engine.h"
#include <algorithm>
#include <thread>
#include <mutex>

namespace engine
{
	using namespace bb;

	namespace
	{
		constexpr u64 CORNERS = bit(0, 0) | bit(0, 7) | bit(7, 0) | bit(7, 7);

		// Pro Ecke: X-Feld und die beiden C-Felder.
		struct CornerZone { u64 corner, x, c; };
		constexpr CornerZone ZONES[4] = {
			{ bit(0, 0), bit(1, 1), bit(0, 1) | bit(1, 0) },
			{ bit(0, 7), bit(1, 6), bit(0, 6) | bit(1, 7) },
			{ bit(7, 0), bit(6, 1), bit(6, 0) | bit(7, 1) },
			{ bit(7, 7), bit(6, 6), bit(6, 7) | bit(7, 6) },
		};

		// Linienmasken fuer die Stabilitaet (Reihen, Spalten, beide Diagonalen).
		struct Lines
		{
			u64 row[8], col[8], diag[15], anti[15];
			Lines()
			{
				for (int i = 0; i < 8; i++) { row[i] = 0; col[i] = 0; }
				for (int i = 0; i < 15; i++) { diag[i] = 0; anti[i] = 0; }
				for (int x = 0; x < 8; x++)
					for (int y = 0; y < 8; y++)
					{
						row[x] |= bit(x, y);
						col[y] |= bit(x, y);
						diag[x - y + 7] |= bit(x, y);
						anti[x + y] |= bit(x, y);
					}
			}
		};
		const Lines LINES;

		// Ordnungswerte fuer die Zugsortierung.
		constexpr int SQV[64] = {
			 100, -20,  10,   5,   5,  10, -20, 100,
			 -20, -50,  -2,  -2,  -2,  -2, -50, -20,
			  10,  -2,   1,   1,   1,   1,  -2,  10,
			   5,  -2,   1,   0,   0,   1,  -2,   5,
			   5,  -2,   1,   0,   0,   1,  -2,   5,
			  10,  -2,   1,   1,   1,   1,  -2,  10,
			 -20, -50,  -2,  -2,  -2,  -2, -50, -20,
			 100, -20,  10,   5,   5,  10, -20, 100,
		};

		inline u64 neighbours(u64 b)
		{
			return shift<-1, -1>(b) | shift<-1, 0>(b) | shift<-1, 1>(b) | shift<0, -1>(b)
				| shift<0, 1>(b) | shift<1, -1>(b) | shift<1, 0>(b) | shift<1, 1>(b);
		}

		inline u64 hashKey(u64 P, u64 O)
		{
			auto mix = [](u64 z) {
				z += 0x9E3779B97F4A7C15ULL;
				z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
				z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
				return z ^ (z >> 31);
			};
			return mix(P) ^ mix(O ^ 0x5851F42D4C957F2DULL);
		}

		inline int terminalScore(u64 P, u64 O)
		{
			const int p = popcount(P), o = popcount(O);
			const int e = 64 - p - o;
			if (p > o) return WIN + (p - o + e); // leere Felder gehen an den Sieger
			if (p < o) return -WIN + (p - o - e);
			return 0;
		}
	}

	u64 stableDiscs(u64 P, u64 O)
	{
		const u64 occ = P | O;
		u64 fullH = 0, fullV = 0, fullD = 0, fullA = 0;
		for (int i = 0; i < 8; i++)
		{
			if ((occ & LINES.row[i]) == LINES.row[i]) fullV |= LINES.row[i]; // Reihe x fest = Linie entlang y
			if ((occ & LINES.col[i]) == LINES.col[i]) fullH |= LINES.col[i];
		}
		for (int i = 0; i < 15; i++)
		{
			if ((occ & LINES.diag[i]) == LINES.diag[i]) fullD |= LINES.diag[i];
			if ((occ & LINES.anti[i]) == LINES.anti[i]) fullA |= LINES.anti[i];
		}
		// Ein Stein ist stabil, wenn auf jeder der 4 Achsen die Linie voll ist oder
		// auf einer Seite ein stabiler eigener Stein bzw. der Rand liegt.
		u64 st = 0;
		for (;;)
		{
			const u64 a1 = fullV | nbFilled<0, -1>(st) | nbFilled<0, 1>(st);
			const u64 a2 = fullH | nbFilled<-1, 0>(st) | nbFilled<1, 0>(st);
			const u64 a3 = fullD | nbFilled<-1, -1>(st) | nbFilled<1, 1>(st);
			const u64 a4 = fullA | nbFilled<-1, 1>(st) | nbFilled<1, -1>(st);
			const u64 ns = P & a1 & a2 & a3 & a4;
			if (ns == st) return st;
			st = ns;
		}
	}

	int evaluate(u64 P, u64 O)
	{
		const u64 occ = P | O;
		const u64 E = ~occ;
		const int empties = popcount(E);

		const int mP = popcount(moves(P, O));
		const int mO = popcount(moves(O, P));

		const u64 adjE = neighbours(E);
		const int fP = popcount(P & adjE); // Frontsteine (an leere Felder angrenzend)
		const int fO = popcount(O & adjE);

		int s = 0;
		s += 30 * (mP - mO);
		s += 8 * (fO - fP);
		s += 400 * (popcount(P & CORNERS) - popcount(O & CORNERS));

		for (const CornerZone& z : ZONES)
		{
			if (occ & z.corner) continue;
			s -= 150 * ((int)((P & z.x) != 0) - (int)((O & z.x) != 0));
			s -= 40 * (popcount(P & z.c) - popcount(O & z.c));
		}

		if (occ & CORNERS) // Stabilitaet entsteht praktisch nur ueber Ecken
			s += 60 * (popcount(stableDiscs(P, O)) - popcount(stableDiscs(O, P)));

		if (empties < 24) // Steinzahl zaehlt erst gegen Ende
			s += (24 - empties) * (popcount(P) - popcount(O));
		return s;
	}

	// ---------------- Transpositionstabelle (lock-frei: keyx = key ^ data) ----------------
	namespace
	{
		struct TTData { int score; int depth; int flag; int move; }; // flag 1 exakt, 2 untere, 3 obere Schranke

		inline u64 pack(int score, int depth, int flag, int move)
		{
			return (u64)(uint32_t)score | ((u64)(uint8_t)depth << 32) | ((u64)(uint8_t)flag << 40) | ((u64)(uint8_t)(int8_t)move << 48);
		}
		inline TTData unpack(u64 d)
		{
			return { (int)(int32_t)(uint32_t)(d & 0xFFFFFFFFULL), (int)(int8_t)(uint8_t)(d >> 32), (int)(uint8_t)(d >> 40), (int)(int8_t)(uint8_t)(d >> 48) };
		}
	}

	// Suchzustand eines Threads.
	struct Searcher
	{
		Engine& e;
		bool checkTime;
		const std::atomic<bool>* phaseStop; // nur Helfer: Abbruch, wenn der Haupt-Thread fertig ist
		int noise;                          // Helfer: veraenderte Zugsortierung (Lazy SMP)
		long long nodes = 0;

		Searcher(Engine& eng, bool ct, const std::atomic<bool>* ps = nullptr, int nz = 0) : e(eng), checkTime(ct), phaseStop(ps), noise(nz) {}
		~Searcher() { e.nodes += nodes; }

		bool halted() const
		{
			return e.abort.load(std::memory_order_relaxed) || (phaseStop && phaseStop->load(std::memory_order_relaxed));
		}

		bool timeUp()
		{
			if (checkTime && (nodes & 1023) == 0)
				if (e.stopRequested.load(std::memory_order_relaxed) || std::chrono::steady_clock::now() > e.deadline)
					e.abort.store(true, std::memory_order_relaxed);
			return halted();
		}

		bool probe(u64 key, TTData& d)
		{
			Engine::TTSlot& s = e.tt[key & e.ttMask];
			const u64 data = s.data.load(std::memory_order_relaxed);
			const u64 keyx = s.keyx.load(std::memory_order_relaxed);
			if ((keyx ^ data) != key || data == 0) return false;
			d = unpack(data);
			return d.flag != 0;
		}

		void store(u64 key, int score, int depth, int flag, int move)
		{
			Engine::TTSlot& s = e.tt[key & e.ttMask];
			const u64 data = pack(score, depth, flag, move);
			s.data.store(data, std::memory_order_relaxed);
			s.keyx.store(key ^ data, std::memory_order_relaxed);
		}

		// Exakte Rechnung fuer die letzten Felder: ohne TT/Bewertung, nur Steindifferenz.
		int solveSmall(u64 P, u64 O, int alpha, int beta, bool passed)
		{
			nodes++;
			if (timeUp()) return 0;
			u64 mv = moves(P, O);
			if (!mv)
			{
				if (passed) return terminalScore(P, O);
				return -solveSmall(O, P, -beta, -alpha, true);
			}
			int best = -WIN - 64;
			while (mv)
			{
				const u64 m = mv & (0 - mv);
				mv &= mv - 1;
				const u64 f = flips(P, O, m);
				const int v = -solveSmall(O & ~f, P | f | m, -beta, -alpha, false);
				if (halted()) return 0;
				if (v > best)
				{
					best = v;
					if (v > alpha) { alpha = v; if (alpha >= beta) break; }
				}
			}
			return best;
		}

		int negamax(u64 P, u64 O, int depth, int alpha, int beta, bool passed)
		{
			const int empties = popcount(~(P | O));
			if (depth >= empties && empties <= 6)
				return solveSmall(P, O, alpha, beta, passed);

			nodes++;
			if (timeUp()) return 0;
			if (depth <= 0) return evaluate(P, O);

			u64 mv = moves(P, O);
			if (!mv)
			{
				if (passed) return terminalScore(P, O);
				return -negamax(O, P, depth, -beta, -alpha, true); // Passen kostet keine Tiefe
			}

			const u64 key = hashKey(P, O);
			int ttMove = -1;
			TTData td;
			if (probe(key, td))
			{
				ttMove = td.move;
				if (td.depth >= depth)
				{
					if (td.flag == 1) return td.score;
					if (td.flag == 2 && td.score >= beta) return td.score;
					if (td.flag == 3 && td.score <= alpha) return td.score;
				}
			}

			int sq[32], key2[32], n = 0;
			while (mv)
			{
				const int s = lsb(mv);
				mv &= mv - 1;
				int k = SQV[s];
				if (noise) k += (int)((hashKey(P ^ (u64)noise, O ^ (u64)s) >> 40) & 31) - 16; // Helfer weichen ab
				if (s == ttMove) k += 1000000;
				else if (depth >= 3)
				{
					const u64 m = 1ULL << s;
					const u64 f = flips(P, O, m);
					k -= 16 * popcount(moves(O & ~f, P | f | m)); // wenig Gegnerzuege zuerst
				}
				sq[n] = s; key2[n] = k; n++;
			}

			const int alphaOrig = alpha;
			int best = -WIN - 64;
			int bestMove = sq[0];
			for (int i = 0; i < n; i++)
			{
				int bi = i;
				for (int j = i + 1; j < n; j++) if (key2[j] > key2[bi]) bi = j;
				std::swap(sq[i], sq[bi]); std::swap(key2[i], key2[bi]);

				const u64 m = 1ULL << sq[i];
				const u64 f = flips(P, O, m);
				const u64 nP = O & ~f, nO = P | f | m;
				int v;
				if (i == 0)
					v = -negamax(nP, nO, depth - 1, -beta, -alpha, false);
				else
				{
					v = -negamax(nP, nO, depth - 1, -alpha - 1, -alpha, false); // Nullfenster (PVS)
					if (v > alpha && v < beta && !halted())
						v = -negamax(nP, nO, depth - 1, -beta, -alpha, false);
				}
				if (halted()) return 0;
				if (v > best)
				{
					best = v;
					bestMove = sq[i];
					if (v > alpha) { alpha = v; if (alpha >= beta) break; }
				}
			}

			store(key, best, depth, best <= alphaOrig ? 3 : (best >= beta ? 2 : 1), bestMove);
			return best;
		}
	};

	Engine::Engine(int ttBits) : tt(new TTSlot[size_t(1) << ttBits]), ttMask((u64(1) << ttBits) - 1) {}

	void Engine::clear()
	{
		for (u64 i = 0; i <= ttMask; i++)
		{
			tt[i].keyx.store(0, std::memory_order_relaxed);
			tt[i].data.store(0, std::memory_order_relaxed);
		}
	}

	void Engine::stop()
	{
		stopRequested = true;
	}

	void Engine::resetStop()
	{
		stopRequested = false;
	}

	// Eine Iteration an der Wurzel. Erster Zug allein (setzt alpha), Rest parallel mit gemeinsamem alpha.
	int Engine::rootSearch(u64 P, u64 O, int* sq, int n, int depth, int alpha, int beta, int& bestIdx, bool checkTime)
	{
		bestIdx = 0;
		int best;
		{
			const u64 m = 1ULL << sq[0];
			const u64 f = flips(P, O, m);
			const u64 nP = O & ~f, nO = P | f | m;
			// Lazy SMP: Helfer rechnen denselben Teilbaum mit anderer Sortierung (teils eine Tiefe mehr)
			// und fuellen die gemeinsame Tabelle; ihr Ergebnis wird verworfen.
			std::atomic<bool> mainDone{ false };
			std::vector<std::thread> helpers;
			if (depth >= 6)
				for (int t = 1; t < nThreads; t++)
					helpers.emplace_back([&, t]() {
						Searcher h(*this, checkTime, &mainDone, t * 7919);
						h.negamax(nP, nO, depth - 1 + (t & 1), -beta, -alpha, false);
					});
			{
				Searcher s(*this, checkTime);
				best = -s.negamax(nP, nO, depth - 1, -beta, -alpha, false);
			}
			mainDone = true;
			for (auto& t : helpers) t.join();
		}
		if (abort || n == 1) return best;
		if (best > alpha) alpha = best;
		if (alpha >= beta) return best;

		std::mutex mtx;
		std::atomic<int> next{ 1 };
		int sharedAlpha = alpha, sharedBest = best, sharedIdx = 0;

		auto worker = [&]() {
			Searcher s(*this, checkTime);
			for (;;)
			{
				const int i = next.fetch_add(1);
				if (i >= n || abort) break;
				int a;
				{ std::lock_guard<std::mutex> lk(mtx); a = sharedAlpha; }
				if (a >= beta) break;
				const u64 m = 1ULL << sq[i];
				const u64 f = flips(P, O, m);
				const u64 nP = O & ~f, nO = P | f | m;
				int v = -s.negamax(nP, nO, depth - 1, -a - 1, -a, false); // Nullfenster
				if (abort) break;
				if (v > a && v < beta)
				{
					// Zug ist besser als das alpha beim Start (untere Schranke). Ein anderer Thread kann
					// alpha inzwischen erhoeht haben -> mit aktuellem alpha nachrechnen, sonst waere der
					// Wert nur eine Schranke und der Zug koennte zu Unrecht verworfen werden.
					int a2;
					{ std::lock_guard<std::mutex> lk(mtx); a2 = sharedAlpha; }
					if (a2 >= beta) break;
					v = -s.negamax(nP, nO, depth - 1, -beta, -a2, false);
					if (abort) break;
				}
				std::lock_guard<std::mutex> lk(mtx);
				if (v > sharedBest)
				{
					sharedBest = v;
					sharedIdx = i;
					if (v > sharedAlpha) sharedAlpha = v;
				}
			}
		};

		const int helpers = std::min(nThreads, n - 1) - 1;
		std::vector<std::thread> pool;
		pool.reserve(std::max(0, helpers));
		for (int t = 0; t < helpers; t++) pool.emplace_back(worker);
		worker();
		for (auto& t : pool) t.join();

		bestIdx = sharedIdx;
		return sharedBest;
	}

	Result Engine::search(u64 P, u64 O, const Settings& s)
	{
		using clock = std::chrono::steady_clock;
		const auto start = clock::now();
		nodes = 0;
		abort = false;
		// stopRequested wird hier bewusst NICHT zurueckgesetzt: ein stop(), das kurz vor dem Start
		// kam, soll wirken. Zuruecksetzen per resetStop() vor dem Start.
		nThreads = s.threads > 0 ? s.threads : std::max(1, (int)std::thread::hardware_concurrency());
		Result res;
		res.threads = nThreads;

		u64 mv = moves(P, O);
		if (!mv || stopRequested) return res;

		int sq[32], n = 0;
		while (mv) { sq[n++] = lsb(mv); mv &= mv - 1; }
		std::sort(sq, sq + n, [](int a, int b) { return SQV[a] > SQV[b]; });
		res.move = sq[0];
		if (n == 1) return res; // nur ein Zug moeglich: sofort spielen

		const int empties = popcount(~(P | O));
		const bool endgame = empties <= s.endgameEmpties && s.maxDepth >= empties;

		// Mittelspiel: iterative Vertiefung. Im Endspiel nur kurz (Zugsortierung + Rueckfall).
		deadline = start + std::chrono::milliseconds(endgame ? std::max(1, s.timeMs / 10) : s.timeMs);
		const int maxDepth = std::min(s.maxDepth, endgame ? std::min(empties - 1, 12) : empties);
		for (int depth = 1; depth <= maxDepth; depth++)
		{
			int bestIdx;
			const int best = rootSearch(P, O, sq, n, depth, -WIN - 64, WIN + 64, bestIdx, depth > 1); // Tiefe 1 immer fertig
			if (abort) break;
			std::rotate(sq, sq + bestIdx, sq + bestIdx + 1); // bester Zug nach vorne
			res.move = sq[0];
			res.score = best;
			res.depth = depth;
			res.exact = depth >= empties; // bis zum Spielende gerechnet
			res.wld = !res.exact && (best >= WIN - 64 || best <= -WIN + 64); // Sieg/Niederlage erzwungen
			if (res.exact || res.wld) break; // tiefer bringt im Mittelspiel nichts mehr
		}

		if (endgame && !res.exact && !stopRequested)
		{
			res.wld = false;
			deadline = start + std::chrono::milliseconds(s.timeMs);
			abort = false;
			// 1) Sieg/Remis/Niederlage (enges Fenster, schnell)
			int bestIdx;
			const int wld = rootSearch(P, O, sq, n, empties, -1, 1, bestIdx, true);
			res.wldSeconds = std::chrono::duration<double>(clock::now() - start).count();
			if (!abort && wld > -1) // Gewinn oder Remis gesichert -> diesen Zug nehmen
			{
				std::rotate(sq, sq + bestIdx, sq + bestIdx + 1);
				res.move = sq[0];
				res.score = wld;
				res.depth = empties;
				res.wld = true;
				// 2) mit der Restzeit die Steindifferenz maximieren
				const int exact = rootSearch(P, O, sq, n, empties, -WIN - 64, WIN + 64, bestIdx, true);
				if (!abort)
				{
					res.move = sq[bestIdx];
					res.score = exact;
					res.exact = true;
					res.wld = false;
				}
			}
			else if (!abort) // alles verloren: knappste Niederlage suchen, sonst Mittelspielzug
			{
				const int exact = rootSearch(P, O, sq, n, empties, -WIN - 64, WIN + 64, bestIdx, true);
				if (!abort)
				{
					res.move = sq[bestIdx];
					res.score = exact;
					res.depth = empties;
					res.exact = true;
				}
				else
				{
					res.score = wld;
					res.wld = true;
				}
			}
		}

		res.stopped = stopRequested;
		res.nodes = nodes;
		res.seconds = std::chrono::duration<double>(clock::now() - start).count();
		return res;
	}
}
