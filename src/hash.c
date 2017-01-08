/**********************************************************************
 * The JeamLand talker system
 * (c) Andy Fiddaman, 1994-96
 *
 * File:	hash.c
 * Function:	Hash table functions
 **********************************************************************/
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/param.h>
#include <netinet/in.h>
#include "jeamland.h"

#undef DEBUG_HASH

static int primes[] = HASH_PRIMES;

/* This works well with the distribution commands and feeling database,
 * but feel free to experiment
 * (and if you come up with a better function than mine,
 *  please send it to me! ;-)
 * A good function will give a low maximum deviation and low number of
 * collisions, although the former is most important as it determines the
 * maximum search length
 */
static int
hash_string(char *s, int hash_size)
{
        int len = HASH_STRLEN;
        long hashval = 0;

        while (len-- && *s != '\0')
                hashval = (((hashval << 1) + 3) ^ len)  + *s++;
                /*hashval = (((hashval << 1) + 1) ^ len)  + *s++;*/
		/*hashval += len * *s++;*/
	hashval %= hash_size;
	return hashval >= 0 ? hashval : -hashval;
}

struct hash *
create_hash(int prime_index, char *id)
{
	struct hash *h;
	int i;

	if (prime_index > HASH_NO_PRIMES - 1)
		fatal("create_hash: prime_index too large");

	h = (struct hash *)xalloc(sizeof(struct hash) +
	    sizeof(void *) * (primes[prime_index] - 1), "hash table");
	h->id = string_copy(id, "hash table id");
	h->hash_size = primes[prime_index];
	h->prime_index = prime_index;
	h->maxdev = h->hashed = h->collisions = h->hit = h->miss = 0;
	for (i = h->hash_size; i--; )
		h->hash_table[i] = (void *)NULL;
	return h;
}

void
free_hash(struct hash *h)
{
	FREE(h->id);
	xfree(h);
}

void insert_hash(struct hash **, void *);

static void
expand_hash(struct hash **old)
{
	struct hash *h = create_hash((*old)->prime_index + 1, (*old)->id);
	int i;

	log_file("syslog", "Resizing %s hash table: %d -> %d.", (*old)->id,
	    (*old)->hash_size, h->hash_size);

	/* Need to rehash every element of the old table :-( */
	for (i = (*old)->hash_size; i--; )
		if ((*old)->hash_table[i] != (void *)NULL)
			insert_hash(&h, (*old)->hash_table[i]);
	free_hash(*old);
	*old = h;

	log_file("syslog", "Hash table expansion complete.");
}

/* Collisions are resolved by placing the element in the next free slot to
 * the right. */
void
insert_hash(struct hash **ht, void *el)
{
	struct hash *h = *ht;
	int i;

#ifdef DEBUG
	if (el == (void *)NULL)
		fatal("Void element in insert_hash(%s, ...)", h->id);
#endif

	i = hash_string(HASH_ID(el), h->hash_size);

	if (h->hash_table[i] != (void *)NULL)
	{
		int dev, found;

		h->collisions++;

		for (dev = 1, found = 0; dev < h->hash_size; dev++)
		{
			if (!strcmp(HASH_ID(el), HASH_ID(h->hash_table[i])))
				log_file("syslog",
				    "Duplicate value in %s hash table: %s",
				    h->id, HASH_ID(el));

			/* Loop around the top of the hash table */
			if (++i >= h->hash_size)
				i = 0;
			if (h->hash_table[i] == (void *)NULL)
			{
				found = 1;
				break;
			}
		}
		if (!found || dev > MAX_HASH_DEV)
		{
			if (h->prime_index > HASH_NO_PRIMES - 1)
				fatal("Hash table overflow [%s] (%s).", h->id,
				    found ? "Maximum dev exceeded" :
				    "no slots");

			/* Expand hash table and try again.. */
			expand_hash(ht);
			/* Isn't recursion a wonderful thing ? */
			insert_hash(ht, el);
			return;
		}
		if (dev > h->maxdev)
			h->maxdev = dev;
#ifdef DEBUG_HASH
		log_file("hash", "%s: %s collided, offset by %d.", h->id,
		    HASH_ID(el), dev);
#endif
	}
	/* Ok.. found a free slot */
	h->hash_table[i] = el;
	h->hashed++;
}

void *
lookup_hash(struct hash *h, char *id)
{
	int i, j, dev = 0;
	void *p;

 	i = j = hash_string(id, h->hash_size);

	while (h->hash_table[i] != (void *)NULL &&
	    strcmp(HASH_ID(h->hash_table[i]), id))
	{
		/* Search only as far as maxdev slots */
		if (++dev > h->maxdev)
		{
#ifdef DEBUG_HASH
			log_file("hash",
			    "%s: Search failed, matches required: %d",
			    h->id, h->maxdev);
#endif
			return (void *)NULL;
		}
		/* Loop around the top of the hash table */
		if (++i >= h->hash_size)
			i = 0;
	}

	/* Simple caching - swap the elements */
	if (h->hash_table[i] != (void *)NULL && i != j)
	{
#ifdef DEBUG_HASH
		log_file("hash", "%s: Cache: swapped %s and %s [%d].",
		    h->id, HASH_ID(h->hash_table[j]),
		    HASH_ID(h->hash_table[i]), i - j);
#endif
		p = h->hash_table[i];
		h->hash_table[i] = h->hash_table[j];
		h->hash_table[j] = p;
	}
	else
		j = i;

	if (h->hash_table[j] != (void *)NULL)
		if (i == j)
			h->hit++;
		else
			h->miss++;
#ifdef DEBUG_HASH
	log_file("hash", "%s: Search %s, matches required: %d", h->id,
	    h->hash_table[j] == (void *)NULL ? "failed" : "succeeded",
	    i - j + 1);
#endif
	return h->hash_table[j];
}

void
hash_stats(struct user *p, struct hash *h, int verbose)
{
	write_socket(p, "%s hash table:\n", h->id);
	write_socket(p, "\tMemory Overhead:   %d\n", h->hash_size *
	    sizeof(void *));
	write_socket(p, "\tTable size:        %d (Prime %d)\n", h->hash_size,
	    h->prime_index + 1);
	write_socket(p, "\tTotal entries:     %d\n", h->hashed);
	write_socket(p, "\tCollisions:        %-5d (%.2f%%)\n", h->collisions,
	    (float)(h->collisions * 100) / (float)(h->hashed));
	write_socket(p, "\tMax search length: %d\n", h->maxdev);
	write_socket(p, "\tHits: %-5d Misses: %-5d (%.2f%%)\n", h->hit, h->miss,
	    h->hit ? (float)(h->hit * 100) / (float)(h->hit + h->miss) : 0);
	/* Warn the admin of large deviations */
	if (h->maxdev > 15)
		write_socket(p, "\nWARNING: Maximum deviation is "
		    "rather large.\n");

	if (verbose)
	{
		char *buf = (char *)xalloc(h->hash_size + 1,
		    "hash_stats buf");
		int i;

		for (i = 0; i < h->hash_size; i++)
			if (h->hash_table[i] == (void *)NULL)
				buf[i] = '.';
			else
				buf[i] = ':';
		buf[i] = '\0';
		write_socket(p, "Schema:\n%s\n", buf);
		xfree(buf);
	}
}

