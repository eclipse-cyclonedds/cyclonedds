#
# Copyright(c) 2021 ZettaScale Technology and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#
undef $/;
while (<>) {
  if (m/(const char \* [0-9a-z]+_metaDescriptor\[\] = \{".*"\};\nconst int  [0-9a-z]+_metaDescriptorArrLength = [0-9]+;\nconst int  [0-9a-z]+_metaDescriptorLength = [0-9]+;)/gs) {
    print $1;
  }
}
