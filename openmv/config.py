# 智能助盲眼镜 v7.0 · 配置类
# 硬件: OpenMV H7 Plus + STM32U5 + ToF(VL53L) + 超声波 + IMU
# 固件: OpenMV 4.8.0 / STM32 V0.1_LED

import sensor


class Config:
    """系统所有可调参数"""

    # ---- UART ----
    UART_BUS = 3
    UART_BAUD = 115200
    UART_TIMEOUT = 1000

    # ---- 摄像头 ----
    FRAMESIZE = sensor.QVGA          # 320x240
    PIXFORMAT = sensor.RGB565

    # ---- 红绿灯 LAB ----
    RED_LIGHT_TH   = (40, 100,  35, 127,   0, 127)  # Amin 15→35 过滤粉色
    GREEN_LIGHT_TH = (70, 100, -80, -30, -50,  50)  # 只抓极亮+极绿(交通灯LED)

    # ---- 斑马线白色 / 障碍物暗色 ----
    WHITE_TH    = (65, 100, -15, 15, -25, 25) # 放宽L和B, 支持图片/屏幕中的斑马线
    OBSTACLE_TH = (0, 100, -128, 127, -128, 127)

    # ---- 盲道黄色 (★ v6: 视觉盲道追踪) ----
    TACTILE_YELLOW_TH = (55, 100, -12, 32, 18, 80)  # L降5, AB微扩

    # ---- ROI ----
    ROI_LIGHT     = (40, 10, 240, 100)
    ROI_CROSSWALK = (0, 120, 320, 120)
    ROI_GROUND    = (0, 160, 320, 80)
    ROI_LATERAL   = (0, 120, 320, 120)
    ROI_OVERHEAD  = (40, 0, 240, 80)
    ROI_TACTILE   = (40, 140, 240, 100)   # ★ v6: 盲道追踪 ROI (画面下半部中央)

    # ---- 距离 (cm) ----
    DIST_BLOCK   = 30
    DIST_CAUTION = 50
    DIST_CLEAN   = 200

    # ---- 面积 (像素) ----
    AREA_LIGHT_MIN     = 180  # 250→180 小面积远距离灯也能检测
    AREA_CAUTION       = 1000
    AREA_BLOCK         = 4000
    OVERHEAD_AREA_MIN  = 200

    # ---- 斑马线 ----
    CROSSWALK_MIN_STRIPES = 4
    CROSSWALK_WIDTH_RATIO = 1.5  # w>h*1.5 典型斑马线条纹
    CROSSWALK_WHITE_RATIO = 0.10

    # ---- 横向拦截物 ----
    LAT_LINE_MIN_LEN = 100

    # ---- 坑洞/凸起 ----
    POTHOLE_DIST_DROP = 30
    POTHOLE_EDGE_MEAN = 30

    # ---- 楼梯 ----
    STAIRS_MIN_LINES   = 6
    STAIRS_AVG_LEN_MIN = 30
    STAIRS_AVG_LEN_MAX = 200

    # ---- 电池 ----
    BAT_LOW_VOLTAGE = 3.6
    BAT_DISCHARGE_CURVE = (
        (4.20, 100), (4.13, 95), (4.06, 90), (4.00, 85),
        (3.93, 78),  (3.86, 70), (3.80, 62), (3.75, 54),
        (3.70, 46),  (3.65, 38), (3.60, 30), (3.55, 22),
        (3.50, 15),  (3.45, 10), (3.40, 5),  (3.30, 2),
        (3.20, 0)
    )

    # ---- 低功耗 ----
    IDLE_FRAMES_TO_SLEEP = 150
    LOWPOWER_FPS = 2

    # ══════════════════════════════════════════════════════════════
    # ★ v5 软突破 — 多级状态阈值 (ms)
    # ══════════════════════════════════════════════════════════════
    #
    # 数据年龄 → 置信度 → 状态:
    #   0       ~ STALE_MS        → 1.0           → OK
    #   STALE_MS ~ DEGRADED_MS    → 1.0→0.5 线性  → STALE
    #   DEGRADED_MS ~ TIMEOUT_MS  → 0.5→0.0 线性  → DEGRADED
    #   > TIMEOUT_MS              → 0.0           → TIMEOUT
    #   连续 DEAD_CONSECUTIVE 次 TIMEOUT → DEAD (硬件故障)
    #
    SENSOR_STALE_MS       = 500
    SENSOR_DEGRADED_MS    = 1500
    SENSOR_TIMEOUT_MS     = 2000    # (保持原名兼容)

    # ══════════════════════════════════════════════════════════════
    # ★ v5 预测回退 — EMA 历史窗口
    # ══════════════════════════════════════════════════════════════
    CONFIDENCE_HISTORY_SIZE = 10
    CONFIDENCE_EMA_ALPHA    = 0.6
    SENSOR_DEAD_CONSECUTIVE = 3     # 连续 TIMEOUT 多少次判 DEAD

    # ══════════════════════════════════════════════════════════════
    # ★ v5 视觉软突破
    # ══════════════════════════════════════════════════════════════

    # -- 时间一致性滤波 --
    VISION_TF_HISTORY    = 3      # 环形缓冲帧数 (5→3 更快响应)
    VISION_TF_CONFIRM    = 0.4    # 40% 以上帧命中才确认输出 (0.6→0.4)
    VISION_TF_HYSTERESIS = 1      # 结果变化后保持 N 帧才允许切换 (3→1)

    # -- 自适应亮度补偿 --
    VISION_BRIGHTNESS_REF  = 50   # 中性灰参考 L 值 (0~100)
    VISION_L_THRESH_SHIFT  = 0.3  # 亮度偏差 → L 阈值偏移系数

    # -- 距离感知面积缩放 --
    VISION_DIST_REF       = 100   # 参考距离 (cm)，该距离下阈值不变
    VISION_AREA_SCALE_MIN = 0.3   # 最远距离缩放下限
    VISION_AREA_SCALE_MAX = 1.5   # 最近距离缩放下限

    # -- 障碍物纹理过滤 --
    VISION_TEXTURE_MIN = 12       # blob 内边缘均值，低于此值视为阴影

    # ══════════════════════════════════════════════════════════════
    # ★ v6 红绿灯闪烁频率检测
    # ══════════════════════════════════════════════════════════════
    # 原理: 中国交通信号灯由 50Hz 交流电网驱动，LED 经全波整流后以 100Hz 闪烁。
    # 30fps 相机虽然不能直接采样 100Hz，但会观测到帧间亮度混叠波动。
    # DC 光源 (车尾灯、红色广告牌) 帧间亮度稳定 → 通过闪烁指数区分。
    FLICKER_HISTORY_SIZE   = 10      # 亮度历史帧数
    FLICKER_CV_THRESH      = 0.06    # 变异系数 (std/mean) 阈值，超此值判定为闪烁
    FLICKER_CONFIRM_RATIO  = 0.6     # 历史窗口中闪烁帧占比达标才确认
    FLICKER_MIN_BLOB_AREA  = 60      # 候选闪烁区域最小面积 (像素)

    # ══════════════════════════════════════════════════════════════
    # ★ v6 斑马线消失预测
    # ══════════════════════════════════════════════════════════════
    # 原理: 检测斑马线最底部条纹位置 + 底部水平边缘。
    # 当底部条纹逼近画面下边缘 → 用户即将走出斑马线 → 临界告警。
    # 底部出现连续水平边缘 → 可能是台阶/路缘 → 提醒注意。
    CROSSWALK_END_Y_NEAR     = 200    # 底部条纹 y 坐标 >= 此值 → 接近末端 (画面240高)
    CROSSWALK_END_Y_CRITICAL = 225    # 底部条纹 y 坐标 >= 此值 → 临界告警
    CROSSWALK_END_EDGE_MIN_LEN = 80   # 底部水平边缘最小长度
    CROSSWALK_END_EDGE_Y_MIN   = 210  # 底部边缘检测起始 y (画面下方30行)
    CROSSWALK_END_EDGE_Y_MAX   = 239  # 底部边缘检测结束 y

    # ══════════════════════════════════════════════════════════════
    # ★ v6 盲道视觉追踪
    # ══════════════════════════════════════════════════════════════
    # 原理: 摄像头画面中检测黄色盲道区域 → 在区域内用 Hough 线检测提取走向 →
    # 判断用户相对盲道的位置: 偏左 / 偏右 / 在中间。
    TACTILE_LINE_MIN_LEN     = 25     # 盲道区域内线段最小长度
    TACTILE_LINE_MAX_ANGLE   = 30     # 线段与垂直方向最大偏角 (度)
    TACTILE_ANGLE_LEFT       = -15    # 主方向 < 此角度 → 偏左
    TACTILE_ANGLE_RIGHT      = 15     # 主方向 > 此角度 → 偏右
    TACTILE_CENTER_TOLERANCE = 40     # 盲道中心距画面中心容差 (像素)
    TACTILE_MIN_BLOB_AREA    = 200    # 盲道黄色区域最小面积
    TACTILE_MIN_LINE_COUNT   = 2      # 有效线段最少条数才输出方向

    # ══════════════════════════════════════════════════════════════
    # ★ v6 ToF 自适应基线校准
    # ══════════════════════════════════════════════════════════════
    # 原理: 前 N 帧统计 ToF 读数，取滑动窗口最小值作为传感器基线偏移，
    # 后续读数减去该偏移得到校准距离，补偿制造公差和温度漂移。
    TOF_CALIB_FRAMES        = 100    # 初始校准帧数
    TOF_CALIB_WINDOW         = 50     # 滑动窗口大小 (维持运行最小值)
    TOF_CALIB_MAX_OFFSET     = 30     # 最大允许偏移量 (cm)，超此值视为真实障碍而非漂移
    TOF_CALIB_EMA_ALPHA      = 0.05   # 基线慢速更新系数 (跟踪温度漂移)

    # ══════════════════════════════════════════════════════════════
    # ★ v6 看门狗 (已禁用)
    # ══════════════════════════════════════════════════════════════
    WDT_TIMEOUT_MS = 5000

    # ══════════════════════════════════════════════════════════════
    # ★ v6 启动自检
    # ══════════════════════════════════════════════════════════════
    SELFTEST_TIMEOUT_MS    = 2000   # 等待首帧传感器数据超时
    SELFTEST_RETRY_MAX      = 1      # 失败重试次数
    SELFTEST_RETRY_DELAY_MS = 200    # 重试间隔

    # ══════════════════════════════════════════════════════════════
    # ★ v6 误报率统计
    # ══════════════════════════════════════════════════════════════
    STATS_BUFFER_SIZE    = 100
    STATS_MARK_TIMEOUT_S = 10

    # ---- 头顶障碍物 ----
    OVERHEAD_DIST_THRESH = 180

    # ---- 缓冲区大小 ----
    DIST_BUFFER_SIZE = 5
    VOLTAGE_BUFFER_SIZE = 5
    # ══════════════════════════════════════════════════════════════
    # ★ v7 视觉检测 — 硬编码参数收敛
    # ══════════════════════════════════════════════════════════════
    # 这些值原散落在 vision.py 中，现统一收敛到 config。

    # -- 红绿灯 --
    LIGHT_ROI_HEIGHT         = 140    # 红绿灯 ROI 高度 (像素)
    LIGHT_AREA_CONF_DIVISOR  = 2.5    # 置信度 = max_area / (阈值 × 此系数)

    # -- 障碍物 --
    OBSTACLE_AREA_CONF_SCALE = 3      # 中距障碍物面积缩放系数

    # -- 俯仰角 --
    PITCH_PIXEL_PER_DEGREE   = -3     # 每度俯仰角的 ROI y 偏移像素 (负号=低头下移)
    RAD_TO_DEG               = 57.3   # 弧度→度转换

    # -- 楼梯 --
    STAIRS_DOWN_DIST_RATIO   = 1.3    # 地面距离突增超过此比例判为下楼

    # -- 坑洞 --
    POTHOLE_EDGE_ROI = (0, 220, 320, 20)  # 坑洞底部边缘检测 ROI

    # -- 转弯建议 --
    TURN_LEFT_ROI  = (0, 100, 50, 120)    # 左侧 ROI
    TURN_RIGHT_ROI = (270, 100, 50, 120)  # 右侧 ROI

    # -- 盲道 AI --
    TACTILE_MODEL_INPUT_SIZE  = 64    # 模型输入尺寸 (像素)
    TACTILE_ML_CONF_THRESHOLD = 0.5   # AI 判定置信度阈值

    # -- 默认距离 (传感器未就绪时的安全值) --
    DEFAULT_GROUND_DIST   = 200.0    # 默认地面距离 (cm)
    DEFAULT_FORWARD_DIST  = 300.0    # 默认前向距离 (cm)
    DEFAULT_DIST_INIT     = 300      # 距离缓冲区初始值
    OVERHEAD_RISK_DIST    = 80       # 头顶风险距离阈值 (cm)
    LATERAL_DIST_THRESH   = 100      # 横向拦截物距离阈值 (cm)
