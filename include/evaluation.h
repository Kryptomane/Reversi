#pragma once
// Statische Bewertung einer Stellung (Mittelspiel-Heuristik).
#include "bitboard.h"

namespace eval
{
	using bb::u64;

	// Bewertung aus Sicht von P (Spieler am Zug); positiv = gut fuer P.
	// Einheiten: eine Ecke = 400, ein Zug Mobilitaet = 30.
	int evaluate(u64 P, u64 O);

	// Steine von P, die bis Spielende nicht mehr umgedreht werden koennen (sichere untere Schranke).
	u64 stableDiscs(u64 P, u64 O);
}
