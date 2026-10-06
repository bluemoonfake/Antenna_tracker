# Hướng dẫn từng file trong freertos/src và freertos/include

Đối chiếu source và CMake ngày 2026-09-28; kiểm tra lại cấu trúc và luồng chính ngày 2026-10-01. Phạm vi: toàn bộ 16 file C trong src và 14 header trong include. Đây là giải thích source hiện tại; đọc source không xác nhận các phần recovered đã hoạt động trên board.

## 1. Hiểu tổng thể trước khi đọc từng file

Ví dụ FC gửi vị trí: USART1 nhận byte → IRQ gọi HAL → callback đưa byte vào stream buffer → task mav_bridge đọc → byte nguyên gốc xếp vào ring chờ USB, đồng thời parser kiểm tra bản tin → GLOBAL_POSITION_INT hợp lệ cập nhật target trong app_state.

Trong recovered, sensor task còn cập nhật vị trí trạm và heading; control task đọc bản snapshot, kiểm tra thời hạn dữ liệu rồi tính hướng ngắm.

- `src/*.c`: định nghĩa biến, hàm và công việc thực thi. Một file C có thể chỉ chứa helper/driver, không nhất thiết tạo task.
- `include/*.h`: khai báo API, kiểu dữ liệu và macro; compiler đọc chúng khi build. Include header không khởi chạy một task hoặc ngoại vi.
- FreeRTOS kernel, HAL, CMSIS/startup và USB middleware nằm bên `.deps`; CMake đưa những nguồn cần thiết vào firmware.
- `common/attitude/attitude.cpp` và `common/tracker_control/servo_control.c` thuộc phần cũ của repo; CMake FreeRTOS hiện không đưa hai file đó vào target.

### Ba profile do CMake chọn

- **boot (M1):** main.c, interrupts.c, boot_board.c, boot_task.c. Ứng dụng kiểm tra boot/task và LED PWM.
- **bridge (M2):** main/interrupts, boot board/task, bridge_board.c, bridge_protocol.c, mavlink_bridge_task.c, app_state.c và ba file USB. Chỉ giao tiếp UART1/USB cùng chẩn đoán boot.
- **recovered:** main/interrupts, board.c, bridge_protocol.c, mavlink_bridge_task.c, app_state.c, ba file USB, gps_ubx.c, sensor_task.c, tracker_math.c, tracker_control_task.c. Servo output đang tắt qua macro.

CMake chọn một board layer phù hợp. Link đồng thời board.c và bridge_board.c sẽ trùng nhiều tên hàm; recovered cũng có board_fatal riêng, nên không link boot_board.c trong profile này.

### Boot theo thứ tự thực tế

1. Startup bên CMSIS chạy Reset_Handler, chuẩn bị vùng nhớ C và gọi main.
2. Trong boot/bridge: init LED và đánh dấu MAIN.
3. Bật cache, HAL_Init, đánh dấu HAL_READY.
4. Thiết lập nguồn/clock, đánh dấu CLOCK_READY.
5. Tạo tài nguyên và task theo profile; xTaskCreate tạo TCB/stack nhưng chưa thực thi vòng task trước scheduler.
6. vTaskStartScheduler tạo task hệ thống cần thiết và bắt đầu chạy task.
7. Task mav_bridge init USB và start UART RX khi scheduler đã chạy; boot_diag bắt đầu TIM15 PWM khi được CPU chạy tới.

### Task hiện tại và lịch

- **boot_diag:** boot/bridge, priority 1, stack 256 words (1024 byte), vTaskDelayUntil 500 ms.
- **mav_bridge:** bridge/recovered, priority 5, stack 1536 words (6144 byte), xử lý rồi vTaskDelay 1 tick; không phải chu kỳ cứng 1 ms.
- **sensors:** recovered, priority 4, stack 768 words (3072 byte), đọc GPS chờ tối đa 10 ms, kiểm tra compass khi elapsed ít nhất 20 ms.
- **control:** recovered, priority 6, stack 768 words (3072 byte), vTaskDelayUntil 20 ms/50 Hz.
- **IDLE:** task hệ thống priority 0, chạy khi không có task ready cao hơn.
- **Timer daemon:** task hệ thống priority 2, stack 512 words (2048 byte), do configUSE_TIMERS=1; đợi timer command/expiry. LED PWM không do task này tạo.

Task priority lớn hơn được ưu tiên khi ready. Khi một task delay/block, task ready tiếp theo được chạy. Các task khác priority cùng mức có thể chia thời gian do time slicing. ISR có hệ ưu tiên NVIC riêng: số nhỏ hơn ưu tiên cao hơn. IRQ UART/USB priority 6 hiện phù hợp ngưỡng FromISR=5.

### Bốn tầng dữ liệu dễ nhầm

1. **Thanh ghi/byte RX và HAL handle:** driver giao tiếp phần cứng.
2. **FreeRTOS stream buffer RX:** ISR bỏ byte vào, task lấy byte ra.
3. **Ring TX trong bridge:** g_to_usb/g_to_fc chứa byte nguyên gốc đang chờ driver chấp nhận.
4. **Parser và app_state:** parser ráp bản tin; app_state lưu thông tin đã hiểu như tọa độ/timestamp.

Driver TX interrupt phải giữ bộ nhớ cho đến callback complete. Vì vậy bridge_board và usb_device copy dữ liệu vào buffer riêng trước khi báo accepted để task có thể consume ring an toàn.

## 2. Mục lục từng file

