#pragma once
// Suchmaschine: iterative Vertiefung, Negamax mit Alpha-Beta (PVS), Transpositionstabelle,
// Zugsortierung und exakte Endspielrechnung; parallel auf allen Kernen. Keine Abhaengigkeit von Qt.

#include "bitboard.h"
#include "transpositiontable.h"
#include <atomic>
#include <chrono>
#include <functional>

namespace engine
{
	using bb::u64;

	constexpr int WIN = 1000000;      // Terminalwert: +-(WIN + Steindifferenz)
	constexpr int INF = WIN + 64;     // groesser als jeder moegliche Wert

	inline bool isProvenResult(int score) { return score >= WIN - 64 || score <= -WIN + 64; }

	struct Result;

	struct Settings
	{
		int timeMs = 2000;       // Obergrenze fuer die Bedenkzeit
		int maxDepth = 60;       // Suchtiefe in Halbzuegen
		int endgameEmpties = 20; // ab so wenigen freien Feldern bis zum Spielende rechnen
		bool maximizeMargin = true; // nach bewiesenem Sieg auch die hoechste Steindifferenz suchen (langsam)
		int threads = 0;         // 0 = alle Kerne
		int noise = 0;           // >0: Zufallsrauschen +-noise auf die Zugbewertung (schwache Stufen)
		bool probCut = false;    // selektive Suche: unwahrscheinliche Zweige per flacher Vorhersage abschneiden
		double probCutConfidence = 1.5; // Sicherheitsabstand in Standardabweichungen (groesser = vorsichtiger)
		std::function<void(const Result&)> onProgress; // optional: nach jeder fertigen Iteration (Such-Thread!)
	};

	// Spielstaerke-Stufen 1 (Anfaenger) .. LEVELS. Die Staerke haengt an der Suchtiefe, nicht an der
	// Zeit -> auf jedem Geraet gleich stark. Die Zeitgrenze ist nur eine Notbremse.
	constexpr int LEVELS = 10;
	Settings levelSettings(int level);

	// Zugvorschlag: rechnet ohne feste Tiefe immer tiefer (bis zur Zeitgrenze) und meldet jede
	// Verbesserung ueber onProgress -> nach kurzer Zeit deutlich staerker als jede Stufe.
	Settings hintSettings();

	struct Result
	{
		int move = -1;        // Feldindex x*8+y, -1 = kein Zug (passen)
		int score = 0;        // aus Sicht des Spielers am Zug
		int depth = 0;        // letzte vollstaendig gerechnete Tiefe
		bool exact = false;   // bis zum Spielende gerechnet (score = +-(WIN + Differenz) bzw. 0)
		bool wld = false;     // nur Sieg/Remis/Niederlage bewiesen
		bool stopped = false; // per stop() abgebrochen
		long long nodes = 0;
		double seconds = 0;
	};

	class Engine
	{
	public:
		explicit Engine(int ttBits = 21) : tt(ttBits) {}

		Result search(u64 P, u64 O, const Settings& s);
		void stop() { stopRequested = true; }       // threadsicher; wirkt auch kurz vor dem Start
		void resetStop() { stopRequested = false; } // vor dem Start einer neuen Suche aufrufen
		void clear() { tt.clear(); }                // nicht waehrend einer Suche

	private:
		friend struct Searcher;
		using Clock = std::chrono::steady_clock;

		struct RootMoves
		{
			int sq[32];
			int n = 0;
			void moveToFront(int idx);
		};

		static RootMoves rootMoves(u64 P, u64 O);
		void iterativeDeepening(u64 P, u64 O, RootMoves& rm, int maxDepth, int empties, Result& res);
		void solveEndgame(u64 P, u64 O, RootMoves& rm, int empties, bool maximizeMargin, Result& res);
		void reportProgress(const Result& res) const { if (progress && *progress) (*progress)(res); }
		int pickWithNoise(u64 P, u64 O, const RootMoves& rm, int depth, int noise);

		int searchRoot(u64 P, u64 O, const RootMoves& rm, int depth, int alpha, int beta, int& bestIdx, bool checkTime);
		int searchFirstRootMove(u64 P, u64 O, int sq, int depth, int alpha, int beta, bool checkTime);
		int searchRemainingRootMoves(u64 P, u64 O, const RootMoves& rm, int depth, int alpha, int beta,
		                             int best, int& bestIdx, bool checkTime);

		TranspositionTable tt;
		int nThreads = 1;
		std::atomic<bool> abort{ false };
		std::atomic<bool> stopRequested{ false };
		std::atomic<long long> nodes{ 0 };
		bool probCut = false;
		double probCutConfidence = 1.5;
		Clock::time_point deadline;
		const std::function<void(const Result&)>* progress = nullptr;
	};
}
