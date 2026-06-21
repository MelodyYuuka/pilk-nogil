#include "utils.h"

#include <Python.h>

PyObject* ObjHandle_write = NULL;
PyObject* ObjHandle_read = NULL;

void init_constant() {
  ObjHandle_write = PyUnicode_InternFromString("write");
  ObjHandle_read = PyUnicode_InternFromString("read");
}
