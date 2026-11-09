#include "engine.h"
#include "evaluation.h"
#include <algorithm>
#include <thread>
#include <mutex>
#include <vector>
#include <random>
#include <cmath>

namespace engine
{
	using namespace bb;

	namespace
	{
		constexpr int SMALL_ENDGAME = 6;        // bis hierhin rechnet solveSmall ohne Tabelle/Bewertung
		constexpr int TT_MIN_DEPTH = 3;         // Tabelle erst ab dieser Resttiefe (sonst verdraengen flache Eintraege tiefe)
		constexpr int MOBILITY_ORDER_DEPTH = 3; // ab dieser Resttiefe nach Gegner-Mobilitaet sortieren
		constexpr int LAZY_SMP_MIN_DEPTH = 6;   // ab dieser Tiefe rechnen Helfer-Threads mit
		constexpr int TT_MOVE_BONUS = 1000000;
		constexpr int STABILITY_CUT_EMPTIES = 8;
		constexpr int EG_MOBILITY_WEIGHT = 128;  // Endspiel-Sortierung: wenig Gegnerzuege zuerst (gemessen ~2x schneller) // Stabilitaets-Schnitt im Endspiel ab so vielen freien Feldern

		// Feldwerte fuer die Zugsortierung (Ecken zuerst, X-/C-Felder zuletzt).
		constexpr int SQUARE_VALUE[64] = {
			 100, -20,  10,   5,   5,  10, -20, 100,
			 -20, -50,  -2,  -2,  -2,  -2, -50, -20,
			  10,  -2,   1,   1,   1,   1,  -2,  10,
			   5,  -2,   1,   0,   0,   1,  -2,   5,
			   5,  -2,   1,   0,   0,   1,  -2,   5,
			  10,  -2,   1,   1,   1,   1,  -2,  10,
			 -20, -50,  -2,  -2,  -2,  -2, -50, -20,
			 100, -20,  10,   5,   5,  10, -20, 100,
		};

		// ---------------- ProbCut ----------------
		// Eine flache Suche (Tiefe shallowDepth(d)) sagt die tiefe Suche (Tiefe d) voraus:
		// tief ~ a * flach + b, Streuung sigma. Kalibriert aus 823 Selbstspiel-Stellungen,
		// getrennt nach Spielphase (freie Felder >= 44 / 32-43 / <= 31).
		constexpr int PROBCUT_MIN_DEPTH = 5;
		struct ProbCutFit { double a, b, sigma; };
		constexpr ProbCutFit PROBCUT_FITS[3][9] = { // Tiefe 5..13
			{ {1.03,  4, 35}, {1.06, -3,  47}, {1.07,  3,  48}, {1.10, 10,  39}, {1.10, -3,  49},
			  {1.16, 17, 57}, {1.14,-10,  56}, {1.15, 13,  54}, {1.16,-12,  51} },
			{ {1.07, 13, 57}, {1.10,  1, 102}, {1.12, 16, 107}, {1.14, 10, 107}, {1.16,  8, 102},
			  {1.23, 18,149}, {1.27, 18, 150}, {1.29, 17, 140}, {1.32, 25, 145} },
			{ {1.13, 30,147}, {1.26,-10, 208}, {1.27, 53, 235}, {1.27,  1, 252}, {1.25, 45, 266},
			  {1.42,  6,333}, {1.41, 73, 357}, {1.42, -8, 377}, {1.49, 58, 345} },
		};

		int shallowDepth(int depth) { return 2 * (depth / 4) + (depth & 1); }

		ProbCutFit probCutFit(int depth, int empties)
		{
			const int phase = empties >= 44 ? 0 : (empties >= 32 ? 1 : 2);
			ProbCutFit f = PROBCUT_FITS[phase][std::min(depth, 13) - PROBCUT_MIN_DEPTH];
			if (depth > 13)
				f.sigma *= 1.0 + 0.1 * (depth - 13); // jenseits der Kalibrierung vorsichtiger
			return f;
		}

		// Endwert: Steindifferenz d aus Sicht von P; leere Felder e gehen an den Sieger.
		int finalScore(int d, int e)
		{
			if (d > 0) return WIN + d + e;
			if (d < 0) return -WIN + d - e;
			return 0;
		}

		int terminalScore(u64 P, u64 O)
		{
			return finalScore(popcount(P) - popcount(O), popcount(~(P | O)));
		}

