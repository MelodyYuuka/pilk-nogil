//
// Memory buffer for BytesIO output support
//

#ifndef PILK_MEMORY_BUFFER_H
#define PILK_MEMORY_BUFFER_H

#include <Python.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"

typedef struct {
    unsigned char* data;
    Py_ssize_t size;
    Py_ssize_t capacity;
} MemoryBuffer;

static inline int memory_buffer_init(MemoryBuffer* buf,
                                     Py_ssize_t initial_capacity) {
    buf->data = (unsigned char*)malloc(initial_capacity);
    if (!buf->data) {
        PyErr_NoMemory();
        return -1;
    }
    buf->size = 0;
    buf->capacity = initial_capacity;
    return 0;
}

static inline int memory_buffer_write(MemoryBuffer* buf, const void* data,
                                      Py_ssize_t size) {
    if (buf->size + size > buf->capacity) {
        Py_ssize_t new_capacity = buf->capacity * 2;
        while (new_capacity < buf->size + size) {
            new_capacity *= 2;
        }
        unsigned char* new_data =
            (unsigned char*)realloc(buf->data, new_capacity);
        if (!new_data) {
            PyErr_NoMemory();
            return -1;
        }
        buf->data = new_data;
        buf->capacity = new_capacity;
    }
    memcpy(buf->data + buf->size, data, size);
    buf->size += size;
    return 0;
}

static inline PyObject* memory_buffer_to_bytes(MemoryBuffer* buf) {
    return PyBytes_FromStringAndSize((const char*)buf->data, buf->size);
}

static inline bool memory_buffer_append_to_bytesio(PyObject* silk_bytesio_obj,
                                                   MemoryBuffer* buf,
                                                   PyObject* write_method) {
    PyObject* mem_view =
        PyMemoryView_FromMemory((char*)buf->data, buf->size, PyBUF_READ);
    if (!mem_view) {
        return false;
    }

    PyObject* res =
        PyObject_CallMethodOneArg(silk_bytesio_obj, write_method, mem_view);

    Py_DECREF(mem_view);

    if (!res) {
        return false;
    }

    Py_DECREF(res);
    return true;
}

static inline void memory_buffer_free(MemoryBuffer* buf) {
    if (buf->data) {
        free(buf->data);
        buf->data = NULL;
    }
    buf->size = 0;
    buf->capacity = 0;
}

#endif  // PILK_MEMORY_BUFFER_H
