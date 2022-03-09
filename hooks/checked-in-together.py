# * Copyright(c) 2022 ZettaScale Technology and others
# *
# * This program and the accompanying materials are made available under the
# * terms of the Eclipse Public License v. 2.0 which is available at
# * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# * v. 1.0 which is available at
# * http://www.eclipse.org/org/documents/edl-v10.php.
# *
# * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

"""
    pre-commit-hook checked-in-together
        files passed as arguments either must all be checked in or none of them
"""

import sys
import hashlib
from subprocess import Popen, PIPE


def added_files():
    p = Popen(
        ['git', 'diff', '--staged', '--name-only'],
        stdout=PIPE,
        stderr=PIPE
    )
    out, err = p.communicate()
    if p.returncode != 0:
        raise RuntimeError(err.decode())

    return set(out.decode().splitlines())


def last_commited_files():
    p = Popen(
        ['git', 'diff', '--name-only', 'HEAD', 'HEAD~1'],
        stdout=PIPE,
        stderr=PIPE
    )
    out, err = p.communicate()
    if p.returncode != 0:
        # There is not always a HEAD, not considered an error
        return set()

    return set(out.decode().splitlines())



def main():
    added = added_files()

    if not added:
        # if we are in the post-added stage we verify the last commit
        # (useful for CI)
        added = last_commited_files()

    checked = [file in added for file in sys.argv[1:]]
    if any(checked) and not all(checked):
        print(f"Must check in all or none of files {' '.join(sys.argv[1:])}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
