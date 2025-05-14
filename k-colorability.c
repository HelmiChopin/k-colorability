#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

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

static void usage(void);
static char *load_file(const char *fname, size_t *sz);
static int parse_header(const char *buf, int *n);
static int run_color_minisat(const char *graph_buf, size_t graph_sz,
                             long k, char **ms_opts, int ms_optc,
                             const char *result_file);

int main(int argc, char *argv[]) {
    progName = argv[0];
    long k_override = 0;
    char *minisat_opts_str = NULL;
    char *outfile = NULL;
    char *infile = NULL;

    struct option longopts[] = {
        {"minisat-op", required_argument, NULL,  0},
        {"help",       no_argument,       NULL, 'h'},
        {0,0,0,0}
    };
    int opt;
    while ((opt = getopt_long(argc, argv, "k:o:h", longopts, NULL)) != -1) {
        switch (opt) {
            case 0:
                /* long option */
                if (strcmp("minisat-op", longopts[optind-1].name)==0)
                    minisat_opts_str = optarg;
                break;
            case 'k':
                k_override = strtol(optarg, NULL, 10);
                if (k_override <= 0)
                    ERROR_EXIT("Invalid k: must be positive integer in base 10.\n%s", "");
                break;
            case 'o':
                outfile = optarg;
                break;
            case 'h':
            default:
                usage();
        }
    }
    if (optind < argc) {
        infile = argv[optind];
    } else {
        infile = "-";
    }

    /* Load entire graph input */
    size_t graph_sz = 0;
    char *graph_buf = load_file(infile, &graph_sz);

    /* Tokenize minisat options */
    char *ms_opts[32];
    int ms_optc = 0;
    if (minisat_opts_str) {
        char *saveptr = NULL;
        char *tok = strtok_r(minisat_opts_str, " \t", &saveptr);
        while (tok && ms_optc < 31) {
            ms_opts[ms_optc++] = tok;
            tok = strtok_r(NULL, " \t", &saveptr);
        }
        ms_opts[ms_optc] = NULL;
    }

    /* Determine max k from graph header */
    int max_k = 0;
    if (parse_header(graph_buf, &max_k) == -1) {
        ERROR_EXIT("Could not parse graph header for vertex count.\n%s", "");
    }

    /* Prepare result file (temporary or final) */
    char tmpfile[] = "/tmp/kcolor.XXXXXX";
    int tfd = mkstemp(tmpfile);
    if (tfd < 0) ERROR_EXIT("mkstemp failed.\n%s", "");
    close(tfd);

    int found = 0;
    long k_start = (k_override > 0 ? k_override : 2);
    for (long k = k_start; !found && k <= max_k; ++k) {
        if (run_color_minisat(graph_buf, graph_sz, k, ms_opts, ms_optc, tmpfile) == 0) {
            found = 1;
            /* Output result */
            FILE *rf = fopen(tmpfile, "r");
            if (!rf) ERROR_EXIT("Opening result file failed. %s\n", tmpfile);
            if (outfile) {
                FILE *of = fopen(outfile, "w");
                if (!of) ERROR_EXIT("Opening output file failed. %s\n", outfile);
                fprintf(of, "k = %ld\n", k);
                char buf[1024];
                while (fgets(buf, sizeof(buf), rf)) fputs(buf, of);
                fclose(of);
            } else {
                printf("k = %ld\n", k);
                char buf[1024];
                while (fgets(buf, sizeof(buf), rf)) fputs(buf, stdout);
            }
            fclose(rf);
        }
    }
    free(graph_buf);
    remove(tmpfile);
    return found ? EXIT_SUCCESS : EXIT_FAILURE;
}

static void usage(void) {
    fprintf(stderr,
        "Usage: %s [-k int] [--minisat-op \"opts\"] [-o outfile] [inputfile]\n",
        progName);
    exit(EXIT_FAILURE);
}

