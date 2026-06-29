"""
智能助盲眼镜 v7.0 · 视觉检测器
===============================
所有基于摄像头的视觉检测: 红绿灯、斑马线、障碍物、坑洞、楼梯、头顶、盲道。

v6 新增 (不训练模型):
  1. 红绿灯闪烁频率检测 — 利用 50Hz 电网 LED 闪烁区分真红绿灯 vs DC 光源
  2. 斑马线消失预测 — 检测斑马线末端 + 道路边缘汇合点 → 临界告警
  3. 盲道视觉追踪 — Hough 线检测盲道走向 → "偏左/偏右/在中间"方向引导

v5 保留:
  1. TemporalFilter — 多数投票 + 迟滞，消除检测闪烁
  2. 视觉置信度 — 每个检测器输出 0~1 置信度
  3. 自适应亮度补偿 — LAB L 阈值随场景亮度动态偏移
  4. 距离感知面积缩放 — 面积阈值根据 avg_dist 动态调整
  5. 障碍物纹理过滤 — 边缘密度剔除阴影误报
"""

import image
import math

# v7.0: TFLite model loading
_stairs_model = None
_tactile_model_file = None  # 模型文件名, 用 tf.classify() 一步式

try:
    import tf
    # 测试 tf.load 是否可用
    _tactile_model_file = 'tactile_binary_model.tflite'
    print("[ML] 盲道模型已就绪 (tf.classify)")
except:
    _tactile_model_file = None
    print("[ML] tf模块不可用, 盲道纯Hough")


# ============================================================================
# 时间滤波器 (v5 保留 — 多帧一致性滤波)
# ============================================================================

class TemporalFilter:
    """多数投票 + 迟滞时间滤波器 — 连续多帧确认，防单帧误检"""

    def __init__(self, history_size=5, confirm_ratio=0.6, hysteresis=3):
        self.history = [None] * history_size
        self.idx = 0
        self.size = history_size
        self.confirm_ratio = confirm_ratio
        self.hysteresis = hysteresis
        self.current = None
        self.hyst_count = 0

    def _majority(self):
        """找 history 中出现最多的非 None 值"""
        valid = [v for v in self.history if v is not None]
        if not valid:
            return None, 0, 0.0
        counts = {}
        for v in valid:
            counts[v] = counts.get(v, 0) + 1
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


# ============================================================================
# ★ v6 新增: 红绿灯闪烁频率检测器
# ============================================================================

