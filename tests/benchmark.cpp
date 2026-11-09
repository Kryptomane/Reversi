// Benchmark: Zeit bis Tiefe D (1 Thread, deterministisch) auf festen Stellungen + Endspiel exakt.
// Die Pruefsumme muss nach reinen Geschwindigkeits-Optimierungen gleich bleiben.
//   benchmark [Tiefe=12] [Leerfelder=18]
#include "testutil.h"
#include <cstdio>
#include <cstdlib>

int main(int argc, char** argv)
{
	const int depth = argc > 1 ? atoi(argv[1]) : 12;
	const int empties = argc > 2 ? atoi(argv[2]) : 18;
	long long checksum = 0;

	double midTime = 0; long long midNodes = 0;
	for (const bb::Position& p : testutil::samplePositions(24, 12, 1))
	{
		engine::Engine e(21);
		engine::Settings s;
		s.timeMs = 1000000; s.maxDepth = depth; s.threads = 1; s.endgameEmpties = 0;
		const engine::Result r = e.search(p.P, p.O, s);
		midTime += r.seconds; midNodes += r.nodes;
		checksum = checksum * 31 + r.score * 7 + r.move;
	}

	double endTime = 0; long long endNodes = 0;
	for (const bb::Position& p : testutil::samplePositions(64 - empties, 8, 2))
	{
		engine::Engine e(21);
		engine::Settings s;
		s.timeMs = 1000000; s.threads = 1; s.endgameEmpties = 64;
		const engine::Result r = e.search(p.P, p.O, s);
		endTime += r.seconds; endNodes += r.nodes;
		checksum = checksum * 31 + r.score;
	}

	printf("Mittelspiel Tiefe %d: %.2f s (%.1f Mio Knoten)   Endspiel %d frei: %.2f s (%.1f Mio Knoten)   Pruefsumme %lld\n",
	       depth, midTime, midNodes / 1e6, empties, endTime, endNodes / 1e6, checksum);
}
