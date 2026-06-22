import io
import wave
from typing import Union, BinaryIO

from .SilkDecoder import SilkDecoder as SilkDecoder
from .SilkEncoder import SilkEncoder as SilkEncoder
from ._pilk import encode, decode  # type: ignore

__title__ = "pilk_nogil"
__description__ = "python silk codec binding"
__url__ = "https://github.com/MelodyYuuka/pilk-nogil"
__version__ = "0.4.2"
# noinspection SpellCheckingInspection
__author__ = "MelodyYuuka"
__author_email__ = "melody@yunmengdu.cn"
__license__ = " GPL-3.0"
__copyright__ = f"Copyright 2022~2026 {__author__}"


def get_duration(silk_path: Union[str, bytes, bytearray, BinaryIO], frame_ms: int = 20) -> int:
    """获取 silk 文件持续时间，单位：ms

    :param silk_path: str | bytes | bytearray | BinaryIO: silk 文件路径或 silk 数据
    :param frame_ms: int: 帧时长，默认 20ms
    :return: int: 持续时间（ms）
    """
    if isinstance(silk_path, (bytes, bytearray)):
        silk_data = silk_path
    elif isinstance(silk_path, str):
        with open(silk_path, "rb") as f:
            silk_data = f.read()
    elif hasattr(silk_path, "read"):
        silk_data = silk_path.read()
    else:
        raise TypeError("silk_path must be str, bytes, bytearray, or file-like object")

    pos = 0
    if len(silk_data) > 0 and silk_data[0:1] == b"\x02":
        pos = 10  # len("\x02#!SILK_V3")
    else:
        pos = 9  # len("#!SILK_V3")

    i = 0
    while pos + 2 <= len(silk_data):
        size = silk_data[pos] + silk_data[pos + 1] * 16
        pos += 2 + size
        if pos > len(silk_data):
            break
        i += 1
    return i * frame_ms


def silk_to_wav(
        silk: Union[str, bytes, bytearray, BinaryIO],
        wav: Union[str, BinaryIO, None],
        rate: int = 24000
) -> Union[None, bytes]:
    """silk 文件转 wav

    :param silk: str | bytes | bytearray | BinaryIO: silk 文件路径或 silk 数据
    :param wav: str | BinaryIO | None: wav 输出文件路径或 BytesIO 对象，None 则返回 bytes
    :param rate: int: 采样率，默认 24000
    :return: None | bytes: 如果 wav 是文件路径返回 None，如果 wav 是 None 返回 bytes
    """
    # 解码为 pcm
    pcm_data = decode(silk, None, pcm_rate=rate)

    # 生成 wav
    if wav is None:
        # 返回 wav bytes
        with io.BytesIO() as wav_buffer:
            with wave.open(wav_buffer, "wb") as f:
                # noinspection PyTypeChecker
                f.setparams((1, 2, rate, 0, "NONE", "NONE"))
                f.writeframes(pcm_data)
            return wav_buffer.getvalue()
    elif isinstance(wav, str):
        # 写入文件
        with wave.open(wav, "wb") as f:
            # noinspection PyTypeChecker
            f.setparams((1, 2, rate, 0, "NONE", "NONE"))
            f.writeframes(pcm_data)
        return None
    elif hasattr(wav, "write"):
        # 写入 BytesIO
        with wave.open(wav, "wb") as f:
            # noinspection PyTypeChecker
            f.setparams((1, 2, rate, 0, "NONE", "NONE"))
            f.writeframes(pcm_data)
        return None
    else:
        raise TypeError("wav must be str, file-like object with write(), or None")
