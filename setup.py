from distutils.core import setup, Extension
import os

os.environ["CXX"] = "g++"
os.environ["CC"] = "gcc"

module1 = Extension('normalizermodule',
                    sources = ['src/bin/normalizermodule.cc'],
                    extra_compile_args=['-std=c++11'],
                    library_dirs = ['/usr/local/lib'],
                    libraries=['stdc++', 'sparrowhawk', 'dl', 'protobuf', 'protoc', 'thrax', 'fst', 'fstfar', 're2'],
                    include_dirs = ['/usr/local/include', 'src/include/'])

setup (name = 'Normalizer',
       version = '1.0',
       description = 'Build an acceptor for verbalization.',
       ext_modules = [module1])
