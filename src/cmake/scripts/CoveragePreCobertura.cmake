#
# Copyright(c) 2006 to 2018 ADLINK Technology Limited and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#

#
# This script assumes that it is called before all tests are run and gcov results are available.
# It can be used to setup the environment needed to get proper Cobertura coverage results.
#
# Example usage:
# $ cmake -DCOVERAGE_SETTINGS=<cham bld>/CoverageSettings.cmake -P <cham src>/cmake/scripts/CoveragePreCobertura.cmake
# $ ctest -T test
# $ ctest -T coverage
# $ ctest -DCOVERAGE_SETTINGS=<cham bld>/CoverageSettings.cmake -P <cham src>/cmake/scripts/CoveragePostCobertura.cmake
# If you start the scripts while in <cham bld> then you don't have to provide the COVERAGE_SETTINGS file.
#
cmake_minimum_required(VERSION 3.5)

#
# Nothing to do really.
# This is just added to provide consistency between Coverage scripts.
#

