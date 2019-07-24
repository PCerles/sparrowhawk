from distutils.core import setup, Extension
import os

os.environ["CXX"] = "g++"
os.environ["CC"] = "gcc"

class get_pybind_include(object):
    """Helper class to determine the pybind11 include path
    The purpose of this class is to postpone importing pybind11
    until it is actually installed, so that the ``get_include()``
    method can be invoked. """

    def __init__(self, user=False):
        self.user = user

    def __str__(self):
        import pybind11
        return pybind11.get_include(self.user)

module1 = Extension('normalizer',
                    sources = ['src/bin/py_normalizer.cc'],
                    extra_compile_args=['-std=c++11', '-O3', '-shared', '-fPIC', '-Wall'],
                    library_dirs = ['/usr/local/lib'],
                    libraries=['stdc++', 'sparrowhawk', 'dl', 'protobuf', 'protoc', 'thrax', 'fst', 'fstfar', 're2'],
                    include_dirs = ['/usr/local/include', 'src/include/', get_pybind_include(), get_pybind_include(user=True)])

setup (name = 'normalizer',
       version = '1.0',
       description = 'Build an acceptor for verbalization.',
       ext_modules = [module1])