		// Quadranten mit ungerader Zahl freier Felder: dort bekommt man meist den letzten Zug.
		u64 oddQuadrants(u64 empty)
		{
			constexpr u64 QUADRANTS[4] = { 0x000000000F0F0F0FULL, 0x00000000F0F0F0F0ULL,
			                               0x0F0F0F0F00000000ULL, 0xF0F0F0F000000000ULL };
			u64 odd = 0;
			for (u64 q : QUADRANTS)
				if (popcount(empty & q) & 1)
					odd |= q;
			return odd;
		}

		// Zugliste eines Knotens mit Sortierschluesseln; umgedrehte Steine werden mitgemerkt.
		struct MoveList
		{
			int sq[32];
			int key[32];
			u64 flip[32];
			int n = 0;

			void add(int s, int k, u64 f) { sq[n] = s; key[n] = k; flip[n] = f; n++; }

			// Selection-Sort-Schritt: Zug mit hoechstem Schluessel ab Position i nach i holen.
			void bringBestTo(int i)
			{
				int best = i;
				for (int j = i + 1; j < n; j++)
					if (key[j] > key[best]) best = j;
				std::swap(sq[i], sq[best]);
				std::swap(key[i], key[best]);
				std::swap(flip[i], flip[best]);
			}
		};
	}

	// ======================= Suche eines Threads =======================

	struct Searcher
	{
		Engine& e;
		bool checkTime;
		const std::atomic<bool>* phaseStop; // Helfer: Abbruch, sobald der Haupt-Thread fertig ist
		int orderingNoise;                  // Helfer: abweichende Zugsortierung (Lazy SMP)
		long long nodes = 0;

		Searcher(Engine& eng, bool ct, const std::atomic<bool>* ps = nullptr, int noise = 0)
			: e(eng), checkTime(ct), phaseStop(ps), orderingNoise(noise) {}
		~Searcher() { e.nodes += nodes; }

		bool halted() const
		{
			return e.abort.load(std::memory_order_relaxed) || (phaseStop && phaseStop->load(std::memory_order_relaxed));
		}

		bool timeUp()
		{
			if (checkTime && (nodes & 1023) == 0)
				if (e.stopRequested.load(std::memory_order_relaxed) || Engine::Clock::now() > e.deadline)
					e.abort.store(true, std::memory_order_relaxed);
			return halted();
		}

		// Genau ein freies Feld m: Ergebnis direkt aus der Zahl umgedrehter Steine.
		int solveLastMove(u64 P, u64 O, u64 m)
		{
			nodes++;
			const int d = 2 * popcount(P) - 63; // Differenz P - O ohne das freie Feld
			if (u64 f = flips(P, O, m)) return finalScore(d + 2 * popcount(f) + 1, 0);
			if (u64 f = flips(O, P, m)) return finalScore(d - 2 * popcount(f) - 1, 0); // P passt, O setzt
            return finalScore(d, 1);                                                    // keiner kann setzen
		}

		// Exakte Rechnung fuer die letzten Felder (Paritaets-Sortierung, keine Tabelle).
		int solveSmall(u64 P, u64 O, int alpha, int beta, bool passed)
		{
			const u64 empty = ~(P | O);
			if (!passed && (empty & (empty - 1)) == 0)
				return solveLastMove(P, O, empty);
			nodes++;
			if (timeUp()) return 0;

			const u64 mv = moves(P, O);
			if (!mv)
				return passed ? terminalScore(P, O) : -solveSmall(O, P, -beta, -alpha, true);

			const u64 odd = oddQuadrants(empty);
			int best = -INF;
			for (u64 group : { mv & odd, mv & ~odd })
			{
				for (; group; group &= group - 1)
				{
					const Position next = play(P, O, group & (0 - group));
					const int v = -solveSmall(next.P, next.O, -beta, -alpha, false);
					if (halted()) return 0;
					if (v > best)
					{
						best = v;
						if (v > alpha) { alpha = v; if (alpha >= beta) return best; }
					}
				}
			}
			return best;
		}

		// ProbCut: Liefert true (und das Ergebnis in result), wenn eine flache Nullfenster-Suche mit
		// hoher Wahrscheinlichkeit zeigt, dass die tiefe Suche ausserhalb von (alpha, beta) endet.
		bool tryProbCut(u64 P, u64 O, int depth, int empties, int alpha, int beta, int& result)
		{
			const ProbCutFit f = probCutFit(depth, empties);
			const int shallow = shallowDepth(depth);
			const double margin = e.probCutConfidence * f.sigma;

			const int upper = (int)std::ceil((beta + margin - f.b) / f.a);
			if (upper < WIN - 64 && negamax(P, O, shallow, upper - 1, upper, false) >= upper && !halted())
			{
				result = beta;
				return true;
			}
			const int lower = (int)std::floor((alpha - margin - f.b) / f.a);
			if (lower > -WIN + 64 && negamax(P, O, shallow, lower, lower + 1, false) <= lower && !halted())
			{
				result = alpha;
				return true;
			}
			return false;
		}

