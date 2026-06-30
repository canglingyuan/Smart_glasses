"""
时序滤波器 — 多帧多数投票 + 迟滞消抖。
用于所有视觉检测结果的帧间一致性过滤。
"""
class TemporalFilter:
    """多数投票 + 迟滞时间滤波器 — 连续多帧确认，防单帧误检。
    参数:
        history_size: 环形缓冲大小 (默认5帧)
        confirm_ratio: 确认阈值比例 (0.4=40%以上帧命中即输出)
        hysteresis: 切换迟滞帧数 (默认3帧)
    """

    def __init__(self, history_size=5, confirm_ratio=0.6, hysteresis=3):
        self.history = [None] * history_size
        self.idx = 0
        self.size = history_size
        self.confirm_ratio = confirm_ratio
        self.hysteresis = hysteresis
        self.current = None
        self.hyst_count = 0

    def _majority(self):
        """找 history 中出现最多的非 None 值。
        返回: (best_value, count, ratio) 或 (None, 0, 0.0)"""
        valid = [v for v in self.history if v is not None]
        if not valid:
            return None, 0, 0.0
        counts = {}
        for v in valid:
            counts[v] = counts.get(v, 0) + 1
        if not counts:  # 防御空 dict (理论上不会触发)
            return None, 0, 0.0
        best = max(counts, key=counts.get)
        best_count = counts[best]
        ratio = best_count / self.size
        return best, best_count, ratio

    def update(self, raw_value, raw_conf=1.0):
        """
        输入原始检测结果，返回 (滤波结果, 置信度)。
        raw_value=None 表示本帧未检测到。
        """
        self.history[self.idx] = raw_value
        self.idx = (self.idx + 1) % self.size

        best, count, ratio = self._majority()

        if best is None:
            self.current = None
            self.hyst_count = 0
            return None, 0.0

        # 迟滞: 当前有输出时，不轻易切换
        if self.current is not None and best != self.current:
            self.hyst_count += 1
            if self.hyst_count < self.hysteresis:
                return self.current, ratio * 0.5
            self.hyst_count = 0

        # 确认: 比例达标才输出
        if ratio >= self.confirm_ratio:
            self.current = best
            self.hyst_count = 0
            return best, min(1.0, ratio * raw_conf)
        elif self.current is not None:
            return self.current, ratio * 0.4
        else:
            return None, 0.0