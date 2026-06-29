"""
智能助盲眼镜 v7.0 · 误报率统计模块
==================================
滑动窗口告警记录 + 用户标记真/假 → 误报率。

用法:
  stats.record(event_type, priority, desc)     # 每次告警自动记录
  stats.mark_last_as_real()                     # 用户按键标记"真实危险"
  stats.mark_last_as_false()                    # 用户按键标记"误触发"
  total, fp_rate, report = stats.summary()      # 查询统计数据
"""

import time


class AlertStats:
    """
    滑动窗口告警统计。

    记录最近 STATS_BUFFER_SIZE 次告警，用户可在 STATS_MARK_TIMEOUT_S 秒内
    通过按键标记该告警是"真实危险"还是"误触发"。
    超时未标记的告警默认视为"真实危险"（保守假设）。
    """

    def __init__(self, cfg):
        self.cfg = cfg
        self._buffer = []               # [{type, priority, desc, tick, real}, ...]
        self._last_alert_tick = 0       # 最近一次告警的 ticks_ms
        self._marked_count = 0          # 已标记告警数

    # ==================================================================
    # 记录告警
    # ==================================================================

    def record(self, event_type, priority, desc):
        """
        记录一次告警事件。在决策引擎输出非 clean 事件时调用。

        参数:
          event_type: 事件类型 ('red', 'obstacle', 'pit', ...)
          priority:   优先级 (0~5)
          desc:       描述文本
        """
        now = time.ticks_ms()

        entry = {
            'type': event_type,
            'priority': priority,
            'desc': desc,
            'tick': now,
            'real': None,          # None=未标记, True=真实, False=误报
        }
        self._buffer.append(entry)
        self._last_alert_tick = now

        # 滑动窗口裁剪
        if len(self._buffer) > self.cfg.STATS_BUFFER_SIZE:
            removed = self._buffer.pop(0)
            if removed['real'] is not None:
                self._marked_count -= 1

    # ==================================================================
    # 用户标记
    # ==================================================================

    def _find_markable(self):
        """
        找到最近一次可标记的告警 (未标记 + 未超时)。
        返回 index 或 None。
        """
        now = time.ticks_ms()
        timeout_ms = self.cfg.STATS_MARK_TIMEOUT_S * 1000

        for i in range(len(self._buffer) - 1, -1, -1):
            entry = self._buffer[i]
            if entry['real'] is not None:
                continue
            age = time.ticks_diff(now, entry['tick'])
            if age <= timeout_ms:
                return i
        return None

    def mark_last_as_real(self):
        """标记最近一次告警为真实危险。返回 True 表示成功标记。"""
        idx = self._find_markable()
        if idx is not None:
            self._buffer[idx]['real'] = True
            self._marked_count += 1
            return True
        return False

    def mark_last_as_false(self):
        """标记最近一次告警为误触发。返回 True 表示成功标记。"""
        idx = self._find_markable()
        if idx is not None:
            self._buffer[idx]['real'] = False
            self._marked_count += 1
            return True
        return False

    # ==================================================================
    # 统计查询
    # ==================================================================

    def summary(self):
        """
        返回: (total, marked, tp, fp, fp_rate, report_text)

        total:   总告警数
        marked:  已标记数
        tp:      标记为"真实危险"的数量 + 未标记的 (默认视为真实)
        fp:      标记为"误触发"的数量
        fp_rate: 误报率 (0~1)，fp / total
        """
        total = len(self._buffer)
        if total == 0:
            return 0, 0, 0, 0, 0.0, '暂无告警记录'

        tp = 0
        fp = 0
        unmarked = 0
        for entry in self._buffer:
            if entry['real'] is True:
                tp += 1
            elif entry['real'] is False:
                fp += 1
            else:
                unmarked += 1

        # 未标记的默认视为真实 (保守)
        tp += unmarked

        fp_rate = fp / total

        report = (
            "告警统计: 总计%d次 | 真实%d | 误报%d | 误报率%.1f%% | 已标记%d/%d" % (
                total, tp, fp, fp_rate * 100, self._marked_count, total
            )
        )

        return total, self._marked_count, tp, fp, fp_rate, report

    def detail_breakdown(self):
        """
        按事件类型分类统计。
        返回: [(event_type, count, tp, fp), ...]
        """
        breakdown = {}  # MicroPython: 普通 dict（Python 3.7+ 保持插入顺序）
        for entry in self._buffer:
            t = entry['type']
            if t not in breakdown:
                breakdown[t] = {'total': 0, 'tp': 0, 'fp': 0}
            breakdown[t]['total'] += 1
            if entry['real'] is False:
                breakdown[t]['fp'] += 1
            else:
                breakdown[t]['tp'] += 1  # None → 默认真实

        result = []
        for t, d in breakdown.items():
            result.append((t, d['total'], d['tp'], d['fp']))
        return result

    # ==================================================================
    # 调试：打印报告
    # ==================================================================

    def log_report(self):
        """打印完整统计报告到串口"""
        _, _, tp, fp, fp_rate, report = self.summary()
        print("========== 误报率统计 ==========")
        print(report)
        print("--- 按类型分解 ---")
        for t, total, tp_c, fp_c in self.detail_breakdown():
            local_rate = fp_c / total if total > 0 else 0
            print("  %s: %d次 (真实%d 误报%d 误报率%.1f%%)" % (
                t, total, tp_c, fp_c, local_rate * 100))
        print("================================")
