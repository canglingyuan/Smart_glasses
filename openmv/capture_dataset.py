# 鏁版嵁闆嗛噰闆嗚剼鏈?鈥?鏅鸿兘鍔╃洸鐪奸暅 # 鐢ㄦ硶: 灏嗘鏂囦欢鏀瑰悕涓?main.py 鏀惧叆 SD 鍗?# 涓婄數鍚庤嚜鍔ㄦ媿鐓э紝鎸夊抚闂撮殧淇濆瓨鍒?SD 鍗?/dataset/ 鐩綍
# 鎷嶅畬鎷斿崱锛岀収鐗囧鍏?Edge Impulse 鏍囨敞

import sensor, pyb, time, os

# ========== 璁剧疆 ==========
SAVE_DIR = "/sd/dataset"        # 淇濆瓨鐩綍
INTERVAL = 2000                 # 鎷嶇収闂撮殧 (ms), 2000=姣忕0.5寮?CLASS    = "unknown"            # 绫诲埆鍚? 鍙敼涓? stairs_up/stairs_down/tactile/not_tactile/obstacle
# ==========================

# 鍒濆鍖?sensor.reset()
sensor.set_pixformat(sensor.RGB565)
sensor.set_framesize(sensor.QVGA)  # 320x240
sensor.skip_frames(time=2000)
sensor.set_auto_gain(True)
sensor.set_auto_whitebal(True)

# 鍒涘缓鐩綍
try:
    os.mkdir(SAVE_DIR)
    print("Created:", SAVE_DIR)
except OSError:
    pass

led_red = pyb.LED(1)
led_green = pyb.LED(2)
clock = time.clock()
count = 0

print("=== 鏁版嵁閲囬泦妯″紡 ===")
print("绫诲埆: %s" % CLASS)
print("闂撮殧: %dms" % INTERVAL)
print("淇濆瓨鍒? %s" % SAVE_DIR)
print("缁跨伅闂?鎷嶇収, 绾㈢伅=璺宠繃")
print("鎸夐敭PC13=鍋滄")
print("")

last_capture = time.ticks_ms()

while True:
    clock.tick()
    img = sensor.snapshot()

    now = time.ticks_ms()
    if time.ticks_diff(now, last_capture) > INTERVAL:
        # 鎷嶇収
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

    # 鎸夐敭鍋滄
    if pyb.Pin("PC13", pyb.Pin.IN, pyb.Pin.PULL_UP).value() == 0:
        print("STOPPED. Total:", count)
        led_green.on(); led_red.on()
        time.sleep_ms(500)
        led_green.off(); led_red.off()
        break

    print("FPS:%.1f | 宸叉媿:%d" % (clock.fps(), count), end="\r")