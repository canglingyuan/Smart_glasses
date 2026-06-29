cmake_minimum_required(VERSION 3.22)
enable_language(C ASM)

set(MX_Defines_Syms 
	USE_HAL_DRIVER 
	STM32U5A5xx
    $<$<CONFIG:Debug>:DEBUG>
)

add_compile_options(-mcpu=cortex-m33 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard)
add_link_options(-mcpu=cortex-m33 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard)

set(MX_Include_Dirs
    ${CMAKE_CURRENT_SOURCE_DIR}/../../LPBAM/LpbamAp1
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/App
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/HC_SR04
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/OLED
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/SYN6288
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/fonts
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/VL53L5CX
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Utilities/lpbam
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Utilities/lpbam/STM32U5
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32U5xx_HAL_Driver/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32U5xx_HAL_Driver/Inc/Legacy
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/CMSIS/Device/ST/STM32U5xx/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/CMSIS/Include
)

set(MX_Application_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../LPBAM/LpbamAp1/lpbam_lpbamap1_config.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../LPBAM/LpbamAp1/lpbam_lpbamap1_scenario_build.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../LPBAM/LpbamAp1/lpbam_lpbamap1_scenario_config.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/stm32u5xx_it.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/stm32u5xx_hal_msp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/sysmem.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/syscalls.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../startup_stm32u5a5xx.s
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/App/app_main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/App/power_manager.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/App/lpbam_driver.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/App/cmd_parser.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/App/filter.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/App/openmv_receiver.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/App/posture_monitor.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/App/sensor_mpu6050.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/App/voice_prompts.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/App/tof_sensor.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/App/sensor_fusion.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/App/scene_controller.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/App/battery_monitor.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/OLED/OLED.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/fonts/fonts.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/SYN6288/syn6288.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/HC_SR04/hcsr04.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/VL53L5CX/vl53l5cx_api.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/VL53L5CX/vl53l5cx_plugin_xtalk.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/VL53L5CX/vl53l5cx_plugin_detection_thresholds.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/VL53L5CX/platform.c
)

set(STM32_Drivers_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/system_stm32u5xx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_ramcfg.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_i2c.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_i2c_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_rcc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_rcc_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_cortex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_flash.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_flash_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_gpio.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_dma.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_dma_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_exti.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_pwr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_pwr_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_gtzc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_icache.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_lptim.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_rtc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_rtc_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_tim.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_tim_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_uart.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_uart_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_adc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_adc_ex.c
)

set(Utilities_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Utilities/lpbam/stm32_adv_lpbam_common.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Utilities/lpbam/STM32U5/stm32_ll_lpbam.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Utilities/lpbam/stm32_lpbam_common.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Utilities/lpbam/stm32_adv_lpbam_i2c.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Utilities/lpbam/stm32_lpbam_i2c.c
)

set(MX_LINK_DIRS )
set(MX_LINK_LIBS STM32_Drivers Utilities ${TOOLCHAIN_LINK_LIBRARIES})

add_library(stm32cubemx INTERFACE)
target_include_directories(stm32cubemx INTERFACE ${MX_Include_Dirs})
target_compile_definitions(stm32cubemx INTERFACE ${MX_Defines_Syms})

add_library(STM32_Drivers OBJECT)
target_sources(STM32_Drivers PRIVATE ${STM32_Drivers_Src})
target_link_libraries(STM32_Drivers PUBLIC stm32cubemx)

add_library(Utilities OBJECT)
target_sources(Utilities PRIVATE ${Utilities_Src})
target_link_libraries(Utilities PUBLIC stm32cubemx)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${MX_Application_Src})
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE ${MX_LINK_DIRS})
target_link_libraries(${CMAKE_PROJECT_NAME} ${MX_LINK_LIBS})
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES ADDITIONAL_CLEAN_FILES ${CMAKE_PROJECT_NAME}.map)
if((CMAKE_C_STANDARD EQUAL 90) OR (CMAKE_C_STANDARD EQUAL 99))
    message(ERROR "Generated code requires C11 or higher")
endif()
