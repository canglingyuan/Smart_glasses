"""
行人灯形状数据采集脚本
对着人行红绿灯，在不同距离（5m/10m/20m/30m）各拍30帧，看 blob 的 roundness 和 h/w
"""
import sensor, image, time

sensor.reset()
sensor.set_pixformat(sensor.RGB565)
sensor.set_framesize(sensor.QVGA)
sensor.skip_frames(time=2000)
sensor.set_auto_gain(False)
sensor.set_auto_whitebal(False)

# 红绿灯 LAB 阈值 (同 config.py)
RED_TH   = (40, 100,  35, 127,   0, 127)
GREEN_TH = (70, 100, -80, -30, -50,  50)
ROI_LIGHT = (40, 10, 240, 100)

clock = time.clock()
frame_count = 0

while frame_count < 60:
    clock.tick()
    img = sensor.snapshot()

    reds = img.find_blobs([RED_TH], roi=ROI_LIGHT, pixels_threshold=30, area_threshold=30, merge=True)
    greens = img.find_blobs([GREEN_TH], roi=ROI_LIGHT, pixels_threshold=30, area_threshold=30, merge=True)

    if reds:
        for b in reds:
            r = b.roundness()
            hw = b.h() / max(b.w(), 1)
            area = b.area()
            print("RED  | roundness=%.3f h/w=%.2f area=%d (%d,%d %dx%d)" %
                  (r, hw, area, b.cx(), b.cy(), b.w(), b.h()))

    if greens:
        for b in greens:
            r = b.roundness()
            hw = b.h() / max(b.w(), 1)
            area = b.area()
            print("GREEN| roundness=%.3f h/w=%.2f area=%d (%d,%d %dx%d)" %
                  (r, hw, area, b.cx(), b.cy(), b.w(), b.h()))

    if not reds and not greens:
        print("NONE | no blob detected")

    frame_count += 1
    time.sleep_ms(100)

print("DONE: %d frames" % frame_count)