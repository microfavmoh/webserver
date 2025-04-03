#pragma once
// the hash function
unsigned int hash(void* key, int len) {
    unsigned char *p = key;
    unsigned h = 0;
    int i;
    for (i = 0; i < len; i++) {
        h += p[i];
        h += (h << 10);
        h ^= (h >> 6);
    }
    h += (h << 3);
    h ^= (h >> 11);
    h += (h << 15);
    h %= HASHMAP_SIZE;
    return h;
}