/**
 * @file   usemem.c
 *
 * Helper program to trigger Linux OOM conditions.
 *
 * @author  Riccardo Murri <riccardo.murri@gmail.com>
 * @version 1.0
 */
/*
 * Copyright (c) 2013 GC3, University of Zurich. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Suite 500, Boston, MA
 * 02110-1335 USA
 *
 */

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


/* defaults */
size_t chunksize = (4 * 1024 * 1024); /* default 4MB */
int cpu_time = 0;
int np = 1;
int verbose = 1;


void usage()
{
 printf("Usage: oom [options] AMOUNT\n"
        "\n"
        "Helper program to generate Linux OOM conditions.\n"
        "Allocates AMOUNT bytes of virtual memory and makes sure they are fully\n"
        "utilized.\n"
        "\n"
        "Options:\n"
        "\n"
        "  --chunksize, -c NUM\n"
        "        Increase memory usage in chunks of NUM bytes at a time.\n"
        "        (By default, increase memory usage by 4MB each iteration.)\n"
        "\n"
        "  --cpu-time, -t NUM\n"
        "        Keep the CPU busy for NUM seconds before starting to allocate\n"
        "        memory.  This option is provided since the Linux OOM killer\n"
        "        prefers to kill processes with low CPU usage.\n"
        "\n"
        "  --processes, -p NUM\n"
        "        Spawn NUM processes that try to allocate and use memory in parallel.\n"
        "        By default, only one process is created, which means that you cannot\n"
        "        allocate more memory than the `vm.overcommit_ratio` Linux setting allows.\n"
        "\n"
        "  --quiet\n"
        "        Do not print any informational message about what the program is doing.\n"
        "\n"
        );
}


size_t
parse_memory_size(const char* const arg)
{
  char* next = NULL;
  size_t size = strtoul(arg, &next, 0);
  int magnitude = 0;
  int base = 1000;

  if (arg == next)
    goto error;

  while ('\0' != *next) {
    switch(*next) {
    case 'G':
    case 'g':
      magnitude = 3;
      break;

    case 'M':
    case 'm':
      magnitude = 2;
      break;

    case 'K': /* accept 'K' as a prefix though it's not SI */
    case 'k':
      magnitude = 1;
      break;

    case 'B':
    case 'b':
      break;

      /* accept 'MiB', 'KiB', etc. as the base-2 equivalents */
    case 'i':
      base = 1024;
      break;

    default:
      goto error;
    };

    ++next;
  };

  while (magnitude-- > 0)
    size *= base;

  return size;

 error:
  fprintf(stderr,
          "Cannot parse '%s' as a memory size."
          " Please use an integer number, followed by GB, MB, kB.\n",
          arg);
  exit(1);
}


/* wake up timer */
int alarm_rang = 0;

void sigalrm(int signum)
{
  alarm_rang = 1;
}


main(const int argc, char * const argv[])
{
  int c;
  int p = 0;
  int fd;
  char* chunk;
  char* mem;
  size_t size = 0;
  size_t remaining;

  while (1)
    {
      static struct option long_options[] =
        {
          {"verbose",     no_argument,       &verbose, 1},
          {"quiet",       no_argument,       &verbose, 0},
          {"chunksize",   required_argument, 0,        'c'},
          {"cpu-time",    required_argument, 0,        't'},
          {"processes",   required_argument, 0,        'p'},
          {"help",        no_argument,       0,        'h'},
          {0, 0, 0, 0}
        };

      /* getopt_long stores the option index here. */
      int option_index;
      c = getopt_long(argc, argv, "c:hp:t:", long_options, &option_index);

      /* Detect the end of the options. */
      if (c == -1)
        break;

      switch (c)
        {
        case 0:
          /* If this option set a flag, do nothing else now. */
          if (long_options[option_index].flag != 0)
            break;

        case 'c':
          chunksize = parse_memory_size(optarg);
          break;

        case 'h':
          usage();
          exit(0);
          break;

        case 'p':
          np = strtoul(optarg, NULL, 0);
          break;

        case 't':
          cpu_time = strtoul(optarg, NULL, 0);
          break;

        case '?':
          /* getopt_long already printed an error message. */
          break;

        default:
          abort ();
        }
    }

  /* amount of memory is 1st arg */
  size = parse_memory_size(argv[optind]);

  /* allocate memory */
  mem = malloc(size);
  if (NULL == mem) {
    fprintf(stderr, "Cannot allocate main memory segment of %lu bytes, aborting.\n", size);
    abort();
  };

  /* fork aux processes */
  setpgid(0, 0); /* create new process group */
  for (p=1; p < np; ++p) {
    int rc = fork();
    if (-1 == rc) {
      fprintf(stderr, "Cannot create auxiliary process #%d, aborting.\n", p);
      kill(0, SIGTERM); /* kill process group */
      abort();
    }
    else if (0 == rc) {
      /* child process does not need to fork */
      break;
    };
  };

  /* waste cpu time for the specified number of seconds */
  if (cpu_time) {
    float x = 0;
    float n = 1.0;
    printf("[#%d] Wasting %u seconds of CPU time by busy-waiting ...\n", p, cpu_time);
    /* set up our signal handler to catch SIGALRM */
    signal(SIGALRM, sigalrm);
    alarm(cpu_time);
    /* need to do some real work here,
       otherwise the compiler could optimize the loop away... */
    while (! alarm_rang) {
      x += 1/(n*n);
      n += 1;
      if (n > 1000000000) {
        x = 0;
        n = 1.0;
      };
    };
  };

  /* fill in a chunk of memory with random data */
  chunk = calloc(chunksize, 1);
  if (NULL == chunk) {
    fprintf(stderr, "[#%d] Cannot allocate memory chunk of %lu bytes, aborting.\n", p, chunksize);
    abort();
  };

  fd = open("/dev/urandom", O_RDONLY);
  if (-1 == fd) {
    fprintf(stderr, "[#%d] Cannot open /dev/urandom: %s. Aborting.\n", p, strerror(errno));
    abort();
  }
  read(fd, chunk, chunksize);

  /* now fill memory */
  remaining = size;
  while (remaining > 0) {
    if (remaining < chunksize) {
      chunksize = remaining;
      remaining = 0; /* last round */
    }
    else
      remaining -= chunksize;
    memcpy(mem, chunk, chunksize);
    mem += chunksize;
  };

  if (verbose)
    printf("[#%d] Successfully written %lu bytes of RAM.\n", p, size);


  while (np > 1) {
    int status;
    wait(&status);
    np--;
  };

  exit(0);
}
