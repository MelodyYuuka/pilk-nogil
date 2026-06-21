#include <Python.h>
#include "constant.h"

PyObject* PyId_write = NULL;
PyObject* PyId_read = NULL;

void init_constant() {
    PyId_write = PyUnicode_InternFromString("write");
    PyId_read = PyUnicode_InternFromString("read");
}
