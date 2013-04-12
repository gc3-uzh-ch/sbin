/**
 * @file   usemem.c
 *
 * Consume a specified amount of main memory.
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>


/* defaults */
long chunksize = (4 * 1024 * 1024); /* default 4MB */
int verbose = 1;

int
main(const int argc, char * const argv[])
{
  int c;
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
          {"brief",       no_argument,       &verbose, 0},
          {"chunksize",   required_argument, 0,        'c'},
          {0, 0, 0, 0}
        };

      /* getopt_long stores the option index here. */
      int option_index;
      c = getopt_long(argc, argv, "c:", long_options, &option_index);

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
          chunksize = strtoul(optarg, NULL, 0);
          break;

        case 'p':
          printf("option -p with value `%s'\n", optarg);
          break;

        case '?':
          /* getopt_long already printed an error message. */
          break;

        default:
          abort ();
        }
    }

  /* amount of memory is 1st arg */
  size = strtoul(argv[optind], NULL, 0);

  /* fill in a chunk of memory with random data */
  chunk = calloc(chunksize, 1);
  if (NULL == chunk) {
    fprintf(stderr, "Cannot allocate memory: %s\n", strerror(errno));
    abort();
  };

  fd = open("/dev/urandom", O_RDONLY);
  if (-1 == fd) {
    fprintf(stderr, "Cannot open /dev/urandom: %s\n", strerror(errno));
    abort();
  }
  read(fd, chunk, chunksize);

  /* now allocate memory and start filling it */
  mem = malloc(size);
  if (NULL == mem) {
    fprintf(stderr, "Cannot allocate memory: %s\n", strerror(errno));
    abort();
  };

  remaining = size;
  while (remaining > 0) {
    if (size < chunksize) {
      chunksize = remaining;
      remaining = 0; /* last round */
    }
    else
      remaining -= chunksize;
    memcpy(mem, chunk, chunksize);
    mem += chunksize;
  };

  if (verbose)
    printf("Successfully written %lu bytes of RAM.\n", size);
  exit(0);
}
