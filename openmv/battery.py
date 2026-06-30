"""
智能助盲眼镜 v7.0 · 电池管理器
===============================
LiPo 1S 放电曲线 → 电池百分比 + 分级提醒。
"""


class BatteryManager:
    def __init__(self, cfg, sensor_reader):
        self.cfg = cfg
        self.sr = sensor_reader
        self.pct = 100
        self.v_buffer = [4.0] * self.cfg.VOLTAGE_BUFFER_SIZE
        self.alert_20_sent = False
        self.alert_10_sent = False
        self.alert_5_sent  = False

    def update(self):
        """返回 (百分比, 提醒指令或None)。
        STM32 发 BATPER: 时直接使用, 否则查放电曲线。"""
        if self.sr.battery_percent > 0:
            self.pct = self.sr.battery_percent
        else:
            raw_v = self.sr.battery_voltage
            self.v_buffer.append(raw_v)
            self.v_buffer.pop(0)
            smooth_v = sum(self.v_buffer) / len(self.v_buffer)
            self.pct = self._voltage_to_percent(smooth_v)

        alert_cmd = None
        if self.pct <= 5 and not self.alert_5_sent:
            alert_cmd = 'BATPER:5'
            self.alert_5_sent = True
        elif self.pct <= 10 and not self.alert_10_sent:
            alert_cmd = 'BATPER:10'
            self.alert_10_sent = True
        elif self.pct <= 20 and not self.alert_20_sent:
            alert_cmd = 'BATPER:20'
            self.alert_20_sent = True
        elif self.pct > 25:
            self.alert_20_sent = False
            self.alert_10_sent = False
            self.alert_5_sent  = False

        return self.pct, alert_cmd

    def _voltage_to_percent(self, v):
        """LiPo放电曲线 → 百分比 (线性插值)"""
        curve = self.cfg.BAT_DISCHARGE_CURVE
        if v >= curve[0][0]:
            return 100
        if v <= curve[-1][0]:
            return 0
        for i in range(len(curve) - 1):
            v1, p1 = curve[i]
            v2, p2 = curve[i + 1]
            if v2 <= v <= v1:
                ratio = (v - v2) / (v1 - v2) if v1 != v2 else 0
                return int(p2 + ratio * (p1 - p2))
        return 0
