//
// Created by foyou on 2022/5/18.
//
#include "pilk_encode.h"

#include "memory_buffer.h"
#include "utils.h"

/*****************************/
/* Silk encoder test program */
/*****************************/

#ifdef _WIN32
#define _CRT_SECURE_NO_DEPRECATE 1
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Define codec specific settings */
#define ENCODE_MAX_BYTES_PER_FRAME 250  // Equals peak bitrate of 100 kbps
#define MAX_INPUT_FRAMES 5
#define FRAME_LENGTH_MS 20
#define MAX_API_FS_KHZ 48

#ifdef _SYSTEM_IS_BIG_ENDIAN
/* Function to convert a little endian int16 to a */
/* big endian int16 or vica verca                 */
void swap_endian(SKP_int16 vec[], /*  I/O array of */
                 SKP_int len      /*  I   length      */
) {
  SKP_int i;
  SKP_int16 tmp;
  SKP_uint8 *p1, *p2;

  for (i = 0; i < len; i++) {
    tmp = vec[i];
    p1 = (SKP_uint8*)&vec[i];
    p2 = (SKP_uint8*)&tmp;
    p1[0] = p2[1];
    p1[1] = p2[0];
  }
}
#endif

// Declare custom error
extern PyObject* PilkError;

PyObject* silk_encode(PyObject* Py_UNUSED(module), PyObject* args,
                      PyObject* keyword_args) {
  SKP_assert(0);
  static char* kwlist[] = {
      "pcm",      "silk",       "pcm_rate",    "silk_rate",   "tencent",
      "max_rate", "complexity", "packet_size", "packet_loss", "use_in_band_fec",
      "use_dtx",  NULL};

  double filetime;
  size_t counter;
  SKP_int32 k, totPackets, totActPackets, ret;
  SKP_int16 nBytes;
  double sumBytes, sumActBytes, nrg;
  SKP_uint8 payload[ENCODE_MAX_BYTES_PER_FRAME * MAX_INPUT_FRAMES];
  SKP_int16 in[FRAME_LENGTH_MS * MAX_API_FS_KHZ * MAX_INPUT_FRAMES];
  PyObject* pcm_obj;
  PyObject* silk_obj;
  FILE *bitOutFile = NULL, *speechInFile = NULL;
  SKP_int32 encSizeBytes;
  void* psEnc;
#ifdef _SYSTEM_IS_BIG_ENDIAN
  SKP_int16 nBytes_LE;
#endif
  /* default settings */
  SKP_int32 API_fs_Hz = 24000;
  SKP_int32 max_internal_fs_Hz = 0;
  SKP_int32 targetRate_bps = -1;
  SKP_int32 smplsSinceLastPacket, packetSize_ms = 20;
  SKP_int32 frameSizeReadFromFile_ms = 20;
  SKP_int32 packetLoss_perc = 0;
#if LOW_COMPLEXITY_ONLY
  SKP_int32 complexity_mode = 0;
#else
  SKP_int32 complexity_mode = 2;
#endif
  SKP_int32 DTX_enabled = 0, INBandFEC_enabled = 0, tencent = 0;
  SKP_SILK_SDK_EncControlStruct encControl;  // Struct for input to encoder
  SKP_SILK_SDK_EncControlStruct encStatus;   // Struct for status of encoder

  /* BytesIO support variables */
  int pcm_from_memory = 0;
  const char* pcm_buffer = NULL;
  Py_ssize_t pcm_size = 0;
  Py_ssize_t pcm_pos = 0;
  int silk_to_memory = 0;
  int silk_to_bytesio = 0;
  PyObject* silk_bytesio_obj = NULL;
  MemoryBuffer silk_buf;
  PyObject* pcm_read_result = NULL; /* 用于保存 BytesIO.read() 的结果 */

  /*解析参数*/
  if (!PyArg_ParseTupleAndKeywords(
          args, keyword_args, "OO|iipiiiipp", kwlist, &pcm_obj, &silk_obj,
          &API_fs_Hz, &targetRate_bps, &tencent, &max_internal_fs_Hz,
          &complexity_mode, &packetSize_ms, &packetLoss_perc,
          &INBandFEC_enabled, &DTX_enabled)) {
    return NULL;
  }

  // 处理 silk_rate
  if (targetRate_bps == -1) {
    targetRate_bps = API_fs_Hz;
  }

  /* If no max internal is specified, set to minimum of API fs and 24 kHz */
  if (max_internal_fs_Hz == 0) {
    max_internal_fs_Hz = 24000;
    if (API_fs_Hz < max_internal_fs_Hz) {
      max_internal_fs_Hz = API_fs_Hz;
    }
  }

  /* Check pcm input type */
  if (PyUnicode_Check(pcm_obj)) {
    /* File path mode */
    speechInFile = Utils_fopen(pcm_obj, "rb");
    if (speechInFile == NULL) {
      return NULL;
    }
  } else if (PyBytes_Check(pcm_obj)) {
    pcm_from_memory = 1;
    pcm_buffer = PyBytes_AS_STRING(pcm_obj);
    pcm_size = PyBytes_GET_SIZE(pcm_obj);
  } else if (PyByteArray_Check(pcm_obj)) {
    pcm_from_memory = 1;
    pcm_buffer = PyByteArray_AS_STRING(pcm_obj);
    pcm_size = PyByteArray_GET_SIZE(pcm_obj);
  } else if (PyObject_HasAttr(pcm_obj, ObjHandle_read)) {
    /* BytesIO mode: call read() to get bytes */
    pcm_read_result = PyObject_CallMethodNoArgs(pcm_obj, ObjHandle_read);
    if (pcm_read_result == NULL) {
      return NULL;
    }
    if (!PyBytes_Check(pcm_read_result)) {
      Py_DECREF(pcm_read_result);
      PyErr_SetString(PyExc_TypeError, "BytesIO.read() must return bytes");
      return NULL;
    }
    pcm_from_memory = 1;
    pcm_buffer = PyBytes_AS_STRING(pcm_read_result);
    pcm_size = PyBytes_GET_SIZE(pcm_read_result);
  } else {
    PyErr_SetString(
        PyExc_TypeError,
        "pcm must be str, bytes, bytearray, or file-like object with read()");
    return NULL;
  }

  /* Check silk output type */
  if (PyUnicode_Check(silk_obj)) {
    /* File path mode */
    bitOutFile = Utils_fopen(silk_obj, "wb");
    if (bitOutFile == NULL) {
      if (pcm_read_result) Py_DECREF(pcm_read_result);
      if (!pcm_from_memory) fclose(speechInFile);
      return NULL;
    }
  } else if (PyObject_HasAttr(silk_obj, ObjHandle_write)) {
    /* BytesIO mode */
    silk_to_bytesio = 1;
    silk_to_memory = 1;
    silk_bytesio_obj = silk_obj;
    /* Estimate initial capacity: SILK compress 10:1~15:1 from 16-bit PCM */
    Py_ssize_t estimated_size = pcm_size > 0 ? (pcm_size / 12) : 65536;
    if (estimated_size < 4096) estimated_size = 4096;
    if (memory_buffer_init(&silk_buf, estimated_size) < 0) {
      if (pcm_read_result) Py_DECREF(pcm_read_result);
      if (!pcm_from_memory) fclose(speechInFile);
      return NULL;
    }
  } else if (silk_obj == Py_None) {
    /* Return bytes mode */
    silk_to_memory = 1;
    /* Estimate initial capacity: SILK compress 10:1~15:1 from 16-bit PCM */
    Py_ssize_t estimated_size = pcm_size > 0 ? (pcm_size / 10) : 65536;
    if (estimated_size < 4096) estimated_size = 4096;
    if (memory_buffer_init(&silk_buf, estimated_size) < 0) {
      if (pcm_read_result) Py_DECREF(pcm_read_result);
      if (!pcm_from_memory) fclose(speechInFile);
      return NULL;
    }
  } else {
    PyErr_SetString(PyExc_TypeError,
                    "silk must be str, file-like object with write(), or None");
    if (pcm_read_result) Py_DECREF(pcm_read_result);
    if (!pcm_from_memory) fclose(speechInFile);
    return NULL;
  }

  Py_BEGIN_ALLOW_THREADS

  /* Add Silk header to stream */
  {
    if (tencent) {
      static const char Tencent_break[] = "\x02";
      if (silk_to_memory) {
        if (memory_buffer_write(&silk_buf, Tencent_break, 1) < 0) {
          Py_BLOCK_THREADS if (pcm_read_result) { Py_DECREF(pcm_read_result); }
          if (!pcm_from_memory) fclose(speechInFile);
          memory_buffer_free(&silk_buf);
          return NULL;
        }
      } else {
        fwrite(Tencent_break, sizeof(char), 1, bitOutFile);
      }
    }

    static const char Silk_header[] = "#!SILK_V3";
    if (silk_to_memory) {
      if (memory_buffer_write(&silk_buf, Silk_header, sizeof(Silk_header) - 1) <
          0) {
        Py_BLOCK_THREADS if (pcm_read_result) { Py_DECREF(pcm_read_result); }
        if (!pcm_from_memory) fclose(speechInFile);
        memory_buffer_free(&silk_buf);
        return NULL;
      }
    } else {
      fwrite(Silk_header, sizeof(char), sizeof(Silk_header) - 1, bitOutFile);
    }
  }

  /* Create Encoder */
  ret = SKP_Silk_SDK_Get_Encoder_Size(&encSizeBytes);
  if (ret) {
    Py_BLOCK_THREADS if (pcm_read_result) { Py_DECREF(pcm_read_result); }
    if (!pcm_from_memory) fclose(speechInFile);
    if (!silk_to_memory)
      fclose(bitOutFile);
    else
      memory_buffer_free(&silk_buf);
    return PyErr_Format(PilkError, "Error: SKP_Silk_create_encoder returned %d",
                        ret);
  }

  psEnc = malloc(encSizeBytes);

  /* Reset Encoder */
  ret = SKP_Silk_SDK_InitEncoder(psEnc, &encStatus);
  if (ret) {
    Py_BLOCK_THREADS free(psEnc);
    if (pcm_read_result) {
      Py_DECREF(pcm_read_result);
    }
    if (!pcm_from_memory) fclose(speechInFile);
    if (!silk_to_memory)
      fclose(bitOutFile);
    else
      memory_buffer_free(&silk_buf);
    return PyErr_Format(PilkError, "Error: SKP_Silk_reset_encoder returned %d",
                        ret);
  }

  /* Set Encoder parameters */
  encControl.API_sampleRate = API_fs_Hz;
  encControl.maxInternalSampleRate = max_internal_fs_Hz;
  encControl.packetSize = (packetSize_ms * API_fs_Hz) / 1000;
  encControl.packetLossPercentage = packetLoss_perc;
  encControl.useInBandFEC = INBandFEC_enabled;
  encControl.useDTX = DTX_enabled;
  encControl.complexity = complexity_mode;
  encControl.bitRate = (targetRate_bps > 0 ? targetRate_bps : 0);

  /*校验参数*/
  if ((API_fs_Hz != 8000) && (API_fs_Hz != 12000) && (API_fs_Hz != 16000) &&
      (API_fs_Hz != 24000) && (API_fs_Hz != 32000) && (API_fs_Hz != 44100) &&
      (API_fs_Hz != 48000)) {
    Py_BLOCK_THREADS free(psEnc);
    if (pcm_read_result) {
      Py_DECREF(pcm_read_result);
    }
    if (!pcm_from_memory) fclose(speechInFile);
    if (!silk_to_memory)
      fclose(bitOutFile);
    else
      memory_buffer_free(&silk_buf);
    PyErr_SetString(PyExc_ValueError,
                    "SKP_SILK_ENC_FS_NOT_SUPPORTED: pcm_rate must be in [8000, "
                    "12000, 16000, 24000, 32000, 44100, 48000]");
    return NULL;
  }

  if ((max_internal_fs_Hz != 8000) && (max_internal_fs_Hz != 12000) &&
      (max_internal_fs_Hz != 16000) && (max_internal_fs_Hz != 24000)) {
    Py_BLOCK_THREADS free(psEnc);
    if (pcm_read_result) {
      Py_DECREF(pcm_read_result);
    }
    if (!pcm_from_memory) fclose(speechInFile);
    if (!silk_to_memory)
      fclose(bitOutFile);
    else
      memory_buffer_free(&silk_buf);
    PyErr_SetString(PyExc_ValueError,
                    "SKP_SILK_ENC_FS_NOT_SUPPORTED: max_rate must be in [8000, "
                    "12000, 16000, 24000]");
    return NULL;
  }

  totPackets = 0;
  totActPackets = 0;
  sumBytes = 0.0;
  sumActBytes = 0.0;
  smplsSinceLastPacket = 0;

  while (1) {
    /* Read input from file or memory */
    if (pcm_from_memory) {
      SKP_int32 bytes_to_read =
          (frameSizeReadFromFile_ms * API_fs_Hz) / 1000 * sizeof(SKP_int16);
      if (pcm_pos + bytes_to_read > pcm_size) {
        break;
      }
      memcpy(in, pcm_buffer + pcm_pos, bytes_to_read);
      counter = bytes_to_read / sizeof(SKP_int16);
      pcm_pos += bytes_to_read;
    } else {
      counter =
          fread(in, sizeof(SKP_int16),
                (frameSizeReadFromFile_ms * API_fs_Hz) / 1000, speechInFile);
#ifdef _SYSTEM_IS_BIG_ENDIAN
      swap_endian(in, counter);
#endif
    }

    if ((SKP_int)counter < ((frameSizeReadFromFile_ms * API_fs_Hz) / 1000)) {
      break;
    }

    /* max payload size */
    nBytes = ENCODE_MAX_BYTES_PER_FRAME * MAX_INPUT_FRAMES;

    /* Silk Encoder */
    ret = SKP_Silk_SDK_Encode(psEnc, &encControl, in, (SKP_int16)counter,
                              payload, &nBytes);
    if (ret) {
      Py_BLOCK_THREADS free(psEnc);
      if (pcm_read_result) {
        Py_DECREF(pcm_read_result);
      }
      if (!pcm_from_memory) fclose(speechInFile);
      if (!silk_to_memory)
        fclose(bitOutFile);
      else
        memory_buffer_free(&silk_buf);
      return PyErr_Format(PilkError,
                          "SKP_Silk_Encode returned %d, pcm file error.", ret);
    }

    /* Get packet size */
    packetSize_ms = (SKP_int)((1000 * (SKP_int32)encControl.packetSize) /
                              encControl.API_sampleRate);

    smplsSinceLastPacket += (SKP_int)counter;

    if (((1000 * smplsSinceLastPacket) / API_fs_Hz) == packetSize_ms) {
      /* Sends a dummy zero size packet in case of DTX period  */
      /* to make it work with the decoder test program.        */
      /* In practice should be handled by RTP sequence numbers */
      totPackets++;
      sumBytes += nBytes;
      nrg = 0.0;
      for (k = 0; k < (SKP_int)counter; k++) {
        nrg += in[k] * (double)in[k];
      }
      if ((nrg / (SKP_int)counter) > 1e3) {
        sumActBytes += nBytes;
        totActPackets++;
      }

      /* Write payload size */
#ifdef _SYSTEM_IS_BIG_ENDIAN
      nBytes_LE = nBytes;
      swap_endian(&nBytes_LE, 1);
      if (silk_to_memory) {
        if (memory_buffer_write(&silk_buf, &nBytes_LE, sizeof(SKP_int16)) < 0) {
          Py_BLOCK_THREADS free(psEnc);
          if (pcm_read_result) {
            Py_DECREF(pcm_read_result);
          }
          if (!pcm_from_memory) fclose(speechInFile);
          memory_buffer_free(&silk_buf);
          return NULL;
        }
      } else {
        fwrite(&nBytes_LE, sizeof(SKP_int16), 1, bitOutFile);
      }
#else
      if (silk_to_memory) {
        if (memory_buffer_write(&silk_buf, &nBytes, sizeof(SKP_int16)) < 0) {
          Py_BLOCK_THREADS free(psEnc);
          if (pcm_read_result) {
            Py_DECREF(pcm_read_result);
          }
          if (!pcm_from_memory) fclose(speechInFile);
          memory_buffer_free(&silk_buf);
          return NULL;
        }
      } else {
        fwrite(&nBytes, sizeof(SKP_int16), 1, bitOutFile);
      }
#endif

      /* Write payload */
      if (silk_to_memory) {
        if (memory_buffer_write(&silk_buf, payload, nBytes) < 0) {
          Py_BLOCK_THREADS free(psEnc);
          if (pcm_read_result) {
            Py_DECREF(pcm_read_result);
          }
          if (!pcm_from_memory) fclose(speechInFile);
          memory_buffer_free(&silk_buf);
          return NULL;
        }
      } else {
        fwrite(payload, sizeof(SKP_uint8), nBytes, bitOutFile);
      }

      smplsSinceLastPacket = 0;
    }
  }

  /* Write dummy because it can not end with 0 bytes */
  nBytes = -1;

  /* Write payload size */
  if (!tencent) {
    if (silk_to_memory) {
      if (memory_buffer_write(&silk_buf, &nBytes, sizeof(SKP_int16)) < 0) {
        Py_BLOCK_THREADS free(psEnc);
        if (pcm_read_result) {
          Py_DECREF(pcm_read_result);
        }
        if (!pcm_from_memory) fclose(speechInFile);
        memory_buffer_free(&silk_buf);
        return NULL;
      }
    } else {
      fwrite(&nBytes, sizeof(SKP_int16), 1, bitOutFile);
    }
  }

  /* Free Encoder */
  free(psEnc);

  /* Close files */
  if (!pcm_from_memory) {
    fclose(speechInFile);
  }
  if (!silk_to_memory) {
    fclose(bitOutFile);
  }

  filetime = totPackets * 1e-3 * packetSize_ms;

  Py_END_ALLOW_THREADS

      /* Clean up pcm_read_result */
      if (pcm_read_result) {
    Py_DECREF(pcm_read_result);
  }

  /* Handle output */
  if (silk_to_bytesio) {
    /* Write to BytesIO */
    bool write_result =
        memory_buffer_append_to_bytesio(silk_bytesio_obj, &silk_buf);
    if (write_result == false) {
      return NULL;
    }
    return PyLong_FromDouble(filetime);
  } else if (silk_to_memory) {
    /* Return bytes */
    PyObject* result = memory_buffer_to_bytes(&silk_buf);
    memory_buffer_free(&silk_buf);
    return result;
  } else {
    /* Return duration */
    return PyLong_FromDouble(filetime);
  }
}
