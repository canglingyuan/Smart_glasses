"""
智能助盲眼镜 交互协议 (适配 STM32 V0.1_LED)
======================================================
STM32 V0.1_LED 固件仅支持单向指令：RED/GREEN/ZEBRA/OBSTACLE/PIT/BUMP/NONE。
不支持 ACK/NAK/BTN/ST 双向协议，不支持 BEEP/EMERG/LOWPOWER 等扩展指令。

下行 (OpenMV → STM32U5):
  RED / GREEN / ZEBRA / OBSTACLE / PIT / BUMP
  OVERHEAD / STAIRS / CROSSWALK_END / TACTILE / LEFT / RIGHT
  NONE
"""



class CommandSender:
    """指令发送器 (去重)"""

    CMD_MAP = {
        'red':       'RED',
        'green':     'GREEN',
        'crosswalk': 'ZEBRA',
        'obstacle':  'OBSTACLE',
        'obstacle_near': 'OBSTACLE_NEAR',
        'lateral':   'LATERAL',
        'pothole':   'PIT',
        'bump':      'BUMP',
        'overhead':  'OVERHEAD',
        'stairs_down': 'STAIRS_DOWN',
        'stairs_up':   'STAIRS_UP',
        'tactile_warn': 'TACTILE_WARN',
        'left':      'LEFT',
        'right':     'RIGHT',
        'clean':     'NONE',
    }

    def __init__(self, uart):
        self.uart = uart
        self.last_cmd = ""

    def send(self, cmd):
        """发送大写指令，避免连续重复"""
        if cmd != self.last_cmd:
            try:
                self.uart.write(cmd + "\n")
                self.last_cmd = cmd
                print(">> UART send:", cmd)
            except Exception:
                pass

    def send_raw(self, raw):
        """直接发送原始字符串 (不去重)。STM32 V0.1 会忽略不识别的指令。"""
        try:
            self.uart.write(raw + "\n")
        except Exception:
            pass

    def reset_dedup(self):
        """重置去重状态"""
        self.last_cmd = ""


class InteractionManager:
    """
    交互管理器 (初赛版 — 适配 STM32 V0.1_LED)

    STM32 仅支持单向指令，无 ACK/NAK/BTN/ST。
    保留 line_handler 钩子和 send/send_raw/reset_dedup 接口。
    """

    def __init__(self, uart, cfg):
        self.uart = uart
        self.cfg = cfg
        self.sender = CommandSender(uart)

        # 统计 (仅映射指令计数)
        self.cmd_sent = 0

        # ★ 兼容旧接口：无实际 STM32 状态
        self.stm32_status = 'OK'
        self.ack_rate = 1.0

    # ==================================================================
    # 上行解析 (由 SensorReader.read() 调用)
    # ==================================================================

    def parse_line(self, line):
        """
        解析非传感器数据行。STM32 V0.1 不发送 ACK/NAK/BTN/ST，
        此方法保留为兼容接口，始终返回 False。
        """
        return False

    # ==================================================================
    # 下行发送
    # ==================================================================

    def send(self, event_type):
        """发送导航指令 (去重)"""
        cmd = CommandSender.CMD_MAP.get(event_type, 'NONE')
        self.sender.send(cmd)
        if cmd != 'NONE':
            self.cmd_sent += 1

    def send_raw(self, raw):
        """
        发送原始字符串 (不去重)。
        STM32 V0.1 只识别 RED/GREEN/ZEBRA/OBSTACLE/PIT/BUMP/NONE，
        其它指令会被静默忽略。
        """
        self.sender.send_raw(raw)
        self.cmd_sent += 1

    def reset_dedup(self):
        """重置去重状态 (模式切换时调用)"""
        self.sender.reset_dedup()

    # ==================================================================
    # 兼容旧接口 (无实际操作)
    # ==================================================================

    def update(self):
        """: STM32 不支持 ACK/NAK，此方法为空"""
        pass

    def pop_button(self):
        """: STM32 不支持 BTN 协议，始终返回 None"""
        return None