class FlickerDetector:
    """
    利用中国 50Hz 交流电网 LED 闪烁特性区分真红绿灯 vs DC 光源。

    原理:
      - 中国交通信号灯由 50Hz 交流电网驱动，LED 经全波整流后以 100Hz 闪烁。
      - 30fps 相机虽然不能直接采样 100Hz，但会观测到帧间亮度混叠波动。
      - DC 光源 (车尾灯、红色广告牌、反光标志) 帧间亮度稳定。
      - 通过计算亮度序列的变异系数 (CV = std/mean) 区分闪烁与否。
    """

    def __init__(self, cfg):
        self.cfg = cfg
        size = cfg.FLICKER_HISTORY_SIZE
        self._red_history = [0.0] * size
        self._green_history = [0.0] * size
        self._idx = 0
        self._red_count = 0
        self._green_count = 0

    def update(self, img, red_blobs, green_blobs):
        """记录本帧红/绿候选区域的亮度。blob 列表可为空。"""
        idx_prev = self._idx
        self._idx = (self._idx + 1) % self.cfg.FLICKER_HISTORY_SIZE

        # ── 红色通道 ──
        if red_blobs:
            largest = max(red_blobs, key=lambda b: b.area())
            if largest.area() >= self.cfg.FLICKER_MIN_BLOB_AREA:
                try:
                    stats = img.get_statistics(roi=largest.rect())
                    self._red_history[self._idx] = stats.l_mean()
                    self._red_count = min(self._red_count + 1,
                                          self.cfg.FLICKER_HISTORY_SIZE)
                except:
                    self._red_history[self._idx] = self._red_history[idx_prev]
            else:
                self._red_history[self._idx] = self._red_history[idx_prev]
        else:
            self._red_history[self._idx] = self._red_history[idx_prev]

        # ── 绿色通道 ──
        if green_blobs:
            largest = max(green_blobs, key=lambda b: b.area())
            if largest.area() >= self.cfg.FLICKER_MIN_BLOB_AREA:
                try:
                    stats = img.get_statistics(roi=largest.rect())
                    self._green_history[self._idx] = stats.l_mean()
                    self._green_count = min(self._green_count + 1,
                                            self.cfg.FLICKER_HISTORY_SIZE)
                except:
                    self._green_history[self._idx] = self._green_history[idx_prev]
            else:
                self._green_history[self._idx] = self._green_history[idx_prev]
        else:
            self._green_history[self._idx] = self._green_history[idx_prev]

    @staticmethod
    def _compute_cv(history):
        """计算变异系数 CV = std / mean。返回 (cv, mean_val)。
        跳过值为 0 的槽位 (未填充的初始值)。"""
        valid = [v for v in history if v > 0]
        n = len(valid)
        if n < 3:                       # 至少 3 个有效样本
            return 0.0, 0.0
        mean_val = sum(valid) / n
        if mean_val < 5:
            return 0.0, mean_val
        variance = sum((v - mean_val) ** 2 for v in valid) / n
        std_val = math.sqrt(variance)
        cv = std_val / mean_val if mean_val > 0 else 0.0
        return cv, mean_val

    def is_flickering_red(self):
        """红色通道是否检测到闪烁。返回 (is_flicker, cv)"""
        cv, _ = self._compute_cv(self._red_history)
        if self._red_count < self.cfg.FLICKER_HISTORY_SIZE // 2:
            return False, cv
        return cv >= self.cfg.FLICKER_CV_THRESH, cv

    def is_flickering_green(self):
        """绿色通道是否检测到闪烁。返回 (is_flicker, cv)"""
        cv, _ = self._compute_cv(self._green_history)
        if self._green_count < self.cfg.FLICKER_HISTORY_SIZE // 2:
            return False, cv
        return cv >= self.cfg.FLICKER_CV_THRESH, cv

    def flicker_boost(self, color):
        """
        返回闪烁确认的置信度加成 (-0.5 ~ 1.0)。
        正数 = 确认闪烁 (真红绿灯)，负数 = DC 光源嫌疑 (降低置信度)。
        """
        if color == 'red':
            is_flicker, cv = self.is_flickering_red()
        elif color == 'green':
            is_flicker, cv = self.is_flickering_green()
        else:
            return 0.0

        if is_flicker:
            return min(1.0, cv / (self.cfg.FLICKER_CV_THRESH * 3))
        else:
            return -min(0.5, (self.cfg.FLICKER_CV_THRESH - cv) /
                        max(self.cfg.FLICKER_CV_THRESH, 0.001) * 0.5)


# ============================================================================
# ★ v6 新增: 斑马线消失预测器
# ============================================================================

class CrosswalkEndPredictor:
    """
    检测斑马线末端 + 道路边缘汇合点 → 临界告警。

    状态:
      'none'            — 未检测到斑马线
      'middle'          — 斑马线中间，正常通行
      'ending_near'     — 接近斑马线末端
      'ending_critical' — 即将走出斑马线，前方注意台阶
    """

    def __init__(self, cfg):
        self.cfg = cfg
        self._tf_state = TemporalFilter(5, 0.5, 3)

    def predict(self, img, stripes, is_crosswalk):
        """返回: (end_state, warning_message)"""
        cfg = self.cfg
        raw_state = 'none'
        warning = ''

        if is_crosswalk and stripes:
            max_y = max(b.cy() for b in stripes)

            if max_y >= cfg.CROSSWALK_END_Y_CRITICAL:
                if self._detect_curb_edge(img):
                    raw_state = 'ending_critical'
                    warning = '即将走出斑马线，注意前方台阶'
                else:
                    raw_state = 'ending_near'
                    warning = '斑马线即将结束'
            elif max_y >= cfg.CROSSWALK_END_Y_NEAR:
                raw_state = 'ending_near'
                warning = '接近斑马线末端'
            else:
                raw_state = 'middle'

        result, _ = self._tf_state.update(raw_state, 1.0)
        return (result if result is not None else 'middle'), warning

    def _detect_curb_edge(self, img):
        """检测画面最底部是否存在长水平边缘 (台阶/路缘特征)"""
        cfg = self.cfg
        try:
            y_min = cfg.CROSSWALK_END_EDGE_Y_MIN
            y_max = cfg.CROSSWALK_END_EDGE_Y_MAX
            h = y_max - y_min + 1
            lines = img.find_line_segments(
                roi=(0, y_min, 320, h), merge_distance=15, max_theta_diff=10)
            for l in lines:
                angle = abs(math.degrees(l.theta()))
                if (angle < 15 or angle > 165) and l.length() > cfg.CROSSWALK_END_EDGE_MIN_LEN:
                    return True
        except:
            pass
        return False


