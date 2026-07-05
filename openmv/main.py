# 智能助盲眼镜 入口
import pyb, time, sys

try:
    from glasses import SmartGlasses
    glasses = SmartGlasses()
    glasses.run()
except Exception as e:
    sys.print_exception(e)
    led = pyb.LED(1)
    while True:
        led.on(); time.sleep_ms(200)
        led.off(); time.sleep_ms(800)