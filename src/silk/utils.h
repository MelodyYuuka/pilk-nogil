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
    FILE* fp = NULL;

#ifdef _WIN32
    /*
     * On Windows the C runtime's fopen() interprets char* filenames using the
     * active ANSI code page (e.g. GBK on a Chinese locale). Since Python 3.6
     * (PEP 529) the filesystem encoding on Windows is UTF-8, so converting a
     * Unicode path to bytes via PyUnicode_FSConverter yields UTF-8 bytes that
     * fopen() cannot understand, which garbles any non-ASCII path and leads to
     * spurious FileNotFoundError.
     *
     * Use the wide-character _wfopen() API instead, which accepts the full
     * Unicode path directly and is independent of the active code page.
     */
    if (!PyUnicode_Check(path)) {
        PyErr_SetString(PyExc_TypeError, "file path must be str");
        return NULL;
    }

    wchar_t wmode[8];
    size_t i;
    for (i = 0; mode[i] != '\0' && i < sizeof(wmode) / sizeof(wchar_t) - 1; i++) {
        wmode[i] = (wchar_t)(unsigned char)mode[i];
    }
    wmode[i] = L'\0';

    wchar_t* wpath = PyUnicode_AsWideCharString(path, NULL);
    if (wpath == NULL) {
        return NULL;
    }

    fp = _wfopen(wpath, wmode);
    if (fp == NULL) {
        PyErr_SetFromErrnoWithFilenameObject(PyExc_OSError, path);
        PyMem_Free(wpath);
        return NULL;
    }

    PyMem_Free(wpath);
    return fp;
#else
    PyObject* path_bytes = NULL;

    if (!PyUnicode_FSConverter(path, &path_bytes)) {
        // FSConverter 失败时已经设置了异常
        return NULL;
    }

    const char* path_str = PyBytes_AS_STRING(path_bytes);
    fp = fopen(path_str, mode);

    if (fp == NULL) {
        PyErr_SetFromErrnoWithFilenameObject(PyExc_OSError, path);
        Py_DECREF(path_bytes);
        return NULL;
    }

    Py_DECREF(path_bytes);
    return fp;
#endif
}