static char *load_file(const char *fname, size_t *sz) {
    FILE *fp = NULL;
    if (strcmp(fname, "-") == 0) {
        fp = stdin;
    } else {
        fp = fopen(fname, "r");
        if (!fp) ERROR_EXIT("Error opening file %s\n", fname);
    }
    if (fp != stdin) fseek(fp, 0, SEEK_END);
    long len = (fp == stdin ? 0 : ftell(fp));
    if (fp != stdin) rewind(fp);
    if (fp == stdin) {
        /* read stdin into buffer */
        size_t cap = 4096;
        char *buf = malloc(cap);
        if (!buf) ERROR_EXIT("Alloc buffer failed.\n%s", "");
        size_t used = 0;
        int c;
        while ((c = fgetc(fp)) != EOF) {
            if (used+1 >= cap) {
                cap *= 2;
                buf = realloc(buf, cap);
                if (!buf) ERROR_EXIT("Realloc failed.\n%s", "");
            }
            buf[used++] = (char)c;
        }
        buf[used] = '\0';
        *sz = used;
        return buf;
    } else {
        char *buf = malloc(len+1);
        if (!buf) ERROR_EXIT("Alloc buffer failed.\n%s", "");
        size_t r = fread(buf, 1, len, fp);
        buf[r] = '\0';
        *sz = r;
        fclose(fp);
        return buf;
    }
}

static int parse_header(const char *buf, int *n) {
    const char *p = buf;
    while (p) {
        if (*p == 'p') {
            int nn, mm;
            if (sscanf(p, "p edge %d %d", &nn, &mm) == 2) {
                *n = nn;
                return 0;
            }
        }
        p = strchr(p, '\n');
        if (p) ++p;
    }
    return -1;
}

static int run_color_minisat(const char *graph_buf, size_t graph_sz,
                             long k, char **ms_opts, int ms_optc,
                             const char *result_file) {
    int p2c[2], c2s[2];
    if (pipe(p2c)|pipe(c2s)) { ERROR("pipe failed:%s\n", strerror(errno)); return -1; }
    pid_t pid1 = fork();
    if (pid1 < 0) { ERROR("fork failed.%s\n", strerror(errno)); return -1; }
    if (pid1 == 0) {
        /* color2sat child */
        dup2(p2c[0], STDIN_FILENO);
        dup2(c2s[1], STDOUT_FILENO);
        close(p2c[1]); close(c2s[0]);
        char ks[32]; snprintf(ks, sizeof(ks), "%ld", k);
        char *argv[] = {(char*)"color2sat", (char*)"-", ks, NULL};
        execvp("color2sat", argv);
        ERROR_EXIT("exec color2sat failed.\n%s", "");
    }
    /* parent */
    close(p2c[0]); close(c2s[1]);
    /* write graph_buf */
    if (write(p2c[1], graph_buf, graph_sz) < 0) { ERROR("write failed:%s\n", strerror(errno)); }
    close(p2c[1]);

    pid_t pid2 = fork();
    if (pid2 < 0) { ERROR("fork failed:%s\n", strerror(errno)); return -1; }
    if (pid2 == 0) {
        /* minisat child */
        dup2(c2s[0], STDIN_FILENO);
        int fd = open(result_file, O_CREAT|O_TRUNC|O_WRONLY, 0600);
        if (fd < 0) ERROR_EXIT("open result_file failed.\n%s", "");
        dup2(fd, STDOUT_FILENO);
        close(c2s[0]);
        /* build args */
        char **argv = malloc((4+ms_optc+1)*sizeof(char*));
        int idx = 0;
        argv[idx++] = "minisat";
        for (int i = 0; i < ms_optc; i++) argv[idx++] = ms_opts[i];
        argv[idx++] = "-";
        argv[idx++] = (char*)result_file;
        argv[idx] = NULL;
        execvp("minisat", argv);
        ERROR_EXIT("exec minisat failed.\n%s", "");
    }
    /* parent */
    close(c2s[0]);
    int st1, st2;
    waitpid(pid1, &st1, 0);
    waitpid(pid2, &st2, 0);
    /* check minisat result file */
    FILE *rf = fopen(result_file, "r");
    if (!rf) return -1;
    char line[32];
    if (!fgets(line, sizeof(line), rf)) { fclose(rf); return -1; }
    fclose(rf);
    /* SAT file begins with SAT */
    return (strncmp(line, "SAT", 3) == 0 ? 0 : -1);
}