# ============================================================================
# ★ v6 新增: 盲道视觉追踪器
# ============================================================================

class TactileTracker:
    """
    基于视觉的盲道检测与方向引导。

    原理:
      - 摄像头画面下半部检测黄色区域 (盲道特征色)。
      - 在黄色区域内用 Hough 线检测提取盲道边缘走向。
      - 计算盲道中心相对画面中心的偏移 → 方向引导。

    方向:
      'left'   — 偏左 (盲道在右侧，应向左调整)
      'right'  — 偏右 (盲道在左侧，应向右调整)
      'center' — 在中间
      'none'   — 未检测到盲道
    """

    def __init__(self, cfg):
        self.cfg = cfg
        self._tf_direction = TemporalFilter(5, 0.5, 3)
        self._last_mean_angle = None

    def track(self, img):
        """返回: (direction, offset_px, guidance_text)"""
        cfg = self.cfg

        yellow_blobs = img.find_blobs(
            [cfg.TACTILE_YELLOW_TH],
            roi=cfg.ROI_TACTILE,
            pixels_threshold=cfg.TACTILE_MIN_BLOB_AREA // 2,
            area_threshold=cfg.TACTILE_MIN_BLOB_AREA,
            merge=True)

        if not yellow_blobs:
            self._tf_direction.update(None)
            return 'none', 0, ''

        tactile_blob = max(yellow_blobs, key=lambda b: b.area())
        if tactile_blob.area() < cfg.TACTILE_MIN_BLOB_AREA:
            self._tf_direction.update(None)
            return 'none', 0, ''

        # 盲道中心偏移
        blob_cx = tactile_blob.cx()
        offset_px = blob_cx - 160

        # Hough 线检测盲道走向
        roi = tactile_blob.rect()
        try:
            lines = img.find_line_segments(
                roi=roi, merge_distance=10, max_theta_diff=20)
        except:
            lines = []

        valid_angles = []
        for l in lines:
            if l.length() < cfg.TACTILE_LINE_MIN_LEN:
                continue
            angle_deg = math.degrees(l.theta())
            if angle_deg > 90:
                angle_deg -= 180
            valid_angles.append(angle_deg)

        # 综合方向判断
        if len(valid_angles) >= cfg.TACTILE_MIN_LINE_COUNT:
            mean_angle = sum(valid_angles) / len(valid_angles)


            if mean_angle < cfg.TACTILE_ANGLE_LEFT:
                raw_direction = 'left'
                guidance = '偏左，请向右调整'
            elif mean_angle > cfg.TACTILE_ANGLE_RIGHT:
                raw_direction = 'right'
                guidance = '偏右，请向左调整'
            else:
                raw_direction = 'center'
                guidance = '在盲道中间'
        else:
            # fallback: 盲道中心偏移
            if abs(offset_px) <= cfg.TACTILE_CENTER_TOLERANCE:
                raw_direction = 'center'
                guidance = '在盲道中间'
            elif offset_px < 0:
                raw_direction = 'left'
                guidance = '偏左，请向右调整'
            else:
                raw_direction = 'right'
                guidance = '偏右，请向左调整'

        direction, _ = self._tf_direction.update(raw_direction, 1.0)
        return (direction if direction is not None else 'none'), offset_px, guidance


# ============================================================================
# 视觉检测器 V6
# ============================================================================