		// Tabelle abfragen: liefert true, wenn der gespeicherte Wert den Knoten schon entscheidet.
		bool probeTable(u64 key, int depth, int alpha, int beta, int& ttMove, int& score)
		{
			TTEntry t;
			if (!e.tt.probe(key, t))
				return false;
			ttMove = t.move;
			if (t.depth < depth)
				return false;
			score = t.score;
			return t.bound == Bound::Exact || (t.bound == Bound::Lower && score >= beta) || (t.bound == Bound::Upper && score <= alpha);
		}

		// Sortierung: Tabellen-Zug zuerst, sonst Feldwert und (tiefer im Baum) wenig Gegnerzuege.
		MoveList orderMoves(u64 P, u64 O, u64 mv, int depth, int ttMove)
		{
			MoveList list;
			const bool byMobility = depth >= MOBILITY_ORDER_DEPTH;
			const bool endgame = depth >= popcount(~(P | O));
			for (; mv; mv &= mv - 1)
			{
				const int s = lsb(mv);
				const u64 m = 1ULL << s;
				const u64 f = byMobility ? flips(P, O, m) : 0;
				int k = SQUARE_VALUE[s];
				if (orderingNoise)
					k += (int)((TranspositionTable::hash(P ^ (u64)orderingNoise, O ^ (u64)s) >> 40) & 31) - 16;
				if (s == ttMove)
					k += TT_MOVE_BONUS;
				else if (endgame) // Endspiel: wenig Gegnerzuege zuerst, stark gewichtet ("fastest first")
					k -= EG_MOBILITY_WEIGHT * popcount(moves(O & ~f, P | f | m));
				else if (byMobility)
					k -= 16 * popcount(moves(O & ~f, P | f | m));
				list.add(s, k, f);
			}
			return list;
		}

		// Principal Variation Search fuer einen Kindknoten: erster Zug mit vollem Fenster,
		// die anderen erst mit Nullfenster pruefen und nur bei Bedarf genau nachrechnen.
		int searchChild(u64 nP, u64 nO, int depth, int alpha, int beta, bool first)
		{
			if (first)
				return -negamax(nP, nO, depth - 1, -beta, -alpha, false);
			int v = -negamax(nP, nO, depth - 1, -alpha - 1, -alpha, false);
			if (v > alpha && v < beta && !halted())
				v = -negamax(nP, nO, depth - 1, -beta, -alpha, false);
			return v;
		}

		int negamax(u64 P, u64 O, int depth, int alpha, int beta, bool passed)
		{
			const int empties = popcount(~(P | O));
			if (depth >= empties && empties <= SMALL_ENDGAME)
				return solveSmall(P, O, alpha, beta, passed);

			nodes++;
			if (timeUp()) return 0;
			if (depth <= 0) return eval::evaluate(P, O);

			const u64 mv = moves(P, O);
			if (!mv) // passen kostet keine Tiefe; koennen beide nicht, ist das Spiel aus
				return passed ? terminalScore(P, O) : -negamax(O, P, depth, -beta, -alpha, true);

			// Endspiel: mehr als 64 - 2 * (stabile Gegnersteine) Differenz ist nicht erreichbar
			if (depth >= empties && empties >= STABILITY_CUT_EMPTIES)
			{
				const int upperBound = finalScore(64 - 2 * popcount(eval::stableDiscs(O, P)), 0);
				if (upperBound <= alpha)
					return upperBound;
			}

			const u64 key = TranspositionTable::hash(P, O);
			const bool useTable = depth >= TT_MIN_DEPTH;
			int ttMove = -1, ttScore = 0;
			if (useTable && probeTable(key, depth, alpha, beta, ttMove, ttScore))
				return ttScore;

			// ProbCut nur im Mittelspiel (nie in exakten Endspielrechnungen) und in Nullfenster-Knoten
			if (e.probCut && depth >= PROBCUT_MIN_DEPTH && depth < empties && beta == alpha + 1)
			{
				int cut;
				if (tryProbCut(P, O, depth, empties, alpha, beta, cut))
					return cut;
				if (halted()) return 0;
			}

			MoveList list = orderMoves(P, O, mv, depth, ttMove);
			const int alphaOrig = alpha;
			int best = -INF, bestMove = list.sq[0];
			for (int i = 0; i < list.n; i++)
			{
				list.bringBestTo(i);
				const u64 m = 1ULL << list.sq[i];
				const u64 f = depth >= MOBILITY_ORDER_DEPTH ? list.flip[i] : flips(P, O, m);
				const int v = searchChild(O & ~f, P | f | m, depth, alpha, beta, i == 0);
				if (halted()) return 0;
				if (v > best)
				{
					best = v;
					bestMove = list.sq[i];
					if (v > alpha) { alpha = v; if (alpha >= beta) break; }
				}
			}

			if (useTable)
			{
				const Bound b = best <= alphaOrig ? Bound::Upper : (best >= beta ? Bound::Lower : Bound::Exact);
				e.tt.store(key, { best, depth, b, bestMove });
			}
			return best;
		}
	};

