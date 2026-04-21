#include "commit.h"
#include "tree.h"
#include "index.h"
#include "pes.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int commit_create(const char *message, ObjectID *commit_id_out)
{
    Commit commit;
    memset(&commit, 0, sizeof(Commit));

    /* build tree from index */
    if (tree_from_index(&commit.tree) != 0)
        return -1;

    commit.has_parent = 0;

    const char *author = pes_author();
    if (!author)
        return -1;

    snprintf(commit.author, sizeof(commit.author), "%s", author);

    commit.timestamp = (uint64_t)time(NULL);
    snprintf(commit.message, sizeof(commit.message), "%s", message);

    void *data = NULL;
    size_t len = 0;

    if (commit_serialize(&commit, &data, &len) != 0)
        return -1;

    if (object_write(OBJ_COMMIT, data, len, commit_id_out) != 0) {
        free(data);
        return -1;
    }

    free(data);

    return 0;
}

/* serialize commit */
int commit_serialize(const Commit *commit, void **data_out, size_t *len_out)
{
    char buf[4096];
    char tree_hex[HASH_HEX_SIZE + 1];
    char parent_hex[HASH_HEX_SIZE + 1];

    hash_to_hex(&commit->tree, tree_hex);

    int pos = 0;

    pos += sprintf(buf + pos, "tree %s\n", tree_hex);

    if (commit->has_parent) {
        hash_to_hex(&commit->parent, parent_hex);
        pos += sprintf(buf + pos, "parent %s\n", parent_hex);
    }

    pos += sprintf(buf + pos, "author %s %lu\n",
                   commit->author,
                   (unsigned long)commit->timestamp);

    pos += sprintf(buf + pos, "committer %s %lu\n\n",
                   commit->author,
                   (unsigned long)commit->timestamp);

    pos += sprintf(buf + pos, "%s\n", commit->message);

    *data_out = malloc(pos);
    memcpy(*data_out, buf, pos);
    *len_out = pos;

    return 0;
}

/* parse commit */
int commit_parse(const void *data, size_t len, Commit *commit_out)
{
    (void)len;

    const char *p = (const char *)data;
    char hex[HASH_HEX_SIZE + 1];

    if (sscanf(p, "tree %64s\n", hex) != 1)
        return -1;

    if (hex_to_hash(hex, &commit_out->tree) != 0)
        return -1;

    p = strchr(p, '\n') + 1;

    if (strncmp(p, "parent ", 7) == 0) {
        if (sscanf(p, "parent %64s\n", hex) != 1)
            return -1;

        if (hex_to_hash(hex, &commit_out->parent) != 0)
            return -1;

        commit_out->has_parent = 1;
        p = strchr(p, '\n') + 1;
    } else {
        commit_out->has_parent = 0;
    }

    char author_buf[256];
    uint64_t ts;

    if (sscanf(p, "author %255[^\n]\n", author_buf) != 1)
        return -1;

    char *last = strrchr(author_buf, ' ');
    if (!last)
        return -1;

    ts = strtoull(last + 1, NULL, 10);
    *last = '\0';

    snprintf(commit_out->author,
             sizeof(commit_out->author),
             "%s",
             author_buf);

    commit_out->timestamp = ts;

    p = strchr(p, '\n') + 1;
    p = strchr(p, '\n') + 1;
    p = strchr(p, '\n') + 1;

    snprintf(commit_out->message,
             sizeof(commit_out->message),
             "%s",
             p);

    return 0;
}

/* walk commits (minimal stub to satisfy linker) */
int commit_walk(commit_walk_fn callback, void *ctx)
{
    (void)callback;
    (void)ctx;
    return 0;
}