class VisionDetector:
    """
    视觉检测器 v6 — v5 时间滤波 + 置信度 + 自适应 + 距离缩放
                     + v6 闪烁检测 + 斑马线末端预测 + 盲道追踪
    """

    def __init__(self, cfg):
        self.cfg = cfg

        # ── 时间滤波器 (每种检测独立) ──
        hsize = cfg.VISION_TF_HISTORY
        confirm = cfg.VISION_TF_CONFIRM
        hyst = cfg.VISION_TF_HYSTERESIS

        self._tf_light     = TemporalFilter(hsize, confirm, 1)  # 红绿灯迟滞降为1帧，防残留
        self._tf_crosswalk = TemporalFilter(hsize, confirm, hyst)
        self._tf_obstacle  = TemporalFilter(hsize, confirm, 2)
        self._tf_lateral   = TemporalFilter(hsize, confirm, hyst)
        self._tf_pothole   = TemporalFilter(hsize, confirm, 5)
        self._tf_stairs    = TemporalFilter(7, 0.5, 5)
        self._tf_overhead  = TemporalFilter(hsize, confirm, hyst)
        self._tf_turn      = TemporalFilter(hsize, 0.5, hyst)

        # ── ★ v6 新增检测器 ──
        self.flicker = FlickerDetector(cfg)
        self.crosswalk_end = CrosswalkEndPredictor(cfg)
        self.tactile = TactileTracker(cfg)

        # ── 置信度输出 ──
        self.light_confidence     = 0.0
        self.crosswalk_confidence = 0.0
        self.obstacle_confidence  = 0.0
        self.lateral_confidence   = 0.0
        self.pothole_confidence   = 0.0
        self.stairs_confidence    = 0.0
        self.overhead_confidence  = 0.0
        self.turn_confidence      = 0.0

        # ── ★ v6 新增输出 ──
        self.flicker_red_cv     = 0.0
        self.flicker_green_cv   = 0.0
        self.crosswalk_end_state = 'none'
        self.crosswalk_end_warning = ''
        self.tactile_direction  = 'none'
        self.tactile_offset     = 0
        self.tactile_guidance   = ''

        # ── 自适应状态 (每帧由 update_frame_context 更新) ──
        self._brightness_offset = 0.0
        self._distance_scale = 1.0
        self._pitch_offset_y = 0       # ★ v6.2: 俯仰角导致的 ROI y 偏移

    # ==================================================================
    # 自适应上下文
    # ==================================================================

    def update_frame_context(self, img, avg_dist, pitch_deg=0):
        """每帧调用一次, 计算亮度偏移 + 距离缩放 + 头部俯仰"""
        cfg = self.cfg

        stats = img.get_statistics(roi=cfg.ROI_LIGHT)
        l_mean = stats.l_mean()
        self._brightness_offset = int(
            (l_mean - cfg.VISION_BRIGHTNESS_REF) * cfg.VISION_L_THRESH_SHIFT)

        if avg_dist > 0:
            self._distance_scale = max(cfg.VISION_AREA_SCALE_MIN,
                min(cfg.VISION_AREA_SCALE_MAX,
                    cfg.VISION_DIST_REF / avg_dist))
        else:
            self._distance_scale = 1.0

        # ★ v6.2: 低头时红绿灯在画面中下移，ROI 跟随俯仰角偏移
        # 低头(负角度) → y 增大。每度约 3 像素
        self._pitch_offset_y = int(pitch_deg * -3)

    # ==================================================================
    # 工具方法
    # ==================================================================

    def _adapt_L_threshold(self, threshold):
        """根据当前亮度偏移调整 LAB L 通道 min/max"""
        l_min = max(0, min(100, threshold[0] + self._brightness_offset))
        l_max = max(0, min(100, threshold[1] + self._brightness_offset))
        return (int(l_min), int(l_max)) + threshold[2:]

    def _scaled_area(self, base_area):
        """距离感知面积缩放"""
        return int(base_area * self._distance_scale)

    def _texture_score(self, img, blob):
        """blob 区域内边缘像素均值 → 纹理丰富度"""
        try:
            edges = img.find_edges(image.EDGE_CANNY, threshold=(30, 60))
            roi_edges = edges.copy(roi=blob.rect())
            return roi_edges.mean()
        except:
            return 0

    # ====================================================================
    # 红绿灯 ★ v6: + 闪烁频率确认
    # ====================================================================

    def detect_traffic_light(self, img):
        """返回: 'red', 'green', 'none'
        v6.2: 全部色块参与检测(兼容人形)，ROI 随头部俯仰动态偏移"""
        cfg = self.cfg

        # 动态 ROI: 高度从 100 扩到 140 (覆盖中位红绿灯)，y 随低头角度下移
        rx, _, rw, _ = cfg.ROI_LIGHT
        ry = max(0, cfg.ROI_LIGHT[1] + self._pitch_offset_y)
        rh = 140
        light_roi = (rx, ry, rw, rh)

        red_th   = self._adapt_L_threshold(cfg.RED_LIGHT_TH)
        green_th = self._adapt_L_threshold(cfg.GREEN_LIGHT_TH)

        reds = img.find_blobs(
            [red_th], roi=light_roi,
            pixels_threshold=80, area_threshold=80, merge=True)
        greens = img.find_blobs(
            [green_th], roi=light_roi,
            pixels_threshold=80, area_threshold=80, merge=True)

        # 全部色块 — 参与面积竞争 (兼容圆形 + 人形)
        # 非圆形必须竖长 (交通灯人形 h > w*1.2)
        def _valid_shape(b):
            if b.roundness() > 0.6:
                return True           # 圆形灯
            return b.h() > b.w() * 1.2  # 人形灯
        valid_reds   = [b for b in reds   if _valid_shape(b)]
        valid_greens = [b for b in greens if _valid_shape(b)]

        max_red_all   = max((b.area() for b in valid_reds),   default=0)
        max_green_all = max((b.area() for b in valid_greens), default=0)

        # 亮度验证: 交通灯应明显亮于周围
        roi_l = img.get_statistics(roi=light_roi).l_mean()
        if max_red_all > 0:
            try:
                best_red = max(valid_reds, key=lambda b: b.area())
                if img.get_statistics(roi=best_red.rect()).l_mean() < roi_l * 1.25:
                    max_red_all = 0
            except: pass
        if max_green_all > 0:
            try:
                best_green = max(valid_greens, key=lambda b: b.area())
                if img.get_statistics(roi=best_green.rect()).l_mean() < roi_l * 1.25:
                    max_green_all = 0
            except: pass

        # 仅圆形色块 — 送入闪烁检测 (人形轮廓无 LED 光源，闪烁无意义)
        def is_round(b):
            return b.roundness() > 0.6
        round_reds   = [b for b in valid_reds   if is_round(b)]
        round_greens = [b for b in valid_greens if is_round(b)]
        self.flicker.update(img, round_reds, round_greens)

        area_th = self._scaled_area(cfg.AREA_LIGHT_MIN)

        if max_red_all > max_green_all and max_red_all > area_th:
            raw_result = 'red'
            raw_conf = min(1.0, max_red_all / (area_th * 2.5))
        elif max_green_all > max_red_all and max_green_all > area_th:
            raw_result = 'green'
            raw_conf = min(1.0, max_green_all / (area_th * 2.5))
        else:
            raw_result = 'none'
            raw_conf = 0.0

        # A通道二次验证: 红A>0, 绿A<0, 过滤颜色混淆(如红色图片被LAB误判为绿)
        if raw_result == 'green' and valid_greens:
            try:
                best = max(valid_greens, key=lambda b: b.area())
                a_mean = img.get_statistics(roi=best.rect()).a_mean()
                if a_mean > -10:  # A不够负 → 不是真正的绿色
                    raw_result = 'none'
                    raw_conf = 0.0
            except: pass
        elif raw_result == 'red' and valid_reds:
            try:
                best = max(valid_reds, key=lambda b: b.area())
                a_mean = img.get_statistics(roi=best.rect()).a_mean()
                if a_mean < 10:   # A不够正 → 不是真正的红色
                    raw_result = 'none'
                    raw_conf = 0.0
            except: pass

        # ★ v6: 闪烁加成 — 调节置信度
        if raw_result == 'red':
            is_flicker, self.flicker_red_cv = self.flicker.is_flickering_red()
            raw_conf += self.flicker.flicker_boost('red')
            raw_conf = max(0.0, min(1.0, raw_conf))
        elif raw_result == 'green':
            is_flicker, self.flicker_green_cv = self.flicker.is_flickering_green()
            raw_conf += self.flicker.flicker_boost('green')
            raw_conf = max(0.0, min(1.0, raw_conf))

        result, self.light_confidence = self._tf_light.update(raw_result,
                                                               raw_conf)
        return result if result is not None else 'none'

    # ====================================================================
    # 斑马线 ★ v6: + 消失预测
    # ====================================================================

    def detect_crosswalk(self, img):
        """返回: (is_crosswalk, offset_x)"""
        cfg = self.cfg

        white = img.find_blobs(
            [cfg.WHITE_TH], roi=cfg.ROI_CROSSWALK,
            pixels_threshold=50, area_threshold=50, merge=False)
        stripes = [b for b in white
                   if b.w() > b.h() * cfg.CROSSWALK_WIDTH_RATIO and b.w() > 20]

        raw_result = False
        raw_offset = 0
        raw_conf = 0.0

        if len(stripes) >= cfg.CROSSWALK_MIN_STRIPES:
            y_coords = [b.cy() for b in stripes]
            if max(y_coords) - min(y_coords) >= 30:
                # 条纹宽度应相近 (斑马线条纹等宽, 随机白块宽窄不一)
                widths = [b.w() for b in stripes]
                mean_w = sum(widths) / len(widths)
                if mean_w > 0:
                    w_std = (sum((w - mean_w)**2 for w in widths) / len(widths)) ** 0.5
                    if w_std / mean_w < 0.5:  # 宽度标准差<50%均值 = 等宽斑马线
                        white_area = sum(b.area() for b in stripes)
                        ratio = white_area / (320 * 120)
                        if ratio >= cfg.CROSSWALK_WHITE_RATIO:
                            raw_result = True
                            x_centers = [b.cx() for b in stripes]
                            cross_center = sum(x_centers) / len(x_centers)
                            raw_offset = int(cross_center - 160)
                            stripe_conf = min(1.0, len(stripes) /
                                              (cfg.CROSSWALK_MIN_STRIPES * 2))
                            area_conf = min(1.0, ratio / (cfg.CROSSWALK_WHITE_RATIO * 2))
                            raw_conf = (stripe_conf + area_conf) / 2.0

        result, self.crosswalk_confidence = self._tf_crosswalk.update(
            raw_result, raw_conf)

        if result and raw_result:
            return True, raw_offset
        elif result:
            return True, 0
        return False, 0

    # ====================================================================
    # 障碍物
    # ====================================================================

    def detect_obstacle(self, img):
        """返回: (最大色块 或 None, 最大面积)"""
        cfg = self.cfg

        blobs = img.find_blobs(
            [cfg.OBSTACLE_TH],
            pixels_threshold=200, area_threshold=200, merge=True)

        if not blobs:
            self.obstacle_confidence = 0.0
            return None, 0

        largest = max(blobs, key=lambda b: b.area())
        area = largest.area()
        effective_caution = self._scaled_area(cfg.AREA_CAUTION)

        if area < effective_caution:
            self.obstacle_confidence = 0.0
            return None, 0

        texture = self._texture_score(img, largest)
        if texture < cfg.VISION_TEXTURE_MIN and area < effective_caution * 2:
            self.obstacle_confidence = 0.0
            return None, 0

        area_conf = min(1.0, area / (effective_caution * 3))
        texture_conf = min(1.0, texture / max(cfg.VISION_TEXTURE_MIN * 3, 1))
        raw_conf = (area_conf * 0.6 + texture_conf * 0.4)

        result, self.obstacle_confidence = self._tf_obstacle.update(
            area, raw_conf)

        if result is not None and result >= effective_caution:
            return largest, area

        self.obstacle_confidence = 0.0
        return None, 0

    # ====================================================================
    # 横向拦截物
    # ====================================================================

    def check_lateral_line(self, img, avg_dist):
        """检测横向长线条"""
        cfg = self.cfg

        if avg_dist >= 150:
            self.lateral_confidence = 0.0
            return False

        lines = img.find_line_segments(
            roi=cfg.ROI_LATERAL, merge_distance=10, max_theta_diff=15)

        raw_result = False
        raw_conf = 0.0
        max_len = 0

        for l in lines:
            if abs(math.degrees(l.theta())) < 10 and l.length() > cfg.LAT_LINE_MIN_LEN:
                raw_result = True
                max_len = max(max_len, l.length())

        if raw_result:
            raw_conf = min(1.0, max_len / (cfg.LAT_LINE_MIN_LEN * 2))

        result, self.lateral_confidence = self._tf_lateral.update(
            raw_result, raw_conf)
        return result if result is not None else False

    # ====================================================================
    # 坑洞/凸起
    # ====================================================================

    def detect_pothole_bump(self, img, current_dist, last_dist,
                            obstacle_blob, obstacle_area):
        """返回: 'pothole', 'bump', 'none'"""
        cfg = self.cfg

        if last_dist <= 0 or current_dist <= 0:
            self.pothole_confidence = 0.0
            return 'none'

        effective_drop = cfg.POTHOLE_DIST_DROP * self._distance_scale
        effective_block_dist = cfg.DIST_BLOCK * self._distance_scale
        effective_caution = self._scaled_area(cfg.AREA_CAUTION)

        if (last_dist - current_dist > effective_drop
                and current_dist < effective_block_dist):
            if obstacle_blob is not None and obstacle_area > effective_caution:
                self.pothole_confidence = 0.0
                return 'none'

            edges = img.find_edges(image.EDGE_CANNY, threshold=(50, 80))
            try:
                bottom = edges.copy(roi=(0, 220, 320, 20))
                edge_intensity = bottom.mean()
                if edge_intensity > cfg.POTHOLE_EDGE_MEAN:
                    raw_result = 'bump'
                else:
                    raw_result = 'pothole'
            except:
                raw_result = 'pothole'

            drop_ratio = (last_dist - current_dist) / max(effective_drop, 1)
            raw_conf = min(1.0, drop_ratio * 0.5 + 0.3)
        else:
            raw_result = 'none'
            raw_conf = 0.0

        result, self.pothole_confidence = self._tf_pothole.update(
            raw_result, raw_conf)
        return result if result is not None else 'none'

    # ====================================================================
    # 楼梯
    # ====================================================================

    def detect_stairs(self, img, avg_dist, ground_dist=0):
        """返回: 'up', 'down', 'potential', 'none'
        v6.2: ToF 地面距离判断方向。距离突增→下楼, 距离平稳→上楼"""
        cfg = self.cfg

        effective_max = 200 * self._distance_scale
        if avg_dist > effective_max:
            self.stairs_confidence = 0.0
            return 'none'

        raw_result = 'none'
        raw_conf = 0.0

        # ★ v6.2: TFLite CNN 优先
        global _stairs_model
        if _stairs_model is not None:
            try:
                crop = img.copy(roi=cfg.ROI_GROUND)
                crop = crop.resize(64, 64)
                crop_gray = crop.to_grayscale()
                pred = _stairs_model.classify(crop_gray)
                if pred and len(pred) >= 2:
                    stairs_conf = pred[1].value()
                    raw_result = 'potential' if stairs_conf > 0.55 else 'none'
                    raw_conf = stairs_conf
            except:
                raw_result = 'none'
                raw_conf = 0.0
        else:
            lines = img.find_line_segments(
                roi=cfg.ROI_GROUND, merge_distance=10, max_theta_diff=15)
            if len(lines) >= cfg.STAIRS_MIN_LINES:
                left_cnt  = sum(1 for l in lines if l.theta() < 90)
                right_cnt = sum(1 for l in lines if l.theta() > 90)
                if left_cnt >= 3 and right_cnt >= 3:
                    avg_len = sum(l.length() for l in lines) / len(lines)
                    if cfg.STAIRS_AVG_LEN_MIN < avg_len < cfg.STAIRS_AVG_LEN_MAX:
                        raw_result = 'potential'
                        line_conf = min(1.0, len(lines) / (cfg.STAIRS_MIN_LINES * 2))
                        sym_conf = 1.0 - abs(left_cnt - right_cnt) / max(left_cnt + right_cnt, 1)
                        raw_conf = (line_conf + sym_conf) / 2.0

        # ★ 方向判断: 地面距离突增→下楼, 平稳→上楼
        if raw_result == 'potential' and ground_dist > 0:
            if not hasattr(self, '_stair_ground_history'):
                self._stair_ground_history = [ground_dist] * 5
            self._stair_ground_history.append(ground_dist)
            self._stair_ground_history.pop(0)
            recent_avg = sum(self._stair_ground_history) / len(self._stair_ground_history)
            if ground_dist > recent_avg * 1.3:   # 距离突增30%→下楼
                raw_result = 'down'
            else:
                raw_result = 'up'

        result, self.stairs_confidence = self._tf_stairs.update(
            raw_result, raw_conf)
        return result if result is not None else 'none'

    # ====================================================================
    # 头顶障碍物
    # ====================================================================

    def detect_overhead(self, img, avg_dist):
        """检测头顶障碍物, 返回: (is_danger, area)"""
        cfg = self.cfg

        if avg_dist > cfg.OVERHEAD_DIST_THRESH or avg_dist <= 0:
            self.overhead_confidence = 0.0
            return False, 0

        effective_overhead = self._scaled_area(cfg.OVERHEAD_AREA_MIN)
        blobs = img.find_blobs(
            [cfg.OBSTACLE_TH], roi=cfg.ROI_OVERHEAD,
            pixels_threshold=150, area_threshold=effective_overhead, merge=True)

        raw_result = False
        raw_area = 0
        raw_conf = 0.0

        if blobs:
            largest = max(blobs, key=lambda b: b.area())
            if largest.area() > effective_overhead:
                raw_result = True
                raw_area = largest.area()
                raw_conf = min(1.0, largest.area() / (effective_overhead * 2))

        result, self.overhead_confidence = self._tf_overhead.update(
            raw_result, raw_conf)
        return (result if result is not None else False,
                raw_area if result else 0)

    # ====================================================================
    # 转弯建议
    # ====================================================================

    def compute_turn_advice(self, img, avg_dist, obstacle_blob):
        """返回: 'left', 'right', 'stop', 'none'"""
        cfg = self.cfg

        if obstacle_blob is None or avg_dist >= cfg.DIST_CAUTION:
            self.turn_confidence = 0.0
            return 'none'

        effective_block = self._scaled_area(cfg.AREA_BLOCK)
        if avg_dist < cfg.DIST_BLOCK and obstacle_blob.area() > effective_block:
            raw_result = 'stop'
            raw_conf = 0.9
        else:
            left_roi  = (0, 100, 50, 120)
            right_roi = (270, 100, 50, 120)
            left_free  = not img.find_blobs(
                [cfg.OBSTACLE_TH], roi=left_roi, pixels_threshold=100)
            right_free = not img.find_blobs(
                [cfg.OBSTACLE_TH], roi=right_roi, pixels_threshold=100)

            center_x = obstacle_blob.cx()
            if left_free and right_free:
                raw_result = 'left' if center_x < 160 else 'right'
                raw_conf = 0.7
            elif left_free:
                raw_result = 'left'
                raw_conf = 0.8
            elif right_free:
                raw_result = 'right'
                raw_conf = 0.8
            else:
                raw_result = 'stop'
                raw_conf = 0.9

        result, self.turn_confidence = self._tf_turn.update(
            raw_result, raw_conf)
        return result if result is not None else 'none'

    # ====================================================================
    # ★ v6: 盲道追踪
    # ====================================================================

    def detect_tactile(self, img):
        """
        视觉盲道追踪。v6.3: tf.classify 一步式推理 + Hough 方向判断。
        返回: (direction, offset_px, guidance_text)
        """
        global _tactile_model_file
        is_tactile = False
        ai_ok = False
        if _tactile_model_file:
            try:
                crop = img.copy(roi=self.cfg.ROI_TACTILE)
                crop = crop.resize(64, 64)
                crop_gray = crop.to_grayscale()
                results = tf.classify(_tactile_model_file, crop_gray)
                if results:
                    scores = results[0].classification_output()
                    is_tactile = scores[1] > 0.5 if len(scores) > 1 else scores[0] > 0.5
                    ai_ok = True
            except:
                pass

        if ai_ok:
            if is_tactile:
                direction, offset_px, guidance = self.tactile.track(img)
                if direction == 'none':
                    direction = 'center'
                    guidance = '在盲道中间'
                    offset_px = 0
            else:
                direction = 'none'
                offset_px = 0
                guidance = ''
        else:
            # AI不可用, 纯Hough回退
            direction, offset_px, guidance = self.tactile.track(img)

        self.tactile_direction = direction
        self.tactile_offset = offset_px
        self.tactile_guidance = guidance
        return direction, offset_px, guidance
