#include "evaluation.h"
#include <array>

namespace eval
{
	using namespace bb;

	namespace
	{
		// Gewichte der Bewertungsmerkmale
		constexpr int W_MOBILITY = 30;      // pro legalem Zug
		constexpr int W_FRONTIER = 8;       // pro Stein neben einem leeren Feld (negativ)
		constexpr int W_CORNER = 400;       // pro Ecke
		constexpr int W_X_SQUARE = 150;     // X-Feld neben freier Ecke (negativ)
		constexpr int W_C_SQUARE = 40;      // C-Feld neben freier Ecke (negativ)
		constexpr int W_STABLE = 60;        // pro stabilem Stein
		constexpr int DISC_PHASE = 24;      // Steinzahl zaehlt erst ab weniger als 24 freien Feldern

		constexpr u64 CORNERS = bit(0, 0) | bit(0, 7) | bit(7, 0) | bit(7, 7);

		// Pro Ecke: das diagonal angrenzende X-Feld und die beiden C-Felder an der Kante.
		struct CornerZone { u64 corner, xSquare, cSquares; };
		constexpr CornerZone CORNER_ZONES[4] = {
			{ bit(0, 0), bit(1, 1), bit(0, 1) | bit(1, 0) },
			{ bit(0, 7), bit(1, 6), bit(0, 6) | bit(1, 7) },
			{ bit(7, 0), bit(6, 1), bit(6, 0) | bit(7, 1) },
			{ bit(7, 7), bit(6, 6), bit(6, 7) | bit(7, 6) },
		};

		// Masken der 15 Diagonalen (x - y konstant) bzw. Gegendiagonalen (x + y konstant).
		constexpr std::array<u64, 15> diagonalMasks(bool anti)
		{
			std::array<u64, 15> m{};
			for (int x = 0; x < SIZE; x++)
				for (int y = 0; y < SIZE; y++)
					m[anti ? x + y : x - y + 7] |= bit(x, y);
			return m;
		}
		constexpr auto DIAGONALS = diagonalMasks(false);
		constexpr auto ANTI_DIAGONALS = diagonalMasks(true);

		// Felder auf vollstaendig besetzten Linien je Achse.
		struct FullLines { u64 alongY, alongX, diagonal, antiDiagonal; };

		FullLines fullLines(u64 occ)
		{
			FullLines f{};
			u64 r = occ & (occ >> 4); r &= r >> 2; r &= r >> 1;    // Byte (x fest) komplett gesetzt?
			f.alongY = (r & Y0) * 0xFFULL;
			u64 c = occ & (occ >> 32); c &= c >> 16; c &= c >> 8; // Bit in allen 8 Bytes gesetzt?
			f.alongX = (c & X0) * Y0;
			for (int i = 0; i < 15; i++)
			{
				if ((occ & DIAGONALS[i]) == DIAGONALS[i]) f.diagonal |= DIAGONALS[i];
				if ((occ & ANTI_DIAGONALS[i]) == ANTI_DIAGONALS[i]) f.antiDiagonal |= ANTI_DIAGONALS[i];
			}
			return f;
		}

		int cornerZonePenalty(u64 P, u64 O)
		{
			const u64 occ = P | O;
			int s = 0;
			for (const CornerZone& z : CORNER_ZONES)
			{
				if (occ & z.corner)
					continue; // Ecke besetzt: X/C-Felder sind dann ungefaehrlich
				s -= W_X_SQUARE * ((int)((P & z.xSquare) != 0) - (int)((O & z.xSquare) != 0));
				s -= W_C_SQUARE * (popcount(P & z.cSquares) - popcount(O & z.cSquares));
			}
			return s;
		}
	}

	u64 stableDiscs(u64 P, u64 O)
	{
		const FullLines full = fullLines(P | O);
		// Fixpunkt: stabil, wenn auf jeder der 4 Achsen die Linie voll ist oder auf einer Seite
		// ein stabiler eigener Stein bzw. der Brettrand liegt. Startet bei 0 und waechst monoton.
		u64 stable = 0;
		for (;;)
		{
			const u64 axisY = full.alongY | neighbourInOrOffBoard<0, -1>(stable) | neighbourInOrOffBoard<0, 1>(stable);
			const u64 axisX = full.alongX | neighbourInOrOffBoard<-1, 0>(stable) | neighbourInOrOffBoard<1, 0>(stable);
			const u64 diag = full.diagonal | neighbourInOrOffBoard<-1, -1>(stable) | neighbourInOrOffBoard<1, 1>(stable);
			const u64 anti = full.antiDiagonal | neighbourInOrOffBoard<-1, 1>(stable) | neighbourInOrOffBoard<1, -1>(stable);
			const u64 next = P & axisY & axisX & diag & anti;
			if (next == stable)
				return stable;
			stable = next;
		}
	}

	int evaluate(u64 P, u64 O)
	{
		const u64 occ = P | O;
		const u64 empty = ~occ;
		const u64 nextToEmpty = neighbours(empty);

		int s = 0;
		s += W_MOBILITY * (popcount(moves(P, O)) - popcount(moves(O, P)));
		s -= W_FRONTIER * (popcount(P & nextToEmpty) - popcount(O & nextToEmpty));
		s += W_CORNER * (popcount(P & CORNERS) - popcount(O & CORNERS));
		s += cornerZonePenalty(P, O);
		if (occ & CORNERS) // stabile Steine entstehen praktisch nur ueber Ecken
			s += W_STABLE * (popcount(stableDiscs(P, O)) - popcount(stableDiscs(O, P)));

		const int empties = popcount(empty);
		if (empties < DISC_PHASE)
			s += (DISC_PHASE - empties) * (popcount(P) - popcount(O));
		return s;
	}
}
