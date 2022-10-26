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
    pre-commit-hook version-check
        ensures package.xml and CMake listed version are in sync
"""

import re
import sys
import json
from xml.etree import ElementTree


cmake_version_regex = re.compile(r"project\s*\(\s*CycloneDDS.*VERSION\s+([0-9]+\.[0-9]+\.[0-9]).*\)", re.IGNORECASE)


def main():
    with open('CMakeLists.txt') as f:
        m = cmake_version_regex.search(f.read())
        if not m:
            print("Could not locate version information in CMakeLists.txt.", file=sys.stderr)
            sys.exit(1)
        cmake_version = m.group(1)

    tree = ElementTree.parse('package.xml')
    package_version = tree.getroot().find('version').text

    if not cmake_version == package_version:
        print(f"package.xml version:    {package_version}", file=sys.stderr)
        print(f"CMakeLists.txt version: {cmake_version}", file=sys.stderr)
        sys.exit(1)

    with open('docs/manual/variables.json') as f:
        vars = json.load(f)
        docs_version = vars['version']
        docs_release = vars['release']

    if not cmake_version == docs_version:
        print(f"package.xml version:                {package_version}", file=sys.stderr)
        print(f"docs/manual/variables.json version: {docs_version}", file=sys.stderr)
        sys.exit(1)

    if not cmake_version.startswith(docs_release):
        print(f"package.xml version:                {package_version}", file=sys.stderr)
        print(f"docs/manual/variables.json release: {docs_release}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
