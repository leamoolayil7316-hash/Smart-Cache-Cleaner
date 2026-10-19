```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>

#define MAX_LINE 2048
#define NAME_LEN 256

typedef struct {
    char id[64];
    char name[NAME_LEN];
    long long size_bytes;
    int access_count;
    int last_access_order;
    int protected_file;
    double regen_confidence;
    double reuse_value;
    double urgency;
    double regret_penalty;
    int loaded_order;
    int evicted;
} FileEntry;

/* RARE: lower score means higher eviction priority. */
static double rareScore(FileEntry f) {
    return f.reuse_value + f.urgency + f.regret_penalty
           - f.regen_confidence;
}

/* Choose a victim using FIFO, LRU, LFU or RARE. */
static int chooseVictim(FileEntry files[], int count,
                        const char *policy) {
    int victim = -1;

    for (int i = 0; i < count; i++) {
        if (files[i].evicted || files[i].protected_file)
            continue;

        if (victim == -1) {
            victim = i;
            continue;
        }

        if (strcasecmp(policy, "FIFO") == 0) {
            if (files[i].loaded_order < files[victim].loaded_order)
                victim = i;
        } else if (strcasecmp(policy, "LRU") == 0) {
            if (files[i].last_access_order <
                files[victim].last_access_order)
                victim = i;
        } else if (strcasecmp(policy, "LFU") == 0) {
            if (files[i].access_count < files[victim].access_count ||
                (files[i].access_count == files[victim].access_count &&
                 files[i].last_access_order <
                 files[victim].last_access_order))
                victim = i;
        } else if (strcasecmp(policy, "RARE") == 0) {
            if (rareScore(files[i]) < rareScore(files[victim]))
                victim = i;
        }
    }

    return victim;
}

/* Read a simple comma-separated CSV row.
   Quoted fields containing commas are not supported. */
static int splitCSV(char *line, char *columns[], int max_columns) {
    int n = 0;
    char *token = strtok(line, ",\r\n");

    while (token != NULL && n < max_columns) {
        columns[n++] = token;
        token = strtok(NULL, ",\r\n");
    }

    return n;
}

/* Load CSV: six basic columns, plus four optional RARE columns. */
static int loadCSV(const char *filename, FileEntry **output,
                   int require_rare_fields) {
    FILE *fp = fopen(filename, "r");
    char line[MAX_LINE];
    FileEntry *files = NULL;
    int count = 0;
    int capacity = 0;

    if (fp == NULL) {
        fprintf(stderr, "Cannot open %s: %s\n",
                filename, strerror(errno));
        return -1;
    }

    /* Skip the header. */
    if (fgets(line, sizeof(line), fp) == NULL) {
        fclose(fp);
        *output = NULL;
        return 0;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        char *columns[10];
        int n;
        FileEntry f = {0};

        if (line[0] == '\n' || line[0] == '\r' || line[0] == '\0')
            continue;

        n = splitCSV(line, columns, 10);

        if (n < 6) {
            fprintf(stderr, "Warning: skipping invalid CSV row.\n");
            continue;
        }

        if (require_rare_fields && n < 10) {
            fprintf(stderr,
                "Error: RARE requires 10 columns.\n"
                "Add regen_confidence, reuse_value, urgency and "
                "regret_penalty after the six basic columns.\n");
            free(files);
            fclose(fp);
            return -1;
        }

        if (count == capacity) {
            int new_capacity = (capacity == 0) ? 16 : capacity * 2;
            FileEntry *tmp = realloc(
                files, (size_t)new_capacity * sizeof(*files));

            if (tmp == NULL) {
                fprintf(stderr, "Memory allocation failed.\n");
                free(files);
                fclose(fp);
                return -1;
            }

            files = tmp;
            capacity = new_capacity;
        }

        snprintf(f.id, sizeof(f.id), "%s", columns[0]);
        snprintf(f.name, sizeof(f.name), "%s", columns[1]);

        f.size_bytes = atoll(columns[2]);
        f.access_count = atoi(columns[3]);
        f.last_access_order = atoi(columns[4]);
        f.protected_file = atoi(columns[5]);

        if (f.size_bytes < 0 || f.access_count < 0) {
            fprintf(stderr, "Warning: skipping invalid size/count.\n");
            continue;
        }

        if (n >= 10) {
            f.regen_confidence = atof(columns[6]);
            f.reuse_value = atof(columns[7]);
            f.urgency = atof(columns[8]);
            f.regret_penalty = atof(columns[9]);
        }

        f.loaded_order = count;
        f.evicted = 0;
        files[count++] = f;
    }

    fclose(fp);
    *output = files;
    return count;
}

int main(int argc, char *argv[]) {
    FileEntry *files = NULL;
    int count;
    int evicted_count = 0;
    long long target;
    long long freed = 0;
    char *endptr;
    const char *filename;
    const char *policy;
    int require_rare;

    if (argc != 4) {
        fprintf(stderr,
            "Usage: %s dataset.csv FIFO|LRU|LFU|RARE target_bytes\n",
            argv[0]);
        fprintf(stderr,
            "Example: %s cache_dataset_rare.csv RARE 10000\n",
            argv[0]);
        return 1;
    }

    filename = argv[1];
    policy = argv[2];

    if (strcasecmp(policy, "FIFO") != 0 &&
        strcasecmp(policy, "LRU") != 0 &&
        strcasecmp(policy, "LFU") != 0 &&
        strcasecmp(policy, "RARE") != 0) {
        fprintf(stderr, "Invalid policy. Use FIFO, LRU, LFU or RARE.\n");
        return 1;
    }

    errno = 0;
    target = strtoll(argv[3], &endptr, 10);

    if (errno != 0 || argv[3][0] == '\0' ||
        *endptr != '\0' || target <= 0) {
        fprintf(stderr, "Target bytes must be a positive integer.\n");
        return 1;
    }

    require_rare = (strcasecmp(policy, "RARE") == 0);
    count = loadCSV(filename, &files, require_rare);

    if (count <= 0) {
        if (count == 0)
            fprintf(stderr, "No valid records found in the dataset.\n");
        free(files);
        return 1;
    }

    printf("\nSMART CACHE CLEANER\n");
    printf("-------------------\n");
    printf("Dataset: %s\n", filename);
    printf("Policy: %s\n", policy);
    printf("Target bytes: %lld\n\n", target);

    printf("%-12s %-25s %-12s %-12s\n",
           "File ID", "File Name", "Size(bytes)", "RARE Score");
    printf("---------------------------------------------------------------\n");

    while (freed < target) {
        int victim = chooseVictim(files, count, policy);

        if (victim == -1)
            break;

        files[victim].evicted = 1;
        freed += files[victim].size_bytes;
        evicted_count++;

        printf("%-12s %-25.25s %-12lld %-12.3f\n",
               files[victim].id,
               files[victim].name,
               files[victim].size_bytes,
               rareScore(files[victim]));
    }

    printf("\nRESULTS\n");
    printf("-------------------\n");
    printf("Dataset entries: %d\n", count);
    printf("Entries selected: %d\n", evicted_count);
    printf("Simulated bytes freed: %lld\n", freed);
    printf("Requested bytes: %lld\n", target);

    if (freed >= target)
        printf("Status: Target reached.\n");
    else
        printf("Status: Target not reached; no eligible entries remain.\n");

    printf("Note: Simulation only. No real files are deleted.\n");

    free(files);
    return 0;
}
```
