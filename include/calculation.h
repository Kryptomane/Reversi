#pragma once
#include <QWidget>
#include <QEvent>
#include <QKeyEvent>
#include "commonincludes.h"

class calc
{
public:
	calc(int number);
	~calc();
	void initBoard();
	int instance;
	void makespecialvectors();
	void countChips(board m);
	void checkInside(board m, field kandidat, int8_t& status);
	bool eckefrei(board m, field kandidat, int8_t color);
	int8_t checkMove(board m, field kandidat, int8_t color);
	cv::Mat paint(board m, int nr, std::string name, std::string evalstring, std::string evalb, bool saved);

	doOut doMove(board& m, field kandidat, int8_t color);
	void fastMove(board& m, field kandidat, int8_t color);
	findOut findMove(board& m, int8_t color);
	std::vector<field> fastfind(board& m, int8_t color);

	bool pcRandomSimulation(board& m, int8_t color);
	void montecarlo(board& m, int8_t color);
	int silasindustries(board& m, int8_t tiefe, int8_t color);
	int evaluation(board m, field kandidat, doOut dos, int8_t color, int8_t status);

	void sayHello();
	
	double reff, refd;
	int aufrufe;
	int8_t BlackChips;
	int8_t WhiteChips;
	int8_t bsize;
	int8_t mind;
	board mboard;
	evalstruct eval;
	
private:
	void makeNewLog();

	std::ofstream logfile;
	tl::TinyLog testfile;

	std::vector<field> xF; //x Felder
	std::vector<field> cF; //c Felder
	std::vector<field> eF; //Ecken
	bool debug = false;
	
	QString exportDirectory;
};


