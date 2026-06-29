"""
智能助盲眼镜 v7.0 · 距离融合器
===============================
物理布局: 超声波(上方) + OpenMV(中部) + ToF(下方)
ToF 照地面(近距精测)，超声照前方(远距+头顶)。

v7.0 改造:
  - 不再数值加权平均。按场景分工: ToF 管地面(<200cm)，超声管前方(>50cm)
  - 暴露 forward_dist / ground_dist 供决策层按场景选用
  - 双传感器数据保留独立置信度

v6 保留:
  - ToF 自适应基线校准
  - EMA 预测回退 (双传感器全挂时)
"""


class ToFCalibrator:
    """ToF 自适应基线校准器。"""

    def __init__(self, cfg):
        self.cfg = cfg
        self._window = []
        self._baseline = 0.0
        self._initialized = False

    def calibrate(self, raw_cm, tof_confidence, frame_count):
        cfg = self.cfg
        if raw_cm <= 0:
            return raw_cm

        if frame_count < cfg.TOF_CALIB_FRAMES:
            if tof_confidence >= 0.8 and 1 <= raw_cm <= 500:
                self._window.append(raw_cm)
                if len(self._window) > cfg.TOF_CALIB_WINDOW:
                    self._window.pop(0)
                self._baseline = min(self._window)
            if not self._initialized and len(self._window) >= cfg.TOF_CALIB_WINDOW // 2:
                self._initialized = True
            return raw_cm

        if tof_confidence >= 0.8 and 1 <= raw_cm <= 500:
            self._window.append(raw_cm)
            if len(self._window) > cfg.TOF_CALIB_WINDOW:
                self._window.pop(0)
            window_min = min(self._window)
            if self._baseline > 0:
                self._baseline += cfg.TOF_CALIB_EMA_ALPHA * (window_min - self._baseline)
            else:
                self._baseline = window_min

        offset = min(self._baseline, cfg.TOF_CALIB_MAX_OFFSET)
        calibrated = raw_cm - offset
        return max(1.0, calibrated)

    @property
    def baseline(self):
        return self._baseline

    @property
    def ready(self):
        return self._initialized


class DistanceFusion:
    """ToF(地面) + 超声波(前方) 分工融合 + 中值滤波 + 预测回退"""

    def __init__(self, cfg, sensor_reader):
        self.cfg = cfg
        self.sr = sensor_reader
        self.dist_buffer = [300] * cfg.DIST_BUFFER_SIZE
        self.last_distance = 300

        self.fusion_quality = 1.0
        self.used_fallback = False

        # 分工后的输出
        self.forward_dist = 300.0   # 超声波: 前方距离
        self.ground_dist = 200.0    # ToF: 地面距离
        self.overhead_risk = False  # 超声波 < 80cm → 头顶危险

        self._tof_calib = ToFCalibrator(cfg)
        self._frame_count = 0
        self.tof_baseline = 0.0

    def fuse(self):
        """
        按场景分工选择距离值 (非数值加权平均)。
        返回综合距离用于通用决策 (坑洞/障碍物等)。
        """
        sr = self.sr
        cfg = self.cfg
        self._frame_count += 1

        tc = sr.tof_confidence
        uc = sr.us_confidence

        # ToF 基线校准
        tof_raw = sr.tof_distance
        tof = self._tof_calib.calibrate(tof_raw, tc, self._frame_count)
        self.tof_baseline = self._tof_calib.baseline

        t_ok = sr.tof_valid and tof > 0 and tc > 0.0
        u_ok = sr.us_valid and sr.us_distance > 0 and uc > 0.0

        # ── 更新分工输出 ──
        if t_ok:
            self.ground_dist = tof
        if u_ok:
            self.forward_dist = sr.us_distance
            self.overhead_risk = (sr.us_distance < 80)
        else:
            self.overhead_risk = False

        # ── 综合距离选择 ──
        # 场景1: 双传感器可用
        if t_ok and u_ok:
            # 近距 (<200cm): 地面危险优先，用 ToF
            if tof <= 200:
                self.fusion_quality = tc
                self.used_fallback = False
                return tof
            # 远距: 前方障碍优先，用超声
            else:
                self.fusion_quality = uc
                self.used_fallback = False
                return sr.us_distance

        # 场景2: 仅 ToF
        if t_ok:
            self.fusion_quality = tc
            self.used_fallback = False
            return tof

        # 场景3: 仅超声
        if u_ok:
            self.fusion_quality = uc
            self.used_fallback = False
            return sr.us_distance

        # 场景4: 全挂 → EMA 预测
        fallback = sr.predictive_distance()
        if fallback is not None:
            self.fusion_quality = 0.3
            self.used_fallback = True
            return fallback

        self.fusion_quality = 0.0
        self.used_fallback = False
        return -1

    def update(self, fused_dist):
        """中值滤波 + 更新 last_distance"""
        if fused_dist > 0:
            self.dist_buffer.append(fused_dist)
            self.dist_buffer.pop(0)
        avg = self._median(self.dist_buffer)
        if avg > 0:
            self.last_distance = avg
        return avg

    @staticmethod
    def _median(buf):
        s = sorted(buf)
        return s[len(s) // 2]
