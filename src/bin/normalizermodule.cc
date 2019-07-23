#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <iostream>
#include <sparrowhawk/normalizer.h>
#include <fst/script/print.h>

// Config
// const string SP_CONFIG      = "sparrowhawk_configuration.ascii_proto";
// const string SP_PATH_PREFIX = "/workspace/sparrowhawk/documentation/grammars/";

// Init
static PyObject * NormalizerError;
static PyObject * normalizer_construct_acceptor(PyObject *self, PyObject *args);

static PyMethodDef NormalizerMethods[] = {
    {"construct_acceptor",  normalizer_construct_acceptor, METH_VARARGS,
     "Construct verbalized acceptor."},

    {NULL, NULL, 0, NULL}        /* Sentinel */
};

static struct PyModuleDef normalizermodule = {
    PyModuleDef_HEAD_INIT,
    "normalizer",   /* name of module */
    NULL, /* module documentation, may be NULL */
    -1,       /* size of per-interpreter state of the module,
                 or -1 if the module keeps state in global variables. */
    NormalizerMethods
};

PyMODINIT_FUNC PyInit_normalizer(void)
{
    PyObject *m;

    m = PyModule_Create(&normalizermodule);
    if (m == NULL)
        return NULL;

    NormalizerError = PyErr_NewException("normalizer.error", NULL, NULL);
    Py_INCREF(NormalizerError);
    PyModule_AddObject(m, "error", NormalizerError);
    return m;
}

static PyObject * normalizer_construct_acceptor(PyObject *self, PyObject *args) 
{
    using speech::sparrowhawk::Normalizer;

    // Parse arguments
    const char * transcript;
    const char * sp_config;
    const char * sp_path_prefix;
    if (!PyArg_ParseTuple(args, "sss", &transcript, &sp_config, &sp_path_prefix)) {
        PyErr_SetString(NormalizerError, "Normalizer argument parsing failed.");
        return NULL;
    }

    std::set_new_handler(FailedNewHandler);

    // Prepare normalizer
    std::unique_ptr<Normalizer> normalizer;
    normalizer.reset(new Normalizer());

    bool res = normalizer->Setup(sp_config, sp_path_prefix);
    assert(res);

    // Construct Acceptor
    fst::SymbolTable syms("test.syms"); // TODO remove
    fst::StdVectorFst acceptor;    
        
    normalizer->ConstructVerbalizer(transcript, &acceptor, &syms);

    // Prepare output string
    std::ostringstream outs;
    fst::script::PrintFst(acceptor, outs);
    string output_string = outs.str();

    return Py_BuildValue("s", output_string.c_str());
}

int main(int argc, char *argv[])
{
    wchar_t *program = Py_DecodeLocale(argv[0], NULL);
    if (program == NULL) {
        fprintf(stderr, "Fatal error: cannot decode argv[0]\n");
        exit(1);
    }

    /* Add a built-in module, before Py_Initialize */
    PyImport_AppendInittab("normalizer", PyInit_normalizer);

    /* Pass argv[0] to the Python interpreter */
    Py_SetProgramName(program);

    /* Initialize the Python interpreter.  Required. */
    Py_Initialize();

    /* Optionally import the module; alternatively,
       import can be deferred until the embedded script
       imports it. */
    PyImport_ImportModule("normalizer");
}
