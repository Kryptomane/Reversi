#include "game.h"

void Game::reset()
{
	black = bb::START_BLACK;
	white = bb::START_WHITE;
	current = Color::Black;
	over = false;
}

Game::Turn Game::play(int square)
{
	const bb::Position next = bb::play(own(), opponentDiscs(), 1ULL << square);
	// next ist aus Sicht des Gegners: next.P = Gegner, next.O = Spieler, der gerade gesetzt hat
	if (current == Color::Black) { white = next.P; black = next.O; }
	else { black = next.P; white = next.O; }
	return advanceTurn();
}

// Der Gegner ist dran, wenn er ziehen kann. Sonst setzt der Spieler erneut; kann keiner, ist Schluss.
Game::Turn Game::advanceTurn()
{
	current = opponent(current);
	if (legalMoves())
		return Turn::Next;
	current = opponent(current);
	if (legalMoves())
		return Turn::OpponentPasses;
	over = true;
	return Turn::Over;
}
