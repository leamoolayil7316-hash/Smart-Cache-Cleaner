
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#define MAX_FILES 1000
#define LINE_LEN 1024
#define ID_LEN 32
#define NAME_LEN 128

typedef struct {
    char id[ID_LEN];
    char name[NAME_LEN];
    long long size_bytes;
    int access_count;
    int last_access_order;
    int protected_file;
    double regen_confidence;
    int regret_count;
    double urgency;
    double reuse_value;
    int loaded_order;
    int selected;
} FileEntry;

static void trim(char *s) {
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) s[--n] = '\0';
    size_t i = 0;
    while (s[i] && isspace((unsigned char)s[i])) i++;
    if (i > 0) memmove(s, s + i, strlen(s + i) + 1);
}

static int equalIgnoreCase(const char *a, const char *b) {
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static int parseDouble(const char *s, double *out) {
    char *end = NULL;
    errno = 0;
    double v = strtod(s, &end);
    if (errno != 0 || end == s || *end != '\0') return 0;
    *out = v;
    return 1;
}

static int loadCSV(const char *path, FileEntry files[]) {
    FILE *fp = fopen(path, "r");
    char line[LINE_LEN];
    int count = 0;
    if (!fp) {
        perror("Cannot open CSV file");
        return -1;
    }

    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        return 0;
    }

    while (fgets(line, sizeof(line), fp) && count < MAX_FILES) {
        char *fields[10];
        int n = 0;
        char *token = strtok(line, ",\r\n");
        while (token && n < 10) {
            trim(token);
            fields[n++] = token;
            token = strtok(NULL, ",\r\n");
        }

        if (n != 10) {
            printf("Warning: skipping CSV row with %d fields (expected 10).\n", n);
            continue;
        }

        long long size = atoll(fields[2]);
        int accesses = atoi(fields[3]);
        int last = atoi(fields[4]);
        int protected_status = atoi(fields[5]);
        double regen = 0.0, urgency = 0.0, reuse = 0.0;
        int regret = atoi(fields[7]);

        if (!parseDouble(fields[6], &regen) ||
            !parseDouble(fields[8], &urgency) ||
            !parseDouble(fields[9], &reuse) ||
            size < 0 || accesses < 0 || last < 0 || regret < 0 ||
            (protected_status != 0 && protected_status != 1) ||
            regen < 0.0 || regen > 1.0 ||
            urgency < 0.0 || urgency > 1.0 ||
            reuse < 0.0 || reuse > 10.0) {
            printf("Warning: skipping invalid CSV row: %s\n", fields[0]);
            continue;
        }

        snprintf(files[count].id, sizeof(files[count].id), "%s", fields[0]);
        snprintf(files[count].name, sizeof(files[count].name), "%s", fields[1]);
        files[count].size_bytes = size;
        files[count].access_count = accesses;
        files[count].last_access_order = last;
        files[count].protected_file = protected_status;
        files[count].regen_confidence = regen;
        files[count].regret_count = regret;
        files[count].urgency = urgency;
        files[count].reuse_value = reuse;
        files[count].loaded_order = count;
        files[count].selected = 0;
        count++;
    }

    fclose(fp);
    return count;
}

static double rareScore(const FileEntry files[], int count, int index) {
    long long maxSize = 1;
    for (int i = 0; i < count; i++) {
        if (!files[i].protected_file && files[i].size_bytes > maxSize)
            maxSize = files[i].size_bytes;
    }

    double sizeFactor = (double)files[index].size_bytes / (double)maxSize;
    double reuseFactor = files[index].reuse_value / 10.0;
    double regretFactor = (double)files[index].regret_count /
                         ((double)files[index].regret_count + 5.0);
    double denominator = 0.10 + reuseFactor + regretFactor;
    if (denominator <= 0.0) denominator = 0.10;

    return (sizeFactor * files[index].regen_confidence * files[index].urgency)
           / denominator;
}

