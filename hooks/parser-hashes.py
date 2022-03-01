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
    pre-commit-hook parser-hashes
        the hashes in parser.c and parser.h must match parser.y
"""

import sys
import hashlib


def main():
    with open("src/idl/src/parser.y") as f:
        hash = hashlib.sha1(f.read().replace('\r', '').encode()).hexdigest()

    with open(f"src/idl/src/parser.y.hash") as f:
            cmake_hash = f.read().strip()
            if hash != cmake_hash:
                print(f"Hashfile incorrect for src/idl/src/parser.y", file=sys.stderr)
                sys.exit(1)

    with open("src/idl/src/parser.c") as f:
        data = f.read().splitlines(False)[-1]
        if hash not in data:
            print("Hash in parser.c does not match parser.y", file=sys.stderr)
            sys.exit(1)

    with open("src/idl/src/parser.h") as f:
        data = f.read().splitlines(False)[-1]
        if hash not in data:
            print("Hash in parser.h does not match parser.y", file=sys.stderr)
            sys.exit(1)


if __name__ == "__main__":
    main()
