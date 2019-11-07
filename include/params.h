/*
  Blok File System
  Copyright (C) 2019 Kamil Noster <kamil.noster@gmail.com>

  The code is derived from:
  Big Brother File System
  Copyright (C) 2012 Joseph J. Pfeiffer, Jr., Ph.D. <pfeiffer@cs.nmsu.edu>
  Which is distributed under the GNU GPLv3.
*/

#ifndef _PARAMS_H_
#define _PARAMS_H_

// The FUSE API has been changed a number of times.  So, our code
// needs to define the version of the API that we assume.  As of this
// writing, the most current API version is 26
#define FUSE_USE_VERSION 26

// need this to get pwrite().  I have to use setvbuf() instead of
// setlinebuf() later in consequence.
#define _XOPEN_SOURCE 500

// maintain bbfs state in here
#include <limits.h>
#include <stdio.h>

struct fs_state {
    char *rootdir;
};
#define BLOK_DATA ((struct fs_state *) fuse_get_context()->private_data)

#endif
