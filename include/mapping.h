/**********************************************************************
 * The JeamLand talker system.
 * (c) Andy Fiddaman, 1993-97
 *
 * File:	mapping.h
 * Function:	Mapping data structure definition.
 **********************************************************************/

#define MAPPING_CHUNK	10

struct mapping {
	int size;
	struct vector	*indices;
	struct vector	*values;
	};

