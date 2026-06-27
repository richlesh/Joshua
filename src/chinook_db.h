// Wrapper header for Chinook endgame database access code
// Original code: http://www.cs.ualberta.ca/~chinook/databases/code.c
// University of Alberta (public domain)
//
// Thread-safe: after chinook_init(), chinook_lookup() is fully reentrant.

#ifndef CHINOOK_DB_H
#define CHINOOK_DB_H

#include <stdint.h>

// Results from database lookup
#define CHINOOK_TIE   0
#define CHINOOK_WIN   1
#define CHINOOK_LOSS  2
#define CHINOOK_UNKNOWN 3

// Initialize the database from the given directory path
// Returns 1 on success, 0 on failure
int chinook_init(const char *db_dir);

// Look up a position in the database (thread-safe, no locks needed)
// locbv_white = white pieces bitmask (Chinook internal square numbering)
// locbv_black = black pieces bitmask
// locbv_kings = kings bitmask (both colors)
// turn: 0 = white to move, 1 = black to move
// Returns CHINOOK_WIN, CHINOOK_LOSS, CHINOOK_TIE, or CHINOOK_UNKNOWN
int chinook_lookup(uint32_t locbv_white, uint32_t locbv_black,
                   uint32_t locbv_kings, int turn);

// Cleanup resources (unmap file)
void chinook_cleanup(void);

#endif // CHINOOK_DB_H