	// ======================= Wurzel =======================

	void Engine::RootMoves::moveToFront(int idx)
	{
		std::rotate(sq, sq + idx, sq + idx + 1);
	}

	Engine::RootMoves Engine::rootMoves(u64 P, u64 O)
	{
		RootMoves rm;
		for (u64 mv = moves(P, O); mv; mv &= mv - 1)
			rm.sq[rm.n++] = lsb(mv);
		std::sort(rm.sq, rm.sq + rm.n, [](int a, int b) { return SQUARE_VALUE[a] > SQUARE_VALUE[b]; });
		return rm;
	}

	// Erster (vermutlich bester) Wurzelzug. Lazy SMP: Helfer rechnen denselben Teilbaum mit
	// anderer Sortierung (teils eine Tiefe mehr) und fuellen die Tabelle; ihr Ergebnis wird verworfen.
	int Engine::searchFirstRootMove(u64 P, u64 O, int sq, int depth, int alpha, int beta, bool checkTime)
	{
		const Position child = play(P, O, 1ULL << sq);
		std::atomic<bool> mainDone{ false };
		std::vector<std::thread> helpers;
		if (depth >= LAZY_SMP_MIN_DEPTH)
			for (int t = 1; t < nThreads; t++)
				helpers.emplace_back([&, t]() {
					Searcher h(*this, checkTime, &mainDone, t * 7919);
					h.negamax(child.P, child.O, depth - 1 + (t & 1), -beta, -alpha, false);
				});
		int value;
		{
			Searcher s(*this, checkTime);
			value = -s.negamax(child.P, child.O, depth - 1, -beta, -alpha, false);
		}
		mainDone = true;
		for (auto& t : helpers) t.join();
		return value;
	}

	// Restliche Wurzelzuege parallel; alle Threads teilen sich alpha und den besten Wert.
	int Engine::searchRemainingRootMoves(u64 P, u64 O, const RootMoves& rm, int depth, int alpha, int beta,
	                                     int best, int& bestIdx, bool checkTime)
	{
		std::mutex mtx;
		std::atomic<int> next{ 1 };
		int sharedAlpha = alpha, sharedBest = best, sharedIdx = bestIdx;
		auto currentAlpha = [&]() { std::lock_guard<std::mutex> lk(mtx); return sharedAlpha; };

		auto worker = [&]() {
			Searcher s(*this, checkTime);
			for (;;)
			{
				const int i = next.fetch_add(1);
				if (i >= rm.n || abort) break;
				const int a = currentAlpha();
				if (a >= beta) break;
				const Position child = play(P, O, 1ULL << rm.sq[i]);
				int v = -s.negamax(child.P, child.O, depth - 1, -a - 1, -a, false); // Nullfenster
				if (abort) break;
				if (v > a && v < beta)
				{
					// Besser als alpha beim Start: mit dem aktuellen alpha genau nachrechnen. Ein anderer
					// Thread kann alpha erhoeht haben; ohne Nachrechnen waere v nur eine Schranke.
					const int a2 = currentAlpha();
					if (a2 >= beta) break;
					v = -s.negamax(child.P, child.O, depth - 1, -beta, -a2, false);
					if (abort) break;
				}
				std::lock_guard<std::mutex> lk(mtx);
				if (v > sharedBest)
				{
					sharedBest = v;
					sharedIdx = i;
					sharedAlpha = std::max(sharedAlpha, v);
				}
			}
		};

		std::vector<std::thread> pool;
		for (int t = 1; t < std::min(nThreads, rm.n - 1); t++)
			pool.emplace_back(worker);
		worker();
		for (auto& t : pool) t.join();

		bestIdx = sharedIdx;
		return sharedBest;
	}

