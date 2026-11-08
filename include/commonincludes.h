#pragma once

#include <vector>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <iterator>
#include <cstdlib>
#include <math.h>
#include <time.h>
#include <stddef.h>


#include "TinyLog.h"

#include "reversi_core.h" // board, field, doOut, findOut + Bitboard-Kern

typedef struct
{
	int normal;
	int sfield;
	int cfield;
	int xfield; 
	int efield;
	int sieg;
	double weight;
}evalstruct;

typedef struct
{
	board m;
	std::vector<field> move;
	doOut doResult;
	findOut findResult;
	int iD;
	double Score;
}Knoten;

typedef struct
{
	field move;
	double Score;
}Knoti;