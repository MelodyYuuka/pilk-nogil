from typing import Union, BinaryIO, overload
import os

@overload
def encode(
    pcm: Union[str, os.PathLike, bytes, bytearray, BinaryIO],
    silk: Union[str, os.PathLike, BinaryIO],
    pcm_rate: int = 24000,
    silk_rate: int = None,
    tencent: bool = False,
    max_rate: int = 24000,
    complexity: int = 2,
    packet_size: int = 20,
    packet_loss: int = 0,
    use_in_band_fec: bool = False,
    use_dtx: bool = False,
) -> float: ...

@overload
def encode(
    pcm: Union[str, os.PathLike, bytes, bytearray, BinaryIO],
    silk: None,
    pcm_rate: int = 24000,
    silk_rate: int = None,
    tencent: bool = False,
    max_rate: int = 24000,
    complexity: int = 2,
    packet_size: int = 20,
    packet_loss: int = 0,
    use_in_band_fec: bool = False,
    use_dtx: bool = False,
) -> bytes: ...

def encode(
    pcm: Union[str, os.PathLike, bytes, bytearray, BinaryIO],
    silk: Union[str, os.PathLike, BinaryIO, None],
    pcm_rate: int = 24000,
    silk_rate: int = None,
    tencent: bool = False,
    max_rate: int = 24000,
    complexity: int = 2,
    packet_size: int = 20,
    packet_loss: int = 0,
    use_in_band_fec: bool = False,
    use_dtx: bool = False,
) -> Union[float, bytes]:
    """silk 编码

    :param pcm: str | os.PathLike | bytes | bytearray | BinaryIO: pcm 输入文件路径或 pcm 数据
    :param silk: str | os.PathLike | BinaryIO | None: 输出保存 silk 的文件路径或 BytesIO 对象，None 则返回 bytes
    :param pcm_rate: int(hz)[可选]: 输入 pcm 文件的 rate, 默认: 24000. 可选值为 [8000, 12000, 16000, 24000, 32000, 44100, 48000]
    :param silk_rate: int(8000~48000 hz)[可选]: 默认: 与 pcm_rate 一致,输出 silk 文件的 rate
    :param tencent: bool[可选]: 默认: False, 是否兼容腾讯系语音
    :param max_rate: int[可选]: 默认: 24000, 最大内部采样率, 可选值为 [8000, 12000, 16000, 24000]
    :param complexity: int[可选]: Set complexity, 0: low, 1: medium, 2: high; default: 2
    :param packet_size: Packet interval in ms, default: 20
    :param packet_loss: Uplink loss estimate, in percent (0-100); default: 0
    :param use_in_band_fec: Enable inband FEC usage (0/1); default: 0
    :param use_dtx: Enable DTX (0/1); default: 0
    :return: float | bytes: silk文件持续时间 或 silk 数据（当 silk=None 时）
    """

@overload
def decode(
    silk: Union[str, os.PathLike, bytes, bytearray, BinaryIO],
    pcm: Union[str, os.PathLike, BinaryIO],
    pcm_rate: int = 24000,
    packet_loss: int = 0,
) -> float: ...

@overload
def decode(
    silk: Union[str, os.PathLike, bytes, bytearray, BinaryIO],
    pcm: None,
    pcm_rate: int = 24000,
    packet_loss: int = 0,
) -> bytes: ...

def decode(
    silk: Union[str, os.PathLike, bytes, bytearray, BinaryIO],
    pcm: Union[str, os.PathLike, BinaryIO, None],
    pcm_rate: int = 24000,
    packet_loss: int = 0,
) -> Union[float, bytes]:
    """silk 解码

    :param silk: str | os.PathLike | bytes | bytearray | BinaryIO: silk 输入文件路径或 silk 数据
    :param pcm: str | BinaryIO | None: 输出保存 pcm 的文件路径或 BytesIO 对象，None 则返回 bytes
    :param pcm_rate: int(hz)[可选]: 输入 pcm 文件的 rate, 默认: 24000. 可选值为 [8000, 12000, 16000, 24000, 32000, 44100, 48000]
    :param packet_loss: Uplink loss estimate, in percent (0-100); default: 0
    :return: float | bytes: pcm 文件持续时间 或 pcm 数据（当 pcm=None 时）
    """
