# 智能助盲眼镜

> **作品简介：** 为解决视障人士出行避障难、路口通行无保障的核心痛点，本作品基于 **OpenMV H7 Plus + STM32U5 双 MCU 异构架构**，打造一款轻量化 AI 智能助盲眼镜。系统采用 OpenMV 视觉模块以 30FPS 实时采集环境画面，运行 **10 项边缘视觉检测算法**——包括红绿灯识别（含 50Hz 电网闪烁检测排除车尾灯/广告牌干扰，支持圆形满灯+人形行人灯双形态）、斑马线检测、障碍物检测、坑洞/凸起识别、上下楼梯判断、头顶障碍物检测、盲道追踪（Edge Impulse 4分类 CNN + Hough 方向引导）——实现全面环境感知。同时 STM32U5 端融合 **ToF 8×8 面阵激光测距、超声波、MPU6050 六轴姿态**，配合独创的**连续置信度衰减模型**和**五级传感器状态机**——传感器短暂断联不立即报警，而是逐步降低信任度，仅硬件真故障才触发提醒——有效消除抖动误报。识别结果通过 **SYN6288 中文语音合成芯片**实时播报，引导用户安全通行。作品从"感知—融合—决策—交互"全链路出发，为视障群体提供了一套实用、可靠、低成本的智能出行辅助方案。

---

基于 OpenMV H7 Plus + STM32U5 的嵌入式智能导盲系统，为视障人士提供实时环境感知和导航辅助。

---

## 硬件架构

```
┌──────────────┐     UART(115200)      ┌──────────────┐
│  OpenMV H7+  │ ◄────────────────── ► │   STM32U5    │
│  (视觉+决策)  │  RED/GREEN/...共19种  │  (传感器Hub)  │
└──────┬───────┘                       └──────┬───────┘
       │                                      │
   OV7725 摄像头                      ┌───────┼───────┐
                                      │       │       │
                                   ToF     超声波    MPU
                                 (VL53L) (HC-SR04) (MPU6050)
                                      │
                                  SYN6288 TTS
                                   + 3W 扬声器
```

| 组件 | 连接 | 用途 |
|------|------|------|
| OpenMV H7 Plus | 主板 | 图像采集、视觉检测、决策 |
| STM32U5 | UART3 | 传感器采集、语音播报、OLED、按键、LED |
| ToF VL53L5CX | STM32U5 I2C | 8×8 面阵地面测距 (1-200cm) |
| 超声波 HC-SR04 | STM32U5 GPIO | 前方长距 (200-400cm) + 头顶 |
| MPU6050 | STM32U5 I2C | 姿态检测 + 跌倒 |
| SYN6288 | STM32U5 UART2 | 中文语音合成 |
| OLED 0.96" | STM32U5 I2C | 状态显示 |

---

## 文件结构 (11 个核心文件 + 1 个模型)

| # | 文件 | 职责 |
|---|------|------|
| 1 | `config.py` | 所有可调阈值、ROI、参数 |
| 2 | `sensors.py` | UART 解析 + 五级状态机 + 置信度衰减 |
| 3 | `fusion.py` | ToF(地面) + 超声(前方/头顶) 分工融合 |
| 4 | `battery.py` | 电池电压→百分比 |
| 5 | `temporal_filter.py` | 时序滤波器（多数投票 + 迟滞） |
| 6 | `vision.py` | 10 项视觉检测 + 闪烁检测 + 盲道追踪 + 时序滤波 + 自适应 |
| 7 | `interaction.py` | UART 指令发送 (19 种独立指令) |
| 8 | `decision.py` | 优先级仲裁 (P0-P4) + 红绿灯-斑马线上下文耦合 |
| 9 | `stats.py` | 误报率滑动窗口统计 |
| 10 | `glasses.py` | 主控制器，自检 + 主循环调度 |
| 11 | `main.py` | 启动入口 + 异常兜底 |

**模型文件:** `trained.tflite` — Edge Impulse 4 分类 CNN（downstairs / normal / tactile / upstairs），83KB，供楼梯和盲道检测共用。

---

## 检测能力

### 10 项视觉检测

| 检测项 | 方法 | 输出 |
|--------|------|------|
| 红绿灯 | LAB色块 + 圆形/人形过滤 + 亮度验证 + 50Hz闪烁CV + A通道确认 | red / green |
| 斑马线 | 白色阈值 + Hough线 + 条纹等宽验证 + 斑马线末端预测 | crosswalk / crosswalk_near / crosswalk_end |
| 大型障碍物 | 暗色blob + Canny纹理过滤 + 距离面积缩放 | obstacle |
| 中距障碍物 | 同上，较低面积阈值 | obstacle_near |
| 横向拦截物 | 水平长线段检测 | lateral |
| 坑洞/凸起 | ToF距离突变 + Canny边缘梯度 | pothole / bump |
| 上下楼梯 | **Edge AI 4分类 + Hough线 + ToF地面距离方向判断** | stairs_up / stairs_down |
| 头顶障碍 | 暗色blob + 超声波 <80cm 双重确认 | overhead |
| 转弯建议 | 障碍物位置 + 左右安全区域检测 | left / right / stop |
| 盲道追踪 | **Edge AI 4分类 + Hough线方向 + 拐弯检测** | tactile / tactile_warn / tactile_turn |

