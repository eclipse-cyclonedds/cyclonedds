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
        Given append_files and hash_files ensure generated files are up to date
"""

import sys
import argparse
import subprocess


def make_parser():
    parser = argparse.ArgumentParser()
    parser.add_argument('--hash-files', type=str, nargs='+')
    parser.add_argument('--append-files', type=str, nargs='+')
    return parser


def main():
    data = make_parser().parse_args(sys.argv[1:])
    hash_file_list = ";".join(data.hash_files)
    append_file_list = ";".join(data.append_files)

    try:
        subprocess.check_call([
            "cmake",
            f"-DHASH_FILES={hash_file_list}",
            f"-DAPPEND_FILES={append_file_list}",
            "-P", "cmake/CheckHashScript.cmake"
        ])
    except subprocess.CalledProcessError:
        sys.exit(1)


if __name__ == "__main__":
    main()
