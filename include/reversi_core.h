#pragma once
// Reversi-Kern ohne Qt/OpenCV: Brett-Typ, Ergebnis-Typen und Bitboard-Zuggenerator.
// Bit-Index eines Feldes: s = x*8 + y  (x = erster Index von board, y = zweiter).

#include <cstdint>
#include <cstddef>
#include <vector>
#include <bit>
#include <algorithm>

constexpr int BSIZE = 8;

// Flaches 8x8-Brett (64 Byte, keine Heap-Allokation, Kopie = memcpy).
// m[x][y] funktioniert wie vorher. Werte: 0 leer, 1 schwarz, 2 weiss, 3/4 Zug-Marker.
struct board
{
	unsigned char c[BSIZE][BSIZE] = {};
	unsigned char* operator[](size_t i) { return c[i]; }
	const unsigned char* operator[](size_t i) const { return c[i]; }
	static constexpr size_t size() { return BSIZE; }
};

struct field
{
	int8_t x;
	int8_t y;
	field(int8_t a, int8_t b) : x(a), y(b) {}
	field() : x(-1), y(-1) {}
};

typedef struct
{
	int8_t In;
	int8_t Half;
	int8_t Out;
}doOut;

typedef struct
{
	std::vector<field> Normal;
	std::vector<field> sField;
	std::vector<field> cField;
	std::vector<field> xField;
	std::vector<field> eField;
	std::vector<field> Ges;
	std::vector<int8_t> status;
}findOut;

namespace bb
{
	using u64 = uint64_t;

	constexpr u64 Y0 = 0x0101010101010101ULL; // y == 0
	constexpr u64 Y7 = 0x8080808080808080ULL; // y == 7
	constexpr u64 X0 = 0x00000000000000FFULL; // x == 0
	constexpr u64 X7 = 0xFF00000000000000ULL; // x == 7

	inline int popcount(u64 b) { return std::popcount(b); }
	inline int lsb(u64 b) { return std::countr_zero(b); }
	constexpr u64 bit(int x, int y) { return 1ULL << (x * 8 + y); }

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

	template<int DX, int DY>
	inline u64 movesDir(u64 P, u64 O, u64 E)
	{
		u64 t = shift<DX, DY>(P) & O;
		t |= shift<DX, DY>(t) & O;
		t |= shift<DX, DY>(t) & O;
		t |= shift<DX, DY>(t) & O;
		t |= shift<DX, DY>(t) & O;
		t |= shift<DX, DY>(t) & O;
		return shift<DX, DY>(t) & E;
	}

	// Alle legalen Zuege fuer P gegen O.
	inline u64 moves(u64 P, u64 O)
	{
		const u64 E = ~(P | O);
		return movesDir<-1, -1>(P, O, E) | movesDir<-1, 0>(P, O, E) | movesDir<-1, 1>(P, O, E)
			| movesDir<0, -1>(P, O, E) | movesDir<0, 1>(P, O, E)
			| movesDir<1, -1>(P, O, E) | movesDir<1, 0>(P, O, E) | movesDir<1, 1>(P, O, E);
	}

	template<int DX, int DY>
	inline u64 flipsDir(u64 P, u64 O, u64 m)
	{
		u64 f = 0;
		u64 x = shift<DX, DY>(m);
		while (x & O)
		{
			f |= x;
			x = shift<DX, DY>(x);
		}
		return (x & P) ? f : 0;
	}

	// Steine, die beim Setzen auf m (ein Bit) umgedreht werden.
	inline u64 flips(u64 P, u64 O, u64 m)
	{
		return flipsDir<-1, -1>(P, O, m) | flipsDir<-1, 0>(P, O, m) | flipsDir<-1, 1>(P, O, m)
			| flipsDir<0, -1>(P, O, m) | flipsDir<0, 1>(P, O, m)
			| flipsDir<1, -1>(P, O, m) | flipsDir<1, 0>(P, O, m) | flipsDir<1, 1>(P, O, m);
	}

	// Bit s gesetzt, wenn das Nachbarfeld (s + Richtung) belegt ist oder ausserhalb liegt.
	template<int DX, int DY>
	constexpr u64 nbFilled(u64 occ)
	{
		constexpr u64 off = (DX < 0 ? X0 : 0) | (DX > 0 ? X7 : 0) | (DY < 0 ? Y0 : 0) | (DY > 0 ? Y7 : 0);
		return shift<-DX, -DY>(occ) | off;
	}

	// Entspricht checkInside: status 1 = alle 4 orthogonalen Nachbarn belegt, 2 = alle 8.
	inline u64 inside1(u64 occ)
	{
		return nbFilled<-1, 0>(occ) & nbFilled<1, 0>(occ) & nbFilled<0, -1>(occ) & nbFilled<0, 1>(occ);
	}
	inline u64 insideDiag(u64 occ)
	{
		return nbFilled<-1, -1>(occ) & nbFilled<-1, 1>(occ) & nbFilled<1, -1>(occ) & nbFilled<1, 1>(occ);
	}

	// Brett -> Bitboards (P = color, O = 3-color). Marker 3/4 werden ignoriert.
	inline void load(const board& m, int color, u64& P, u64& O)
	{
		const unsigned char* c = &m.c[0][0];
		const unsigned char opp = (unsigned char)(3 - color);
		u64 p = 0, o = 0;
		for (int k = 0; k < 64; k++)
		{
			p |= (u64)(c[k] == color) << k;
			o |= (u64)(c[k] == opp) << k;
		}
		P = p;
		O = o;
	}

	inline void clearMarkers(board& m)
	{
		unsigned char* c = &m.c[0][0];
		for (int k = 0; k < 64; k++)
			if (c[k] > 2) c[k] = 0;
	}

