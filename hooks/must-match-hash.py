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
    pre-commit-hook must-match-hash
        given file 'a' there should be a file 'a.hash' that is matching
"""

import sys
import hashlib


def main():
    for file in sys.argv[1:]:
        with open(file) as f:
            hash = hashlib.sha1(f.read().replace('\r', '').encode()).hexdigest()

        print(hash)

        with open(f"{file}.hash") as f:
            cmake_hash = f.read().strip()
            print(cmake_hash)
            if hash != cmake_hash:
                print(f"Hashfile incorrect for {file}", file=sys.stderr)
                sys.exit(1)


if __name__ == "__main__":
    main()
