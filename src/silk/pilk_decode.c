//
// Created by foyou on 2022/5/18.
//

#include "pilk_decode.h"

#include "memory_buffer.h"
#include "utils.h"

/*****************************/
/* Silk decoder test program */
/*****************************/

#include "SKP_Silk_SigProc_FIX.h"

/* Define codec specific settings should be moved to h file */
#define DECODE_MAX_BYTES_PER_FRAME 1024
#define MAX_INPUT_FRAMES 5
#define MAX_FRAME_LENGTH 480
#define FRAME_LENGTH_MS 20
#define MAX_API_FS_KHZ 48
#define MAX_LBRR_DELAY 2

/* SILK header constants */
static const char SILK_HEADER[] = "!SILK_V3";
static const char SILK_HEADER_TENCENT[] = "#!SILK_V3";

/* Seed for the random number generator, which is used for simulating packet
 * loss */
static SKP_int32 rand_seed = 1;

// Declare custom error
extern PyObject* PilkError;

PyObject* silk_decode(PyObject* Py_UNUSED(module), PyObject* args,
                      PyObject* keyword_args) {
  static char* kwlist[] = {"silk", "pcm", "pcm_rate", "packet_loss", NULL};

  double filetime;
  size_t counter;
  SKP_int32 totPackets, i, k;
  SKP_int16 ret, len, tot_len;
  SKP_int16 nBytes;
  SKP_uint8 payload[DECODE_MAX_BYTES_PER_FRAME * MAX_INPUT_FRAMES *
                    (MAX_LBRR_DELAY + 1)];
  SKP_uint8 *payloadEnd = NULL, *payloadToDec = NULL;
  SKP_uint8 FECpayload[DECODE_MAX_BYTES_PER_FRAME * MAX_INPUT_FRAMES],
      *payloadPtr;
  SKP_int16 nBytesFEC;
  SKP_int16 nBytesPerPacket[MAX_LBRR_DELAY + 1], totBytes;
  SKP_int16 out[((FRAME_LENGTH_MS * MAX_API_FS_KHZ) << 1) * MAX_INPUT_FRAMES],
      *outPtr;
  PyObject* silk_obj;
  PyObject* pcm_obj;
  FILE *bitInFile = NULL, *speechOutFile = NULL;
  SKP_int32 packetSize_ms = 0, API_Fs_Hz = 0;
  SKP_int32 decSizeBytes;
  void* psDec;
  SKP_float loss_prob;
  SKP_int32 frames, lost;
  SKP_SILK_SDK_DecControlStruct DecControl;

  /* BytesIO support variables */
  int silk_from_memory = 0;
  const char* silk_buffer = NULL;
  Py_ssize_t silk_size = 0;
  Py_ssize_t silk_pos = 0;
  int pcm_to_memory = 0;
  int pcm_to_bytesio = 0;
  PyObject* pcm_bytesio_obj = NULL;
  MemoryBuffer pcm_buf;
  PyObject* silk_read_result = NULL; /* 用于保存 BytesIO.read() 的结果 */

  /* default settings */
  loss_prob = 0.0f;

  /* get arguments */
  if (!PyArg_ParseTupleAndKeywords(args, keyword_args, "OO|ii", kwlist,
                                   &silk_obj, &pcm_obj, &API_Fs_Hz, &lost)) {
    return NULL;
  }

  /* Check silk input type */
  if (PyUnicode_Check(silk_obj)) {
    /* File path mode */
    bitInFile = Utils_fopen(silk_obj, "rb");
    if (bitInFile == NULL) {
      return NULL;
    }
  } else if (PyBytes_Check(silk_obj)) {
    silk_from_memory = 1;
    silk_buffer = PyBytes_AS_STRING(silk_obj);
    silk_size = PyBytes_GET_SIZE(silk_obj);
  } else if (PyByteArray_Check(silk_obj)) {
    silk_from_memory = 1;
    silk_buffer = PyByteArray_AS_STRING(silk_obj);
    silk_size = PyByteArray_GET_SIZE(silk_obj);
  } else if (PyObject_HasAttr(silk_obj, ObjHandle_read)) {
    /* BytesIO mode: call read() to get bytes */
    silk_read_result = PyObject_CallMethodNoArgs(silk_obj, ObjHandle_read);
    if (silk_read_result == NULL) {
      return NULL;
    }
    if (!PyBytes_Check(silk_read_result)) {
      Py_DECREF(silk_read_result);
      PyErr_SetString(PyExc_TypeError, "BytesIO.read() must return bytes");
      return NULL;
    }
    silk_from_memory = 1;
    silk_buffer = PyBytes_AS_STRING(silk_read_result);
    silk_size = PyBytes_GET_SIZE(silk_read_result);
  } else {
    PyErr_SetString(
        PyExc_TypeError,
        "silk must be str, bytes, bytearray, or file-like object with read()");
    return NULL;
  }

  /* Check Silk header */
  {
    char header_buf[50];
    if (silk_from_memory) {
      if (silk_size < 1) {
        if (silk_read_result) Py_DECREF(silk_read_result);
        return PyErr_Format(PilkError, "Error: silk data too short");
      }
      header_buf[0] = silk_buffer[0];
      silk_pos = 1;
    } else {
      fread(header_buf, sizeof(char), 1, bitInFile);
    }
    header_buf[1] = '\0'; /* Terminate with a null character */
    if (strcmp(header_buf, "\x02") != 0) {
      /* Not Tencent format, read standard header */
      if (silk_from_memory) {
        if (silk_pos + (Py_ssize_t)(sizeof(SILK_HEADER) - 1) > silk_size) {
          if (silk_read_result) Py_DECREF(silk_read_result);
          return PyErr_Format(PilkError, "Error: silk data too short");
        }
        memcpy(header_buf, silk_buffer + silk_pos, sizeof(SILK_HEADER) - 1);
        silk_pos += sizeof(SILK_HEADER) - 1;
      } else {
        counter =
            fread(header_buf, sizeof(char), sizeof(SILK_HEADER) - 1, bitInFile);
      }
      header_buf[sizeof(SILK_HEADER) - 1] =
          '\0'; /* Terminate with a null character */
      if (strcmp(header_buf, SILK_HEADER) != 0) {
        /* Non-equal strings */
        if (silk_read_result) Py_DECREF(silk_read_result);
        if (!silk_from_memory) fclose(bitInFile);
        return PyErr_Format(PilkError, "Error: Wrong Header %s", header_buf);
      }
    } else {
      /* Tencent format */
      if (silk_from_memory) {
        if (silk_pos + (Py_ssize_t)(sizeof(SILK_HEADER_TENCENT) - 1) >
            silk_size) {
          if (silk_read_result) Py_DECREF(silk_read_result);
          return PyErr_Format(PilkError, "Error: silk data too short");
        }
        memcpy(header_buf, silk_buffer + silk_pos,
               sizeof(SILK_HEADER_TENCENT) - 1);
        silk_pos += sizeof(SILK_HEADER_TENCENT) - 1;
      } else {
        counter = fread(header_buf, sizeof(char),
                        sizeof(SILK_HEADER_TENCENT) - 1, bitInFile);
      }
      header_buf[sizeof(SILK_HEADER_TENCENT) - 1] =
          '\0'; /* Terminate with a null character */
      if (strcmp(header_buf, SILK_HEADER_TENCENT) != 0) {
        /* Non-equal strings */
        if (silk_read_result) Py_DECREF(silk_read_result);
        if (!silk_from_memory) fclose(bitInFile);
        return PyErr_Format(PilkError, "Error: Wrong Header %s", header_buf);
      }
    }
  }

  /* Check pcm output type */
  if (PyUnicode_Check(pcm_obj)) {
    /* File path mode */
    speechOutFile = Utils_fopen(pcm_obj, "wb");
    if (speechOutFile == NULL) {
      if (silk_read_result) Py_DECREF(silk_read_result);
      if (!silk_from_memory) fclose(bitInFile);
      return NULL;
    }
  } else if (PyObject_HasAttr(pcm_obj, ObjHandle_write)) {
    /* BytesIO mode */
    pcm_to_bytesio = 1;
    pcm_to_memory = 1;
    pcm_bytesio_obj = pcm_obj;
    /* Estimate initial capacity: PCM is ~12x larger than SILK */
    Py_ssize_t estimated_size = silk_size > 0 ? (silk_size * 15) : 65536;
    if (estimated_size < 4096) estimated_size = 4096;
    if (estimated_size > 10 * 1024 * 1024)
      estimated_size = 10 * 1024 * 1024; /* Max 10MB */
    if (memory_buffer_init(&pcm_buf, estimated_size) < 0) {
      if (silk_read_result) Py_DECREF(silk_read_result);
      if (!silk_from_memory) fclose(bitInFile);
      return NULL;
    }
  } else if (pcm_obj == Py_None) {
    /* Return bytes mode */
    pcm_to_memory = 1;
    /* Estimate initial capacity: PCM is ~12x larger than SILK */
    Py_ssize_t estimated_size = silk_size > 0 ? (silk_size * 12) : 65536;
    if (estimated_size < 4096) estimated_size = 4096;
    if (estimated_size > 10 * 1024 * 1024)
      estimated_size = 10 * 1024 * 1024; /* Max 10MB */
    if (memory_buffer_init(&pcm_buf, estimated_size) < 0) {
      if (silk_read_result) Py_DECREF(silk_read_result);
      if (!silk_from_memory) fclose(bitInFile);
      return NULL;
    }
  } else {
    PyErr_SetString(PyExc_TypeError,
                    "pcm must be str, file-like object with write(), or None");
    if (silk_read_result) Py_DECREF(silk_read_result);
    if (!silk_from_memory) fclose(bitInFile);
    return NULL;
  }

  Py_BEGIN_ALLOW_THREADS

      /* Set the samplingrate that is requested for the output */
      if (API_Fs_Hz == 0) {
    DecControl.API_sampleRate = 24000;
  }
  else {
    DecControl.API_sampleRate = API_Fs_Hz;
  }

  /* Initialize to one frame per packet, for proper concealment before first
   * packet arrives */
  DecControl.framesPerPacket = 1;

  /* Create decoder */
  SKP_Silk_SDK_Get_Decoder_Size(&decSizeBytes);

  psDec = malloc(decSizeBytes);

  /* Reset decoder */
  SKP_Silk_SDK_InitDecoder(psDec);

  totPackets = 0;
  payloadEnd = payload;

  /* Simulate the jitter buffer holding MAX_FEC_DELAY packets */
  for (i = 0; i < MAX_LBRR_DELAY; i++) {
    /* Read payload size */
    if (silk_from_memory) {
      if (silk_pos + (Py_ssize_t)sizeof(SKP_int16) > silk_size) {
        break;
      }
      memcpy(&nBytes, silk_buffer + silk_pos, sizeof(SKP_int16));
      silk_pos += sizeof(SKP_int16);
      counter = 1;
    } else {
      counter = fread(&nBytes, sizeof(SKP_int16), 1, bitInFile);
    }
#ifdef _SYSTEM_IS_BIG_ENDIAN
    swap_endian(&nBytes, 1);
#endif
    /* Read payload */
    if (silk_from_memory) {
      if (silk_pos + nBytes > silk_size) {
        break;
      }
      memcpy(payloadEnd, silk_buffer + silk_pos, nBytes);
      silk_pos += nBytes;
      counter = nBytes;
    } else {
      counter = fread(payloadEnd, sizeof(SKP_uint8), nBytes, bitInFile);
    }

    if ((SKP_int16)counter < nBytes) {
      break;
    }
    nBytesPerPacket[i] = nBytes;
    payloadEnd += nBytes;
    totPackets++;
  }

  while (1) {
    /* Read payload size */
    if (silk_from_memory) {
      if (silk_pos + (Py_ssize_t)sizeof(SKP_int16) > silk_size) {
        break;
      }
      memcpy(&nBytes, silk_buffer + silk_pos, sizeof(SKP_int16));
      silk_pos += sizeof(SKP_int16);
      counter = 1;
    } else {
      counter = fread(&nBytes, sizeof(SKP_int16), 1, bitInFile);
    }
#ifdef _SYSTEM_IS_BIG_ENDIAN
    swap_endian(&nBytes, 1);
#endif
    if (nBytes < 0 || counter < 1) {
      break;
    }

    /* Read payload */
    if (silk_from_memory) {
      if (silk_pos + nBytes > silk_size) {
        break;
      }
      memcpy(payloadEnd, silk_buffer + silk_pos, nBytes);
      silk_pos += nBytes;
      counter = nBytes;
    } else {
      counter = fread(payloadEnd, sizeof(SKP_uint8), nBytes, bitInFile);
    }
    if ((SKP_int16)counter < nBytes) {
      break;
    }

    /* Simulate losses */
    rand_seed = SKP_RAND(rand_seed);
    if ((((float)((rand_seed >> 16) + (1 << 15))) / 65535.0f >=
         (loss_prob / 100.0f)) &&
        (counter > 0)) {
      nBytesPerPacket[MAX_LBRR_DELAY] = nBytes;
      payloadEnd += nBytes;
    } else {
      nBytesPerPacket[MAX_LBRR_DELAY] = 0;
    }

    if (nBytesPerPacket[0] == 0) {
      /* Indicate lost packet */
      lost = 1;

      /* Packet loss. Search after FEC in next packets. Should be done in the
       * jitter buffer */
      payloadPtr = payload;
      for (i = 0; i < MAX_LBRR_DELAY; i++) {
        if (nBytesPerPacket[i + 1] > 0) {
          SKP_Silk_SDK_search_for_LBRR(payloadPtr, nBytesPerPacket[i + 1],
                                       (i + 1), FECpayload, &nBytesFEC);
          if (nBytesFEC > 0) {
            payloadToDec = FECpayload;
            nBytes = nBytesFEC;
            lost = 0;
            break;
          }
        }
        payloadPtr += nBytesPerPacket[i + 1];
      }
    } else {
      lost = 0;
      nBytes = nBytesPerPacket[0];
      payloadToDec = payload;
    }

    /* Silk decoder */
    outPtr = out;
    tot_len = 0;

    if (lost == 0) {
      /* No Loss: Decode all frames in the packet */
      frames = 0;
      do {
        /* Decode 20 ms */
        SKP_Silk_SDK_Decode(psDec, &DecControl, 0, payloadToDec, nBytes, outPtr,
                            &len);

        frames++;
        outPtr += len;
        tot_len += len;
        if (frames > MAX_INPUT_FRAMES) {
          /* Hack for corrupt stream that could generate too many frames */
          outPtr = out;
          tot_len = 0;
          frames = 0;
        }
        /* Until last 20 ms frame of packet has been decoded */
      } while (DecControl.moreInternalDecoderFrames);
    } else {
      /* Loss: Decode enough frames to cover one packet duration */
      for (i = 0; i < DecControl.framesPerPacket; i++) {
        /* Generate 20 ms */
        SKP_Silk_SDK_Decode(psDec, &DecControl, 1, payloadToDec, nBytes, outPtr,
                            &len);
        outPtr += len;
        tot_len += len;
      }
    }

    packetSize_ms = tot_len / (DecControl.API_sampleRate / 1000);
    totPackets++;

    /* Write output to file or memory */
    if (pcm_to_memory) {
      if (memory_buffer_write(&pcm_buf, out, tot_len * sizeof(SKP_int16)) < 0) {
        Py_BLOCK_THREADS free(psDec);
        if (silk_read_result) {
          Py_DECREF(silk_read_result);
        }
        if (!silk_from_memory) fclose(bitInFile);
        memory_buffer_free(&pcm_buf);
        return NULL;
      }
    } else {
#ifdef _SYSTEM_IS_BIG_ENDIAN
      swap_endian(out, tot_len);
#endif
      fwrite(out, sizeof(SKP_int16), tot_len, speechOutFile);
    }

    /* Update buffer */
    totBytes = 0;
    for (i = 0; i < MAX_LBRR_DELAY; i++) {
      totBytes += nBytesPerPacket[i + 1];
    }
    /* Check if the received totBytes is valid */
    if (totBytes < 0 || totBytes > (SKP_int16)sizeof(payload)) {
      Py_BLOCK_THREADS free(psDec);
      if (silk_read_result) {
        Py_DECREF(silk_read_result);
      }
      if (!silk_from_memory) fclose(bitInFile);
      if (!pcm_to_memory)
        fclose(speechOutFile);
      else
        memory_buffer_free(&pcm_buf);
      return PyErr_Format(PilkError, "Packets decoded: %d", totPackets);
    }
    SKP_memmove(payload, &payload[nBytesPerPacket[0]],
                totBytes * sizeof(SKP_uint8));
    payloadEnd -= nBytesPerPacket[0];
    SKP_memmove(nBytesPerPacket, &nBytesPerPacket[1],
                MAX_LBRR_DELAY * sizeof(SKP_int16));
  }

  /* Empty the recieve buffer */
  for (k = 0; k < MAX_LBRR_DELAY; k++) {
    if (nBytesPerPacket[0] == 0) {
      /* Indicate lost packet */
      lost = 1;

      /* Packet loss. Search after FEC in next packets. Should be done in the
       * jitter buffer */
      payloadPtr = payload;
      for (i = 0; i < MAX_LBRR_DELAY; i++) {
        if (nBytesPerPacket[i + 1] > 0) {
          SKP_Silk_SDK_search_for_LBRR(payloadPtr, nBytesPerPacket[i + 1],
                                       (i + 1), FECpayload, &nBytesFEC);
          if (nBytesFEC > 0) {
            payloadToDec = FECpayload;
            nBytes = nBytesFEC;
            lost = 0;
            break;
          }
        }
        payloadPtr += nBytesPerPacket[i + 1];
      }
    } else {
      lost = 0;
      nBytes = nBytesPerPacket[0];
      payloadToDec = payload;
    }

    /* Silk decoder */
    outPtr = out;
    tot_len = 0;

    if (lost == 0) {
      /* No loss: Decode all frames in the packet */
      frames = 0;
      do {
        /* Decode 20 ms */
        ret = SKP_Silk_SDK_Decode(psDec, &DecControl, 0, payloadToDec, nBytes,
                                  outPtr, &len);
        if (ret) {
          printf("\nSKP_Silk_SDK_Decode returned %d", ret);
        }

        frames++;
        outPtr += len;
        tot_len += len;
        if (frames > MAX_INPUT_FRAMES) {
          /* Hack for corrupt stream that could generate too many frames */
          outPtr = out;
          tot_len = 0;
          frames = 0;
        }
        /* Until last 20 ms frame of packet has been decoded */
      } while (DecControl.moreInternalDecoderFrames);
    } else {
      /* Loss: Decode enough frames to cover one packet duration */

      /* Generate 20 ms */
      for (i = 0; i < DecControl.framesPerPacket; i++) {
        SKP_Silk_SDK_Decode(psDec, &DecControl, 1, payloadToDec, nBytes, outPtr,
                            &len);
        outPtr += len;
        tot_len += len;
      }
    }

    packetSize_ms = tot_len / (DecControl.API_sampleRate / 1000);
    totPackets++;

    /* Write output to file or memory */
    if (pcm_to_memory) {
      if (memory_buffer_write(&pcm_buf, out, tot_len * sizeof(SKP_int16)) < 0) {
        Py_BLOCK_THREADS free(psDec);
        if (silk_read_result) {
          Py_DECREF(silk_read_result);
        }
        if (!silk_from_memory) fclose(bitInFile);
        memory_buffer_free(&pcm_buf);
        return NULL;
      }
    } else {
#ifdef _SYSTEM_IS_BIG_ENDIAN
      swap_endian(out, tot_len);
#endif
      fwrite(out, sizeof(SKP_int16), tot_len, speechOutFile);
    }

    /* Update Buffer */
    totBytes = 0;
    for (i = 0; i < MAX_LBRR_DELAY; i++) {
      totBytes += nBytesPerPacket[i + 1];
    }

    /* Check if the received totBytes is valid */
    if (totBytes < 0 || totBytes > (SKP_int16)sizeof(payload)) {
      Py_BLOCK_THREADS free(psDec);
      if (silk_read_result) {
        Py_DECREF(silk_read_result);
      }
      if (!silk_from_memory) fclose(bitInFile);
      if (!pcm_to_memory)
        fclose(speechOutFile);
      else
        memory_buffer_free(&pcm_buf);
      return PyErr_Format(PilkError, "Packets decoded: %d", totPackets);
    }

    SKP_memmove(payload, &payload[nBytesPerPacket[0]],
                totBytes * sizeof(SKP_uint8));
    payloadEnd -= nBytesPerPacket[0];
    SKP_memmove(nBytesPerPacket, &nBytesPerPacket[1],
                MAX_LBRR_DELAY * sizeof(SKP_int16));
  }

  /* Free decoder */
  free(psDec);

  /* Close files */
  if (!silk_from_memory) {
    fclose(bitInFile);
  }
  if (!pcm_to_memory) {
    fclose(speechOutFile);
  }

  filetime = totPackets * 1e-3 * packetSize_ms;

  Py_END_ALLOW_THREADS

      /* Clean up silk_read_result */
      if (silk_read_result) {
    Py_DECREF(silk_read_result);
  }

  /* Handle output */
  if (pcm_to_bytesio) {
    /* Write to BytesIO */
    bool write_result =
        memory_buffer_append_to_bytesio(pcm_bytesio_obj, &pcm_buf);
    if (write_result == false) {
      return NULL;
    }
    return PyLong_FromDouble(filetime);
  } else if (pcm_to_memory) {
    /* Return bytes */
    PyObject* result = memory_buffer_to_bytes(&pcm_buf);
    memory_buffer_free(&pcm_buf);
    return result;
  } else {
    /* Return duration */
    return PyLong_FromDouble(filetime);
  }
}
