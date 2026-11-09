#pragma once
// Spielstand einer Partie: Steine beider Farben, wer am Zug ist, Passen und Spielende.
#include "bitboard.h"
#include <vector>

enum class Color { Black, White };

inline Color opponent(Color c) { return c == Color::Black ? Color::White : Color::Black; }

class Game
{
public:
	// Was nach einem Zug passiert: der Gegner ist dran, der Gegner muss passen, oder das Spiel ist aus.
	enum class Turn { Next, OpponentPasses, Over };

	Game() { reset(); }
	void reset();

	Color toMove() const { return s.current; }
	bool isOver() const { return s.over; }
	bb::u64 discs(Color c) const { return c == Color::Black ? s.black : s.white; }
	int count(Color c) const { return bb::popcount(discs(c)); }

	// Sicht des Spielers am Zug: eigene (P) und gegnerische (O) Steine, z. B. fuer die Engine.
	bb::u64 own() const { return discs(s.current); }
	bb::u64 opponentDiscs() const { return discs(opponent(s.current)); }

	bb::u64 legalMoves() const { return s.over ? 0 : bb::moves(own(), opponentDiscs()); }
	bool isLegal(int square) const { return square >= 0 && square < 64 && (legalMoves() >> square & 1); }

	// Setzt fuer den Spieler am Zug (Zug muss legal sein) und bestimmt, wer als Naechstes dran ist.
	Turn play(int square);

	// Nimmt den letzten Zug zurueck (Stellung, Spieler am Zug und Spielende wie vorher).
	bool canUndo() const { return !history.empty(); }
	void undo();

private:
	struct State
	{
		bb::u64 black = 0, white = 0;
		Color current = Color::Black;
		bool over = false;
	};

	Turn advanceTurn();

	State s;
	std::vector<State> history; // Stellung vor jedem Zug
};
