/**
 * @file color2sat.c
 * @author Michael Helm
 * @brief Reads a graph in DIMACS format and an integer k, then converts the k-coloring problem into an equivalent SAT problem in DIMACS CNF format and prints it to stdout. 
 * Outputs can be piped into a SAT solver or a file.
 * @date 2025-05-14
 * 
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

char *progName = "<not set>";

/**
 * Prints formatted error messages to stderr.
 * @param msg The error message as a formatted string, like in fprintf(...).
 * @param ... Variable number of arguments for the formatted string msg.
 */
#define ERROR( msg, ... )                              \
    {                                                  \
        fprintf(stderr, "ERROR: " msg, ##__VA_ARGS__); \
    }

/**
 * Prints formatted error messages to stderr and terminates with EXIT_FAILURE.
 * Global variables: progName.
 * @param msg The error message as a formatted string, like in fprintf(...).
 * @param ... Variable number of arguments for the formatted string msg.
 */
#define ERROR_EXIT( msg, ... )                                         \
    {                                                                  \
        fprintf(stderr, "[%s] [%s] ERROR: " msg, progName, strerror(errno), ##__VA_ARGS__); \
        exit(EXIT_FAILURE);                                            \
    }

/**
 * Graph structure: number of vertices n, number of edges m,
 * and an edge list of size m*2.
 */
typedef struct {
    int n;
    int m;
    int (*edges)[2];
} Graph;

/**
 * Print usage and exit.
 */
static void usage(void);

/**
 * Read DIMACS .col graph from File stream.
 * DIMACS Format has to match the format described here https://mat.tepper.cmu.edu/COLOR/instances.html
 * Returns allocated Graph*, or exits on failure.
 * @param file the name of the input file. <name|-> - for stdin
 * @return Pointer to the allocated Graph structure.
 * @details The caller is responsible for freeing the memory.
 */
static Graph *read_graph(const char *file);

/**
 * Free Graph and its resources.
 * @param g Pointer to the Graph structure to be freed.
 */
static void free_graph(Graph *g);

int main(int argc, char *argv[]) {
    progName = argv[0];
    if (argc != 3)
        usage();

    const char *graphFile = argv[1];
    char *endptr = NULL;
    long k = strtol(argv[2], &endptr, 10);
    if (*endptr != '\0' || k <= 0) {
        ERROR_EXIT("Invalid k: must be positive integer in base 10.\n%s", "");
    }

    Graph *g = read_graph(graphFile);
    int n = g->n;
    int m = g->m;
    int (*edges)[2] = g->edges;

    // Precompute CNF clause count
    int num_vars = n * k;
    long long num_clauses = 0;
    num_clauses += n;                               // at least one color per vertex
    num_clauses += (long long)n * k * (k - 1) / 2;  // at most one color per vertex
    num_clauses += (long long)m * k;                // adjacent vertices differ in color

    printf("c CNF: %ld-coloring of %d vertices, %d edges\n", k, n, m);
    printf("p cnf %d %lld\n", num_vars, num_clauses);

    /* 1. Every vertex is assigned at least one color:
    For each vertex v ∈ V, the following clause must be satisfied:
    (x_v,1 ∨ x_v,2 ∨ ... ∨ x_v,k) */
    for (int v = 1; v <= n; v++) {
        for (int i = 1; i <= k; i++) {
            printf("%ld ", (v - 1) * k + i);
        }
        printf("0\n");
    }

    /* 2. Every vertex is assigned at most one color:
    For each vertex v ∈ V and for every possible pair of colors {c_i, c_j}, 
    the following clause must be satisfied:
    ¬x_v,ci ∨ ¬x_v,cj */    
    for (int v = 1; v <= n; v++) {
        for (int i = 1; i <= k; i++) {
            for (int j = i + 1; j <= k; j++) {
                printf("-%ld -%ld 0\n", (v - 1) * k + i, (v - 1) * k + j);
            }
        }
    }

    /* 3. Every adjacent vertices have different colors:
    For each edge {u, w} ∈ E and for each possible color c, 
    the following clause must be satisfied:
    ¬x_u,c ∨ ¬x_w,c */
    for (int e = 0; e < m; e++) {
        int u = edges[e][0];
        int v = edges[e][1];
        for (int i = 1; i <= k; i++) {
            printf("-%ld -%ld 0\n", (u - 1) * k + i, (v - 1) * k + i);
        }
    }

    free_graph(g);
    return EXIT_SUCCESS;
}

static void usage(void) {
    fprintf(stderr, "Usage: %s <input_graph.col | -> <k>\nThe program reads a graph in DIMACS format from stdin and transforms it into a CNF for k-colorability\n", progName);
    exit(EXIT_FAILURE);
}

static Graph *read_graph(const char *file) {
    FILE *fp = NULL;
    if (strcmp(file, "-") == 0) {
        fp = stdin;
    } else {
        fp = fopen(file, "r");
        if (!fp) {
            ERROR_EXIT("Error opening file %s\n", file);
        }
    }

    char line[256];
    int n = 0, m = 0;

    /* parse problem line */
    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == 'p') {
            if (sscanf(line, "p edge %d %d", &n, &m) != 2) {
                ERROR_EXIT("Invalid problem line format.\n%s", "");
            }
            break;
        }
    }
    if (n <= 0 || m < 0) {
        ERROR_EXIT("Invalid n or m: %d vertices, %d edges.\n", n, m);
    }

    Graph *g = malloc(sizeof(*g));
    if (!g) 
        ERROR_EXIT("Alloc Graph failed.\n%s", "");
    g->n = n;
    g->m = m;
    g->edges = malloc(m * sizeof(*g->edges));
    if (!g->edges) 
        ERROR_EXIT("Alloc edges failed.\n%s", "");

    int count = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == 'e' && count < m) {
            int u, v;
            if (sscanf(line, "e %d %d", &u, &v) == 2) {
                g->edges[count][0] = u;
                g->edges[count][1] = v;
                count++;
            } 
        } else {
            ERROR_EXIT("%d: Line [%s]\nInvalid edge line format or count exceeding problem size.\n", count, line);
        }
    }
    if (count != m) {
        ERROR("Warning: read %d edges, expected %d.\nResetting edge count and continuing...", count, m);
        g->m = count;
    }
    return g;
}

static void free_graph(Graph *g) {
    free(g->edges);
    free(g);
}
