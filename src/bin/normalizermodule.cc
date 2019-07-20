#include <Python.h>
#include <iostream>
#include <sparrowhawk/normalizer.h>
#include <fst/script/print.h>

static PyObject *NormalizerError;
const string SP_CONFIG      = "sparrowhawk_configuration.ascii_proto";
const string SP_PATH_PREFIX = "/tts/sparrowhawk/documentation/grammars/";

/* Init */
static PyObject * normalizer_construct_acceptor(PyObject *self, PyObject *args);

static PyMethodDef NormalizerMethods[] = {
    {"construct_acceptor",  normalizer_construct_acceptor, METH_VARARGS,
     "Construct verbalized acceptor."},

    {NULL, NULL, 0, NULL}        /* Sentinel */
};

PyMODINIT_FUNC initnormalizer(void)
{
    PyObject *m;

    m = Py_InitModule("normalizer", NormalizerMethods);
    if (m == NULL)
        return;

    NormalizerError = PyErr_NewException("spam.error", NULL, NULL);
    Py_INCREF(NormalizerError);
    PyModule_AddObject(m, "error", NormalizerError);
}
/********/


static PyObject * normalizer_construct_acceptor(PyObject *self, PyObject *args) 
{
    using speech::sparrowhawk::Normalizer;

    const char * transcript;

    if (!PyArg_ParseTuple(args, "s", &transcript)) {
        PyErr_SetString(NormalizerError, "System command failed");
        return NULL;
    }


    std::cout << "Transcript: " << transcript << std::endl;
    std::set_new_handler(FailedNewHandler);
    std::unique_ptr<Normalizer> normalizer;
    normalizer.reset(new Normalizer());
    bool res = normalizer->Setup(SP_CONFIG, SP_PATH_PREFIX);
    assert(res);

    fst::SymbolTable syms("test.syms"); // TODO remove
    fst::StdVectorFst acceptor;    
        
    normalizer->ConstructVerbalizer(transcript, &acceptor, &syms);

    // debug 
    normalizer->format_and_save_fst(&acceptor, "python_out", "/tts/images2/");

    // format output
    std::ostringstream outs;
    fst::script::PrintFst(acceptor, outs);
    string output_string = outs.str();

    return Py_BuildValue("s", output_string.c_str());
}

int
main(int argc, char *argv[])
{
    /* Pass argv[0] to the Python interpreter */
    Py_SetProgramName(argv[0]);

    /* Initialize the Python interpreter.  Required. */
    Py_Initialize();

    /* Add a static module */
    initnormalizer();
}
