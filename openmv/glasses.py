"""
智能助盲眼镜 主控制器
=====================================================
硬件: OpenMV H7 Plus + STM32U5 + ToF + 超声波 + MPU6050


:
  - 初赛提交版本，代码精简整理
"""

import sensor
import time
import math
from pyb import UART, LED

from config import Config
from sensors import SensorReader
from fusion import DistanceFusion
from battery import BatteryManager
from vision import VisionDetector
from interaction import InteractionManager
from decision import DecisionEngine
from stats import AlertStats


class SmartGlasses:
    """智能导盲眼镜 · 主控制器"""

    def __init__(self):
        self.cfg = Config()

        # 硬件
        self.uart = UART(self.cfg.UART_BUS, self.cfg.UART_BAUD,
                         timeout=self.cfg.UART_TIMEOUT)
        self.led_red   = LED(1)
        self.led_green = LED(2)

        # 看门狗 — 暂时禁用 (STM32 就绪后启用)
        self.wdt = None

        # 摄像头初始化
        sensor.reset()
        sensor.set_pixformat(self.cfg.PIXFORMAT)
        sensor.set_framesize(self.cfg.FRAMESIZE)
        sensor.skip_frames(time=2000)
        sensor.set_auto_gain(False)
        sensor.set_auto_whitebal(False)

        self.clock = time.clock()

        # 模块
        self.sensors   = SensorReader(self.uart, self.cfg)
        self.fusion    = DistanceFusion(self.cfg, self.sensors)
        self.battery   = BatteryManager(self.cfg, self.sensors)
        self.vision    = VisionDetector(self.cfg)

        self.interact  = InteractionManager(self.uart, self.cfg)
        self.sensors.set_line_handler(self.interact.parse_line)

        self.decision  = DecisionEngine(self.cfg)
        self.stats     = AlertStats(self.cfg)

        # 状态
        self.frame_count  = 0
        self._last_desc = ""
        self._muted = False
        self._last_light = 'none'
        self._last_crosswalk = False
        self._last_tactile = 'none'

    # ==================================================================
    # 启动自检
    # ==================================================================

    def _self_test(self):
        """上电自检 — 依次检测所有传感器。LED 闪码报结果。"""

        # 启动信号 — 红绿同时快闪 3 次
        for _ in range(3):
            self.led_red.on()
            self.led_green.on()
            time.sleep_ms(50)
            self.led_red.off()
            self.led_green.off()
            time.sleep_ms(50)

        print("========== 启动自检 ==========")

        def _led_ok(blinks):
            for _ in range(blinks):
                self.led_green.on()
                time.sleep_ms(50)
                self.led_green.off()
                time.sleep_ms(50)
            time.sleep_ms(100)

        def _led_fail():
            for _ in range(5):
                self.led_red.on()
                time.sleep_ms(30)
                self.led_red.off()
                time.sleep_ms(30)

        def _check_sensor(name, check_fn, ok_blinks):
            for attempt in range(self.cfg.SELFTEST_RETRY_MAX + 1):
                ok = check_fn()
                if ok:
                    print("  [OK] %s" % name)
                    _led_ok(ok_blinks)
                    return True
                if attempt < self.cfg.SELFTEST_RETRY_MAX:
                    print("  [RETRY %d/%d] %s" % (attempt + 1, self.cfg.SELFTEST_RETRY_MAX, name))
                    _led_fail()
                    time.sleep_ms(self.cfg.SELFTEST_RETRY_DELAY_MS)
            print("  [FAIL] %s (重试%d次后放弃)" % (name, self.cfg.SELFTEST_RETRY_MAX))
            _led_fail()
            return False

        results = {}

        # 1. 摄像头
        results['CAM'] = _check_sensor('摄像头', self._test_camera, 5)

        # 2. 等待传感器首帧数据
        deadline = time.ticks_add(time.ticks_ms(), self.cfg.SELFTEST_TIMEOUT_MS)
        while time.ticks_diff(deadline, time.ticks_ms()) > 0:
            self.sensors.read()
            self.sensors.update_diagnostics()
            if self.sensors.status['TOF'] in ('OK', 'STALE'):
                break
            time.sleep_ms(50)

        # 3-5. ToF / 超声波 / IMU
        results['TOF'] = _check_sensor(
            'ToF', lambda: self.sensors.status['TOF'] in ('OK', 'STALE'), 1)
        results['US'] = _check_sensor(
            '超声波', lambda: self.sensors.status['US'] in ('OK', 'STALE'), 2)
        results['IMU'] = _check_sensor(
            'IMU', lambda: self.sensors.status['IMU'] in ('OK', 'STALE'), 3)

        # 汇总
        total = len(results)
        passed = sum(1 for v in results.values() if v)
        failed = [k for k, v in results.items() if not v]

        print("自检结果: %d/%d 通过" % (passed, total))
        if failed:
            print("  未通过: %s" % ', '.join(failed))

        if passed == total:
            self.led_green.on()
            time.sleep_ms(500)
            self.led_green.off()
            self._critical_fail = False
        else:
            critical_fail = 'CAM' in failed or 'TOF' in failed
            self._critical_fail = critical_fail
            if critical_fail:
                print("!! 关键传感器(CAM/ToF)失败，进入安全模式 !!")
                for _ in range(10):
                    self.led_red.on()
                    time.sleep_ms(50)
                    self.led_red.off()
                    time.sleep_ms(50)

        print("========== 自检完成，进入主循环 ==========")

    def _test_camera(self):
        try:
            img = sensor.snapshot()
            stats = img.get_statistics()
            return stats.l_mean() > 0
        except Exception:
            return False

    # ==================================================================

    # ==================================================================
    # 主循环
    # ==================================================================

    def run(self):

        print('=== 跳过自检 (无STM32) ===')

        print("智能助盲眼镜  启动")

        while True:
            self.clock.tick()
            self.frame_count += 1

            # ---- 1. 传感器输入 ----
            self.sensors.read()
            self.sensors.update_diagnostics()

            # ---- 2. 距离融合 ----
            fused_dist = self.fusion.fuse()
            prev_dist = self.fusion.last_distance
            avg_dist = self.fusion.update(fused_dist)

            # ---- 3. 电池管理 + 低电量日志 ----
            bat_pct, bat_alert_cmd = self.battery.update()
            if bat_alert_cmd:
                print("!! 电池电量: %d%%" % bat_pct)

            # ---- 4. 图像捕获 ----
            img = sensor.snapshot()

            # ---- 5. 视觉检测 ----
            # 计算头部俯仰角
            try:
                ax = self.sensors.imu_ax
                ay = self.sensors.imu_ay
                az = self.sensors.imu_az
                pitch_deg = 0
                if az != 0 or ay != 0:
                    mag_sq = ay * ay + az * az
                    if mag_sq > 0.001:  # 避免 atan2(0,0) 平台差异
                        pitch_deg = math.atan2(-ax, math.sqrt(mag_sq)) * 57.3
            except Exception:
                pitch_deg = 0

            self.vision.update_frame_context(img, avg_dist, pitch_deg)
            light = self.vision.detect_traffic_light(img)
            crosswalk, cross_offset = self.vision.detect_crosswalk(img)
            obstacle_blob, obstacle_area = self.vision.detect_obstacle(img)
            lateral_line = self.vision.check_lateral_line(img, avg_dist)
            pothole = self.vision.detect_pothole_bump(
                img, avg_dist, prev_dist, obstacle_blob, obstacle_area)
            stairs = self.vision.detect_stairs(img, avg_dist, self.fusion.ground_dist)
            overhead_danger, overhead_area = self.vision.detect_overhead(
                img, self.fusion.forward_dist)
            turn_advice = self.vision.compute_turn_advice(img, avg_dist, obstacle_blob)
            tactile_direction, _, _ = self.vision.detect_tactile(img)

            # ---- 5.5 透明障碍检测 (ToF/超声协同视觉) ----
            # 若传感器报告近距离物体但视觉未检测到blob, 疑似玻璃门等透明障碍
            if obstacle_blob is None:
                fwd_dist = self.fusion.forward_dist
                gnd_dist = self.fusion.ground_dist
                if (fwd_dist > 0 and fwd_dist < self.cfg.DIST_CAUTION) or (gnd_dist > 0 and gnd_dist < self.cfg.DIST_CAUTION):
                    obstacle_blob = True  # 标记为透明障碍
                    obstacle_area = self.cfg.AREA_BLOCK + 1  # >4000 触发 OBSTACLE 播报(触发OBSTACLE播报)
                    print("  [TRANSPARENT] 测距%.0fcm但视觉无物，疑似透明障碍" % min(fwd_dist if fwd_dist > 0 else 999, gnd_dist if gnd_dist > 0 else 999))


            # ---- 6. 综合决策 ----
            event_type, desc, priority, turn = self.decision.evaluate(
                avg_dist, light, crosswalk, cross_offset,
                obstacle_blob, obstacle_area, lateral_line,
                pothole, stairs, overhead_danger, turn_advice,
                tactile_direction)
            self._last_desc = desc

            # ---- 7. 传感器降级 (仅本地日志) ----
            tof_dead = self.sensors.status.get('TOF') == 'DEAD'
            us_dead  = self.sensors.status.get('US') == 'DEAD'
            if tof_dead and us_dead:
                pass  # 双挂已在 evaluate 中处理
            elif tof_dead:
                print("  [DEGRADED] ToF异常，仅超声波测距")
            elif us_dead:
                print("  [DEGRADED] 超声波异常，仅ToF测距")

            # ---- 8. 记录告警 ----
            if event_type != 'clean':
                self.stats.record(event_type, priority, desc)

            # ---- 9. 输出 ----
            self.interact.send(event_type)
            if event_type in ('red', 'obstacle',
                              'pothole', 'bump', 'overhead', 'lateral',
                              'stairs_down'):
                self.led_red.on(); self.led_green.off()
            elif event_type in ('crosswalk', 'stairs_up',
                                'caution', 'green',
                                'tactile_warn', 'obstacle_near'):
                self.led_red.off(); self.led_green.on()
            else:
                self.led_red.off(); self.led_green.off()


            # ---- 10. 调试 ----
            # 灯状态变化 → 立即打印；无变化 → 每30帧打印
            debug_now = False
            if (light != self._last_light or crosswalk != self._last_crosswalk
                  or tactile_direction != self._last_tactile):
                self._last_light = light
                self._last_crosswalk = crosswalk
                self._last_tactile = tactile_direction
                debug_now = True
            elif self.frame_count % 30 == 0:
                debug_now = True

            if debug_now:
                fps = self.clock.fps()
                st = self.sensors.status
                total_alerts, _, _, _, fp_rate, _ = self.stats.summary()

                print("FPS:%.1f | 状态:%s(P%d) | D:%.0fcm(前%.0f/地%.0f q=%.2f) | "
                      "灯:%s | 斑马线:%s | 电量:%d%% | "
                      "ToF:%s(c=%.2f) US:%s(c=%.2f) | 盲道:%s%s | "
                      "告警:%d FP:%.1f%%" % (
                          fps, event_type, priority, avg_dist,
                          self.fusion.forward_dist, self.fusion.ground_dist,
                          self.fusion.fusion_quality,
                          light, crosswalk, bat_pct,
                          st['TOF'], self.sensors.tof_confidence,
                          st['US'], self.sensors.us_confidence,
                          tactile_direction if tactile_direction != 'none' else '-',
                          " [头顶!]" if self.fusion.overhead_risk else "",
                          total_alerts, fp_rate * 100))
