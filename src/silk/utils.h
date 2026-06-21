#pragma once

#include <Python.h>

extern PyObject* ObjHandle_write;
extern PyObject* ObjHandle_read;

void init_constant();

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