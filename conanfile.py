from conans import ConanFile, CMake, tools
import os
import re


def parse_cmake_project_version(cmake_file_path):
    """
    Returns the project version of the given cmake file as a string.
    If the cmake file is not found, None is returned.
    If the cmake file doesn't contain a project or no version is
    specified, a runtime error is raised.

    This is a pimped version of
    https://docs.conan.io/en/1.4/howtos/capture_version.html
    """
    try:
        with open(cmake_file_path, 'r') as cmake_file:
            cmake_contents = cmake_file.read().lower()
            proj_chunk = re.search(r'project\s*\(([^\)]+)\)', cmake_contents)
            if proj_chunk is None or proj_chunk.group(0) is None:
                raise RuntimeError('No project found in %s' % cmake_file_path)
            ver = re.search(r'version[\s\n]+(\d+(\.\d+(\.\d+)?)?)', proj_chunk.group(0))
            if ver is None or ver.group(1) is None:
                raise RuntimeError('No project version specified in %s' % cmake_file_path)

            return ver.group(1)
    except Exception:
        return None


class CycloneDDSConan(ConanFile):
    name = "cyclonedds"
    version = parse_cmake_project_version("CMakeLists.txt")
    license = "Eclipse Public License - v 2.0"
    author = 'Eclipse Cyclone DDS Contributors'
    description = "Eclipse Cyclone DDS project"
    url = 'https://github.com/eclipse-cyclonedds/cyclonedds'
    generators = "cmake"
    scm = {
        "revision": "auto",
        "type": "git",
        "url": "auto"
    }
    options = {
        "openssl_version": ["1.0.2", "1.1.1c"],
        "enable_ssl": [True, False],
        "enable_security": [True, False],
        "enable_lifespan": [True, False],
        "enable_deadline_missed": [True, False],
        "enable_type_discovery": [True, False]
    }
    default_options = {
        "openssl_version": "1.1.1c",
        "enable_ssl": True,
        "enable_security": True,
        "enable_lifespan": True,
        "enable_deadline_missed": True,
        "enable_type_discovery": True
    }

    _cmake = None

    build_requires = ("cunit/2.1-3@bincrafters/stable", "maven/3.6.3")

    def build_requirements(self):
        self.build_requires("OpenSSL/{}@conan/stable".format(self.options.openssl_version))

    def _configure_cmake(self):
        if self._cmake:
            return self._cmake
        self._cmake = CMake(self)

        self._cmake.definitions["ENABLE_SSL"] = True
        self._cmake.definitions["ENABLE_SECURITY"] = True
        self._cmake.definitions["ENABLE_LIFESPAN"] = True
        self._cmake.definitions["ENABLE_DEADLINE_MISSED"] = True
        self._cmake.definitions["ENABLE_TYPE_DISCOVERY"] = True
        return self._cmake

    def build(self):
        cmake = self._configure_cmake()
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = self._configure_cmake()
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = tools.collect_libs(self)
        self.env_info.LD_LIBRARY_PATH.extend([
            os.path.join(self.package_folder, x) for x in self.cpp_info.libdirs
        ])
        self.env_info.PATH.extend([
            os.path.join(self.package_folder, x) for x in self.cpp_info.bindirs
        ])
