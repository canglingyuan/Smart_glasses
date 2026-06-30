"""
智能助盲眼镜 v7.0 · 传感器数据读取器
=====================================
从 UART 读取 STM32U5 发来的传感器数据包并解析。

v5 新增:
  1. 置信度衰减 (0.0~1.0) — 替代二元 OK/TIMEOUT
  2. 多级状态 (OK → STALE → DEGRADED → TIMEOUT → DEAD)
  3. 连续超时计数 — 区分"暂时中断"和"硬件故障"
  4. EMA 预测回退 — 短暂中断时用历史值填补
  5. 软投票危险分 — 供决策引擎加权评分

支持协议:
  D:<cm>           超声波距离
  TOF:<mm>         ToF 测距 (毫米)
  IMU:<ax>,<ay>,<az>,<gx>,<gy>,<gz>  6轴IMU
  BAT:<voltage>    电池电压
  WAKE             唤醒指令

注: 颜色传感器 (CLR) 已移除。

v5 新增: 未匹配行转发到 InteractionManager (ACK / NAK / BTN / ST)
"""

import time


class SensorReader:
    def __init__(self, uart, cfg):
        self.uart = uart
        self.cfg = cfg

        # ── 超声波 ──
        self.us_distance = 300
        self.us_valid = False
        self.us_last_ticks = 0
        self.us_confidence = 0.0          # ★ 置信度 0~1
        self.us_history = []              # ★ EMA 历史
        self.us_consecutive_timeouts = 0  # ★ 连续超时计数

        # ── ToF (cm) ──
        self.tof_distance = -1
        self.tof_valid = False
        self.tof_last_ticks = 0
        self.tof_confidence = 0.0         # ★
        self.tof_history = []             # ★
        self.tof_consecutive_timeouts = 0 # ★

        # ── IMU 6轴 ──
        self.imu_ax = 0.0
        self.imu_ay = 0.0
        self.imu_az = 1.0
        self.imu_gx = 0.0
        self.imu_gy = 0.0
        self.imu_gz = 0.0
        self.imu_last_ticks = 0
        self.imu_confidence = 0.0         # ★
        self.imu_consecutive_timeouts = 0 # ★

        # ── 电池 ──
        self.battery_voltage = 4.0
        self.battery_percent = 100    # ★ 由 STM32 BATPER: 直接设置
        self.battery_ok = True

        # ── 传感器多级状态 ──
        self.status = {
            'TOF': 'TIMEOUT', 'US': 'TIMEOUT', 'IMU': 'TIMEOUT',
            'BAT': 'TIMEOUT'
        }

        # ── 低功耗标记 (由外部设置) ──
        self.low_power_mode = False

        # ── ★ v5: 未匹配行转发钩子 ──
        self._line_handler = None   # callable(line) → bool

    def set_line_handler(self, handler):
        """★ v5: 设置未匹配行的转发目标 (如 InteractionManager.parse_line)"""
        self._line_handler = handler

    def read(self):
        """从 UART 读取一帧传感器数据"""
        try:
            if not self.uart.any():
                return
            buf = self.uart.read()
            if not buf:
                return
            lines = buf.decode().split('\n')
        except Exception:
            return

        now = time.ticks_ms()
        count = 0
        for line in lines:
            if count > 10:  # 最多处理10行, 防STM32狂发数据
                break
            count += 1
            line = line.strip()
            if not line:
                continue

            # ---- 超声波距离 ----
            if line.startswith('D:'):
                try:
                    val = float(line[2:])
                    if val > 0:
                        self.us_distance = val
                        self.us_valid = True
                        self.us_last_ticks = now
                except Exception:
                    pass

            # ---- ToF 测距 (mm → cm) ----
            elif line.startswith('TOF:'):
                try:
                    val = float(line[4:]) / 10.0
                    if 1 <= val <= 500:
                        self.tof_distance = val
                        self.tof_valid = True
                        self.tof_last_ticks = now
                except Exception:
                    pass

            # ---- IMU 6轴 ----
            elif line.startswith('IMU:'):
                try:
                    parts = line[4:].split(',')
                    n = len(parts)
                    if n >= 6:
                        self.imu_ax = float(parts[0])
                        self.imu_ay = float(parts[1])
                        self.imu_az = float(parts[2])
                        self.imu_gx = float(parts[3])
                        self.imu_gy = float(parts[4])
                        self.imu_gz = float(parts[5])
                    elif n >= 1:
                        self.imu_gx = float(parts[0])
                    self.imu_last_ticks = now
                except Exception:
                    pass

            # ---- 电池电压 ----
            elif line.startswith('BATPER:'):
                try:
                    pct = int(line[7:])
                    if 0 <= pct <= 100:
                        self.battery_percent = pct
                        self.battery_ok = (pct >= 10)
                        self.battery_voltage = 3.7  # 百分比模式下电压仅占位
                except Exception:
                    pass
            elif line.startswith('BAT:'):
                try:
                    self.battery_voltage = float(line[4:])
                    self.battery_ok = (
                        self.battery_voltage >= self.cfg.BAT_LOW_VOLTAGE
                    )
                except Exception:
                    pass

            # ---- 唤醒 ----
            elif line == 'WAKE':
                self.low_power_mode = False

            # ---- ★ v5: 未匹配行 → 转发到交互层 ----
            else:
                if self._line_handler is not None:
                    self._line_handler(line)

    def sensor_age_ms(self, last_ticks):
        """计算传感器数据距今的毫秒数"""
        return time.ticks_diff(time.ticks_ms(), last_ticks)

    # ==================================================================
    # ★ 置信度衰减曲线
    # ==================================================================

    def _confidence_from_age(self, age_ms):
        """
        年龄 → 置信度 (0.0 ~ 1.0)

        OK        (0 ~ 500ms):         1.0
        STALE     (500 ~ 1500ms):      1.0 → 0.5 线性衰减
        DEGRADED  (1500 ~ 2000ms):     0.5 → 0.0 线性衰减
        TIMEOUT   (> 2000ms):          0.0
        """
        cfg = self.cfg

        if age_ms <= cfg.SENSOR_STALE_MS:
            return 1.0

        if age_ms <= cfg.SENSOR_DEGRADED_MS:
            progress = (age_ms - cfg.SENSOR_STALE_MS) / (
                cfg.SENSOR_DEGRADED_MS - cfg.SENSOR_STALE_MS)
            return 1.0 - 0.5 * progress

        if age_ms <= cfg.SENSOR_TIMEOUT_MS:
            progress = (age_ms - cfg.SENSOR_DEGRADED_MS) / (
                cfg.SENSOR_TIMEOUT_MS - cfg.SENSOR_DEGRADED_MS)
            return 0.5 * (1.0 - progress)

        return 0.0

    # ==================================================================
    # ★ 多级状态判定
    # ==================================================================

    def _status_from_age(self, age_ms, has_data, consecutive_timeouts):
        """
        年龄 + 首次数据标记 + 连续超时计数 → 多级状态
        返回值: 'OK' | 'STALE' | 'DEGRADED' | 'TIMEOUT' | 'DEAD'
        """
        cfg = self.cfg

        if consecutive_timeouts >= cfg.SENSOR_DEAD_CONSECUTIVE:
            return 'DEAD'

        if not has_data:
            return 'TIMEOUT'

        if age_ms <= cfg.SENSOR_STALE_MS:
            return 'OK'
        if age_ms <= cfg.SENSOR_DEGRADED_MS:
            return 'STALE'
        if age_ms <= cfg.SENSOR_TIMEOUT_MS:
            return 'DEGRADED'

        return 'TIMEOUT'

    # ==================================================================
    # ★ 在线诊断 (软突破版)
    # ==================================================================

    def update_diagnostics(self):
        """
        更新所有传感器的置信度、多级状态、连续超时计数、历史缓冲区。
        每帧调用一次 (在 read() 之后)。
        """
        cfg = self.cfg
        now = time.ticks_ms()

        def diagnose(valid, last_ticks, consecutive_key,
                     history, value):
            """对一个传感器执行完整诊断"""
            if last_ticks == 0:
                age = 99999
            else:
                age = time.ticks_diff(now, last_ticks)

            conf = self._confidence_from_age(age)
            status = self._status_from_age(
                age, bool(last_ticks > 0),
                getattr(self, consecutive_key))

            # 连续超时计数
            if status in ('TIMEOUT', 'DEAD'):
                setattr(self, consecutive_key,
                        getattr(self, consecutive_key) + 1)
            else:
                setattr(self, consecutive_key, 0)

            # 历史缓冲区 (仅新鲜数据)
            if status in ('OK', 'STALE') and value is not None and value > 0:
                history.append(value)
                if len(history) > cfg.CONFIDENCE_HISTORY_SIZE:
                    history.pop(0)

            return conf, status

        self.tof_confidence, self.status['TOF'] = diagnose(
            self.tof_valid, self.tof_last_ticks,
            'tof_consecutive_timeouts', self.tof_history, self.tof_distance)

        self.us_confidence, self.status['US'] = diagnose(
            self.us_valid, self.us_last_ticks,
            'us_consecutive_timeouts', self.us_history, self.us_distance)

        self.imu_confidence, self.status['IMU'] = diagnose(
            True, self.imu_last_ticks,
            'imu_consecutive_timeouts', [], self.imu_ax)

        self.status['BAT'] = 'OK' if self.battery_voltage > 0 else 'TIMEOUT'

    # ==================================================================
    # ★ 预测回退 (EMA)
    # ==================================================================

    def _predictive_fallback(self, history):
        """
        指数加权移动平均预测。
        越新值权重越高 (alpha=0.6)。
        返回 None 如果没有任何历史数据。
        """
        if not history:
            return None
        if len(history) == 1:
            return history[0]

        alpha = self.cfg.CONFIDENCE_EMA_ALPHA
        val = history[-1]
        weight_sum = 1.0
        weighted_sum = val
        w = 1.0 - alpha

        for v in reversed(history[:-1]):
            weighted_sum += w * v
            weight_sum += w
            w *= (1.0 - alpha)

        return weighted_sum / weight_sum if weight_sum > 0 else val

    def predictive_distance(self):
        """
        当 ToF 和超声波都不可用时，返回 EMA 预测距离。
        优先 ToF 历史（更精确），其次超声波历史。
        返回 None 表示完全无法预测。
        """
        if len(self.tof_history) >= 3:
            return self._predictive_fallback(self.tof_history)

        if len(self.us_history) >= 3:
            return self._predictive_fallback(self.us_history)

        combined = self.tof_history[-3:] + self.us_history[-3:]
        if combined:
            return self._predictive_fallback(combined)

        return None