### 优先级仲裁 (P0-P4)

```
P4 — 红灯 / 头顶障碍物              ← 紧急
P3 — 坑洞/凸起/大型障碍物/拦截物/下楼梯
P2 — 中距障碍物/斑马线接近/疑似红灯
P1 — 斑马线/上楼梯/盲道中间
P0 — 绿灯/转弯建议                   ← 信息
```

---

## UART 协议

### 上行 (STM32 → OpenMV)

| 格式 | 说明 |
|------|------|
| `D:<cm>` | 超声波距离 |
| `TOF:<mm>` | ToF 距离 (mm) |
| `IMU:<ax>,<ay>,<az>,<gx>,<gy>,<gz>` | 6轴 MPU |
| `BAT:<voltage>` | 电池电压 |
| `BATPER:<pct>` | 电池百分比 |

### 下行 (OpenMV → STM32) — 共 19 个独立指令

| 指令 | 含义 | 优先级 |
|------|------|:---:|
| RED | 前方红灯 | P4 |
| GREEN | 前方绿灯 | P0 |
| ZEBRA | 斑马线区域 | P1 |
| OBSTACLE | 大型障碍物 | P3 |
| OBSTACLE_NEAR | 中距障碍物 | P2 |
| LATERAL | 横向拦截物 | P3 |
| PIT | 前方坑洼/凸起 | P3 |
| BUMP | 路面凸起 | P3 |
| OVERHEAD | 头顶障碍物 | P4 |
| STAIRS_UP | 上楼梯 | P1 |
| STAIRS_DOWN | 下楼梯 | P3 |
| CROSSWALK_END | 斑马线末端 | P4 |
| CROSSWALK_NEAR | 斑马线接近 | P2 |
| TACTILE | 盲道中间 | P1 |
| TACTILE_WARN | 盲道偏离 | P2 |
| TACTILE_TURN | 盲道拐弯 | P2 |
| LEFT | 向左绕行 | P0 |
| RIGHT | 向右绕行 | P0 |
| NONE | 无事件 | — |

> STM32 当前仅支持 RED/GREEN/ZEBRA/OBSTACLE/PIT/BUMP/NONE。其余指令预留，后续固件更新逐一添加语音。

---

## 核心创新点

1. **50Hz 闪烁检测** — CV变异系数区分交流红绿灯 vs DC光源，排除车尾灯/广告牌
2. **人形行人灯识别** — roundness + h/w 几何过滤，同时支持圆形满灯和行人灯
3. **红绿灯-斑马线上下文耦合** — 无斑马线区域的"红灯"降级为疑似，防止车尾灯误报
4. **连续置信度衰减** — 替代二值超时判断，消除传感器抖动误报
5. **五级传感器状态机** — OK→STALE→DEGRADED→TIMEOUT→DEAD，连续3次超时才判故障
6. **软投票（设计理念）** — 优先级仲裁体现多维度综合判断，非单传感器一票否决
7. **ToF自适应基线校准** — 前100帧自动学习基线，慢速EMA跟踪温漂
8. **Edge AI 4分类模型** — 83KB TFLite 同时识别上下楼梯和盲道，Hough 回退兜底
9. **时序滤波消抖** — 多数投票+迟滞，帧间跳跃率从23%降到3%
10. **距离自适应面积缩放** — 面积阈值跟随测距动态调整

---

## 快速开始

### 方式一：SD 卡烧录（推荐）

1. SD 卡插入电脑
2. 双击 `sync_to_openmv.bat`（自动去 BOM + 同步所有文件）
3. 弹出 SD 卡 → 插入 OpenMV → 上电

### 方式二：OpenMV IDE 逐个烧录

1. IDE 连接 OpenMV，按文件编号顺序 Ctrl+Shift+S 保存到内置 Flash
2. 顺序：`config → sensors → fusion → battery → temporal_filter → vision → interaction → decision → stats → glasses → main`
3. 上电 → 观察 LED 自检闪码 → 绿灯长亮进入主循环

### 自检闪码

```
红绿快闪×3 → 绿灯依次闪（对应 ToF/超声/IMU/BAT） → 绿灯长亮 2s = 成功
红灯持续闪（200ms on / 800ms off）= 启动异常
```

### 常见问题

| 现象 | 原因 | 解决 |
|------|------|------|
| `NameError: name '﻿' isn't defined` | 文件带 BOM | 运行 `sync_to_openmv.bat` 自动清除 |
| 红灯持续闪 | 导入/硬件错误 | IDE 串口看 Traceback |
| 无 STM32 连接 | 自检超时 | 先启动 STM32 再给 OpenMV 上电 |

---

> **许可:** [待定]
