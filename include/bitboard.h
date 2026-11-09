#pragma once
// Bitboard-Grundlagen: ein Brett ist ein Paar 64-Bit-Zahlen (eigene Steine P, gegnerische O).
// Feldindex s = x*8 + y  (x = Spalte, y = Zeile). Keine Abhaengigkeit von Qt.

#include <cstdint>
#include <bit>

namespace bb
{
	using u64 = uint64_t;

	constexpr int SIZE = 8;

	constexpr u64 Y0 = 0x0101010101010101ULL; // y == 0
	constexpr u64 Y7 = 0x8080808080808080ULL; // y == 7
	constexpr u64 X0 = 0x00000000000000FFULL; // x == 0
	constexpr u64 X7 = 0xFF00000000000000ULL; // x == 7

	constexpr int square(int x, int y) { return x * SIZE + y; }
	constexpr u64 bit(int x, int y) { return 1ULL << square(x, y); }
	inline int popcount(u64 b) { return std::popcount(b); }
	inline int lsb(u64 b) { return std::countr_zero(b); }

	// Startaufstellung: Schwarz auf d5/e4, Weiss auf d4/e5.
	constexpr u64 START_BLACK = bit(3, 4) | bit(4, 3);
	constexpr u64 START_WHITE = bit(3, 3) | bit(4, 4);

	// Verschiebt jedes Bit von (x,y) nach (x+DX, y+DY); was vom Brett faellt, verschwindet.
	template<int DX, int DY>
	constexpr u64 shift(u64 b)
	{
		constexpr int d = DX * 8 + DY;
		u64 r;
		if constexpr (d > 0) r = b << d;
		else r = b >> (-d);
		if constexpr (DY > 0) r &= ~Y0;
		if constexpr (DY < 0) r &= ~Y7;
		return r;
	}

	// Zuege in einer Richtung: zusammenhaengende Gegnersteine ab einem eigenen Stein, dahinter ein leeres Feld.
	template<int DX, int DY>
	inline u64 movesInDirection(u64 P, u64 O, u64 empty)
	{
		u64 t = shift<DX, DY>(P) & O;
		t |= shift<DX, DY>(t) & O;
		t |= shift<DX, DY>(t) & O;
		t |= shift<DX, DY>(t) & O;
		t |= shift<DX, DY>(t) & O;
		t |= shift<DX, DY>(t) & O;
		return shift<DX, DY>(t) & empty;
	}

	// Alle legalen Zuege fuer P gegen O.
	inline u64 moves(u64 P, u64 O)
	{
		const u64 e = ~(P | O);
		return movesInDirection<-1, -1>(P, O, e) | movesInDirection<-1, 0>(P, O, e) | movesInDirection<-1, 1>(P, O, e)
			| movesInDirection<0, -1>(P, O, e) | movesInDirection<0, 1>(P, O, e)
			| movesInDirection<1, -1>(P, O, e) | movesInDirection<1, 0>(P, O, e) | movesInDirection<1, 1>(P, O, e);
	}

	// Umgedrehte Steine in einer Richtung beim Setzen auf m.
	template<int DX, int DY>
	inline u64 flipsInDirection(u64 P, u64 O, u64 m)
	{
		u64 line = 0;
		u64 x = shift<DX, DY>(m);
		while (x & O)
		{
			line |= x;
			x = shift<DX, DY>(x);
		}
		return (x & P) ? line : 0;
	}

	// Steine, die beim Setzen auf das Feld m (genau ein Bit) umgedreht werden.
	inline u64 flips(u64 P, u64 O, u64 m)
	{
		return flipsInDirection<-1, -1>(P, O, m) | flipsInDirection<-1, 0>(P, O, m) | flipsInDirection<-1, 1>(P, O, m)
			| flipsInDirection<0, -1>(P, O, m) | flipsInDirection<0, 1>(P, O, m)
			| flipsInDirection<1, -1>(P, O, m) | flipsInDirection<1, 0>(P, O, m) | flipsInDirection<1, 1>(P, O, m);
	}

	// Alle Felder, die an ein Feld aus b angrenzen.
	inline u64 neighbours(u64 b)
	{
		return shift<-1, -1>(b) | shift<-1, 0>(b) | shift<-1, 1>(b) | shift<0, -1>(b)
			| shift<0, 1>(b) | shift<1, -1>(b) | shift<1, 0>(b) | shift<1, 1>(b);
	}

	// Bit s gesetzt, wenn das Nachbarfeld (s + Richtung) in b liegt oder ausserhalb des Bretts.
	template<int DX, int DY>
	constexpr u64 neighbourInOrOffBoard(u64 b)
	{
		constexpr u64 off = (DX < 0 ? X0 : 0) | (DX > 0 ? X7 : 0) | (DY < 0 ? Y0 : 0) | (DY > 0 ? Y7 : 0);
		return shift<-DX, -DY>(b) | off;
	}

	// Ein Zug als Ergebnis: neue Stellung aus Sicht des naechsten Spielers (Rollen getauscht).
	struct Position { u64 P, O; };
	inline Position play(u64 P, u64 O, u64 m)
	{
		const u64 f = flips(P, O, m);
		return { O & ~f, P | f | m };
	}
}
