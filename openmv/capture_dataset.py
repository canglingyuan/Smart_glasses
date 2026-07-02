# 数据集采集脚本 — 智能助盲眼镜 v7.0
# 用法: 将此文件改名为 main.py 放入 SD 卡
# 上电后自动拍照，按帧间隔保存到 SD 卡 /dataset/ 目录
# 拍完拔卡，照片导入 Edge Impulse 标注

import sensor, pyb, time, os

# ========== 设置 ==========
SAVE_DIR = "/sd/dataset"        # 保存目录
INTERVAL = 2000                 # 拍照间隔 (ms), 2000=每秒0.5张
CLASS    = "unknown"            # 类别名, 可改为: stairs_up/stairs_down/tactile/not_tactile/obstacle
# ==========================

# 初始化
sensor.reset()
sensor.set_pixformat(sensor.RGB565)
sensor.set_framesize(sensor.QVGA)  # 320x240
sensor.skip_frames(time=2000)
sensor.set_auto_gain(True)
sensor.set_auto_whitebal(True)

# 创建目录
try:
    os.mkdir(SAVE_DIR)
    print("Created:", SAVE_DIR)
except:
    pass

led_red = pyb.LED(1)
led_green = pyb.LED(2)
clock = time.clock()
count = 0

print("=== 数据采集模式 ===")
print("类别: %s" % CLASS)
print("间隔: %dms" % INTERVAL)
print("保存到: %s" % SAVE_DIR)
print("绿灯闪=拍照, 红灯=跳过")
print("按键PC13=停止")
print("")

last_capture = time.ticks_ms()

while True:
    clock.tick()
    img = sensor.snapshot()

    now = time.ticks_ms()
    if time.ticks_diff(now, last_capture) > INTERVAL:
        # 拍照
        filename = "%s/img_%s_%04d.jpg" % (SAVE_DIR, CLASS, count)
        try:
            img.save(filename, quality=85)
            count += 1
            led_green.on()
            time.sleep_ms(50)
            led_green.off()
            print("[%d] %s" % (count, filename))
        except Exception as e:
            led_red.on()
            time.sleep_ms(200)
            led_red.off()
            print("ERR:", str(e)[:60])
        last_capture = now

    # 按键停止
    if pyb.Pin("PC13", pyb.Pin.IN, pyb.Pin.PULL_UP).value() == 0:
        print("STOPPED. Total:", count)
        led_green.on(); led_red.on()
        time.sleep_ms(500)
        led_green.off(); led_red.off()
        break

    print("FPS:%.1f | 已拍:%d" % (clock.fps(), count), end="\r")