static int chooseVictim(FileEntry files[], int count, const char *policy) {
    int victim = -1;

    for (int i = 0; i < count; i++) {
        if (files[i].selected || files[i].protected_file) continue;

        if (victim == -1) {
            victim = i;
            continue;
        }

        if (equalIgnoreCase(policy, "FIFO")) {
            if (files[i].loaded_order < files[victim].loaded_order) victim = i;
        } else if (equalIgnoreCase(policy, "LRU")) {
            if (files[i].last_access_order < files[victim].last_access_order)
                victim = i;
        } else if (equalIgnoreCase(policy, "LFU")) {
            if (files[i].access_count < files[victim].access_count ||
                (files[i].access_count == files[victim].access_count &&
                 files[i].last_access_order < files[victim].last_access_order))
                victim = i;
        } else if (equalIgnoreCase(policy, "RARE")) {
            double current = rareScore(files, count, i);
            double best = rareScore(files, count, victim);
            if (current > best) victim = i;
        }
    }
    return victim;
}

static void printUsage(const char *program) {
    printf("Usage: %s dataset.csv FIFO|LRU|LFU|RARE target_bytes\n", program);
    printf("Example: %s cache_dataset.csv RARE 10000\n", program);
}

int main(int argc, char *argv[]) {
    FileEntry files[MAX_FILES];
    long long target, total = 0, recovered = 0;
    int count, selectedCount = 0;
    char policy[16];

    if (argc != 4) {
        printUsage(argv[0]);
        return 1;
    }

    snprintf(policy, sizeof(policy), "%s", argv[2]);
    if (!equalIgnoreCase(policy, "FIFO") &&
        !equalIgnoreCase(policy, "LRU") &&
        !equalIgnoreCase(policy, "LFU") &&
        !equalIgnoreCase(policy, "RARE")) {
        printf("Invalid policy. Choose FIFO, LRU, LFU, or RARE.\n");
        printUsage(argv[0]);
        return 1;
    }

    char *end = NULL;
    errno = 0;
    target = strtoll(argv[3], &end, 10);
    if (errno != 0 || end == argv[3] || *end != '\0' || target <= 0) {
        printf("Target bytes must be a positive integer.\n");
        return 1;
    }

    count = loadCSV(argv[1], files);
    if (count <= 0) {
        printf("No valid dataset entries found.\n");
        return 1;
    }

    for (int i = 0; i < count; i++) total += files[i].size_bytes;

    printf("\nSMART CACHE CLEANER - SAFE SIMULATION\n");
    printf("=====================================\n");
    printf("Policy: %s\n", policy);
    printf("Dataset entries: %d\n", count);
    printf("Total listed size: %lld bytes\n", total);
    printf("Target space: %lld bytes\n\n", target);
    printf("%-8s %-23s %-12s %-9s %-10s %s\n",
           "ID", "File Name", "Size(bytes)", "Accesses", "RARE score", "Reason");
    printf("------------------------------------------------------------------------------------------\n");

    while (recovered < target) {
        int v = chooseVictim(files, count, policy);
        if (v == -1) break;

        double score = rareScore(files, count, v);
        printf("%-8s %-23s %-12lld %-9d %-10.4f ",
               files[v].id, files[v].name, files[v].size_bytes,
               files[v].access_count, score);

        if (equalIgnoreCase(policy, "FIFO")) {
            printf("Oldest loaded CSV entry\n");
        } else if (equalIgnoreCase(policy, "LRU")) {
            printf("Oldest last-access order\n");
        } else if (equalIgnoreCase(policy, "LFU")) {
            printf("Lowest access count; LRU tie-break\n");
        } else {
            printf("RARE: size benefit, rebuild confidence, urgency, reuse and regret\n");
        }

        files[v].selected = 1;
        recovered += files[v].size_bytes;
        selectedCount++;
    }

    printf("\nRESULTS\n");
    printf("=======\n");
    printf("Files recommended: %d\n", selectedCount);
    printf("Simulated space recovered: %lld bytes\n", recovered);
    printf("Protected files were skipped.\n");
    if (recovered >= target) printf("Target reached successfully.\n");
    else printf("Target not reached: eligible files exhausted.\n");
    printf("Simulation only: no real files were deleted or modified.\n");

    if (equalIgnoreCase(policy, "RARE")) {
        printf("\nRARE scoring notes:\n");
        printf("- Higher size and rebuild confidence can raise eviction priority.\n");
        printf("- Higher urgency raises priority when cleanup is needed.\n");
        printf("- Higher reuse value and regret history lower eviction priority.\n");
        printf("- Regret history is supplied by the dataset; this version does not learn it automatically.\n");
    }
    return 0;
}
