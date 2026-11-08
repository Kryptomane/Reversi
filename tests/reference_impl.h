// Automatisch aus dem Original (git HEAD: src/calculation.cpp) extrahiert - Referenz fuer den Vergleichstest.
#pragma once
#include "reversi_core.h"
#include <vector>
#include <algorithm>
#include <string>
#include <sstream>
namespace ref {
typedef std::vector<std::vector<unsigned char>> board;
struct calc {
	int8_t bsize = 8;
	bool debug = false;
	std::vector<field> xF, cF, eF;
	int8_t BlackChips, WhiteChips;
	calc() {
	xF.push_back(field(1, bsize-2));
	xF.push_back(field(bsize-2, bsize - 2));
	xF.push_back(field(bsize-2, 1));
	xF.push_back(field(1, 1));
	cF.push_back(field(0, 1));
	cF.push_back(field(1, 0));
	cF.push_back(field(0, bsize - 2));
	cF.push_back(field(1, bsize - 1));
	cF.push_back(field(bsize - 2, 0));
	cF.push_back(field(bsize - 1, 1));
	cF.push_back(field(bsize - 2, bsize - 1));
	cF.push_back(field(bsize - 1, bsize - 2));
	eF.push_back(field(0, 0));
	eF.push_back(field(0, bsize-1));
	eF.push_back(field(bsize-1, 0));
	eF.push_back(field(bsize-1, bsize-1));
	}
	int paint(board, int, const char*, std::string, std::string, bool) { return 0; }
	void checkInside(board m, field kandidat, int8_t& status);
	bool eckefrei(board m, field kandidat, int8_t color);
	int8_t checkMove(board m, field kandidat, int8_t color);
	void countChips(board m);
	doOut doMove(board& m, field kandidat, int8_t color);
	void fastMove(board& m, field kandidat, int8_t color);
	findOut findMove(board& m, int8_t color);
	std::vector<field> fastfind(board& m, int8_t color);
};
inline void calc::checkInside(board m, field kandidat, int8_t & status)
{
	status = 0;
	int8_t stx = std::max(0, kandidat.x - 1);
	int8_t sty = std::max(0, kandidat.y-1);
	int8_t fix = std::min(bsize - 1, kandidat.x + 1);
	int8_t fiy = std::min(bsize - 1, kandidat.y + 1);
	if (m[stx][kandidat.y] == 1 || m[stx][kandidat.y] == 2)
	{
		if (m[kandidat.x][sty] == 1 || m[kandidat.x][sty] == 2)
		{
			if (m[fix][kandidat.y] == 1 || m[fix][kandidat.y] == 2)
			{
				if (m[kandidat.x][fiy] == 1 || m[kandidat.x][fiy] == 2)
				{
					status = 1;
					if (m[stx][sty] == 1 || m[stx][sty] == 2)
					{
						if (m[stx][fiy] == 1 || m[stx][fiy] == 2)
						{
							if (m[fix][sty] == 1 || m[fix][sty] == 2)
							{
								if (m[fix][fiy] == 1 || m[fix][fiy] == 2)
								{
									status = 2;
								}
							}
						}
					}
				}
			}
		}
	}
}

inline void calc::countChips(board m)
{
	WhiteChips = 0; BlackChips = 0;
	for (int8_t i=0; i<m.size(); i++)
	{
		for (int8_t j=0; j<m[0].size(); j++)
		{ 
			if (m[i][j] == 1)
				BlackChips++;
			if (m[i][j] == 2)
				WhiteChips++;
		}
	}
}

inline void calc::fastMove(board& m, field kandidat, int8_t color)
{
	m[kandidat.x][kandidat.y] = color;
	std::vector<field> line;
	int8_t sti = std::max(0, kandidat.x - 1);
	int8_t stj = std::max(0, kandidat.y - 1);
	int8_t fii = std::min(kandidat.x + 1, bsize - 1);
	int8_t fij = std::min(kandidat.y + 1, bsize - 1);
	for (int8_t i = sti; i <= fii; i++)
	{
		for (int8_t j = stj; j <= fij; j++)
		{
			bool lineswitch = false;
			if (m[i][j] == 3 - color)
			{
				line.push_back(field(i, j));
				bool abbruch = false;
				int8_t ri = i - kandidat.x;
				int8_t rj = j - kandidat.y;
				int8_t laufx = i + ri;
				int8_t laufy = j + rj;
				for (; abbruch == false; )
				{
					if (laufx < 0 || laufx >= bsize || laufy < 0 || laufy >= bsize)
					{
						abbruch = true;
					}
					else
					{
						if (m[laufx][laufy] == 3 - color) //laufe
						{
							line.push_back(field(laufx, laufy));
						}
						if (m[laufx][laufy] == color) // eigener Stein
						{
							abbruch = true;
							lineswitch = true;
						}
						if (m[laufx][laufy] != color && m[laufx][laufy] != 3 - color) //anderes Feld
						{
							abbruch = true;
						}
						laufx += ri;
						laufy += rj;
					}
				
				}
			}
			if (lineswitch)
			{
				for (int8_t k = 0; k < line.size(); k++)
				{
					m[line[k].x][line[k].y] = color;
				}
			}
			line.erase(line.begin(), line.end());
		}
	}
}

inline doOut calc::doMove(board& m, field kandidat, int8_t color)
{
	doOut a;
	a.Half = 0;
	a.In = 0;
	a.Out = 0;
	std::vector<field> line;
	std::vector<field> change;
	change.push_back(kandidat);
	int8_t sti = std::max(0, kandidat.x - 1);
	int8_t stj = std::max(0, kandidat.y - 1);
	int8_t fii = std::min(kandidat.x + 1, bsize - 1);
	int8_t fij = std::min(kandidat.y + 1, bsize - 1);
	for (int8_t i = sti; i <= fii; i++)
	{
		for (int8_t j = stj; j <= fij; j++)
		{
			bool lineswitch = false;
			if (m[i][j] == 3 - color)
			{
				line.push_back(field(i, j));
				bool abbruch = false;
				int8_t ri = i - kandidat.x;
				int8_t rj = j - kandidat.y;
				int8_t laufx = i + ri;
				int8_t laufy = j + rj;
				for (; abbruch == false;)
				{
					if (laufx < 0 || laufx >= bsize || laufy < 0 || laufy >= bsize)
						abbruch = true;
					else
					{
						if (m[laufx][laufy] == 3 - color) //laufe
						{
							line.push_back(field(laufx, laufy));
						}
						if (m[laufx][laufy] == color) // eigener Stein
						{
							abbruch = true;
							lineswitch = true;
						}
						if (m[laufx][laufy] != color && m[laufx][laufy] != 3-color) //anderes Feld
						{
							abbruch = true;
						}
						laufx += ri;
						laufy += rj;
					}
				}
			}
			if (lineswitch)
			{
				change.insert(change.end(), line.begin(), line.end());
			}
			line.erase(line.begin(), line.end());
		}
	}
	int8_t status;
	for (int8_t i = 0; i < change.size(); i++)
	{
		m[change[i].x][change[i].y] = color;
		checkInside(m, change[i], status);
		if (status == 2)
			a.In++;
		if (status == 1)
			a.Half++;
		if (status == 0)
			a.Out++;
	}
	
	if (debug)
	{
		static int blabla = 0;
		std::ostringstream karl, karla; 
		karl << " ";
		paint(m, blabla, "Dodel", karl.str(), karla.str(), debug);
		blabla++;
	}
	return a; 
}

inline findOut calc::findMove(board& m, int8_t color)
{
	for (int8_t i = 0; i < bsize; i++)
		for (int8_t j = 0; j < bsize; j++)
			if (m[i][j] != 1 && m[i][j] != 2)
				m[i][j] = 0;	
	findOut a;
	for (int8_t xx = 0; xx < bsize; xx++)
	{
		for (int8_t yy = 0; yy < bsize; yy++)
		{
			if (m[xx][yy] == color)
			{
				//testfile << "Finde Züge für" << xx << " " << yy << std::endl;
				int8_t stx = std::max(0, xx - 1);
				int8_t sty = std::max(0, yy - 1);
				int8_t fix = std::min(xx + 1, bsize - 1);
				int8_t fiy = std::min(yy + 1, bsize - 1);
				for (int8_t i = stx; i <= fix; i++)
				{
					for (int8_t j = sty; j <= fiy; j++)
					{
						if (m[i][j] == 3 - color)
						{
							//testfile << "Fremder Stein bei " << i << " " << j << std::endl;
							bool abbruch = false;
							int8_t rx = i - xx;
							int8_t ry = j - yy;
							int8_t laufx = i + rx;
							int8_t laufy = j + ry;
							for (int8_t k = 2; abbruch == false; k++)
							{
								if (laufx < 0 || laufx >= bsize || laufy < 0 || laufy >= bsize)
									abbruch = true;
								else
								{
									if (m[laufx][laufy] == color) // eigener Stein
									{
										abbruch = true;
									}
									if (m[laufx][laufy] == 0 || m[laufx][laufy] == 3-color+2) // freies feld
									{
										abbruch = true;
										m[laufx][laufy] = color + 2;
										int8_t status = checkMove(m, field(laufx, laufy), color);
										if (status == 4)
										{
											a.sField.push_back(field(laufx, laufy));
										}
										if (status == 1)
										{
											a.cField.push_back(field(laufx, laufy));
										}
										if (status == 2)
										{
											a.xField.push_back(field(laufx, laufy));
										}
										if (status == 3)
										{
											a.eField.push_back(field(laufx, laufy));
										}
										if (status == 0)
										{
											a.Normal.push_back(field(laufx, laufy));
										}
										a.Ges.push_back(field(laufx, laufy));
										a.status.push_back(status);
									}
									if (m[laufx][laufy] == color + 2)
									{
										abbruch = true;
									}
									laufx += rx;
									laufy += ry;
								}
							}
						}
					}
				}
			}
		}
	}
	return a;
}

inline std::vector<field> calc::fastfind(board& m, int8_t color)
{
	std::vector<field> erg;
	for (int8_t i = 0; i < bsize; i++)
		for (int8_t j = 0; j < bsize; j++)
			if (m[i][j] != 1 && m[i][j] != 2)
				m[i][j] = 0;
	for (int8_t xx = 0; xx < bsize; xx++)
	{
		for (int8_t yy = 0; yy < bsize; yy++)
		{
			if (m[xx][yy] == color)
			{
				int8_t stx = std::max(0, xx - 1);
				int8_t sty = std::max(0, yy - 1);
				int8_t fix = std::min(xx + 1, bsize - 1);
				int8_t fiy = std::min(yy + 1, bsize - 1);
				for (int8_t i = stx; i <= fix; i++)
				{
					for (int8_t j = sty; j <= fiy; j++)
					{
						if (m[i][j] == 3 - color)
						{
							bool abbruch = false;
							int8_t rx = i - xx;
							int8_t ry = j - yy;
							int8_t laufx = i + rx;
							int8_t laufy = j + ry;
							for (; abbruch == false; )
							{
								if (laufx < 0 || laufx >= bsize || laufy < 0 || laufy >= bsize)
									abbruch = true;
								else
								{
									if (m[laufx][laufy] == color) // eigener Stein
									{
										abbruch = true;
									}
									if (m[laufx][laufy] == 0 || m[laufx][laufy] == 5 - color) // freies feld
									{
										abbruch = true;
										m[laufx][laufy] = color + 2;
										erg.push_back(field(laufx, laufy));
									}
									if (m[laufx][laufy] == color + 2)
									{
										abbruch = true;
									}
									laufx += rx;
									laufy += ry;
								}
								
							}
						}
					}
				}
			}
		}
	}
	return erg;
}
inline bool calc::eckefrei(board m, field kandidat, int8_t color)
{
	for (int8_t i = std::max(0, kandidat.x - 1); i <= std::min(kandidat.x + 1, bsize - 1); i++)
	{
		for (int8_t j = std::max(0, kandidat.y - 1); j <= std::min(kandidat.y + 1, bsize - 1); j++)
		{
			for (int8_t k = 0; k < eF.size(); k++)
			{
				if ((m[eF[k].x][eF[k].y] == 1 || m[eF[k].x][eF[k].y] == 2) && i == eF[k].x && j == eF[k].y)
					return false;
			}
		}
	}
	return true;
}

inline int8_t calc::checkMove(board m, field kandidat, int8_t color)
{
	if (kandidat.x == 0 || kandidat.x == bsize - 1)
	{
		if (m[kandidat.x][std::max(kandidat.y - 1, 0)] == 3 - color && m[kandidat.x][std::min(kandidat.y + 1, bsize - 1)] == 3 - color)
			return 4;
	}
	if (kandidat.y == 0 || kandidat.y == bsize - 1)
	{
		if (m[std::max(kandidat.x - 1, 0)][kandidat.y] == 3 - color && m[std::min(kandidat.x + 1, bsize - 1)][kandidat.y] == 3 - color)
			return 4;
	}
	for (int i = 0; i < cF.size(); i++)
	{
		if (cF[i].x == kandidat.x && cF[i].y == kandidat.y && eckefrei(m, kandidat, color))
			return 1;
	}
	for (int i = 0; i < xF.size(); i++)
	{
		if (xF[i].x == kandidat.x && xF[i].y == kandidat.y && eckefrei(m, kandidat, color))
			return 2;
	}
	for (int i = 0; i < eF.size(); i++)
	{
		if (eF[i].x == kandidat.x && eF[i].y == kandidat.y && eckefrei(m, kandidat, color))
			return 3;
	}
	return 0;
}
}
