/**********************************************************************
 * The JeamLand talker system
 * (c) Andy Fiddaman, 1994-96
 *
 * File:	hash.h
 * Function:
 **********************************************************************/

/* Significant number of string characters */
#define HASH_STRLEN	10

/* Maximum permitted deviation before resizing table */
#define MAX_HASH_DEV	15

/* Increasingly larger prime numbers
 * These are used for table sizes */
#define HASH_NO_PRIMES 6
#define HASH_PRIMES { 37, 97, 307, 599, 997, 1493, 0 }

struct hash {
	char	*id;
	int	 hash_size;
	int	 prime_index;

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

