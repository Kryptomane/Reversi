#pragma once
// Suchmaschine: iterative Vertiefung, Negamax mit Alpha-Beta (PVS), Transpositionstabelle,
// Zugsortierung und exakte Endspielrechnung. Parallel: an der Wurzel wird der erste Zug allein
// gerechnet (setzt alpha), die restlichen Zuege verteilen sich auf einen Thread-Pool mit
// gemeinsamem alpha und gemeinsamer (lock-freier) Transpositionstabelle.
// Arbeitet nur auf Bitboards (reversi_core.h), keine Abhaengigkeit von Qt/OpenCV.

#include "reversi_core.h"
#include <vector>
#include <chrono>
#include <atomic>
#include <memory>

namespace engine
{
	using bb::u64;

	constexpr int WIN = 1000000; // Terminalwert: WIN + Steindifferenz

	struct Settings
	{
		int timeMs = 2000;       // Bedenkzeit pro Zug
		int maxDepth = 60;       // Obergrenze Suchtiefe (Halbzuege)
		int endgameEmpties = 20; // ab so wenigen freien Feldern bis zum Spielende rechnen
		int threads = 0;         // 0 = alle Kerne
	};

	struct Result
	{
		int move = -1;        // Feldindex x*8+y, -1 = kein Zug (passen)
		int score = 0;        // aus Sicht des Spielers am Zug
		int depth = 0;        // letzte vollstaendig gerechnete Tiefe
		bool exact = false;   // Partie bis zum Ende durchgerechnet (score = WIN + Differenz bzw. 0)
		bool wld = false;     // nur Sieg/Remis/Niederlage bewiesen, Differenz nicht maximiert
		bool stopped = false; // per stop() abgebrochen
		int threads = 1;
		long long nodes = 0;
		double seconds = 0;
		double wldSeconds = 0; // Zeit bis Sieg/Remis/Niederlage bewiesen war (Endspiel)
	};

	// Statische Bewertung aus Sicht von P (Spieler am Zug).
	int evaluate(u64 P, u64 O);

	// Stabile Steine von P (konservative Abschaetzung).
	u64 stableDiscs(u64 P, u64 O);

	class Engine
	{
	public:
		explicit Engine(int ttBits = 21);
		Result search(u64 P, u64 O, const Settings& s);
		void stop();      // threadsicher: laufende (oder gleich startende) Suche abbrechen
		void resetStop(); // vor dem Start einer neuen Suche aufrufen (vom aufrufenden Thread)
		void clear();  // Transpositionstabelle leeren (nicht waehrend einer Suche)

	private:
		friend struct Searcher;
		struct TTSlot { std::atomic<u64> keyx{ 0 }; std::atomic<u64> data{ 0 }; };

		int rootSearch(u64 P, u64 O, int* sq, int n, int depth, int alpha, int beta, int& bestIdx, bool checkTime);

		std::unique_ptr<TTSlot[]> tt;
		u64 ttMask;
		int nThreads = 1;
		std::atomic<bool> abort{ false };
		std::atomic<bool> stopRequested{ false };
		std::atomic<long long> nodes{ 0 };
		std::chrono::steady_clock::time_point deadline;
	};

	// Hilfsfunktion fuer die GUI: Bitboards fuer 'color' aus dem Brett holen und Zug suchen.
	inline Result bestMove(Engine& e, const board& m, int8_t color, const Settings& s)
	{
		u64 P, O;
		bb::load(m, color, P, O);
		return e.search(P, O, s);
	}
}
