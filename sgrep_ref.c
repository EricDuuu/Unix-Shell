/*
 For reference
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Program helpers
 */
#define die(...)                                                               \
  do {                                                                         \
    fprintf(stderr, ##__VA_ARGS__);                                            \
    fprintf(stderr, "\n");                                                     \
    exit(1);                                                                   \
  } while (0)

#define die_perror(s)                                                          \
  do {                                                                         \
    perror(s);                                                                 \
    exit(1);                                                                   \
  } while (0)

static inline void *xmalloc(size_t size) {
  void *p = malloc(size);

  if (!p)
    die_perror("malloc");
  return p;
}

/*
 * Usage
 */
static const char *progname;

static void usage(void) {
  die("Usage: %s [-n] [-c] [-h] [-p PATTERN]... FILE...", progname);
}

/*
 * Pattern matching configuration
 */
struct pattern {
  char *str;
  struct pattern *next;
};

struct config {
  char numline;
  char color;
  struct pattern *p, *p_tail;
};

static char **parse_opts(struct config *conf, char **argv) {
  while (*argv && *argv[0] == '-') {
    if (!strcmp(*argv, "-h")) {
      usage();
    } else if (!strcmp(*argv, "-n")) {
      conf->numline = 1;
    } else if (!strcmp(*argv, "-c")) {
      conf->color = 1;
    } else if (!strcmp(*argv, "-p")) {
      /* Pattern */
      struct pattern *p = xmalloc(sizeof(struct pattern));

      /* Skip over option name */
      if (++argv == NULL)
        die("Missing pattern after `-p`");

      p->str = *argv;
      p->next = NULL;

      if (!conf->p) {
        /* First pattern */
        conf->p = p;
        conf->p_tail = p;
      } else {
        /* Any other patterns */
        conf->p_tail->next = p;
        conf->p_tail = p;
      }
    }

    /* Next argument */
    argv++;
  }

  return argv;
}

/*
 * Grep logic
 */
#define COLOR_CODE "\e[0;31m" /* Red */
#define RESET_CODE "\e[0;0m"

static void grep_line(struct config *conf, char *bufline, int line) {
  struct pattern *p = conf->p;

  /* Try and match each pattern until one works */
  while (p) {
    char *word;

    if ((word = strstr(bufline, p->str))) {
      if (conf->numline)
        printf("%d: ", line);

      if (conf->color) {
        printf("%.*s", (int)(word - bufline), bufline);
        printf(COLOR_CODE "%s" RESET_CODE, p->str);
        printf("%s", word + strlen(p->str));
      } else {
        printf("%s", bufline);

        return;
      }
    }

    p = p->next;
  }
}

#define BUFLINE_LEN 1024

static void grep_lines(struct config *conf, FILE *fp) {
  char bufline[BUFLINE_LEN];
  int line = 1;

  /* Go through each line of a file */
  while (fgets(bufline, BUFLINE_LEN, fp))
    grep_line(conf, bufline, line++);
}

static void grep_files(struct config *conf, char **files) {
  /* Go through all files */
  while (*files) {
    FILE *fp = fopen(*files, "r");

    if (!fp)
      die_perror("fopen");

    grep_lines(conf, fp);

    fclose(fp);
    files++;
  }
}

int main(int argc, char *argv[]) {
  char **arg_ptr = &argv[1];
  struct config conf;

  progname = argv[0];

  if (argc < 2)
    usage();

  memset(&conf, 0, sizeof(struct config));

  arg_ptr = parse_opts(&conf, arg_ptr);
  grep_files(&conf, arg_ptr);

  return 0;
}