	inline void setBits(board& m, u64 b, unsigned char v)
	{
		while (b)
		{
			int s = lsb(b);
			m.c[s >> 3][s & 7] = v;
			b &= b - 1;
		}
	}

	// Feldtyp fuer checkMove: 1 = C-Feld, 2 = X-Feld, 3 = Ecke, 0 sonst; plus zugehoerige Ecke.
	struct SqInfo { int8_t type; int8_t corner; };
	inline const SqInfo* sqTable()
	{
		static const SqInfo* t = []() {
			static SqInfo tab[64];
			for (int s = 0; s < 64; s++) tab[s] = { 0, -1 };
			auto set = [](int x, int y, int8_t type, int cx, int cy) { tab[x * 8 + y] = { type, (int8_t)(cx * 8 + cy) }; };
			set(0, 1, 1, 0, 0); set(1, 0, 1, 0, 0);
			set(0, 6, 1, 0, 7); set(1, 7, 1, 0, 7);
			set(6, 0, 1, 7, 0); set(7, 1, 1, 7, 0);
			set(6, 7, 1, 7, 7); set(7, 6, 1, 7, 7);
			set(1, 1, 2, 0, 0); set(1, 6, 2, 0, 7); set(6, 1, 2, 7, 0); set(6, 6, 2, 7, 7);
			set(0, 0, 3, 0, 0); set(0, 7, 3, 0, 7); set(7, 0, 3, 7, 0); set(7, 7, 3, 7, 7);
			return tab;
		}();
		return t;
	}

	// Identisch zu calc::checkMove (Status 4 stabil, 3 Ecke, 2 X, 1 C, 0 normal).
	inline int8_t moveStatus(u64 O, u64 occ, int s)
	{
		const int x = s >> 3, y = s & 7;
		auto isOpp = [O](int xx, int yy) { return (O >> (xx * 8 + yy)) & 1ULL; };
		if (x == 0 || x == 7)
			if (isOpp(x, std::max(y - 1, 0)) && isOpp(x, std::min(y + 1, 7)))
				return 4;
		if (y == 0 || y == 7)
			if (isOpp(std::max(x - 1, 0), y) && isOpp(std::min(x + 1, 7), y))
				return 4;
		const SqInfo& si = sqTable()[s];
		if (si.type == 1 || si.type == 2)
			return ((occ >> si.corner) & 1ULL) ? 0 : si.type;
		return si.type; // 3 = Ecke, 0 = normal
	}

	// ---- Ersatz fuer die calc-Methoden (gleiches Verhalten, siehe tests/movegen_test.cpp) ----

	// findMove: setzt Marker color+2 auf alle legalen Felder und klassifiziert sie.
	// Reihenfolge der Zuege: nach Feldindex (x, dann y).
	inline findOut findMove(board& m, int8_t color)
	{
		clearMarkers(m);
		u64 P, O;
		load(m, color, P, O);
		u64 mv = moves(P, O);
		const u64 occ = P | O;
		findOut a;
		a.Ges.reserve(popcount(mv));
		a.status.reserve(popcount(mv));
		while (mv)
		{
			const int s = lsb(mv);
			mv &= mv - 1;
			const field f((int8_t)(s >> 3), (int8_t)(s & 7));
			m.c[s >> 3][s & 7] = (unsigned char)(color + 2);
			const int8_t st = moveStatus(O, occ, s);
			switch (st)
			{
			case 4: a.sField.push_back(f); break;
			case 1: a.cField.push_back(f); break;
			case 2: a.xField.push_back(f); break;
			case 3: a.eField.push_back(f); break;
			default: a.Normal.push_back(f); break;
			}
			a.Ges.push_back(f);
			a.status.push_back(st);
		}
		return a;
	}

	inline std::vector<field> fastfind(board& m, int8_t color)
	{
		clearMarkers(m);
		u64 P, O;
		load(m, color, P, O);
		u64 mv = moves(P, O);
		std::vector<field> erg;
		erg.reserve(popcount(mv));
		while (mv)
		{
			const int s = lsb(mv);
			mv &= mv - 1;
			m.c[s >> 3][s & 7] = (unsigned char)(color + 2);
			erg.emplace_back((int8_t)(s >> 3), (int8_t)(s & 7));
		}
		return erg;
	}

	inline void fastMove(board& m, field k, int8_t color)
	{
		u64 P, O;
		load(m, color, P, O);
		const u64 mbit = bit(k.x, k.y);
		const u64 f = flips(P, O, mbit);
		m.c[k.x][k.y] = (unsigned char)color;
		setBits(m, f, (unsigned char)color);
	}

	inline doOut doMove(board& m, field k, int8_t color)
	{
		u64 P, O;
		load(m, color, P, O);
		const u64 mbit = bit(k.x, k.y);
		const u64 f = flips(P, O, mbit);
		m.c[k.x][k.y] = (unsigned char)color;
		setBits(m, f, (unsigned char)color);

		const u64 occ = P | O | mbit;
		const u64 changed = f | mbit;
		const u64 in1 = inside1(occ);
		const u64 in2 = in1 & insideDiag(occ);
		doOut a;
		a.In = (int8_t)popcount(changed & in2);
		a.Half = (int8_t)popcount(changed & in1 & ~in2);
		a.Out = (int8_t)popcount(changed & ~in1);
		return a;
	}

	inline void countChips(const board& m, int8_t& black, int8_t& white)
	{
		u64 B, W;
		load(m, 1, B, W);
		black = (int8_t)popcount(B);
		white = (int8_t)popcount(W);
	}
}
