"""
智能助盲眼镜 v7.0 · 低功耗管理器
=================================
低功耗模式管理: 降分辨率、关LED。
"""

import sensor
from pyb import LED


class PowerManager:
    def __init__(self, cfg, cmd_sender):
        self.cfg = cfg
        self.cmd = cmd_sender
        self.active = False

    def enter(self):
        """进入低功耗模式"""
        self.active = True
        sensor.set_framesize(sensor.QQVGA)
        self.cmd.reset_dedup()
        LED(1).off()
        LED(2).off()

    def exit(self):
        """退出低功耗模式"""
        self.active = False
        sensor.set_framesize(sensor.QVGA)
        self.cmd.reset_dedup()
