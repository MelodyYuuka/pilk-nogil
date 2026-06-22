#pragma once

#include <Python.h>

// Per-module state
typedef struct {
    PyObject* ObjHandle_write;
    PyObject* ObjHandle_read;
} PilkState;

void init_constant(PilkState* state);

static inline PilkState* pilk_get_state(PyObject* module) {
    return (PilkState*)PyModule_GetState(module);
}

static inline FILE* Utils_fopen(PyObject* path, const char* mode) {
    FILE* bitInFile = NULL;
    PyObject* path_bytes = NULL;

    if (!PyUnicode_FSConverter(path, &path_bytes)) {
        // FSConverter 失败时已经设置了异常
        return NULL;
    }

    const char* path_str = PyBytes_AS_STRING(path_bytes);
    bitInFile = fopen(path_str, mode);

    if (bitInFile == NULL) {
        PyErr_SetFromErrnoWithFilenameObject(PyExc_OSError, path);
        Py_DECREF(path_bytes);
        return NULL;
    }

    Py_DECREF(path_bytes);
    return bitInFile;
}