# 智能助盲眼镜 v7.0 · 入口
import pyb, time

try:
    from glasses import SmartGlasses
    glasses = SmartGlasses()
    glasses.run()
except Exception as e:
    led = pyb.LED(1)
    while True:
        print("FATAL:", str(e)[:80])
        led.on(); time.sleep_ms(200)
        led.off(); time.sleep_ms(800)
