# OpenMV 烧录手册 — 智能助盲眼镜 v7.0

> 适配 STM32 V0.1_LED 固件。STM32 支持 7 个下行指令（RED/GREEN/ZEBRA/OBSTACLE/PIT/BUMP/NONE）。

---

## 你需要什么

- OpenMV Cam H7 Plus ×1
- Micro USB 数据线 ×1
- PC 已安装 [OpenMV IDE v4.8.0+](https://openmv.io/pages/download)
- SD 卡 FAT32 ≤32GB（可选，方式 B 需要）

---

## 快速开始：烧录方式

### 方式: SD 卡

**优点：** 空间大，方便反复更新  
**缺点：** 需要 SD 卡

```
1. SD 卡格式化为 FAT32
2. 插入 OpenMV 卡槽
3. IDE→工具→Reader/Writer→SD Card 标签页，逐个 Write
4. 或：读卡器直接将文件拷贝到 SD 卡根目录
5. 上电 → OpenMV 优先运行 SD 卡上的 main.py
```


## 烧录顺序（必须严格遵守）

MicroPython 在 `import` 时即时编译。被依赖的文件必须先烧录。

| # | 文件 | 依赖 | 操作 |
|---|------|------|------|
| 1 | `config.py` | 无 | Ctrl+Shift+S |
| 2 | `sensors.py` | config | Ctrl+Shift+S |
| 3 | `fusion.py` | config, sensors | Ctrl+Shift+S |
| 4 | `battery.py` | config, sensors | Ctrl+Shift+S |
| 5 | `vision.py` | config | Ctrl+Shift+S |
| 6 | `interaction.py` | 无 | Ctrl+Shift+S |
| 7 | `power.py` | config, interaction | Ctrl+Shift+S |
| 8 | `decision.py` | config | Ctrl+Shift+S |
| 9 | `stats.py` | config | Ctrl+Shift+S |
| 10 | `glasses.py` | 以上全部 | Ctrl+Shift+S |
| **最后** | `main.py` | glasses | **最后** Ctrl+Shift+S |

额外烧录: `tactile_binary_model.tflite`（盲道 CNN 模型，烧录到 Flash 或 SD 卡根目录）

---

## 自检 LED 闪码速查

```
红绿同时快闪 ×3  🔄 自检启动
绿灯闪 ×5        📷 摄像头 OK
绿灯闪 ×1        📏 ToF OK
绿灯闪 ×2        📡 超声波 OK
绿灯闪 ×3        🧭 IMU OK
绿灯长亮 2 秒     ✅ 全部通过，进入主循环
红灯快闪 ×5      ❌ 某项自检失败
红灯持续闪       🚨 CAM/ToF 关键传感器失败
```

---

## 故障排查

| 症状 | 优先检查 | 操作 |
|------|---------|------|
| 上电完全无反应 | main.py 是否烧录 | Reader/Writer 查看文件列表 |
| `ImportError` | 烧录顺序/遗漏 | 全部清空重烧，严格按顺序 |
| 自检后卡住，无串口输出 | STM32U5 是否上电 | 检查 UART 接线 |
| `AttributeError: QVGA` | sensor.py 命名冲突 | 确认用的是 sensors.py |
| 保存提示空间不足 | Flash 满了 | 工具→重置 OpenMV Cam |

---

## 清空与重置

**清空内置 Flash：** IDE → 工具 → 重置 OpenMV Cam  
**清空 SD 卡：** 用读卡器格式化

重置后需要**重新按顺序烧录全部 10 个 .py 文件 + 模型文件**。

---

## 文件清单

```
smart_glasses_v7.0/
├── main.py                     ← 启动入口 (最后烧录)
├── glasses.py                  ← 主控制器
├── config.py                   ← 配置参数
├── sensors.py                  ← 传感器读取
├── fusion.py                   ← 距离融合
├── vision.py                   ← 视觉检测
├── decision.py                 ← 决策引擎
├── interaction.py              ← 双向协议
├── battery.py                  ← 电池管理
├── power.py                    ← 低功耗管理
├── stats.py                    ← 误报统计
├── tactile_binary_model.tflite ← 盲道 CNN 模型
├── README.md                   ← 本文档
└── FLASHING.md                 ← 烧录手册
```

**需要烧录的文件：10 个 .py + 1 个 .tflite**
