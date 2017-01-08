/**********************************************************************
 * The JeamLand talker system.
 * (c) Andy Fiddaman, 1993-97
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

static int
hash_string1(char *s, int hash_size)
{
        int len = HASH_STRLEN;
        unsigned hashval = 0;

        while (len-- && *s != '\0')
                hashval = (((hashval << 1) + 3) ^ len)  + *s++;
	return hashval % hash_size;
}

struct hash *
create_hash(int prime_index, char *id, int (*hfunc)(char *, int), int flags)
{
	struct hash *h;
	int i;

	if (prime_index > HASH_NO_PRIMES - 1)
		fatal("create_hash: prime_index too large");

	if (hfunc == (int (*)(char *, int))NULL)
		hfunc = hash_string1;

	h = (struct hash *)xalloc(sizeof(struct hash) +
	    sizeof(void *) * (primes[prime_index] - 1), "hash table");
	h->id = string_copy(id, "hash table id");
	h->hash_size = primes[prime_index];
	h->prime_index = prime_index;
	h->hfunc = hfunc;
	h->flags = flags;
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

static void
expand_hash(struct hash **old)
{
	struct hash *h = create_hash((*old)->prime_index + 1, (*old)->id,
	    (*old)->hfunc, (*old)->flags);
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

static int
hash_strcmp(struct hash *h, char *str1, char *str2)
{
	if (h->flags & HASH_CASEI)
		return strcasecmp(str1, str2);
	else
		return strcmp(str1, str2);
}

/* Collisions are resolved by placing the element in the next free slot to
 * the right. This may change some day.. */
int
insert_hash(struct hash **ht, void *el)
{
	struct hash *h = *ht;
	int i;

	FUN_START("insert_hash");

#ifdef DEBUG
	if (el == (void *)NULL)
		fatal("Void element in insert_hash(%s, ...)", h->id);
#endif

	i = (*ht)->hfunc(HASH_ID(el), h->hash_size);

	FUN_LINE;

	if (h->hash_table[i] != (void *)NULL)
	{
		int dev, found;

		h->collisions++;

		for (dev = 1, found = 0; dev < h->hash_size; dev++)
		{
			if (!hash_strcmp(h, HASH_ID(el),
			    HASH_ID(h->hash_table[i])))
			{
				log_file("error",
				    "Duplicate value in %s hash table: "
				    "%s @ slot %d",
				    h->id, HASH_ID(el), i);
				FUN_END;
				return 0;
			}

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
			FUN_END;
			return insert_hash(ht, el);
		}
		if (dev > h->maxdev)
			h->maxdev = dev;
#ifdef DEBUG_HASH
		log_file("hash", "%s: Coll :[%3d]: %s.",
		    h->id, dev, HASH_ID(el));
#endif
	}
	/* Ok.. found a free slot */
	h->hash_table[i] = el;
	h->hashed++;
	FUN_END;
	return 1;
}

static int
lookup_hash_index(struct hash *h, char *id)
{
	int i, j, dev = 0;

	if (h == (struct hash *)NULL)
		return -1;

	FUN_START("lookup_hash_index");

 	i = j = h->hfunc(id, h->hash_size);

	while (h->hash_table[i] != (void *)NULL &&
	    hash_strcmp(h, HASH_ID(h->hash_table[i]), id))
	{
		/* Search only as far as maxdev slots */
		if (++dev > h->maxdev)
		{
#ifdef DEBUG_HASH
			log_file("hash", "%s: FailM:[%3d]: %s.",
			    h->id, h->maxdev + 1, id);
#endif
			FUN_END;
			return -1;
		}
		/* Loop around the top of the hash table */
		if (++i >= h->hash_size)
			i = 0;
	}

	FUN_LINE;

	if (h->hash_table[i] == (void *)NULL)
	{
#ifdef DEBUG_HASH
		log_file("hash", "%s: Fail :[%3d]: %s.", h->id, dev, id);
#endif
		FUN_END;
		return -1;
	}

	/* Simple caching - swap the elements */
	if (i != j)
	{
		void *p;

#ifdef DEBUG_HASH
		log_file("hash", "%s: Cache:[%3d]: %s / %s.",
		    h->id, dev, HASH_ID(h->hash_table[j]),
		    HASH_ID(h->hash_table[i]));
#endif

		p = h->hash_table[i];
		h->hash_table[i] = h->hash_table[j];
		h->hash_table[j] = p;
		h->miss++;
	}
	else
		h->hit++;

#ifdef DEBUG_HASH
	log_file("hash", "%s: Succ :[%3d]: %s.", h->id, dev, id);
#endif
	FUN_END;

	return j;
}

void *
lookup_hash(struct hash *h, char *id)
{
	int i = lookup_hash_index(h, id);

	if (i == -1)
		return (void *)NULL;
	return h->hash_table[i];
}

void
remove_hash(struct hash *h, char *id)
{
	int i;

	FUN_START("remove_hash");

 	i = lookup_hash_index(h, id);

	FUN_LINE;

	if (i != -1)
	{
		h->hash_table[i] = (void *)NULL;
		if (!h->hashed--)
			log_file("error",
			    "Hashed count wrong for %s when removing %s.",
			    h->id, id);
	}
	else
		log_file("error", "remove_hash: no such element %s in %s.",
		    id, h->id);

	FUN_END;
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
	    h->hashed == 0 ? 0.0 :
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

