// index.c — Staging area implementation
//
// Text format of .pes/index (one entry per line, sorted by path):
//
//   <mode-octal> <64-char-hex-hash> <mtime-seconds> <size> <path>
//
// Example:
//   100644 a1b2c3d4e5f6...  1699900000 42 README.md
//   100644 f7e8d9c0b1a2...  1699900100 128 src/main.c
//
// This is intentionally a simple text format. No magic numbers, no
// binary parsing. The focus is on the staging area CONCEPT (tracking
// what will go into the next commit) and ATOMIC WRITES (temp+rename).
//
// PROVIDED functions: index_find, index_remove, index_status
// TODO functions:     index_load, index_save, index_add


// ─── PROVIDED ────────────

// index.c — Staging area implementation
#include "pes.h"
#include "index.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

/* ===============================
   PROVIDED FUNCTIONS (unchanged)
   =============================== */

IndexEntry* index_find(Index *index, const char *path) {
    for (int i = 0; i < index->count; i++) {
        if (strcmp(index->entries[i].path, path) == 0)
            return &index->entries[i];
    }
    return NULL;
}

int index_remove(Index *index, const char *path) {
    for (int i = 0; i < index->count; i++) {
        if (strcmp(index->entries[i].path, path) == 0) {

            int remaining = index->count - i - 1;

            if (remaining > 0)
                memmove(&index->entries[i],
                        &index->entries[i + 1],
                        remaining * sizeof(IndexEntry));

            index->count--;

            return index_save(index);
        }
    }

    fprintf(stderr, "error: '%s' is not in the index\n", path);
    return -1;
}


int index_status(const Index *index) {

    printf("Staged changes:\n");

    if (index->count == 0) {
        printf("  (nothing to show)\n\n");
    } else {
        for (int i = 0; i < index->count; i++) {
            printf("  staged:     %s\n", index->entries[i].path);
        }
        printf("\n");
    }

    printf("Unstaged changes:\n");
    printf("  (nothing to show)\n\n");

    printf("Untracked files:\n");
    printf("  (nothing to show)\n\n");

    return 0;
}

/* ===============================
   LOAD INDEX
   =============================== */

int index_load(Index *index) {

    FILE *f = fopen(INDEX_FILE, "r");

    index->count = 0;

    if (!f)
        return 0;

    while (1) {

        IndexEntry *e = &index->entries[index->count];

        char hash_hex[HASH_HEX_SIZE + 1];

        int r = fscanf(
            f,
            "%o %64s %lu %u %s",
            &e->mode,
            hash_hex,
            &e->mtime_sec,
            &e->size,
            e->path
        );

        if (r != 5)
            break;

        hex_to_hash(hash_hex, &e->hash);

        index->count++;
    }

    fclose(f);

    return 0;
}

/* ===============================
   SAVE INDEX
   =============================== */
int index_save(const Index *index) {

    char tmp_path[] = INDEX_FILE ".tmp";

    FILE *f = fopen(tmp_path, "w");
    if (!f)
        return -1;

    for (int i = 0; i < index->count; i++) {

        char hex[HASH_HEX_SIZE + 1];

        hash_to_hex(&index->entries[i].hash, hex);

        fprintf(
            f,
            "%o %s %lu %u %s\n",
            index->entries[i].mode,
            hex,
            index->entries[i].mtime_sec,
            index->entries[i].size,
            index->entries[i].path
        );
    }

    fflush(f);
    fsync(fileno(f));
    fclose(f);

    rename(tmp_path, INDEX_FILE);

    return 0;
}

/* ===============================
   ADD FILE TO INDEX
   =============================== */

int index_add(Index *index, const char *path) {

    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "error: cannot open '%s'\n", path);
        return -1;
    }

    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    rewind(f);

    void *data = malloc(size);

    fread(data, 1, size, f);
    fclose(f);

    ObjectID hash;

    if (object_write(OBJ_BLOB, data, size, &hash) != 0) {
        free(data);
        return -1;
    }

    free(data);

    struct stat st;

    if (stat(path, &st) != 0)
        return -1;

    IndexEntry *entry = index_find(index, path);

    if (!entry) {
        entry = &index->entries[index->count++];
        strcpy(entry->path, path);
    }

    entry->hash = hash;
    entry->mode = st.st_mode;
    entry->mtime_sec = st.st_mtime;
    entry->size = st.st_size;

    return index_save(index);
}
