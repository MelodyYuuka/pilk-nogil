#include "utils.h"

#include <Python.h>

void init_constant(PilkState* state) {
  state->ObjHandle_write = PyUnicode_InternFromString("write");
  state->ObjHandle_read = PyUnicode_InternFromString("read");
}
