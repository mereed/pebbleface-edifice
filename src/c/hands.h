#pragma once

#include "pebble.h"

static const GPathInfo MINUTE_HAND_POINTS = {
	6,
  (GPoint []) {
    { -4, 18 },
    { -5, -70 },
    { 0, -86 },
    { 5, -70 },
	{ 4, 18 },
	{ -4, 18 },
			

	
  }
};

static const GPathInfo HOUR_HAND_POINTS = {
6,
  (GPoint []) {
    { -4, 4 },
    { -5, -36 },
	{ 0, -44 },
	{ 5, -36 },
	{ 4, 18 },
	{ -4, 18 },	
		
//	{ -3, -20 },
//	{ -4, -36 },
//	{ 0, -43 },
//	{ 4, -36 },
//	{ 3, -20 },
//	{ -3, -20 },
	
		
		
		
  }
};

static const GPathInfo SECOND_HAND_POINTS = {
  4,
  (GPoint []) {
    { -3, 30 },
    { 0, -90 },
    { 3, 30 },
	{ -3, 30 },
  }
};
