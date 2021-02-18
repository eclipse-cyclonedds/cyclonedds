/*
 * Copyright(c) 2021 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#include <stdio.h>
#include <string.h>
#include "cyclone-config.h"

#ifdef _WIN32
#define SEPARATOR "\\"
#else
#define SEPARATOR "/"
#endif

void print_usage(char* prog);
void print_usage(char* prog)
{
    printf("cyclone-config for Cyclone " VERSION " installed to " PREFIX "\nUsage: $ %s --help|--prefix|--libdir|--includedir|--bindir\n", prog);
}


int main(int argc, char* argv[]) {
    if (argc < 2 || argc > 2) {
        print_usage(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "--prefix") == 0) {
        printf(PREFIX);
    }
    else if (strcmp(argv[1], "--libdir") == 0) {
        printf(PREFIX SEPARATOR "lib");
    }
    else if (strcmp(argv[1], "--includedir") == 0) {
        printf(PREFIX SEPARATOR "include");
    }
    else if (strcmp(argv[1], "--libdir") == 0) {
        printf(PREFIX SEPARATOR "lib");
    }
    else if (strcmp(argv[1], "--bindir") == 0) {
        printf(PREFIX SEPARATOR "bin");
    }
    else if (strcmp(argv[1], "--version") == 0) {
        printf(VERSION);
    }
    else if (strcmp(argv[1], "--help") == 0) {
        print_usage(argv[0]);
    }
    else {
        print_usage(argv[0]);
        return 1;
    }
    return 0;
}