	// Eine Iteration an der Wurzel; bestIdx = Index des besten Zugs in rm.
	int Engine::searchRoot(u64 P, u64 O, const RootMoves& rm, int depth, int alpha, int beta, int& bestIdx, bool checkTime)
	{
		bestIdx = 0;
		const int best = searchFirstRootMove(P, O, rm.sq[0], depth, alpha, beta, checkTime);
		if (abort || rm.n == 1) return best;
		alpha = std::max(alpha, best);
		if (alpha >= beta) return best;
		return searchRemainingRootMoves(P, O, rm, depth, alpha, beta, best, bestIdx, checkTime);
	}

	// ======================= Ablauf eines Suchauftrags =======================

	// Tiefe 1, 2, 3, ... bis maxDepth oder Zeitende; jede Iteration sortiert die naechste vor.
	void Engine::iterativeDeepening(u64 P, u64 O, RootMoves& rm, int maxDepth, int empties, Result& res)
	{
		for (int depth = 1; depth <= maxDepth; depth++)
		{
			int bestIdx;
			const int best = searchRoot(P, O, rm, depth, -INF, INF, bestIdx, depth > 1); // Tiefe 1 immer fertig
			if (abort) return;
			rm.moveToFront(bestIdx);
			res.move = rm.sq[0];
			res.score = best;
			res.depth = depth;
			res.exact = depth >= empties;
			res.wld = !res.exact && isProvenResult(best);
			reportProgress(res);
			if (res.exact || res.wld) return; // tiefer rechnen aendert nichts mehr
		}
	}

	// Bis zum Spielende: erst Sieg/Remis/Niederlage (enges Fenster, schnell), dann - falls gewuenscht -
	// mit der Restzeit die genaue Steindifferenz. Ohne maximizeMargin: bei bewiesenem Sieg/Remis diesen
	// Zug nehmen, bei bewiesener Niederlage den besten Mittelspielzug behalten (setzt auf Fehler des Gegners).
	void Engine::solveEndgame(u64 P, u64 O, RootMoves& rm, int empties, bool maximizeMargin, Result& res)
	{
		res.wld = false;
		abort = false;
		int bestIdx;
		const int wld = searchRoot(P, O, rm, empties, -1, 1, bestIdx, true);
		if (abort) return;

		if (wld > -1) // Gewinn oder Remis gesichert -> diesen Zug auf jeden Fall nehmen
		{
			rm.moveToFront(bestIdx);
			res.move = rm.sq[0];
			res.score = wld;
			res.depth = empties;
			res.wld = true;
			reportProgress(res);
		}
		if (!maximizeMargin)
		{
			if (wld <= -1) { res.score = wld; res.wld = true; } // verloren: Mittelspielzug bleibt
			return;
		}
		const int exact = searchRoot(P, O, rm, empties, -INF, INF, bestIdx, true);
		if (!abort)
		{
			res.move = rm.sq[bestIdx];
			res.score = exact;
			res.depth = empties;
			res.exact = true;
			res.wld = false;
			reportProgress(res);
		}
		else if (wld <= -1) // verloren, aber knappste Niederlage nicht gefunden: Mittelspielzug behalten
		{
			res.score = wld;
			res.wld = true;
		}
	}

	// Schwache Stufen: alle Wurzelzuege genau bewerten, Rauschen addieren, besten waehlen.
	// Bewiesene Siege/Niederlagen werden nicht verrauscht. Liefert -1 bei Abbruch.
	int Engine::pickWithNoise(u64 P, u64 O, const RootMoves& rm, int depth, int noise)
	{
		static thread_local std::mt19937 rng{ std::random_device{}() };
		std::uniform_int_distribution<int> dist(-noise, noise);
		Searcher s(*this, true);
		int bestIdx = 0, best = 0;
		for (int i = 0; i < rm.n; i++)
		{
			const Position child = play(P, O, 1ULL << rm.sq[i]);
			int v = -s.negamax(child.P, child.O, depth - 1, -INF, INF, false);
			if (abort) return -1;
			if (!isProvenResult(v))
				v += dist(rng);
			if (i == 0 || v > best) { best = v; bestIdx = i; }
		}
		return bestIdx;
	}

