#include "game.h"

void Game::reset()
{
	s = State{ bb::START_BLACK, bb::START_WHITE, Color::Black, false };
	history.clear();
}

Game::Turn Game::play(int square)
{
	history.push_back(s);
	const bb::Position next = bb::play(own(), opponentDiscs(), 1ULL << square);
	// next ist aus Sicht des Gegners: next.P = Gegner, next.O = Spieler, der gerade gesetzt hat
	if (s.current == Color::Black) { s.white = next.P; s.black = next.O; }
	else { s.black = next.P; s.white = next.O; }
	return advanceTurn();
}

void Game::undo()
{
	if (history.empty())
		return;
	s = history.back();
	history.pop_back();
}

// Der Gegner ist dran, wenn er ziehen kann. Sonst setzt der Spieler erneut; kann keiner, ist Schluss.
Game::Turn Game::advanceTurn()
{
	s.current = opponent(s.current);
	if (legalMoves())
		return Turn::Next;
	s.current = opponent(s.current);
	if (legalMoves())
		return Turn::OpponentPasses;
	s.over = true;
	return Turn::Over;
}
