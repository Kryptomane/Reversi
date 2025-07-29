#pragma once

#include <vector>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <direct.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <iterator>
#include <cstdlib>
#include <math.h>
#include <time.h>
#include <stddef.h>

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/opencv.hpp>
//#include <opencv2/imgcodecs/imgcodecs.hpp>

#include "TinyLog.h"

typedef std::vector<std::vector<unsigned char>> board;

struct field
{
	int8_t x;
	int8_t y;
	field(int8_t a, int8_t b)
	{	
		x = a;
		y = b;
	}
	field()
	{
		x = -1;
		y = -1;
	}
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