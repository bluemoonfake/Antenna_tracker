cmake_minimum_required(VERSION 3.22)
project(microair_h743_tracker_freertos C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(TRACKER_PROFILE "boot" CACHE STRING "Firmware milestone: boot, bridge or recovered")
set_property(CACHE TRACKER_PROFILE PROPERTY STRINGS boot bridge recovered)
set(TRACKER_FC_BAUD_RATE "57600" CACHE STRING "FC UART baud rate")
set(TRACKER_TARGET_SYSTEM_ID "1" CACHE STRING "MAVLink target system ID (1-255)")
option(TRACKER_ENABLE_SERVO_OUTPUT
       "Enable physical servo output after hardware safety validation"
       OFF)
if(NOT TRACKER_FC_BAUD_RATE MATCHES "^[1-9][0-9]*$" OR TRACKER_FC_BAUD_RATE LESS 1200
   OR TRACKER_FC_BAUD_RATE GREATER 2000000)
  message(FATAL_ERROR "TRACKER_FC_BAUD_RATE must be between 1200 and 2000000")
endif()
if(NOT TRACKER_TARGET_SYSTEM_ID MATCHES "^[0-9]+$" OR TRACKER_TARGET_SYSTEM_ID LESS 1
   OR TRACKER_TARGET_SYSTEM_ID GREATER 255)
  message(FATAL_ERROR "TRACKER_TARGET_SYSTEM_ID must be between 1 and 255")
endif()
if(NOT TRACKER_PROFILE STREQUAL "boot" AND NOT TRACKER_PROFILE STREQUAL "bridge"
   AND NOT TRACKER_PROFILE STREQUAL "recovered")
  message(FATAL_ERROR "Unsupported TRACKER_PROFILE: ${TRACKER_PROFILE}")
endif()

if(TRACKER_ENABLE_SERVO_OUTPUT)
  set(TRACKER_ENABLE_SERVO_OUTPUT_VALUE 1)
else()
  set(TRACKER_ENABLE_SERVO_OUTPUT_VALUE 0)
endif()

get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(FREERTOS_DEPS_DIR "${REPO_ROOT}/.deps" CACHE PATH "Downloaded firmware dependencies")
set(CMSIS_ROOT "${FREERTOS_DEPS_DIR}/cmsis_5")
set(DEVICE_ROOT "${FREERTOS_DEPS_DIR}/cmsis_device_h7")
set(HAL_ROOT "${FREERTOS_DEPS_DIR}/stm32h7xx_hal_driver")
set(FREERTOS_ROOT "${FREERTOS_DEPS_DIR}/freertos_kernel")
set(USB_ROOT "${FREERTOS_DEPS_DIR}/stm32_usb_device")

foreach(required_dir IN ITEMS CMSIS_ROOT DEVICE_ROOT HAL_ROOT FREERTOS_ROOT USB_ROOT)
  if(NOT EXISTS "${${required_dir}}")
    message(FATAL_ERROR "Missing ${required_dir}: ${${required_dir}}. Run freertos/fetch_dependencies.sh")
  endif()
endforeach()

set(CPU_FLAGS
  -mcpu=cortex-m7
  -mfpu=fpv5-d16
  -mfloat-abi=hard
  -mthumb
)

if(TRACKER_PROFILE STREQUAL "boot")
  set(PROFILE_SOURCES src/main.c src/boot_board.c src/interrupts.c src/boot_task.c)
else()
  # Shared communication sources; each profile supplies its own board layer.
  set(PROFILE_SOURCES
    ${USB_ROOT}/Core/Src/usbd_core.c
    ${USB_ROOT}/Core/Src/usbd_ctlreq.c
    ${USB_ROOT}/Core/Src/usbd_ioreq.c
    ${USB_ROOT}/Class/CDC/Src/usbd_cdc.c
    src/main.c
    src/interrupts.c
    src/usb_device.c
    src/usbd_conf.c
    src/usbd_desc.c
    src/app_state.c
    src/bridge_protocol.c
    src/mavlink_bridge_task.c
  )
  if(TRACKER_PROFILE STREQUAL "bridge")
    list(APPEND PROFILE_SOURCES src/bridge_board.c src/boot_board.c src/boot_task.c)
  else()
    list(APPEND PROFILE_SOURCES src/board.c src/gps_ubx.c src/bmi088_spi.c src/sensor_task.c
         ${REPO_ROOT}/common/attitude/attitude.cpp
         src/tracker_math.c src/tracker_control_task.c)
  endif()
endif()

add_executable(${PROJECT_NAME}
  ${DEVICE_ROOT}/Source/Templates/gcc/startup_stm32h743xx.s
  ${DEVICE_ROOT}/Source/Templates/system_stm32h7xx.c

  ${HAL_ROOT}/Src/stm32h7xx_hal.c
  ${HAL_ROOT}/Src/stm32h7xx_hal_cortex.c
  ${HAL_ROOT}/Src/stm32h7xx_hal_dma.c
  ${HAL_ROOT}/Src/stm32h7xx_hal_dma_ex.c
  ${HAL_ROOT}/Src/stm32h7xx_hal_exti.c
  ${HAL_ROOT}/Src/stm32h7xx_hal_flash.c
  ${HAL_ROOT}/Src/stm32h7xx_hal_flash_ex.c
  ${HAL_ROOT}/Src/stm32h7xx_hal_gpio.c
  ${HAL_ROOT}/Src/stm32h7xx_hal_i2c.c
  ${HAL_ROOT}/Src/stm32h7xx_hal_i2c_ex.c
  ${HAL_ROOT}/Src/stm32h7xx_hal_pcd.c
  ${HAL_ROOT}/Src/stm32h7xx_hal_pcd_ex.c
  ${HAL_ROOT}/Src/stm32h7xx_hal_pwr.c
  ${HAL_ROOT}/Src/stm32h7xx_hal_pwr_ex.c
  ${HAL_ROOT}/Src/stm32h7xx_hal_rcc.c
  ${HAL_ROOT}/Src/stm32h7xx_hal_rcc_ex.c
  ${HAL_ROOT}/Src/stm32h7xx_hal_spi.c
  ${HAL_ROOT}/Src/stm32h7xx_hal_spi_ex.c
  ${HAL_ROOT}/Src/stm32h7xx_hal_tim.c
  ${HAL_ROOT}/Src/stm32h7xx_hal_tim_ex.c
  ${HAL_ROOT}/Src/stm32h7xx_hal_uart.c
  ${HAL_ROOT}/Src/stm32h7xx_hal_uart_ex.c
  ${HAL_ROOT}/Src/stm32h7xx_ll_usb.c

  ${FREERTOS_ROOT}/croutine.c
  ${FREERTOS_ROOT}/event_groups.c
  ${FREERTOS_ROOT}/list.c
  ${FREERTOS_ROOT}/queue.c
  ${FREERTOS_ROOT}/stream_buffer.c
  ${FREERTOS_ROOT}/tasks.c
  ${FREERTOS_ROOT}/timers.c
  ${FREERTOS_ROOT}/portable/GCC/ARM_CM7/r0p1/port.c
  ${FREERTOS_ROOT}/portable/MemMang/heap_4.c

  ${PROFILE_SOURCES}
)

if(TRACKER_PROFILE STREQUAL "boot")
  target_compile_definitions(${PROJECT_NAME} PRIVATE TRACKER_BOOT_ONLY=1 TRACKER_BRIDGE_ONLY=0)
elseif(TRACKER_PROFILE STREQUAL "bridge")
  target_compile_definitions(${PROJECT_NAME} PRIVATE TRACKER_BOOT_ONLY=0 TRACKER_BRIDGE_ONLY=1)
else()
  target_compile_definitions(${PROJECT_NAME} PRIVATE TRACKER_BOOT_ONLY=0 TRACKER_BRIDGE_ONLY=0)
endif()
target_compile_options(${PROJECT_NAME} PRIVATE -g3)
set_source_files_properties(
  src/sensor_task.c
  ${REPO_ROOT}/common/attitude/attitude.cpp
  PROPERTIES INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/compat"
)

set_target_properties(${PROJECT_NAME} PROPERTIES
  SUFFIX ".elf"
  LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/linker/microair_h743_app.ld"
)

target_include_directories(${PROJECT_NAME} PRIVATE
  include
  ${REPO_ROOT}/common/attitude
  ${CMSIS_ROOT}/CMSIS/Core/Include
  ${DEVICE_ROOT}/Include
  ${HAL_ROOT}/Inc
  ${HAL_ROOT}/Inc/Legacy
  ${FREERTOS_ROOT}/include
  ${FREERTOS_ROOT}/portable/GCC/ARM_CM7/r0p1
  ${USB_ROOT}/Core/Inc
  ${USB_ROOT}/Class/CDC/Inc
)
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE ${REPO_ROOT}/third_party)

