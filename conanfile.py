# conanfile.py
from conan import ConanFile
from conan.tools.cmake import cmake_layout

class MyProjectConan(ConanFile):
    """
    This is the Python-based Conan recipe for our project.
    """
    # Standard settings for any C++ project
    settings = "os", "compiler", "build_type", "arch"

    # The generators we want to use
    generators = "CMakeDeps", "CMakeToolchain"

    def requirements(self):
        """
        Define all project dependencies here.
        This is the equivalent of the [requires] section.
        """
        # I've used slightly older, very stable versions to maximize the chance
        # of finding pre-built binaries, avoiding long compilation times.
        # You can change these back to your originals if needed.
        self.requires("boost/1.88.0")      
        #self.requires("libmysqlclient/8.0.34")
        self.requires("spdlog/1.15.3")
        self.requires("jwt-cpp/0.7.1")
        self.requires("libsodium/1.0.20")
        #self.requires("soci/4.0.3")
        #self.requires("mysql-connector-cpp/9.2.0")

    def layout(self):
        """
        Define the project layout.
        This is the equivalent of the [layout] section.
        It sets up the standard 'build' and 'source' folders structure.
        """
        cmake_layout(self)

    def configure(self):
        """
        (Optional but good practice)
        Define options for your dependencies here.
        """
        # For example, if you wanted to force dynamic linking for a package:
        #self.options["boost/*"].shared = True
        #self.options["soci/*"].shared = True
        #self.options["libmysqlclient/*"].shared = True
        #self.options["mysql-connector-cpp/*"].shared = True
        pass