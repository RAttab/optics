/* key.h
   Rémi Attab (remi.attab@gmail.com), 26 Feb 2016
   FreeBSD-style copyright and disclaimer apply
*/

#pragma once


// -----------------------------------------------------------------------------
// key
// -----------------------------------------------------------------------------

struct key
{
    size_t pos;
    char buffer[optics_name_max_len * 3];
};

size_t key_push(struct key *key, const char *suffix);
void key_pop(struct key *key, size_t pos);
