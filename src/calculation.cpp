#include "calculation.h"
#include "boardwidget.h"
#include "apppaths.h"
#include <QPainter>

calc::calc(int number)
{
	bsize = BSIZE; // Bitboard-Kern ist fest auf 8x8
	instance = number;
	exportDirectory = logDirectory();
	makeNewLog();
	initBoard();
	makespecialvectors();
	refd = 0;
	reff = 0;
	aufrufe = 0;
}

void calc::makeNewLog()
{
	logfile.flush();
	logfile.close();
	QString path = exportDirectory + "CALC_" + QString::number(instance) + ".txt";
	logfile.open(path.toStdString());
	testfile.init(tl::TinyLog::INFO, &logfile);
}

void calc::initBoard()
{
	for (int8_t i = 0; i < bsize; i++)
		for (int8_t j = 0; j < bsize; j++)
			mboard[i][j] = 0;
	int8_t middle = bsize / 2;
	mboard[middle - 1][middle] = 1;
	mboard[middle][middle - 1] = 1;
	mboard[middle - 1][middle - 1] = 2;
	mboard[middle][middle] = 2;
	findMove(mboard, 1);
	countChips(mboard);
}

void calc::makespecialvectors()
{
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

void calc::sayHello()
{
	testfile << "Hallo! Ich bin Instanz: " << instance << std::endl;
	testfile << "Folgende Parameter wurden mir übergeben:" << std::endl;
	testfile << "mind e x c normal stable sieg gewicht" << std::endl;
	testfile << (int)mind << " " << eval.efield << " " << eval.xfield << " " <<  eval.cfield << " " << eval.normal << " " << eval.sfield << " " << eval.sieg << " " << eval.weight  << std::endl;
	testfile << " " << std::endl;
}

void calc::checkInside(const board& m, field kandidat, int8_t & status)
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

void calc::countChips(const board& m)
{
	bb::countChips(m, BlackChips, WhiteChips);
}

// Zuggenerator: Bitboard-Kern in reversi_core.h (Vergleichstest: tests/movegen_test.cpp)
void calc::fastMove(board& m, field kandidat, int8_t color)
{
	bb::fastMove(m, kandidat, color);
}

doOut calc::doMove(board& m, field kandidat, int8_t color)
{
	doOut a = bb::doMove(m, kandidat, color);
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

findOut calc::findMove(board& m, int8_t color)
{
	return bb::findMove(m, color);
}

std::vector<field> calc::fastfind(board& m, int8_t color)
{
	return bb::fastfind(m, color);
}

// Zufallspartie komplett auf Bitboards; das Endbrett wird nach m zurueckgeschrieben.
bool calc::pcRandomSimulation(board& m, int8_t color)
{
	double simtime = clock();
	bb::u64 P, O; // P = Spieler am Zug (color), O = Gegner
	bb::load(m, color, P, O);
	for (;;)
	{
		bb::u64 mv = bb::moves(P, O);
		if (!mv)
		{
			// Passen: Gegner ist am Zug. Kann der auch nicht, ist das Spiel aus.
			if (!bb::moves(O, P))
				break;
			std::swap(P, O);
			color = 3 - color;
			continue;
		}
		for (int ran = rand() % bb::popcount(mv); ran > 0; ran--)
			mv &= mv - 1;
		const bb::u64 mbit = mv & (0 - mv);
		const bb::u64 f = bb::flips(P, O, mbit);
		P |= f | mbit;
		O &= ~f;
		std::swap(P, O);
		color = 3 - color;
	}
	const bb::u64 black = (color == 1) ? P : O;
	const bb::u64 white = (color == 1) ? O : P;
	for (int k = 0; k < 64; k++)
		m.c[k / 8][k % 8] = (black >> k & 1) ? 1 : ((white >> k & 1) ? 2 : 0);
	BlackChips = (int8_t)bb::popcount(black);
	WhiteChips = (int8_t)bb::popcount(white);
	reff += (clock() - simtime) / CLOCKS_PER_SEC;
	return WhiteChips > BlackChips;
}

//1 == schwarz == mensch
void calc::montecarlo(board& m, int8_t color)
{
	std::vector<field> res = fastfind(m, color);
	field kandidat(-1, -1);
	int refbl = -1;
	int refwh = -1;
	for (int i = 0; i < res.size(); i++)
	{
		board temp = m;
		fastMove(temp, res[i], color);
		int whitewin = 0;
		int blackwin = 0;
		reff = 0;
		refd = 0;
		for (int j = 0; j < 500; j++)
		{
			board temp2 = temp;
			if (pcRandomSimulation(temp2, 3 - color))
				whitewin++;
			else
				blackwin++;
			//paint(temp2, j, true);
		}
		testfile << "Zug: " << (int)res[i].x << " " << (int)res[i].y << " Siege: " << whitewin << " Verluste: " << blackwin << std::endl;
		testfile << "Finde: " << reff << " DO: " << refd << std::endl;
		if ((whitewin > refwh && color==2) || (blackwin > refbl && color == 1))
			kandidat = res[i];
	}
	if (kandidat.x != -1)
	{
		testfile << "Zug ausgeführt: " << (int)kandidat.x << " " << (int)kandidat.y << std::endl;
		fastMove(m, kandidat, color);
	}
}

//aufruf mit mboard, 0, 2 = weiß
int calc::silasindustries(board& m, int8_t tiefe, int8_t color)
{
	aufrufe++;
	std::vector<Knoti> Ele;
	findOut a;
	if (tiefe < mind)
	{
		a = findMove(m, color);
		if (a.Ges.size() == 0)
		{
			//testfile << "Color " << (int)color << " Tiefe: " << (int)tiefe << std::endl;
			a = findMove(m, 3 - color);
			if (a.Ges.size() == 0)
			{
				countChips(m);
				if (BlackChips > WhiteChips)
					return -eval.sieg;
				else
					return eval.sieg;
			}
			else
			{
				Ele.resize(a.Ges.size());
				for (int8_t i = 0; i < a.Ges.size(); i++)
				{
					if (tiefe == 0)
					{
						Ele[i].move = a.Ges[i];
						Ele[i].Score = 0;
					}
					board temp = m;
					doOut dotemp = doMove(temp, a.Ges[i], 3 - color);
					if (color==1)
						Ele[i].Score = evaluation(temp, a.Ges[i], dotemp, 3-color, a.status[i])*(-1);
					else 
						Ele[i].Score = evaluation(temp, a.Ges[i], dotemp, 3-color, a.status[i]);
					Ele[i].Score += eval.weight * silasindustries(temp, tiefe + 1, color) /(double) a.Ges.size();
				}
			}
		}
		else
		{
			Ele.resize(a.Ges.size());
			for (int8_t i = 0; i < a.Ges.size(); i++)
			{
				if (tiefe == 0)
				{
					Ele[i].move = a.Ges[i];
					Ele[i].Score = 0;
				}
				board temp = m;
				doOut dotemp = doMove(temp, a.Ges[i], color);
				if (color == 1)
					Ele[i].Score = evaluation(temp, a.Ges[i], dotemp, color, a.status[i])*(-1);
				else
					Ele[i].Score = evaluation(temp, a.Ges[i], dotemp, color, a.status[i]);
				Ele[i].Score += silasindustries(temp, tiefe +1, 3 - color) / a.Ges.size();
			}
		}
	}
	if (tiefe >= mind)
	{
		return 0;
	}
	int index = 0;
	int RefScore = Ele[0].Score;
	if (tiefe == 0)
	{
		for (int i = 0; i < Ele.size(); i++)//speichere extern
		{
			//testfile << "Ele " << i << " Zug: " << (int)Ele[i].move.x << " " << (int)Ele[i].move.y << " Score: " << Ele[i].Score << std::endl;
			if (color == 1)
			{
				if (Ele[i].Score < RefScore)
				{
					index = i;
					RefScore = Ele[i].Score;
				}
			}
			else 
			{
				if (Ele[i].Score > RefScore)
				{
					index = i;
					RefScore = Ele[i].Score;
				}
			}
		}
		static int zug = 0;
		//testfile << "Zug: " << zug << " Farbe: " << (int)color << " Mache: " << (int)Ele[index].move.x << " " << (int)Ele[index].move.y << std::endl;
		doMove(m, Ele[index].move, color);
		zug++;
	}
}

bool calc::eckefrei(const board& m, field kandidat, int8_t color)
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

int8_t calc::checkMove(const board& m, field kandidat, int8_t color)
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

int calc::evaluation(const board& m, field kandidat, doOut dos, int8_t color, int8_t special)
{
	int evalint = 0;
	//int8_t special = checkMove(m, kandidat, color);
	if (special == 4) //stable
		evalint += eval.sfield;
	if (special == 3) //Ecke
		evalint += eval.efield;
	if (special == 2) //x Feld
		evalint += eval.xfield;
	if (special == 1) //c Feld
		evalint += eval.cfield;
	if (special == 0)
		evalint += eval.normal;
	evalint -= dos.Out;

	/*
	findOut fr = findMove(m, 3-color);
	eval -= 3*fr.Normal.size();
	eval -= (10 * fr.eField.size());
	*/
	return evalint;
}

// Brett als Bild (fuer Debug-Ausgaben); die GUI zeichnet direkt im BoardWidget.
QImage calc::paint(const board& m, int nr, std::string name, std::string evalstring, std::string evalb, bool saved, int hint)
{
	const int groesse = 600;
	QImage img(groesse, groesse, QImage::Format_RGB32);
	QPainter p(&img);
	drawBoard(p, QRectF(0, 0, groesse, groesse), m, hint);
	if (saved)
	{
		p.setPen(Qt::red);
		p.drawText(30, 200, QString::fromStdString(evalstring));
		p.drawText(30, 250, QString::fromStdString(evalb));
		p.end();
		std::ostringstream bla;
		bla << name << "_" << instance << "_" << nr << ".png";
		img.save(exportDirectory + QString::fromStdString(bla.str()));
	}
	return img;
}

calc::~calc()
{
}

/*
void calc::pcClassMove(board& m, int8_t color)
{
findOut res = findMove(m,color);
bool moved = false;
if (res.Ges.size() > 0)
{
bool moved = false;
if (res.eField.size() > 0)
{
smallrandom(m, res.eField, color);
moved = true;
}
if (res.sField.size() > 0 && !moved)
{
smallrandom(m, res.sField, color);
moved = true;
}
if (res.Normal.size() > 0 && !moved)
{
smallrandom(m, res.Normal, color);
moved = true;
}
if (res.cField.size() > 0 && !moved)
{
smallrandom(m, res.cField, color);
moved = true;
}
if (res.xField.size() > 0 && !moved)
{
smallrandom(m, res.xField, color);
moved = true;
}
}
}

void calc::smallrandom(board& m, std::vector<field> moves, int8_t color)
{
int8_t ran = rand() % moves.size();
fastMove(m, moves[ran], color);
}

*/

/* Rekurwive Variante
void calc::pcRandomSimulation(board& m, int8_t color)
{
std::vector<field> res = fastfind(m, color);
if (res.size() > 0)
{
int8_t ran = rand() % res.size();
fastMove(m, res[ran], color);
pcRandomSimulation(m, 3 - color);
}
else
{
res = fastfind(m, 3 - color);
if (res.size() > 0)
{
int8_t ran = rand() % res.size();
fastMove(m, res[ran], 3-color);
pcRandomSimulation(m, color);
}
else
{
//Spielende
countChips(m);
}
}
}
*/
/*
//aufruf mit mboard, 2, 0
void calc::movetree(board m, int8_t color, int8_t tiefe)
{
	if (tiefe < 3)
	{
		findOut a = findMove(m, color);
		if (a.Ges.size() == 0)
		{
			findOut a = findMove(m, 3 - color);
			if (a.Ges.size() == 0)
			{
				//Blatt gefunden
			}
			else
			{
				for (int8_t i = 0; i < a.Ges.size(); i++)
				{
					Knoten Ele;
					Ele.m = m;
					Ele.move.push_back(a.Ges[i]);
					Ele.doResult = doMove(Ele.m, a.Ges[i], 3 - color);
					Ele.iD = i;
					WM.push_back(Ele);
					movetree(Ele.m, color, tiefe + 1);
				}
			}
		}
		else
		{
			for (int8_t i = 0; i < a.Ges.size(); i++)
			{
				Knoten Ele;
				Ele.m = m;
				Ele.move.push_back(a.Ges[i]);
				Ele.doResult = doMove(Ele.m, a.Ges[i], color);
				Ele.iD = i;
				WM.push_back(Ele);
				movetree(Ele.m, 3 - color, tiefe + 1);
			}
		}
	}
}

void calc::painttree()
{
	for (int k = 0; k < WM.size(); k++)
	{
		paint(WM[k].m, k, "tree", false);
	}
}
*/
