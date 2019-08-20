#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <sparrowhawk/normalizer.h>

namespace py = pybind11;
namespace sp = speech::sparrowhawk;

PYBIND11_MODULE(normalizer, m) {
    py::class_<sp::Normalizer>(m, "Normalizer")
        .def(py::init<>())
        .def("setup", &sp::Normalizer::Setup)
        .def("construct_verbalizer_string", &sp::Normalizer::ConstructVerbalizerString);
}

