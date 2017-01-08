/**********************************************************************
 * The JeamLand talker system.
 * (c) Andy Fiddaman, 1993-97
 *
 * File:	hash.h
 * Function:
 **********************************************************************/

/* Significant number of string characters */
#define HASH_STRLEN	15

/* Maximum permitted deviation before resizing table */
#define MAX_HASH_DEV	10

/* Increasingly larger prime numbers
 * These are used for table sizes */
#define HASH_NO_PRIMES 10 
#define HASH_PRIMES { 37, 97, 307, 599, 997, 1493, 2003, 2503, 3001, 4001, 0 }

/* Hash table flags */
#define HASH_CASEI	0x1

struct hash {
	char	*id;
	int	 hash_size;
	int	 prime_index;
	int	(*hfunc)(char *, int);
	int	flags;

	/* Statistics */
	int	 maxdev;
	int 	 hashed;
	int	 collisions;
	int	 hit, miss;

	/* Must be at end! */
	void	*hash_table[1];
	};

struct hash_dummy {
	char *id;
};

#define HASH_ID(xx)	((struct hash_dummy *)xx)->id

