from conans import ConanFile, CMake, tools
import os

from subprocess import Popen, PIPE

class TestPackageConan(ConanFile):
    generators = "cmake"

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def test(self):
        pub = Popen(['./HelloworldPublisher'],stdout=PIPE)
        sub = Popen(['./HelloworldSubscriber'],stdout=PIPE)
        pub_output = pub.communicate()[0].decode('ascii')
        sub_output = sub.communicate()[0].decode('ascii')
        pub.wait()
        sub.wait()

        print(pub_output)
        print(sub_output)

        if "Writing : Message (1, Hello World)" not in pub_output:
            raise RuntimeError("Unexpected output from publisher")

        if "Received : Message (1, Hello World)" not in sub_output:
            raise RuntimeError("Unexpected output from subscriber")


