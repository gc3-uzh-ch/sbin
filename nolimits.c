#define NOLIMITS_CONF "/etc/security/nolimits.conf"

#define _GNU_SOURCE
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h> /* getline() */
#include <string.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>


/** Remove leading whitespace. String @i str is modified in-place. */
void lstrip(char* str)
{
  /* remove initial whitespace */
  char *p = str;
  while(isspace(*p))
    p++;

  /* move string */
  if (p > str) {
    char *q = str;
    while('\0' != *p) {
      *q = *p;
      q++;
      p++;
    };
    *q = '\0';
  };
}


/** Remove trailing whitespace. String @i str is modified in-place. */
void rstrip(char* str)
{
  /* remove trailing whitespace */
  char *p = str + strlen(str) - 1;
  while(p > str && isspace(*p))
    p--;
  *(p + 1) = '\0';
}


/** Remove leading and trailing whitespace. String @i is modified in-place. */
void strip(char* str)
{
  lstrip(str);
  rstrip(str);
}


/** Parse configuration file and return the path to the real program
    to run when @c nolimits is invoked as "path. Return @c NULL if @c
    path is not found in the configuration file. */
char*
real_exec_for(const char*path)
{
    FILE* conf = fopen(NOLIMITS_CONF, "r");
    char* line = NULL;
    size_t len = 0;
    ssize_t read;
    int lineno = 1;

    if (NULL == conf)
        return NULL;

    while(-1 != (read = getline(&line, &len, conf))) {
        char *colon, *end;

        /* truncate at comment sign */
        end = strchr(line, '#');
        if (NULL != end)
            *end = '\0';

        lstrip(line);
        if (0 == strlen(line))
            goto next;

        colon = strchr(line, ':');
        if (NULL == colon) {
            fprintf(stderr, "WARNING: malformed line %d '%s' in configuration file '%s'. Ignoring.\n",
                    lineno, line, NOLIMITS_CONF);
            goto next;
        };

        *colon = '\0';
        rstrip(line);
        if (0 == strcmp(line, path)) {
            char *result = ++colon;
            strip(result);
            fclose(conf);
            return result;
        };

      next:
        free(line);
        line = NULL;
        lineno++;
    }; /* while(read) */

    fclose(conf);
    return NULL;
}


int
main(const int argc, char* const* argv)
{
  const uid_t uid = getuid(); /* get the calling user's UID and GID */
  int rc;
  struct rlimit unlimited;
  char *real_exec;
  char *argv0 = strdup(argv[0]); /* take a copy, as we'll modify it in-place */

  /* find out the path to the wrapped command */
  real_exec = real_exec_for(argv0);
  if (NULL == real_exec) {
    fprintf(stderr, "Wrapper command '%s' not found in configuration file '%s'. Aborting.\n",
            argv[0], NOLIMITS_CONF);
    exit(EXIT_FAILURE);
  };
  if ('/' != *real_exec) {
    fprintf(stderr, "Error in config file '%s': wrapped command '%s' is not an absolute path. Aborting for security reasons.\n",
                NOLIMITS_CONF, real_exec);
    exit(EXIT_FAILURE);
  };

  /* remove cpu-time limits */
  unlimited.rlim_cur = RLIM_INFINITY;
  unlimited.rlim_max = RLIM_INFINITY;
  rc = setrlimit(RLIMIT_CPU, &unlimited);
  if (0 != rc) {
    const int err = errno;
    if (EINVAL == err)
      fprintf(stderr, "BUG: Invalid resource limit specification!\n");
    else if (EPERM == err)
      fprintf(stderr, "Not enough privileges to lift resource limits.\n");
    else
      perror(NULL);
    exit(err);
  };

  /* give up root permission */
  rc = setuid(uid);
  if (0 != rc) {
    const int err = errno;
    if (EAGAIN == err)
      fprintf(stderr, "Maximum number of processes for UID %d reached.\n", uid);
    else if (EPERM == err)
      fprintf(stderr, "Not enough privileges to change UID.\n");
    else
      perror(NULL);
    exit(err);
  };

  /* execute the wrapped command */
  execv(real_exec, argv);
  const int err = errno;
  fprintf(stderr, "Could not execute wrapped program '%s': %s", real_exec, strerror(err));
  free(argv0);
  return err;
}