	Result Engine::search(u64 P, u64 O, const Settings& s)
	{
		const auto start = Clock::now();
		nodes = 0;
		abort = false;
		nThreads = s.threads > 0 ? s.threads : std::max(1, (int)std::thread::hardware_concurrency());
		progress = &s.onProgress;
		probCut = s.probCut;
		probCutConfidence = s.probCutConfidence;
		Result res;
		auto finish = [&]() {
			res.stopped = stopRequested;
			res.nodes = nodes;
			res.seconds = std::chrono::duration<double>(Clock::now() - start).count();
			return res;
		};

		RootMoves rm = rootMoves(P, O);
		if (rm.n == 0 || stopRequested) return finish();
		res.move = rm.sq[0];
		if (rm.n == 1) return finish(); // nur ein Zug moeglich: sofort spielen

		const int empties = popcount(~(P | O));
		const bool endgame = empties <= s.endgameEmpties;

		// Im Endspiel nur kurz vorsortieren (10 % der Zeit), dann bis zum Ende rechnen.
		deadline = start + std::chrono::milliseconds(endgame ? std::max(1, s.timeMs / 10) : s.timeMs);
		const int maxDepth = std::min(s.maxDepth, endgame ? std::min(empties - 1, 12) : empties);
		iterativeDeepening(P, O, rm, maxDepth, empties, res);

		if (endgame && !res.exact && !stopRequested)
		{
			deadline = start + std::chrono::milliseconds(s.timeMs);
			solveEndgame(P, O, rm, empties, s.maximizeMargin, res);
		}

		if (s.noise > 0 && !res.exact && !res.wld && res.depth > 0 && !stopRequested)
		{
			const int idx = pickWithNoise(P, O, rm, res.depth, s.noise);
			if (idx >= 0) res.move = rm.sq[idx];
		}
		return finish();
	}

	// ======================= Spielstaerke-Stufen =======================

	namespace
	{
		// Namen stehen in der GUI. selective: ProbCut + nur Sieg/Remis/Niederlage im Endspiel (schneller)
		struct Level { int depth; int noise; int endgame; int maxSeconds; bool selective; };

		// Rauschen (Stufen 1-5) in Bewertungseinheiten: Ecke = 400, ein Zug Mobilitaet = 30.
		// Stufen 8-10 entsprechen der frueheren Zeitsteuerung mit ca. 3 s / 5 s / 10 s Bedenkzeit.
		constexpr Level LEVEL_TABLE[LEVELS] = {
			//  Tiefe Rauschen Endspiel Notbremse(s) selektiv
			{  1, 600,  0, 10, false },  //  1 Anfaenger
			{  1, 300,  0, 10, false },  //  2 Leicht
			{  2, 200,  0, 10, false },  //  3 Leicht+
			{  3, 120,  4, 10, false },  //  4 Mittel
			{  4,  50,  6, 10, false },  //  5 Mittel+
			{  5,   0,  8, 10, false },  //  6 Fortgeschritten
			{  7,   0, 10, 10, false },  //  7 Stark
			{ 15,   0, 20, 20, false },  //  8 Meister      (~3 s alt)
			{ 16,   0, 21, 25, false },  //  9 Grossmeister (~5 s alt)
			{ 18,   0, 24, 40, true  },  // 10 Weltklasse: Tiefe 18 mit ProbCut in der Zeit der frueheren
			                             //    Tiefe 17; perfektes Endspiel ab 24 statt 22 freien Feldern
		};

		const Level& levelData(int level) { return LEVEL_TABLE[std::clamp(level, 1, LEVELS) - 1]; }
	}

	Settings levelSettings(int level)
	{
		const Level& l = levelData(level);
		Settings s;
		s.maxDepth = l.depth;
		s.noise = l.noise;
		s.endgameEmpties = l.endgame;
		s.timeMs = l.maxSeconds * 1000;
		s.probCut = l.selective;
		s.maximizeMargin = !l.selective;
		return s;
	}

	Settings hintSettings()
	{
		Settings s;
		s.maxDepth = 60;       // keine feste Tiefe: so tief wie die Zeit erlaubt
		s.endgameEmpties = 24; // perfektes Endspiel wie die staerkste Stufe
		s.timeMs = 30000;      // laeuft, solange der Spieler nachdenkt (spaetestens 30 s)
		s.probCut = true;      // in derselben Zeit deutlich tiefer
		s.maximizeMargin = false;
		return s;
	}
}