target_compile_definitions(${PROJECT_NAME} PRIVATE
  STM32H743xx
  USE_HAL_DRIVER
  USE_USB_FS
  USER_VECT_TAB_ADDRESS
  VECT_TAB_OFFSET=0x00020000U
  MAVLINK_COMM_NUM_BUFFERS=2
  BOARD_FC_BAUD_RATE=${TRACKER_FC_BAUD_RATE}U
  TRACKER_TARGET_SYSTEM_ID=${TRACKER_TARGET_SYSTEM_ID}U
  TRACKER_ENABLE_SERVO_OUTPUT=${TRACKER_ENABLE_SERVO_OUTPUT_VALUE}
)

target_compile_options(${PROJECT_NAME} PRIVATE
  ${CPU_FLAGS}
  -ffunction-sections
  -fdata-sections
  -fno-common
  -Wall
  -Wextra
  $<$<COMPILE_LANGUAGE:C>:-Werror=implicit-function-declaration>
  $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
  $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
)

target_link_options(${PROJECT_NAME} PRIVATE
  ${CPU_FLAGS}
  -T${CMAKE_CURRENT_SOURCE_DIR}/linker/microair_h743_app.ld
  --specs=nano.specs
  --specs=nosys.specs
  -Wl,--gc-sections
  -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.map
  -Wl,--print-memory-usage
)

target_link_libraries(${PROJECT_NAME} PRIVATE m)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
  COMMAND ${CMAKE_OBJCOPY} -O binary
          $<TARGET_FILE:${PROJECT_NAME}>
          ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.bin
  COMMAND ${CMAKE_OBJCOPY} -O ihex
          $<TARGET_FILE:${PROJECT_NAME}>
          ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.hex
  COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}>
  VERBATIM
)
