/*-*- Mode: C; c-basic-offset: 8; indent-tabs-mode: nil -*-*/

/***
  This file is part of systemd.

  Copyright 2010 Lennart Poettering
  Copyright (C) 2012 Roberto Sassu - Politecnico di Torino, Italy
                                     TORSEC group -- http://security.polito.it

  systemd is free software; you can redistribute it and/or modify it
  under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation; either version 2.1 of the License, or
  (at your option) any later version.

  systemd is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with systemd; If not, see <http://www.gnu.org/licenses/>.
***/

#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "ima-setup.h"
#include "util.h"
#include "log.h"

#define IMA_SECFS_DIR "/sys/kernel/security/ima"
#define IMA_SECFS_POLICY IMA_SECFS_DIR "/policy"
#define IMA_POLICY_PATH "/etc/ima/ima-policy"

int ima_setup(void) {
        int r = 0;

#ifdef HAVE_IMA
        _cleanup_close_ int policyfd = -1, imafd = -1;
        struct stat st;
        char *policy;

        if (access(IMA_SECFS_DIR, F_OK) < 0) {
                log_debug("IMA support is disabled in the kernel, ignoring.");
                return 0;
        }

        policyfd = open(IMA_POLICY_PATH, O_RDONLY|O_CLOEXEC);
        if (policyfd < 0) {
                log_full_errno(errno == ENOENT ? LOG_DEBUG : LOG_WARNING, errno,
                               "Failed to open the IMA custom policy file "IMA_POLICY_PATH", ignoring: %m");
                return 0;
        }

        if (access(IMA_SECFS_POLICY, F_OK) < 0) {
                log_warning("Another IMA custom policy has already been loaded, ignoring.");
                return 0;
        }

        imafd = open(IMA_SECFS_POLICY, O_WRONLY|O_CLOEXEC);
        if (imafd < 0) {
                log_error_errno(errno, "Failed to open the IMA kernel interface "IMA_SECFS_POLICY", ignoring: %m");
                return 0;
        }

        if (fstat(policyfd, &st) < 0)
                return log_error_errno(errno, "Failed to fstat "IMA_POLICY_PATH": %m");

        policy = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, policyfd, 0);
        if (policy == MAP_FAILED)
                return log_error_errno(errno, "Failed to mmap "IMA_POLICY_PATH": %m");

        r = loop_write(imafd, policy, (size_t) st.st_size, false);
        if (r < 0)
                log_error_errno(r, "Failed to load the IMA custom policy file "IMA_POLICY_PATH": %m");
        else
                log_info("Successfully loaded the IMA custom policy "IMA_POLICY_PATH".");

        munmap(policy, st.st_size);
#endif /* HAVE_IMA */
        return r;
}
