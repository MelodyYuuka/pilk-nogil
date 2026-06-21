"""silk 解码"""
from typing import Union, BinaryIO
from typing_extensions import Literal

from ._pilk import decode  # type: ignore

PCM_RATE = Literal[
    # pcm data rate 可选值
    8000, 12000, 16000, 24000, 32000, 44100, 48000
]

# Uplink loss estimate, in percent (0-100); default: 0
PACKET_LOSS = int


class SilkDecoder:
    """..."""

    def __init__(self, pcm_rate: PCM_RATE = 24000, packet_loss: PACKET_LOSS = 0):
        self.pcm_rate = pcm_rate
        self.packet_loss = packet_loss

    def decode(self, silk: Union[str, bytes, bytearray, BinaryIO], pcm: Union[str, BinaryIO, None]) -> Union[float, bytes]:
        """silk 解码

        :param silk: str | bytes | bytearray | BinaryIO: silk 输入文件路径或 silk 数据
        :param pcm: str | BinaryIO | None: 输出保存 pcm 的文件路径或 BytesIO 对象，None 则返回 bytes
        :return: float | bytes: pcm 文件持续时间 或 pcm 数据（当 pcm=None 时）
        """
        return decode(silk, pcm, pcm_rate=self.pcm_rate, packet_loss=self.packet_loss)