1. [`src/main.c`](#file-1) — Điểm vào của ứng dụng: chuẩn bị clock, tài nguyên và task, sau đó giao CPU cho FreeRTOS.
2. [`src/boot_board.c`](#file-2) — Phần phần cứng tối thiểu để quan sát boot bằng LED và giữ chân servo ở mức thấp.
3. [`src/boot_task.c`](#file-3) — Bắt đầu PWM LED và cập nhật số liệu chứng minh scheduler/task hoạt động.
4. [`src/interrupts.c`](#file-4) — Các điểm vào ngắt chuyển sự kiện phần cứng cho HAL hoặc FreeRTOS.
5. [`src/bridge_board.c`](#file-5) — Board layer của M2: UART1 và stream buffer USB, gửi UART bằng interrupt, có bộ đếm lỗi và retry RX.
6. [`src/board.c`](#file-6) — Board layer rộng hơn cho bản recovered: UART FC/GPS, I2C, SPI và timer PWM servo.
7. [`src/mavlink_bridge_task.c`](#file-7) — Điều phối bridge hai chiều, phân tích MAVLink từ FC, cập nhật target và chèn heartbeat tracker.
8. [`src/bridge_protocol.c`](#file-8) — Thuật toán ring buffer, nhận diện khung MAVLink, kiểm tra parser và tạo heartbeat.
9. [`src/usb_device.c`](#file-9) — Lớp USB CDC ứng dụng: cổng serial ảo, buffer sở hữu của driver và trạng thái phiên USB.
10. [`src/usbd_conf.c`](#file-10) — Adapter giữa middleware USB Device và phần cứng USB2_OTG_FS của STM32.
11. [`src/usbd_desc.c`](#file-11) — Cung cấp danh tính và chuỗi mô tả USB để máy tính nhận ra thiết bị.
12. [`src/app_state.c`](#file-12) — Giữ bản dữ liệu chung mới nhất giữa task giao tiếp, sensor và control.
13. [`src/gps_ubx.c`](#file-13) — Ráp và giải mã bản tin GPS UBX-NAV-PVT.
14. [`src/sensor_task.c`](#file-14) — Đọc GPS trạm và compass QMC5883L rồi cập nhật app_state.
15. [`src/tracker_math.c`](#file-15) — Từ tọa độ trạm/target tính hướng ngắm và đổi góc sang độ rộng xung servo.
16. [`src/tracker_control_task.c`](#file-16) — Kiểm tra dữ liệu còn mới, tính góc tracking rồi quyết định PWM hoặc dừng output.
17. [`include/board.h`](#file-17) — Hợp đồng truy cập phần cứng dùng chung giữa task và board layer.
18. [`include/boot_task.h`](#file-18) — Các mốc boot, struct chẩn đoán và API bắt đầu boot task/PWM.
19. [`include/bridge_protocol.h`](#file-19) — Kiểu ring byte và trạng thái parser MAVLink, cùng API buffer/protocol.
20. [`include/bridge_status.h`](#file-20) — Tách thống kê do task quản lý và thống kê sự kiện IO/ISR.
21. [`include/usb_device.h`](#file-21) — Giao diện nhỏ của USB CDC ứng dụng.
22. [`include/usbd_conf.h`](#file-22) — Cấu hình thư viện USB Device và ánh xạ các dịch vụ bộ nhớ/log/delay.
23. [`include/usbd_desc.h`](#file-23) — Expose bảng descriptor USB cho usb_device.c.
24. [`include/app_state.h`](#file-24) — Mô hình trạng thái tracker và các hàm cập nhật/snapshot.
25. [`include/gps_ubx.h`](#file-25) — Tách trạng thái đang ráp UBX và vị trí đã giải mã.
26. [`include/tracker_tasks.h`](#file-26) — Danh sách hàm khởi tạo ba task ứng dụng.
27. [`include/tracker_config.h`](#file-27) — ID MAVLink, lịch control, timeout và công tắc đầu ra servo.
28. [`include/tracker_math.h`](#file-28) — Hợp đồng đầu vào/đầu ra cho geometry và đổi xung servo.
29. [`include/FreeRTOSConfig.h`](#file-29) — Quy định scheduler, tick, heap, đồng bộ, task timer và mức ngắt gọi API RTOS.
30. [`include/stm32h7xx_hal_conf.h`](#file-30) — Chọn module HAL và giá trị clock/callback/assert dùng trong driver.

<a id="file-1"></a>

## 1. src/main.c

[Đọc source](../../../freertos/src/main.c)

**Vai trò:** Điểm vào của ứng dụng: chuẩn bị clock, tài nguyên và task, sau đó giao CPU cho FreeRTOS.

**Profile sử dụng:** boot, bridge, recovered. Với header, đây là nơi API/cấu hình có ý nghĩa sử dụng; include gián tiếp có thể xuất hiện trong profile khác.

**Ngữ cảnh:** Trước scheduler; tick hook và fault hook trong runtime.

### Biến, kiểu dữ liệu hoặc cấu hình

- g_bootloader_delay_signature: hằng uint64_t được đặt trong section .bootloader_signature và giữ lại bởi linker; metadata cho bootloader đang có trên board.
- Ba struct local oscillator, clock, peripheral chứa cấu hình RCC trước khi HAL ghi vào phần cứng.

### Hàm hoặc API chính

- **`system_clock_config()`**: Chọn LDO, bật SYSCFG rồi VOS0 và chờ VOSRDY. Với HSE=8 MHz: PLL1 VCO=960 MHz, P=480 MHz cho SYSCLK, Q=240 MHz, R=120 MHz. Chọn HCLK=240 MHz, APB1/2/3/4=120 MHz. PLL3 Q=48 MHz cấp USB; SPI123 chọn PLL1 Q, I2C và UART chọn clock bus tương ứng. Các lỗi HAL dẫn đến board_fatal().
- **`main()`**: boot/bridge: khởi tạo LED và đánh dấu từng mốc. Bật I-cache/D-cache, HAL_Init(), cấu hình clock. boot tạo boot_diag; bridge tạo boot_diag, board, app_state và mav_bridge; recovered tạo board, app_state, mav_bridge, sensors và control. xTaskCreate mới tạo task; vTaskStartScheduler() bắt đầu thực thi task.
- **`vApplicationTickHook()`**: Gọi HAL_IncTick() mỗi RTOS tick để HAL_GetTick() tiếp tục tăng sau khi scheduler chạy.
- **`vApplicationMallocFailedHook(), vApplicationStackOverflowHook(), tracker_assert_failed()`**: Đưa lỗi cấp phát, kiểm tra stack hoặc configASSERT về board_fatal(). Nếu scheduler trả về, main cũng gọi fatal.

### Luồng phối hợp

Reset/startup bên .deps → main → tạo task → vTaskStartScheduler → task ready có priority cao nhất chạy.

### Điều cần hiểu đúng

- File này là ứng dụng tại 0x08020000. Chữ boot trong profile chỉ mốc kiểm tra khởi động, không tạo bootloader mới.
- Clock và cache được thiết lập trước scheduler. USB và UART RX được bắt đầu trong task bridge sau scheduler.


<a id="file-2"></a>

## 2. src/boot_board.c

[Đọc source](../../../freertos/src/boot_board.c)

**Vai trò:** Phần phần cứng tối thiểu để quan sát boot bằng LED và giữ chân servo ở mức thấp.

**Profile sử dụng:** boot, bridge. Với header, đây là nơi API/cấu hình có ý nghĩa sử dụng; include gián tiếp có thể xuất hiện trong profile khác.

**Ngữ cảnh:** Init trước scheduler; bắt đầu PWM trong boot_diag; fatal có thể được gọi từ task/hook/exception.

### Biến, kiểu dữ liệu hoặc cấu hình

- g_led_timer: handle HAL của TIM15.
- LED PE4/PE5/PE6 active low; TIM15 CH2 dùng PE6 AF4. PE13/PE14 giữ GPIO low trong boot/bridge.

### Hàm hoặc API chính

- **`boot_board_init()`**: Bật clock GPIOE; đặt latch PE4/5/6 high để LED tắt, PE13/14 low rồi cấu hình output push-pull. main bật PE4 để báo đã vào main.
- **`boot_board_led_pwm_start()`**: Reset/bật TIM15; chia timer clock 240 MHz về tick 1 MHz, ARR=999, CCR2=500. PWM 1 kHz, duty 50%, polarity low. Bắt đầu PWM rồi chuyển PE6 sang AF4; tắt LED xanh dương PE4.
- **`board_fatal()`**: Tắt IRQ, reset/tắt TIM15, đưa GPIO về trạng thái init rồi bật LED đỏ PE5 và đứng trong vòng lặp vô hạn.

### Luồng phối hợp

boot_diag gọi PWM start một lần → timer tự tạo xung → task tiếp tục cập nhật chẩn đoán.

### Điều cần hiểu đúng

- PWM này không dùng timer ISR hoặc DMA; đèn còn sáng không tự chứng minh task còn chạy.
- board_fatal() này chỉ được link trong boot/bridge; recovered dùng phiên bản trong board.c.


<a id="file-3"></a>

## 3. src/boot_task.c

[Đọc source](../../../freertos/src/boot_task.c)

**Vai trò:** Bắt đầu PWM LED và cập nhật số liệu chứng minh scheduler/task hoạt động.

**Profile sử dụng:** boot, bridge. Với header, đây là nơi API/cấu hình có ý nghĩa sử dụng; include gián tiếp có thể xuất hiện trong profile khác.

**Ngữ cảnh:** Task boot_diag: priority 1, stack 256 words = 1024 byte trên Cortex-M7.

### Biến, kiểu dữ liệu hoặc cấu hình

- g_boot_status: struct volatile để xem bằng debugger.
- wake_time và period: mốc thời gian cho vTaskDelayUntil(), period=500 ms.

### Hàm hoặc API chính

- **`boot_task()`**: Đánh dấu TASK_RUNNING; bắt đầu PWM LED; đánh dấu PWM_RUNNING. Mỗi vòng tăng task_iterations, chụp RTOS ticks, HAL millis, heap còn trống, heap thấp nhất và stack high-water; ngủ tới mốc 500 ms tiếp theo.
- **`boot_task_start()`**: xTaskCreate(..., "boot_diag", 256, NULL, 1, NULL); trả true khi task được tạo.

### Luồng phối hợp

Mỗi 500 ms: thức dậy → chụp số liệu → Blocked → đến hạn lại Ready.

### Điều cần hiểu đúng

- stack_high_water_words là lượng stack tối thiểu còn dư từng quan sát được, không phải tổng stack hay stack đang dùng.
- vTaskDelayUntil() giữ mốc lịch định kỳ; có thể trễ nếu task khác/ngắt chiếm CPU.


<a id="file-4"></a>

## 4. src/interrupts.c

[Đọc source](../../../freertos/src/interrupts.c)

**Vai trò:** Các điểm vào ngắt chuyển sự kiện phần cứng cho HAL hoặc FreeRTOS.

**Profile sử dụng:** boot, bridge, recovered. Với header, đây là nơi API/cấu hình có ý nghĩa sử dụng; include gián tiếp có thể xuất hiện trong profile khác.

**Ngữ cảnh:** Exception/ISR của CPU, chạy ngoài lịch task thông thường.

### Biến, kiểu dữ liệu hoặc cấu hình

- g_usb_pcd được định nghĩa trong usbd_conf.c; IRQ USB dùng handle này.
- xPortSysTickHandler() được cung cấp bởi FreeRTOS port.

### Hàm hoặc API chính

- **`HardFault_Handler(), MemManage_Handler(), BusFault_Handler(), UsageFault_Handler()`**: Chuyển các exception lỗi đến board_fatal(). NMI_Handler và DebugMon_Handler hiện có thân rỗng.
- **`SysTick_Handler()`**: Trước scheduler: HAL_IncTick(). Sau khi scheduler bắt đầu: xPortSysTickHandler() xử lý RTOS tick; tick hook trong main tăng HAL tick.
- **`USART1_IRQHandler()`**: Gọi HAL_UART_IRQHandler(board_fc_uart_handle()); HAL gọi RX/TX/error callback tương ứng trong board layer.
- **`USART3_IRQHandler()`**: Trong recovered, chuyển IRQ GPS cho HAL. Trong bridge, thân không gọi HAL GPS; boot không biên dịch nhóm IRQ này.
- **`OTG_FS_IRQHandler()`**: Gọi HAL_PCD_IRQHandler(&g_usb_pcd), bắt đầu chuỗi xử lý USB.

### Luồng phối hợp

IRQ → handler → HAL → callback → stream buffer → task đọc dữ liệu.

### Điều cần hiểu đúng

- SVC_Handler và PendSV_Handler được ánh xạ sang port FreeRTOS qua FreeRTOSConfig.h; code lưu/khôi phục context nằm trong port .deps.
- NVIC priority 6 của IRQ và task priority 6 của control thuộc hai hệ khác nhau.


<a id="file-5"></a>

## 5. src/bridge_board.c

[Đọc source](../../../freertos/src/bridge_board.c)

**Vai trò:** Board layer của M2: UART1 và stream buffer USB, gửi UART bằng interrupt, có bộ đếm lỗi và retry RX.

**Profile sử dụng:** bridge. Với header, đây là nơi API/cấu hình có ý nghĩa sử dụng; include gián tiếp có thể xuất hiện trong profile khác.

**Ngữ cảnh:** Init trước scheduler; API read/write trong task; HAL callback trong ISR.

### Biến, kiểu dữ liệu hoặc cấu hình

- g_uart: USART1 PA9 TX / PA10 RX, AF7, BOARD_FC_BAUD_RATE, 8N1.
- g_fc_rx và g_usb_rx: hai FreeRTOS stream buffer, mỗi buffer 4096 byte.
- g_rx_byte: một byte đang nhận qua HAL. g_tx_buffer[128]: vùng nhớ driver sở hữu cho TX; g_tx_busy/g_tx_length/g_rx_restart theo dõi tiến trình.

### Hàm hoặc API chính

- **`board_init()`**: Tạo stream buffer FC/USB, bật GPIOA/USART1, cấu hình chân và UART. IRQ chưa bật để tránh callback gọi RTOS trước scheduler.
- **`board_start_io()`**: Assert scheduler đang chạy, đặt IRQ USART1 priority 6, xóa pending, đăng ký HAL_UART_Receive_IT(..., 1 byte), bật IRQ.
- **`restart_rx(), board_poll_io()`**: Đăng ký lần nhận tiếp theo; nếu thất bại thì ghi cờ/counter. Task gọi poll để thử lại khi HAL RX đã READY.
- **`board_fc_read(), board_usb_read()`**: Lấy byte từ stream buffer; wait_ms=0 trả ngay, wait_ms>0 có thể block chờ dữ liệu.
- **`board_fc_write()`**: Chấp nhận tối đa 128 byte khi TX rảnh; copy vào g_tx_buffer rồi HAL_UART_Transmit_IT(). true nghĩa driver nhận đoạn dữ liệu, chưa có nghĩa đã gửi hết. timeout_ms không dùng trong phiên bản này.
- **`HAL_UART_RxCpltCallback(), HAL_UART_ErrorCallback(), HAL_UART_TxCpltCallback()`**: RX: đưa byte vào stream qua FromISR, đếm byte/drop, rearm RX. Error: đếm và thử rearm nếu READY. TX complete: cộng byte hoàn tất và bỏ cờ busy.
- **`board_usb_rx_from_isr(), board_usb_reset_rx(), board_usb_write()`**: Đưa USB RX vào stream và đếm drop; reset stream trong critical section khi đổi phiên USB; chuyển TX cho usb_device_transmit().
- **`board_millis(), board_fc_uart_handle()`**: Cung cấp HAL_GetTick() và địa chỉ handle cho interrupts.c.

### Luồng phối hợp

UART RX IRQ → stream g_fc_rx → mav_bridge → ring g_to_usb → USB TX.

### Điều cần hiểu đúng

- Hai stream RX thuộc board layer; hai ring TX g_to_fc/g_to_usb thuộc task bridge. Chúng là các tầng buffer khác nhau.
- File này cung cấp cùng API board.h với board.c, được CMake chọn thay thế; không khởi tạo GPS, I2C, SPI hoặc servo.


<a id="file-6"></a>

## 6. src/board.c

[Đọc source](../../../freertos/src/board.c)

**Vai trò:** Board layer rộng hơn cho bản recovered: UART FC/GPS, I2C, SPI và timer PWM servo.

**Profile sử dụng:** recovered. Với header, đây là nơi API/cấu hình có ý nghĩa sử dụng; include gián tiếp có thể xuất hiện trong profile khác.

**Ngữ cảnh:** Init trước scheduler; read/write HAL từ task; UART/USB RX callback trong ISR.

### Biến, kiểu dữ liệu hoặc cấu hình

- g_fc_uart USART1 PA9/PA10; g_gps_uart USART3 PD8/PD9, GPS baud 230400.
- g_i2c1 PB6/PB7 external; g_i2c2 PB10/PB11 internal; g_spi2 PC2/PC3/PD3.
- CS: BMI088 gyro PD5, accel PD4, BMI270 PA15. g_tim1 CH3 PE13 pitch, CH4 PE14 yaw.
- Ba stream buffer RX 4096 byte: FC, GPS, USB; hai biến RX byte và cờ g_servo_started.

### Hàm hoặc API chính

- **`gpio_init()`**: Bật clock GPIOA–E, CS output high; UART AF7 push-pull; I2C AF4 open-drain/pull-up; SPI AF5; servo AF1 TIM1. Cấu hình mode/chân chưa tự tạo tín hiệu PWM.
- **`uart_init()`**: Khởi tạo USART1 và USART3 8N1; copy cấu hình FC sang GPS rồi đổi Instance/baud; đặt NVIC priority 6.
- **`i2c_init()`**: Khởi tạo I2C1/2 địa chỉ 7 bit, timing 0x10c0ecff, analog filter bật. Đây là giá trị cấu hình bus, không phải địa chỉ sensor.
- **`spi_init()`**: SPI2 master 8 bit, mode 3 (CPOL high, CPHA second edge), MSB first, prescaler 128; chip-select điều khiển bằng GPIO.
- **`pwm_init()`**: TIM1 prescaler 239: với timer clock 240 MHz, mỗi tick 1 µs. ARR=(1000000/330)-1; cấu hình CH3/4 PWM1, pulse=0. Tần số xấp xỉ 330 Hz do phép chia nguyên; chưa HAL_TIM_PWM_Start().
- **`board_init(), board_start_io()`**: Init GPIO, tạo ba stream rồi init UART/I2C/SPI/PWM. Sau scheduler, start_io bắt đầu UART RX từng byte và bật IRQ1/3.
- **`stream_read(), board_fc_read(), board_gps_read(), board_usb_read(), board_millis()`**: Các wrapper nhận stream buffer với timeout và cung cấp thời gian HAL.
- **`board_fc_write(), board_usb_write(), board_usb_reset_rx()`**: UART TX ở đây dùng HAL_UART_Transmit() polling đồng bộ, có timeout; USB TX dùng usb_device; reset RX USB xóa byte chờ và tính drop.
- **`board_usb_rx_from_isr(), HAL_UART_RxCpltCallback(), HAL_UART_ErrorCallback()`**: Đưa RX vào stream và yêu cầu yield khi cần; UART phân biệt Instance1/3 và đăng ký nhận byte tiếp theo. Bản này bỏ qua return code của send/rearm và thiếu phần lớn counter/retry của bridge_board.c.
- **`board_i2c_read(), board_i2c_write()`**: Đọc/ghi thanh ghi sensor bằng HAL_I2C_Mem_*, chuyển địa chỉ 7 bit sang address<<1 theo API HAL, timeout 20 ms.
- **`chip_select_port(), board_spi_read()`**: Chọn CS theo ID board; kéo CS low, gửi reg|0x80 rồi nhận dữ liệu, kéo CS high. Đây là thao tác SPI cơ bản; sensor đặc thù còn cần quy trình riêng.
- **`board_servo_set(), board_servo_stop()`**: Clamp yaw 800–2200 µs, pitch 1100–2200 µs; cập nhật compare CH4/CH3 và start PWM lần đầu. stop dừng cả hai channel.
- **`board_i2c_external(), board_i2c_internal(), board_fc_uart_handle(), board_gps_uart_handle()`**: Trả địa chỉ handle ngoại vi để sensor task hoặc interrupt handler sử dụng.
- **`board_fatal()`**: Tắt IRQ rồi đứng vô hạn. Khác boot_board.c, phiên bản này không bật LED lỗi hay gọi dừng PWM.

### Luồng phối hợp

sensor task dùng GPS/I2C; control dùng PWM; mav_bridge dùng FC UART/USB qua API board.h.

### Điều cần hiểu đúng

- Khi build bridge, file này không được link. Hành vi UART TX ở đây khác M2: polling có thể giữ CPU trong lời gọi HAL.
- SPI được khởi tạo nhưng sensor_task.c hiện chưa đọc IMU BMI088/BMI270; I2C2 cũng chưa được sensor task hiện tại dùng.


<a id="file-7"></a>

## 7. src/mavlink_bridge_task.c

[Đọc source](../../../freertos/src/mavlink_bridge_task.c)

**Vai trò:** Điều phối bridge hai chiều, phân tích MAVLink từ FC, cập nhật target và chèn heartbeat tracker.

**Profile sử dụng:** bridge, recovered. Với header, đây là nơi API/cấu hình có ý nghĩa sử dụng; include gián tiếp có thể xuất hiện trong profile khác.

**Ngữ cảnh:** Task mav_bridge: priority 5, stack 1536 words = 6144 byte.

### Biến, kiểu dữ liệu hoặc cấu hình

- g_to_fc/g_to_usb: hai ring byte TX 4096; g_protocol: trạng thái parser persistent.
- g_bridge_status và g_bridge_io_status được định nghĩa ở đây; driver/ISR cập nhật bộ đếm IO.
- USB generation, rx_loss và thời gian giúp nhận biết thay phiên USB và mất byte UART.

### Hàm hoặc API chính

- **`mavlink_bridge_task()`**: Init parser; init USB và bắt đầu UART RX sau scheduler. Mỗi vòng kiểm tra USB session/configuration, clear ring và USB RX nếu đổi session; reset parser khi UART loss/error tăng, expire frame dở, nhận FC/USB, flush TX, cập nhật diagnostics rồi vTaskDelay(1 tick).
- **`receive_fc()`**: Đọc tối đa 128 byte; từng byte đưa vào g_to_usb nếu USB configured, đồng thời đưa vào parser. Khi parser trả bản tin hợp lệ, gọi handle_fc_message(). Thử chèn heartbeat tại ranh giới wire frame.
- **`handle_fc_message()`**: Chỉ xử lý SYSID=TRACKER_TARGET_SYSTEM_ID=1: HEARTBEAT cập nhật timestamp, GLOBAL_POSITION_INT decode lat/lon/alt và cập nhật target. Các message khác vẫn đi qua đường chuyển tiếp.
- **`receive_usb()`**: Đọc tối đa 128 byte USB và enqueue vào g_to_fc. Chiều này chuyển byte nguyên trạng, không có parser lệnh STOP trong file.
- **`flush_outputs()`**: Peek ring, gửi tối đa 128 byte FC hoặc 64 byte USB. Chỉ consume khi board API nhận dữ liệu. Trong bridge, driver copy nên ring có thể tái sử dụng byte trước khi TX hoàn tất.
- **`update_diagnostics()`**: Loop/frame/drop/pending counters cập nhật mỗi vòng. Time/heap/stack lấy khoảng mỗi 500 ms. tracker_heartbeats đếm bản tin đã enqueue, chưa đảm bảo host đã nhận.
- **`mavlink_bridge_task_start()`**: Tạo task mav_bridge priority 5 và trả kết quả xTaskCreate.

### Luồng phối hợp

FC bytes → USB queue; FC message hợp lệ → app_state.target. USB bytes → FC queue.

### Điều cần hiểu đúng

- vTaskDelay(1) là ngủ một tick sau công việc; chu kỳ cả vòng gồm thời gian xử lý và chờ khác, không đảm bảo chính xác 1 ms.
- USB disconnected vẫn có thể parse FC để cập nhật target, nhưng byte FC không giữ lại để gửi ở phiên USB sau.


<a id="file-8"></a>

## 8. src/bridge_protocol.c

[Đọc source](../../../freertos/src/bridge_protocol.c)

**Vai trò:** Thuật toán ring buffer, nhận diện khung MAVLink, kiểm tra parser và tạo heartbeat.

**Profile sử dụng:** bridge, recovered. Với header, đây là nơi API/cấu hình có ý nghĩa sử dụng; include gián tiếp có thể xuất hiện trong profile khác.

**Ngữ cảnh:** Hàm thường chạy trong mav_bridge, không tạo task/ISR và không gọi HAL trực tiếp.

### Biến, kiểu dữ liệu hoặc cấu hình

- bridge_buffer_t chứa byte TX nguyên gốc; bridge_protocol_t giữ trạng thái một khung đang nhận và thống kê.

### Hàm hoặc API chính

- **`bridge_buffer_enqueue()`**: Nếu không đủ chỗ cho toàn đoạn thì từ chối và cộng dropped_bytes; nếu đủ thì copy tối đa hai đoạn khi vòng qua cuối mảng, cập nhật write_index/count.
- **`bridge_buffer_peek(), bridge_buffer_consume(), bridge_buffer_clear()`**: Peek trả đoạn liên tục tối đa limit và con trỏ, chưa xóa. Consume dịch read_index sau khi gửi được nhận. Clear loại byte đang chờ, cộng drop rồi reset các index.
- **`bridge_protocol_init(), bridge_protocol_reset(), bridge_protocol_expire()`**: Init zero và at_boundary=true. Reset bỏ tiến trình frame dở, tăng resets và at_boundary=false; khi chưa nhận xong frame mà im 250 ms thì expire reset.
- **`bridge_protocol_receive()`**: Nhận từng byte. Nhận diện MAVLink1/2, cộng payload length vào tổng frame 8/12 byte overhead, cộng 13 byte nếu v2 signed. Bỏ local parsing khi có incompat flags không hỗ trợ. Gọi mavlink_frame_char_buffer(); tại cuối khung mới trả true nếu hợp lệ, đếm valid/bad và mở boundary.
- **`bridge_protocol_heartbeat()`**: Nếu đang ở boundary, ring đủ chỗ và đã ít nhất 1000 ms từ lần enqueue trước: pack heartbeat SYS1/COMP191, serialize byte rồi enqueue to_usb.

### Luồng phối hợp

Byte nhận vừa được chuyển tiếp bởi task vừa được thuật toán đọc; chỉ bản tin hợp lệ mới cấp cho cập nhật trạng thái.

### Điều cần hiểu đúng

- CRC lỗi vẫn có thể được chuyển tiếp nguyên byte vì đường forwarding ở task độc lập với kết quả parse.
- Code tính độ dài khung signed để giữ ranh giới; không thiết lập khóa hoặc chính sách xác thực MAVLink signing. Heartbeat có thể bị hoãn khi ring đầy hoặc đang giữa frame.


<a id="file-9"></a>

## 9. src/usb_device.c

[Đọc source](../../../freertos/src/usb_device.c)

**Vai trò:** Lớp USB CDC ứng dụng: cổng serial ảo, buffer sở hữu của driver và trạng thái phiên USB.

**Profile sử dụng:** bridge, recovered. Với header, đây là nơi API/cấu hình có ý nghĩa sử dụng; include gián tiếp có thể xuất hiện trong profile khác.

**Ngữ cảnh:** Init/TX/connected trong task; CDC RX/TX complete/reset callbacks qua USB IRQ.

### Biến, kiểu dữ liệu hoặc cấu hình

- g_usb_device: handle middleware; RX/TX buffer mỗi cái 64 byte.
- g_tx_busy/g_tx_length theo dõi TX đang chạy; g_generation tăng khi init/deinit/reset phiên.
- g_line_coding[7]: baud, stop bits, parity, data bits được host đọc/ghi cho cổng ảo.

### Hàm hoặc API chính

- **`usb_device_init()`**: Assert scheduler running; USBD_Init với descriptor, đăng ký lớp CDC và callback interface, USBD_Start.
- **`cdc_init(), cdc_deinit(), abort_tx()`**: Bỏ TX đang chờ và đếm aborted nếu có, tăng generation; init gắn buffer RX/TX vào CDC.
- **`cdc_control()`**: Xử lý SET_LINE_CODING/GET_LINE_CODING. Thay line coding cổng USB ảo không thay baud USART1 phần cứng.
- **`cdc_receive()`**: Chuyển byte cho board_usb_rx_from_isr(), gắn lại buffer và đăng ký nhận packet kế tiếp.
- **`cdc_transmit_complete()`**: Cộng byte TX hoàn tất, bỏ busy để task có thể gửi đoạn tiếp.
- **`usb_device_transmit()`**: Chỉ nhận đoạn 1–64 byte khi configured, class data tồn tại và TX rảnh. Trong critical section, copy vào buffer riêng rồi yêu cầu CDC transmit. true nghĩa được nhận để gửi.
- **`usb_device_connected(), usb_device_reset_from_isr()`**: Đọc configured/generation có bảo vệ; reset abort TX, tăng generation và reset_events.

### Luồng phối hợp

mav_bridge → usb_device_transmit → middleware CDC → HAL PCD; USB RX đi chiều ngược lại.

### Điều cần hiểu đúng

- CONFIGURED nghĩa host hoàn tất cấu hình USB; không chứng minh có chương trình đang đọc ttyACM hoặc đã nhận dữ liệu.
- Driver sở hữu bản copy tránh trỏ vào vùng ring đã bị task tái sử dụng.


<a id="file-10"></a>

## 10. src/usbd_conf.c

[Đọc source](../../../freertos/src/usbd_conf.c)

**Vai trò:** Adapter giữa middleware USB Device và phần cứng USB2_OTG_FS của STM32.

**Profile sử dụng:** bridge, recovered. Với header, đây là nơi API/cấu hình có ý nghĩa sử dụng; include gián tiếp có thể xuất hiện trong profile khác.

**Ngữ cảnh:** HAL MSP/init từ task; HAL PCD callbacks trong IRQ; USBD_LL gọi theo đường middleware.

### Biến, kiểu dữ liệu hoặc cấu hình

- g_usb_pcd: handle bộ điều khiển USB; pData nối hai chiều giữa HAL PCD và USBD handle.
- PCD Full Speed, embedded PHY, DMA=0, VBUS sensing=0, SOF=0; cấu hình FIFO RX/TX.
- Một vùng static cấp bộ nhớ cho USBD_CDC_HandleTypeDef.

### Hàm hoặc API chính

- **`HAL_PCD_MspInit(), HAL_PCD_MspDeInit()`**: Cấu hình PA11/PA12 AF10, clock USB2_OTG_FS, USB voltage detector và NVIC priority 6. Deinit tắt IRQ/clock và tháo GPIO.
- **`HAL_PCD_SetupStageCallback(), DataOutStageCallback(), DataInStageCallback()`**: Đưa yêu cầu control, dữ liệu host→device hoặc hoàn thành device→host sang USBD_LL tương ứng.
- **`HAL_PCD_ResetCallback(), DisconnectCallback(), ConnectCallback(), SuspendCallback(), ResumeCallback()`**: Chuyển sự kiện cho middleware; reset/disconnect còn báo usb_device_reset_from_isr() để đổi generation và bỏ TX dở.
- **`HAL_PCD_SOFCallback(), ISOOUTIncompleteCallback(), ISOINIncompleteCallback()`**: Các adapter sự kiện SOF/isochronous được cung cấp; cấu hình hiện tại tắt SOF và dùng CDC.
- **`USBD_LL_Init(), DeInit(), Start(), Stop()`**: Gắn PCD vào USBD, khởi tạo controller/FIFO, chuyển kết quả HAL thành USBD_OK/FAIL.
- **`USBD_LL_OpenEP(), CloseEP(), FlushEP(), StallEP(), ClearStallEP(), IsStallEP()`**: Quản lý endpoint, vùng giao tiếp logic của USB.
- **`USBD_LL_SetUSBAddress(), Transmit(), PrepareReceive(), GetRxDataSize()`**: Đặt địa chỉ USB, gửi/đăng ký nhận packet qua HAL và đọc số byte đã nhận.
- **`USBD_static_malloc(), USBD_static_free(), USBD_LL_Delay()`**: Cấp một vùng static cho CDC thay heap; free không làm gì; delay dùng HAL_Delay().

### Luồng phối hợp

OTG_FS_IRQHandler → HAL_PCD_IRQHandler → callbacks ở đây → USB middleware → CDC callbacks ở usb_device.c.

### Điều cần hiểu đúng

- USBD_static_malloc bỏ qua size, thiết kế dành cho cấu hình một lớp CDC hiện tại.
- Middleware xử lý USB control trong ngắt; phần MAVLink/target được đưa sang task qua stream.


<a id="file-11"></a>

## 11. src/usbd_desc.c

[Đọc source](../../../freertos/src/usbd_desc.c)

**Vai trò:** Cung cấp danh tính và chuỗi mô tả USB để máy tính nhận ra thiết bị.

**Profile sử dụng:** bridge, recovered. Với header, đây là nơi API/cấu hình có ý nghĩa sử dụng; include gián tiếp có thể xuất hiện trong profile khác.

**Ngữ cảnh:** Hàm descriptor được middleware gọi khi host yêu cầu thông tin USB.

### Biến, kiểu dữ liệu hoặc cấu hình

- tracker_usb_descriptors: bảng con trỏ hàm callback descriptor.
- VID 0x1b8c, PID 0x0036; USB version 2.00; language ID 0x0409.
- Device/language/string/serial descriptor buffers; string chuyển sang Unicode định dạng USB.

### Hàm hoặc API chính

- **`device_descriptor(), language_descriptor()`**: Trả dữ liệu mô tả thiết bị, VID/PID, class và ngôn ngữ, cùng chiều dài.
- **`make_string(), manufacturer_descriptor(), product_descriptor()`**: Dùng USBD_GetString tạo chuỗi USB: manufacturer MicoAir, product MicoAir H743 FreeRTOS Tracker.
- **`hex_to_unicode(), serial_descriptor()`**: Lấy UID MCU, chuyển thành 12 ký tự hex trong descriptor serial dài 26 byte.
- **`configuration_descriptor(), interface_descriptor()`**: Trả chuỗi tên MAVLink CDC và MAVLink Bridge. Đây là string descriptor; cấu hình endpoint CDC do thư viện CDC cung cấp.

### Luồng phối hợp

Host enumerate → yêu cầu descriptor → các callback này trả danh tính.

### Điều cần hiểu đúng

- File này không parse MAVLink hoặc tạo task; /dev/ttyACM* do hệ điều hành host tạo khi nhận lớp CDC.


<a id="file-12"></a>

## 12. src/app_state.c

[Đọc source](../../../freertos/src/app_state.c)

**Vai trò:** Giữ bản dữ liệu chung mới nhất giữa task giao tiếp, sensor và control.

**Profile sử dụng:** bridge, recovered. Với header, đây là nơi API/cấu hình có ý nghĩa sử dụng; include gián tiếp có thể xuất hiện trong profile khác.

**Ngữ cảnh:** Chỉ gọi từ task; sử dụng FreeRTOS mutex.

### Biến, kiểu dữ liệu hoặc cấu hình

- g_state: target, station, heading và FC heartbeat cùng timestamp/valid.
- g_state_mutex: bảo vệ copy/update tránh control đọc một struct đang bị ghi dở.

### Hàm hoặc API chính

- **`app_state_init()`**: Zero trạng thái và tạo mutex trước scheduler.
- **`update_position()`**: Ghi lat/lon đơn vị 1e-7 độ, altitude mm, timestamp ms; valid nếu lat hoặc lon khác 0.
- **`app_state_update_target(), app_state_update_station()`**: Take mutex tối đa 10 ms, ghi vị trí target/station rồi Give. Nếu hết thời gian lấy lock thì không cập nhật.
- **`app_state_update_heartbeat(), app_state_update_heading()`**: Ghi timestamp heartbeat hoặc heading độ và cờ valid, cùng cách khóa mutex 10 ms.
- **`app_state_snapshot()`**: Nếu con trỏ hợp lệ, đợi mutex bằng portMAX_DELAY, copy toàn g_state cho caller rồi nhả mutex. Caller tính toán trên bản copy.

### Luồng phối hợp

mav_bridge ghi target/heartbeat; sensors ghi station/heading; control đọc snapshot.

### Điều cần hiểu đúng

- valid ở đây không tự hết hạn theo thời gian; control kiểm tra tuổi timestamp riêng.
- Đánh dấu vị trí (0,0) không hợp lệ là quy ước hiện tại, không phải kiểm tra chất lượng GPS tổng quát.


<a id="file-13"></a>

## 13. src/gps_ubx.c

[Đọc source](../../../freertos/src/gps_ubx.c)

**Vai trò:** Ráp và giải mã bản tin GPS UBX-NAV-PVT.

**Profile sử dụng:** recovered. Với header, đây là nơi API/cấu hình có ý nghĩa sử dụng; include gián tiếp có thể xuất hiện trong profile khác.

**Ngữ cảnh:** Hàm parser thường được sensor task gọi từng byte; không tạo task.

### Biến, kiểu dữ liệu hoặc cấu hình

- State machine: sync1 → sync2 → class → ID → length low/high → payload → checksum A/B.
- payload tối đa 92 byte; parser chỉ xuất vị trí cho class 0x01, ID 0x07, length 92.

### Hàm hoặc API chính

- **`checksum(), read_i32()`**: Tính hai checksum tích lũy UBX và đọc int32 little-endian từ payload.
- **`gps_ubx_init()`**: Zero parser, bắt đầu tìm sync byte 0xb5.
- **`gps_ubx_parse()`**: Nhận từng byte; tìm 0xb5/0x62, đọc header/length, từ chối length>92, ráp payload/checksum. Khi NAV-PVT đúng checksum: lấy fix_type, số vệ tinh, lon/lat, hMSL mm; valid khi fix_type>=3 và flags.gnssFixOK set; trả true cho message hoàn chỉnh.

### Luồng phối hợp

USART3 → GPS stream → sensor_task → gps_ubx_parse → station position.

### Điều cần hiểu đúng

- true nghĩa giải mã được NAV-PVT; caller còn phải kiểm tra position.valid.
- File này không gửi lệnh cấu hình GPS và không parse NMEA; module phải đang phát UBX-NAV-PVT ở baud phù hợp.


<a id="file-14"></a>

## 14. src/sensor_task.c

[Đọc source](../../../freertos/src/sensor_task.c)

**Vai trò:** Đọc GPS trạm và compass QMC5883L rồi cập nhật app_state.

**Profile sử dụng:** recovered. Với header, đây là nơi API/cấu hình có ý nghĩa sử dụng; include gián tiếp có thể xuất hiện trong profile khác.

**Ngữ cảnh:** Task sensors: priority 4, stack 768 words = 3072 byte.

### Biến, kiểu dữ liệu hoặc cấu hình

- parser UBX và position nằm trên stack của task; mảng nhận GPS 128 byte.
- QMC5883L địa chỉ I2C 0x0d, thanh ghi data/status/control; compass_ready ghi kết quả init.

### Hàm hoặc API chính

- **`read_i16()`**: Đọc giá trị signed 16 bit little-endian cho trục compass.
- **`compass_init()`**: Qua I2C1 external: ghi reset 0x80 vào control2, cấu hình control1 0x1d.
- **`compass_update()`**: Kiểm tra data-ready, đọc 6 byte X/Y/Z; tính heading=atan2(Y,X) thành độ và đưa về 0–360, cập nhật timestamp.
- **`sensor_task()`**: Init parser, delay 100 ms, init compass một lần. Lặp đọc GPS tối đa 128 byte/chờ 10 ms; NAV-PVT valid cập nhật station. Khi đủ ít nhất 20 ms từ lần kiểm tra compass trước thì thử đọc compass.
- **`sensor_task_start()`**: Tạo sensors priority 4, stack 768 words.

### Luồng phối hợp

GPS UART + compass I2C → station/heading → control đọc.

### Điều cần hiểu đúng

- Hiện chưa đọc gyro/accelerometer/barometer/IST8310; việc board có handle SPI/I2C2 không có nghĩa đã có task IMU.
- Heading chưa áp dụng calibration hard/soft iron, hướng lắp trục hoặc bù nghiêng; comment nguồn ghi rõ cần bổ sung. Compass init fail hiện không có retry trong vòng lặp.
- Mốc 20 ms là kiểm tra elapsed, không phải vòng sensor có chu kỳ cố định 20 ms.


<a id="file-15"></a>

## 15. src/tracker_math.c

[Đọc source](../../../freertos/src/tracker_math.c)

**Vai trò:** Từ tọa độ trạm/target tính hướng ngắm và đổi góc sang độ rộng xung servo.

**Profile sử dụng:** recovered. Với header, đây là nơi API/cấu hình có ý nghĩa sử dụng; include gián tiếp có thể xuất hiện trong profile khác.

**Ngữ cảnh:** Hàm tính toán được control gọi; không truy cập HAL/FreeRTOS.

### Biến, kiểu dữ liệu hoặc cấu hình

- EARTH_RADIUS_M=6371000; DEG_TO_RAD/RAD_TO_DEG.
- Yaw pulse 800–2200 µs; pitch 1100–2200 µs; tracker_solution_t chứa góc và khoảng cách.

### Hàm hoặc API chính

- **`clampf(), wrap_180()`**: Giới hạn số thực và đưa góc về khoảng -180 đến +180 độ.
- **`tracker_solution_compute()`**: Từ lat/lon e7 và altitude mm: đổi sang rad/m, haversine tính khoảng cách ngang, atan2 tính bearing 0–360 và elevation, hypot dạng sqrt tính slant distance. Từ chối con trỏ NULL hoặc tọa độ (0,0); kiểm tra góc hữu hạn.
- **`tracker_solution_to_servo()`**: Yaw tương đối=bearing-heading, wrap ±180 rồi ánh xạ tuyến tính sang 800–2200 µs. Elevation clamp 0–90 rồi ánh xạ sang 1100–2200 µs.

### Luồng phối hợp

station + target → bearing/elevation → relative yaw/pitch → yaw_us/pitch_us.

### Điều cần hiểu đúng

- Đây là geometry và ánh xạ tuyến tính; chưa có PID, encoder feedback hay hiệu chuẩn cơ khí đầy đủ.
- Hướng tham chiếu heading phải phù hợp bearing địa lý; compass chưa có hiệu chỉnh declination/calibration trong sensor task hiện tại.


<a id="file-16"></a>

## 16. src/tracker_control_task.c

[Đọc source](../../../freertos/src/tracker_control_task.c)

**Vai trò:** Kiểm tra dữ liệu còn mới, tính góc tracking rồi quyết định PWM hoặc dừng output.

**Profile sử dụng:** recovered. Với header, đây là nơi API/cấu hình có ý nghĩa sử dụng; include gián tiếp có thể xuất hiện trong profile khác.

**Ngữ cảnh:** Task control: priority 6, stack 768 words = 3072 byte; lịch 50 Hz/20 ms.

### Biến, kiểu dữ liệu hoặc cấu hình

- state snapshot local; tracker_solution_t local; yaw_us/pitch_us tính mỗi vòng.
- Timeout FC heartbeat/target 2000 ms, station GPS 3000 ms, heading 1000 ms từ tracker_config.h.

### Hàm hoặc API chính

- **`state_is_fresh()`**: Yêu cầu mọi cờ valid và tuổi các timestamp không vượt timeout tương ứng.
- **`tracker_control_task()`**: Dừng servo lúc vào task. Mỗi mốc 20 ms: lấy snapshot, kiểm tra freshness, compute solution và convert pulse. Nếu build cho phép output thì board_servo_set(); nếu lỗi hoặc dữ liệu cũ thì stop. Khi macro output=0 hiện tại, luôn stop cả khi phép tính thành công.
- **`tracker_control_task_start()`**: Tạo control priority 6 và stack 768 words; lịch dùng vTaskDelayUntil().

### Luồng phối hợp

Snapshot → freshness → geometry → pulse → PWM nếu được bật.

### Điều cần hiểu đúng

- TRACKER_ENABLE_SERVO_OUTPUT mặc định 0. Có code tính tracking không đồng nghĩa servo đang chạy.
- Timeout nằm trong task này; chưa có task giám sát độc lập, lệnh STOP hoặc watchdog phần cứng trong src hiện tại.


<a id="file-17"></a>

## 17. include/board.h

[Đọc source](../../../freertos/include/board.h)

**Vai trò:** Hợp đồng truy cập phần cứng dùng chung giữa task và board layer.

**Profile sử dụng:** boot, bridge, recovered. Với header, đây là nơi API/cấu hình có ý nghĩa sử dụng; include gián tiếp có thể xuất hiện trong profile khác.

**Ngữ cảnh:** Được compiler đọc; các hàm chạy theo implementation board layer được chọn.

### Biến, kiểu dữ liệu hoặc cấu hình

- BOARD_FC_BAUD_RATE mặc định 57600U nếu CMake chưa định nghĩa; GPS 230400U, servo 330U.
- ID chip-select BMI088 gyro=1, accel=2, BMI270=3.

### Hàm hoặc API chính

- **`API init/time/IO`**: board_init/start_io/poll_io/millis; read FC/GPS/USB, write FC/USB; reset USB RX và callback dữ liệu USB từ ISR.
- **`API sensor/servo`**: board_i2c_read/write, board_spi_read, servo_set/stop; getters handle và board_fatal.

### Luồng phối hợp

Task include board.h → compiler kiểm tra lời gọi → linker chọn implementation từ bridge_board.c hoặc board.c.

### Điều cần hiểu đúng

- Header khai báo API rộng; bridge không cung cấp GPS/I2C/SPI/servo vì không có caller trong profile đó. board_poll_io chỉ được gọi trong bridge.
- boot chỉ dùng phần HAL/board_fatal của hợp đồng; HAL type definitions đến từ stm32h7xx_hal.h.


<a id="file-18"></a>

## 18. include/boot_task.h

[Đọc source](../../../freertos/include/boot_task.h)

**Vai trò:** Các mốc boot, struct chẩn đoán và API bắt đầu boot task/PWM.

**Profile sử dụng:** boot, bridge. Với header, đây là nơi API/cấu hình có ý nghĩa sử dụng; include gián tiếp có thể xuất hiện trong profile khác.

**Ngữ cảnh:** Kiểu dữ liệu và khai báo cho main, boot task và boot board.

### Biến, kiểu dữ liệu hoặc cấu hình

- boot_stage_t: MAIN=1, HAL_READY=2, CLOCK_READY=3, TASK_CREATED=4, TASK_RUNNING=5, PWM_RUNNING=6.
- boot_status_t: boot_stage, iterations, ticks, HAL millis, heap free/min, stack high-water words.
- extern volatile g_boot_status: khai báo biến, vùng nhớ thật được định nghĩa trong boot_task.c.

### Hàm hoặc API chính

- **`boot_board_init(), boot_board_led_pwm_start(), boot_task_start()`**: Khai báo các hàm do boot_board.c và boot_task.c thực hiện.

### Luồng phối hợp

main ghi boot_stage trước scheduler; boot_diag ghi stage và diagnostics sau scheduler.

### Điều cần hiểu đúng

- Đọc p g_boot_status khi halt MCU để lấy snapshot. volatile không thay cho mutex/atomic và không làm toàn struct được đọc đồng thời.


<a id="file-19"></a>

## 19. include/bridge_protocol.h

[Đọc source](../../../freertos/include/bridge_protocol.h)

**Vai trò:** Kiểu ring byte và trạng thái parser MAVLink, cùng API buffer/protocol.

**Profile sử dụng:** bridge, recovered. Với header, đây là nơi API/cấu hình có ý nghĩa sử dụng; include gián tiếp có thể xuất hiện trong profile khác.

**Ngữ cảnh:** Dùng bởi bridge task và implementation thuật toán.

### Biến, kiểu dữ liệu hoặc cấu hình

- bridge_buffer_t: data[4096], read_index/write_index/count/dropped_bytes.
- bridge_protocol_t: rx_message/rx_status ráp một bản tin; frame index/length/version/flags/boundary; thời gian và counters.
- BRIDGE_FRAME_GAP_MS=250; includes mavlink/common/mavlink.h cho kiểu thư viện.

### Hàm hoặc API chính

- **`buffer enqueue/peek/consume/clear`**: API hàng đợi byte chờ gửi.
- **`protocol init/reset/expire/receive/heartbeat`**: API ráp bản tin, reset khung dở và tạo heartbeat.

### Luồng phối hợp

g_to_fc/g_to_usb có kiểu buffer; g_protocol có kiểu parser.

### Điều cần hiểu đúng

- Mỗi kiểu giải quyết một việc: giữ byte chờ truyền và nhớ tiến trình hiểu một bản tin.


<a id="file-20"></a>

## 20. include/bridge_status.h

[Đọc source](../../../freertos/include/bridge_status.h)

**Vai trò:** Tách thống kê do task quản lý và thống kê sự kiện IO/ISR.

**Profile sử dụng:** bridge, recovered. Với header, đây là nơi API/cấu hình có ý nghĩa sử dụng; include gián tiếp có thể xuất hiện trong profile khác.

**Ngữ cảnh:** Kiểu chẩn đoán dùng bởi task, driver và debugger.

### Biến, kiểu dữ liệu hoặc cấu hình

- bridge_status_t: IO/configured, iterations/time/heap/stack, valid/bad/reset/target/heartbeat, link change, pending/drop hai chiều.
- bridge_io_status_t: UART RX/drop/error/restart, TX accepted/completed/start error; USB RX/drop, TX accepted/completed/aborted và reset.
- extern volatile g_bridge_status/g_bridge_io_status định nghĩa trong mavlink_bridge_task.c.

### Hàm hoặc API chính

- **`Không có hàm`**: File này chỉ định nghĩa struct và khai báo biến extern.

### Luồng phối hợp

Driver/ISR ghi IO counters; task tổng hợp protocol/queue/runtime; debugger xem cả hai.

### Điều cần hiểu đúng

- TX accepted khác TX completed. Halt CPU khi muốn so sánh snapshot nhất quán.
- Phần lớn IO counters được bridge_board.c hỗ trợ; board.c recovered chưa cập nhật đầy đủ cùng mức M2.


<a id="file-21"></a>

## 21. include/usb_device.h

[Đọc source](../../../freertos/include/usb_device.h)

**Vai trò:** Giao diện nhỏ của USB CDC ứng dụng.

**Profile sử dụng:** bridge, recovered. Với header, đây là nơi API/cấu hình có ý nghĩa sử dụng; include gián tiếp có thể xuất hiện trong profile khác.

**Ngữ cảnh:** API task và callback reset từ ISR.

### Biến, kiểu dữ liệu hoặc cấu hình

- Chỉ expose bool, size_t, uint8_t; giấu handle USBD và buffer bên usb_device.c.

### Hàm hoặc API chính

- **`usb_device_init(), usb_device_transmit()`**: Bắt đầu USB sau scheduler; nhận đoạn dữ liệu TX khi driver sẵn sàng.
- **`usb_device_connected(), usb_device_reset_from_isr()`**: Lấy configured/generation, hoặc báo reset từ đường IRQ.

### Luồng phối hợp

Bridge task gọi API này; usbd_conf.c báo reset phiên.

### Điều cần hiểu đúng

- Caller phải tôn trọng ngữ cảnh task/ISR và giới hạn đoạn TX 64 byte của implementation.


<a id="file-22"></a>

## 22. include/usbd_conf.h

[Đọc source](../../../freertos/include/usbd_conf.h)

**Vai trò:** Cấu hình thư viện USB Device và ánh xạ các dịch vụ bộ nhớ/log/delay.

**Profile sử dụng:** bridge, recovered. Với header, đây là nơi API/cấu hình có ý nghĩa sử dụng; include gián tiếp có thể xuất hiện trong profile khác.

**Ngữ cảnh:** Middleware đọc macro ở thời điểm compile.

### Biến, kiểu dữ liệu hoặc cấu hình

- Một configuration, một lớp hỗ trợ, max string descriptor 64; self-powered=0.
- Debug/log, LPM, BOS và register callback tùy chọn tắt.
- USBD_malloc/free → static allocator; memset/memcpy chuẩn; Delay → HAL_Delay.

### Hàm hoặc API chính

- **`USBD_static_malloc(), USBD_static_free()`**: Prototype implementation trong usbd_conf.c.

### Luồng phối hợp

USB middleware include usbd_conf.h → dùng cấu hình của ứng dụng.

### Điều cần hiểu đúng

- USBD_MAX_NUM_INTERFACES=1 là giá trị macro hiện tại của middleware; endpoint/interface descriptors của lớp CDC nằm trong thư viện.
- Không phải cấu hình thuật toán MAVLink.


<a id="file-23"></a>

## 23. include/usbd_desc.h

[Đọc source](../../../freertos/include/usbd_desc.h)

**Vai trò:** Expose bảng descriptor USB cho usb_device.c.

**Profile sử dụng:** bridge, recovered. Với header, đây là nơi API/cấu hình có ý nghĩa sử dụng; include gián tiếp có thể xuất hiện trong profile khác.

**Ngữ cảnh:** Khai báo descriptor callback table cho USB init.

### Biến, kiểu dữ liệu hoặc cấu hình

- extern USBD_DescriptorsTypeDef tracker_usb_descriptors; type đến từ usbd_def.h.

### Hàm hoặc API chính

- **`Không có hàm`**: Biến được định nghĩa trong usbd_desc.c; USBD_Init nhận địa chỉ bảng này.

### Luồng phối hợp

usb_device_init → &tracker_usb_descriptors → descriptor callbacks.

### Điều cần hiểu đúng

- extern chỉ thông báo biến tồn tại; không cấp vùng nhớ mới ở mỗi file include.


<a id="file-24"></a>

## 24. include/app_state.h

[Đọc source](../../../freertos/include/app_state.h)

**Vai trò:** Mô hình trạng thái tracker và các hàm cập nhật/snapshot.

**Profile sử dụng:** bridge, recovered. Với header, đây là nơi API/cấu hình có ý nghĩa sử dụng; include gián tiếp có thể xuất hiện trong profile khác.

**Ngữ cảnh:** Kiểu dữ liệu/API trao đổi giữa task.

### Biến, kiểu dữ liệu hoặc cấu hình

- tracker_position_t: lat/lon e7, altitude mm, timestamp ms, valid.
- tracker_state_snapshot_t: target (máy bay), station (trạm anten), heading độ/timestamp/valid, heartbeat FC/timestamp.

### Hàm hoặc API chính

- **`app_state_init(), app_state_update_*(), app_state_snapshot()`**: Khai báo API mutex-protected trong app_state.c; caller không truy cập trực tiếp g_state qua header.

### Luồng phối hợp

Giao tiếp cung cấp target; sensor cung cấp station/heading; control dùng snapshot.

### Điều cần hiểu đúng

- Ví dụ latitude_e7=110000000 nghĩa 11°; altitude_mm=23700 nghĩa 23.7 m.
- Header không tự tạo mutex, không tự cập nhật hay hết hạn dữ liệu.


<a id="file-25"></a>

## 25. include/gps_ubx.h

[Đọc source](../../../freertos/include/gps_ubx.h)

**Vai trò:** Tách trạng thái đang ráp UBX và vị trí đã giải mã.

**Profile sử dụng:** recovered. Với header, đây là nơi API/cấu hình có ý nghĩa sử dụng; include gián tiếp có thể xuất hiện trong profile khác.

**Ngữ cảnh:** Kiểu/parser API cho sensor task.

### Biến, kiểu dữ liệu hoặc cấu hình

- gps_ubx_parser_t: state, class/ID, length/offset, checksum A/B, checksum A đã nhận và payload[92].
- gps_ubx_position_t: lat/lon e7, altitude_msl_mm, fix_type, satellites, valid.

### Hàm hoặc API chính

- **`gps_ubx_init(), gps_ubx_parse()`**: Init parser, nhận một byte; trả true khi NAV-PVT checksum đúng đã được giải mã vào position.

### Luồng phối hợp

Byte đến rời rạc được tích lũy trong parser; position chỉ xuất khi hoàn chỉnh.

### Điều cần hiểu đúng

- altitude_msl_mm là cao độ so với mực nước biển, đơn vị mm; không phải cao tương đối so với điểm home.
- valid là điều kiện fix của parser hiện tại; sensor task dùng valid để quyết định update station.


<a id="file-26"></a>

## 26. include/tracker_tasks.h

[Đọc source](../../../freertos/include/tracker_tasks.h)

**Vai trò:** Danh sách hàm khởi tạo ba task ứng dụng.

**Profile sử dụng:** bridge, recovered. Với header, đây là nơi API/cấu hình có ý nghĩa sử dụng; include gián tiếp có thể xuất hiện trong profile khác.

**Ngữ cảnh:** main include để tạo task.

### Biến, kiểu dữ liệu hoặc cấu hình

- Không khai báo TCB hoặc stack thủ công; xTaskCreate trong các file .c tạo chúng qua FreeRTOS.

### Hàm hoặc API chính

- **`mavlink_bridge_task_start()`**: Implementation mavlink_bridge_task.c; bridge và recovered.
- **`sensor_task_start(), tracker_control_task_start()`**: Implementation sensor_task.c và tracker_control_task.c; recovered.

### Luồng phối hợp

main → *_task_start → xTaskCreate → scheduler chạy hàm task.

### Điều cần hiểu đúng

- Các hàm *_start trả bool thành công tạo task; không phải chính vòng xử lý vô hạn của task.


<a id="file-27"></a>

## 27. include/tracker_config.h

[Đọc source](../../../freertos/include/tracker_config.h)

**Vai trò:** ID MAVLink, lịch control, timeout và công tắc đầu ra servo.

**Profile sử dụng:** bridge, recovered. Với header, đây là nơi API/cấu hình có ý nghĩa sử dụng; include gián tiếp có thể xuất hiện trong profile khác.

**Ngữ cảnh:** Macro ứng dụng, được quyết định khi compile.

### Biến, kiểu dữ liệu hoặc cấu hình

- TRACKER_SYSTEM_ID=1, COMPONENT_ID=191, TARGET_SYSTEM_ID=1.
- CONTROL_RATE_HZ=50, telemetry timeout=2000 ms, station GPS=3000 ms, heading=1000 ms.
- TRACKER_ENABLE_SERVO_OUTPUT=0.

### Hàm hoặc API chính

- **`Không có hàm`**: Macro được bridge_protocol, bridge task và control task sử dụng.

### Luồng phối hợp

ID → heartbeat/filter target; rate/timeout/output switch → control.

### Điều cần hiểu đúng

- Đây là cấu hình compile-time, chưa có parameter service thay đổi lúc chạy.
- Macro output=0 giữ servo tắt trong control task; cần xác nhận chiều quay, giới hạn và stop trước khi bật.


<a id="file-28"></a>

## 28. include/tracker_math.h

[Đọc source](../../../freertos/include/tracker_math.h)

**Vai trò:** Hợp đồng đầu vào/đầu ra cho geometry và đổi xung servo.

**Profile sử dụng:** recovered. Với header, đây là nơi API/cấu hình có ý nghĩa sử dụng; include gián tiếp có thể xuất hiện trong profile khác.

**Ngữ cảnh:** Kiểu/API tính toán cho control.

### Biến, kiểu dữ liệu hoặc cấu hình

- tracker_solution_t: bearing_deg/elevation_deg, horizontal_distance_m/slant_distance_m.
- Đầu vào vị trí dùng int32 e7/mm; đầu ra xung yaw/pitch uint16 microsecond.

### Hàm hoặc API chính

- **`tracker_solution_compute()`**: Tính góc/khoảng cách từ vị trí station và target.
- **`tracker_solution_to_servo()`**: Dùng solution và heading trạm đổi thành yaw_us/pitch_us.

### Luồng phối hợp

Control include header → gọi implementation tracker_math.c.

### Điều cần hiểu đúng

- Tên _deg/_m/_mm/_e7/_us ghi rõ đơn vị, tránh trộn độ/radian hoặc mm/m.


<a id="file-29"></a>

## 29. include/FreeRTOSConfig.h

[Đọc source](../../../freertos/include/FreeRTOSConfig.h)

**Vai trò:** Quy định scheduler, tick, heap, đồng bộ, task timer và mức ngắt gọi API RTOS.

**Profile sử dụng:** boot, bridge, recovered. Với header, đây là nơi API/cấu hình có ý nghĩa sử dụng; include gián tiếp có thể xuất hiện trong profile khác.

**Ngữ cảnh:** Cấu hình kernel/port ở compile time.

### Biến, kiểu dữ liệu hoặc cấu hình

- Preemption=1, time slicing=1, tickless=0; tick 1000 Hz (1 ms), 8 mức task priority 0–7, số tick không dùng 16 bit.
- Heap 96 KiB; minimum stack 256 words; mutex, counting semaphore, task notification bật; recursive mutex/queue sets tắt.
- Software timer daemon priority 2, stack 512 words, queue 8; configUSE_TIMERS=1 tạo daemon khi scheduler bắt đầu.
- Malloc hook, tick hook, stack overflow check level 2 bật; FPU bật, MPU tắt; SVC/PendSV ánh xạ sang port.

### Hàm hoặc API chính

- **`configASSERT(condition)`**: Nếu điều kiện false gọi tracker_assert_failed() trong main.c.
- **`INCLUDE_* và config*`**: Chọn API/feature kernel được cung cấp, như delay, delayUntil, tick, stack watermark, scheduler state.

### Luồng phối hợp

Kernel/port trong .deps dùng macro này; task và ISR phải tuân thủ cấu hình.

### Điều cần hiểu đúng

- Task priority: số lớn ưu tiên cao. NVIC priority: số nhỏ ưu tiên cao. Hai thang không so sánh trực tiếp.
- configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY=5: các IRQ gọi FromISR phải có mức số 5–15 với cấu hình 4 bit hiện tại; UART/USB dùng 6. SysTick/PendSV mức thấp 15.
- Bật timer service không có nghĩa LED PWM được timer task tạo; TIM15/TIM1 là timer phần cứng riêng.


<a id="file-30"></a>

## 30. include/stm32h7xx_hal_conf.h

[Đọc source](../../../freertos/include/stm32h7xx_hal_conf.h)

**Vai trò:** Chọn module HAL và giá trị clock/callback/assert dùng trong driver.

**Profile sử dụng:** boot, bridge, recovered. Với header, đây là nơi API/cấu hình có ý nghĩa sử dụng; include gián tiếp có thể xuất hiện trong profile khác.

**Ngữ cảnh:** HAL STM32 đọc macro và include ở compile time.

### Biến, kiểu dữ liệu hoặc cấu hình

- Bật HAL Cortex/DMA/EXTI/Flash/GPIO/I2C/PCD/PWR/RCC/SPI/TIM/UART.
- HSE_VALUE=8 MHz, HSI=64 MHz, CSI=4 MHz, LSI=32 kHz, LSE=32768 Hz; VDD_VALUE=3300 mV, tick IRQ priority=15.
- Các USE_HAL_*_REGISTER_CALLBACKS=0 dùng callback được override bằng tên hàm HAL thay vì đăng ký con trỏ lúc chạy.

### Hàm hoặc API chính

- **`assert_param(), assert_failed()`**: Nếu USE_FULL_ASSERT được bật thì assert_param gọi assert_failed(file,line); cấu hình CMake hiện chưa bật macro đó nên assert_param không kiểm tra.
- **`Các #include HAL module`**: Làm các kiểu/hàm HAL của module đã bật có sẵn cho stm32h7xx_hal.h.

### Luồng phối hợp

board.h → stm32h7xx_hal.h → HAL configuration → driver types/functions.

### Điều cần hiểu đúng

- USE_RTOS=0 là macro cấu hình của HAL; scheduler vẫn là FreeRTOS được main khởi động.
- Định nghĩa HSE_VALUE không bật oscillator; thao tác bật và cấu hình PLL thực hiện trong main. Bật module DMA trong HAL không đồng nghĩa đường UART/USB đang dùng DMA.

## 3. Thứ tự đọc gợi ý

### Nếu đang học M2

1. main.c → FreeRTOSConfig.h: task được tạo và scheduler bắt đầu ở đâu.
2. board.h → bridge_board.c → interrupts.c: phần cứng đưa byte vào task thế nào.
3. mavlink_bridge_task.c → bridge_protocol.h/.c: byte đi hai chiều và parser nhận bản tin thế nào.
4. usb_device.c → usbd_conf.c → usbd_desc.c: CDC ứng dụng, adapter phần cứng và danh tính USB.
5. app_state.h/.c → bridge_status.h: dữ liệu target và số liệu debugger.
6. boot_task.h/.c → boot_board.c: chẩn đoán task và PWM LED độc lập.

### Khi chuyển sang sensor/tracking

board.c → gps_ubx.h/.c → sensor_task.c → app_state.h/.c → tracker_config.h → tracker_math.h/.c → tracker_control_task.c.

### Từ vựng khi đọc code

- **Handle HAL:** struct lưu peripheral Instance, Init và trạng thái driver; peripheral thật là phần cứng MCU.
- **static biến ở cấp file:** chỉ file đó thấy tên biến, vùng nhớ tồn tại suốt chương trình.
- **static hàm:** chỉ có thể gọi trực tiếp bằng tên từ trong cùng file C.
- **extern:** khai báo biến/hàm được định nghĩa ở nơi khác, không tạo một bản sao biến mới.
- **volatile:** yêu cầu compiler thực hiện truy cập biến theo quy tắc volatile; không tự tạo mutex/atomic hay bảo đảm snapshot nhiều trường.
- **typedef struct:** đặt tên kiểu dữ liệu; khai báo kiểu chưa cấp vùng nhớ cho một instance.
- **Mutex:** bảo vệ dữ liệu chung trong task; lấy/nhả mutex khác việc ISR gửi stream qua FromISR.
- **Critical section:** che các mức IRQ theo cơ chế port trong khoảng ngắn; không dùng để thực hiện công việc dài.
- **FromISR + portYIELD_FROM_ISR:** thao tác RTOS phù hợp ngữ cảnh IRQ và yêu cầu chuyển task nếu đánh thức task cần chạy.
- **Stack words:** trên port Cortex-M7 này 1 word=4 byte; watermark là lượng dư tối thiểu từng quan sát, không phải cam kết không overflow trong tương lai.

## 4. Trạng thái triển khai nhìn từ source

- Có boot/task diagnostics, LED PWM phần cứng, bridge UART/USB hai chiều, parser MAVLink và cập nhật target.
- Có source GPS UBX, QMC5883L, geometry, đổi xung servo và kiểm tra dữ liệu cũ trong recovered.
- Chưa có IMU fusion/PID/lệnh STOP/watchdog độc lập trong những file này. Sensor task hiện chưa sử dụng driver IMU cũ.
- Servo output mặc định tắt. Heading chưa có hiệu chuẩn trục/hard-soft iron/bù nghiêng trong implementation này.
- UART và counter/retry của board.c recovered khác bridge_board.c M2; không suy ra mọi hành vi M2 đã được chuyển sang recovered.
- Đây là tài liệu đọc source; không chạy build, test hoặc thao tác board trong lần giải thích này.
