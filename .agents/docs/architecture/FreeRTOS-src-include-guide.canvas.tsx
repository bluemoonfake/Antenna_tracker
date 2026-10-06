import { Button, Callout, Card, CardBody, CardHeader, Code, CollapsibleSection, Divider, Grid, H1, H2, H3, Row, Select, Stack, Table, Text, useCanvasAction, useCanvasState, useHostTheme } from "cursor/canvas";

const ROOT = "/home/ubuntu/Dev/PX4_tracker/Tracker/Antenna_tracker";
const GUIDE = "/home/ubuntu/Dev/PX4_tracker/Tracker/Antenna_tracker/.agents/docs/architecture/freertos-src-include-guide.md";
const FILES = [
  {
    "path": "src/main.c",
    "group": "Khởi động",
    "profiles": [
      "boot",
      "bridge",
      "recovered"
    ],
    "context": "Trước scheduler; tick hook và fault hook trong runtime.",
    "role": "Điểm vào của ứng dụng: chuẩn bị clock, tài nguyên và task, sau đó giao CPU cho FreeRTOS.",
    "data": [
      "g_bootloader_delay_signature: hằng uint64_t được đặt trong section .bootloader_signature và giữ lại bởi linker; metadata cho bootloader đang có trên board.",
      "Ba struct local oscillator, clock, peripheral chứa cấu hình RCC trước khi HAL ghi vào phần cứng."
    ],
    "functions": [
      [
        "system_clock_config()",
        "Chọn LDO, bật SYSCFG rồi VOS0 và chờ VOSRDY. Với HSE=8 MHz: PLL1 VCO=960 MHz, P=480 MHz cho SYSCLK, Q=240 MHz, R=120 MHz. Chọn HCLK=240 MHz, APB1/2/3/4=120 MHz. PLL3 Q=48 MHz cấp USB; SPI123 chọn PLL1 Q, I2C và UART chọn clock bus tương ứng. Các lỗi HAL dẫn đến board_fatal()."
      ],
      [
        "main()",
        "boot/bridge: khởi tạo LED và đánh dấu từng mốc. Bật I-cache/D-cache, HAL_Init(), cấu hình clock. boot tạo boot_diag; bridge tạo boot_diag, board, app_state và mav_bridge; recovered tạo board, app_state, mav_bridge, sensors và control. xTaskCreate mới tạo task; vTaskStartScheduler() bắt đầu thực thi task."
      ],
      [
        "vApplicationTickHook()",
        "Gọi HAL_IncTick() mỗi RTOS tick để HAL_GetTick() tiếp tục tăng sau khi scheduler chạy."
      ],
      [
        "vApplicationMallocFailedHook(), vApplicationStackOverflowHook(), tracker_assert_failed()",
        "Đưa lỗi cấp phát, kiểm tra stack hoặc configASSERT về board_fatal(). Nếu scheduler trả về, main cũng gọi fatal."
      ]
    ],
    "flow": "Reset/startup bên .deps → main → tạo task → vTaskStartScheduler → task ready có priority cao nhất chạy.",
    "notes": [
      "File này là ứng dụng tại 0x08020000. Chữ boot trong profile chỉ mốc kiểm tra khởi động, không tạo bootloader mới.",
      "Clock và cache được thiết lập trước scheduler. USB và UART RX được bắt đầu trong task bridge sau scheduler."
    ]
  },
  {
    "path": "src/boot_board.c",
    "group": "Khởi động",
    "profiles": [
      "boot",
      "bridge"
    ],
    "context": "Init trước scheduler; bắt đầu PWM trong boot_diag; fatal có thể được gọi từ task/hook/exception.",
    "role": "Phần phần cứng tối thiểu để quan sát boot bằng LED và giữ chân servo ở mức thấp.",
    "data": [
      "g_led_timer: handle HAL của TIM15.",
      "LED PE4/PE5/PE6 active low; TIM15 CH2 dùng PE6 AF4. PE13/PE14 giữ GPIO low trong boot/bridge."
    ],
    "functions": [
      [
        "boot_board_init()",
        "Bật clock GPIOE; đặt latch PE4/5/6 high để LED tắt, PE13/14 low rồi cấu hình output push-pull. main bật PE4 để báo đã vào main."
      ],
      [
        "boot_board_led_pwm_start()",
        "Reset/bật TIM15; chia timer clock 240 MHz về tick 1 MHz, ARR=999, CCR2=500. PWM 1 kHz, duty 50%, polarity low. Bắt đầu PWM rồi chuyển PE6 sang AF4; tắt LED xanh dương PE4."
      ],
      [
        "board_fatal()",
        "Tắt IRQ, reset/tắt TIM15, đưa GPIO về trạng thái init rồi bật LED đỏ PE5 và đứng trong vòng lặp vô hạn."
      ]
    ],
    "flow": "boot_diag gọi PWM start một lần → timer tự tạo xung → task tiếp tục cập nhật chẩn đoán.",
    "notes": [
      "PWM này không dùng timer ISR hoặc DMA; đèn còn sáng không tự chứng minh task còn chạy.",
      "board_fatal() này chỉ được link trong boot/bridge; recovered dùng phiên bản trong board.c."
    ]
  },
  {
    "path": "src/boot_task.c",
    "group": "Khởi động",
    "profiles": [
      "boot",
      "bridge"
    ],
    "context": "Task boot_diag: priority 1, stack 256 words = 1024 byte trên Cortex-M7.",
    "role": "Bắt đầu PWM LED và cập nhật số liệu chứng minh scheduler/task hoạt động.",
    "data": [
      "g_boot_status: struct volatile để xem bằng debugger.",
      "wake_time và period: mốc thời gian cho vTaskDelayUntil(), period=500 ms."
    ],
    "functions": [
      [
        "boot_task()",
        "Đánh dấu TASK_RUNNING; bắt đầu PWM LED; đánh dấu PWM_RUNNING. Mỗi vòng tăng task_iterations, chụp RTOS ticks, HAL millis, heap còn trống, heap thấp nhất và stack high-water; ngủ tới mốc 500 ms tiếp theo."
      ],
      [
        "boot_task_start()",
        "xTaskCreate(..., \"boot_diag\", 256, NULL, 1, NULL); trả true khi task được tạo."
      ]
    ],
    "flow": "Mỗi 500 ms: thức dậy → chụp số liệu → Blocked → đến hạn lại Ready.",
    "notes": [
      "stack_high_water_words là lượng stack tối thiểu còn dư từng quan sát được, không phải tổng stack hay stack đang dùng.",
      "vTaskDelayUntil() giữ mốc lịch định kỳ; có thể trễ nếu task khác/ngắt chiếm CPU."
    ]
  },
  {
    "path": "src/interrupts.c",
    "group": "Ngắt",
    "profiles": [
      "boot",
      "bridge",
      "recovered"
    ],
    "context": "Exception/ISR của CPU, chạy ngoài lịch task thông thường.",
    "role": "Các điểm vào ngắt chuyển sự kiện phần cứng cho HAL hoặc FreeRTOS.",
    "data": [
      "g_usb_pcd được định nghĩa trong usbd_conf.c; IRQ USB dùng handle này.",
      "xPortSysTickHandler() được cung cấp bởi FreeRTOS port."
    ],
    "functions": [
      [
        "HardFault_Handler(), MemManage_Handler(), BusFault_Handler(), UsageFault_Handler()",
        "Chuyển các exception lỗi đến board_fatal(). NMI_Handler và DebugMon_Handler hiện có thân rỗng."
      ],
      [
        "SysTick_Handler()",
        "Trước scheduler: HAL_IncTick(). Sau khi scheduler bắt đầu: xPortSysTickHandler() xử lý RTOS tick; tick hook trong main tăng HAL tick."
      ],
      [
        "USART1_IRQHandler()",
        "Gọi HAL_UART_IRQHandler(board_fc_uart_handle()); HAL gọi RX/TX/error callback tương ứng trong board layer."
      ],
      [
        "USART3_IRQHandler()",
        "Trong recovered, chuyển IRQ GPS cho HAL. Trong bridge, thân không gọi HAL GPS; boot không biên dịch nhóm IRQ này."
      ],
      [
        "OTG_FS_IRQHandler()",
        "Gọi HAL_PCD_IRQHandler(&g_usb_pcd), bắt đầu chuỗi xử lý USB."
      ]
    ],
    "flow": "IRQ → handler → HAL → callback → stream buffer → task đọc dữ liệu.",
    "notes": [
      "SVC_Handler và PendSV_Handler được ánh xạ sang port FreeRTOS qua FreeRTOSConfig.h; code lưu/khôi phục context nằm trong port .deps.",
      "NVIC priority 6 của IRQ và task priority 6 của control thuộc hai hệ khác nhau."
    ]
  },
  {
    "path": "src/bridge_board.c",
    "group": "Phần cứng",
    "profiles": [
      "bridge"
    ],
    "context": "Init trước scheduler; API read/write trong task; HAL callback trong ISR.",
    "role": "Board layer của M2: UART1 và stream buffer USB, gửi UART bằng interrupt, có bộ đếm lỗi và retry RX.",
    "data": [
      "g_uart: USART1 PA9 TX / PA10 RX, AF7, BOARD_FC_BAUD_RATE, 8N1.",
      "g_fc_rx và g_usb_rx: hai FreeRTOS stream buffer, mỗi buffer 4096 byte.",
      "g_rx_byte: một byte đang nhận qua HAL. g_tx_buffer[128]: vùng nhớ driver sở hữu cho TX; g_tx_busy/g_tx_length/g_rx_restart theo dõi tiến trình."
    ],
    "functions": [
      [
        "board_init()",
        "Tạo stream buffer FC/USB, bật GPIOA/USART1, cấu hình chân và UART. IRQ chưa bật để tránh callback gọi RTOS trước scheduler."
      ],
      [
        "board_start_io()",
        "Assert scheduler đang chạy, đặt IRQ USART1 priority 6, xóa pending, đăng ký HAL_UART_Receive_IT(..., 1 byte), bật IRQ."
      ],
      [
        "restart_rx(), board_poll_io()",
        "Đăng ký lần nhận tiếp theo; nếu thất bại thì ghi cờ/counter. Task gọi poll để thử lại khi HAL RX đã READY."
      ],
      [
        "board_fc_read(), board_usb_read()",
        "Lấy byte từ stream buffer; wait_ms=0 trả ngay, wait_ms>0 có thể block chờ dữ liệu."
      ],
      [
        "board_fc_write()",
        "Chấp nhận tối đa 128 byte khi TX rảnh; copy vào g_tx_buffer rồi HAL_UART_Transmit_IT(). true nghĩa driver nhận đoạn dữ liệu, chưa có nghĩa đã gửi hết. timeout_ms không dùng trong phiên bản này."
      ],
      [
        "HAL_UART_RxCpltCallback(), HAL_UART_ErrorCallback(), HAL_UART_TxCpltCallback()",
        "RX: đưa byte vào stream qua FromISR, đếm byte/drop, rearm RX. Error: đếm và thử rearm nếu READY. TX complete: cộng byte hoàn tất và bỏ cờ busy."
      ],
      [
        "board_usb_rx_from_isr(), board_usb_reset_rx(), board_usb_write()",
        "Đưa USB RX vào stream và đếm drop; reset stream trong critical section khi đổi phiên USB; chuyển TX cho usb_device_transmit()."
      ],
      [
        "board_millis(), board_fc_uart_handle()",
        "Cung cấp HAL_GetTick() và địa chỉ handle cho interrupts.c."
      ]
    ],
    "flow": "UART RX IRQ → stream g_fc_rx → mav_bridge → ring g_to_usb → USB TX.",
    "notes": [
      "Hai stream RX thuộc board layer; hai ring TX g_to_fc/g_to_usb thuộc task bridge. Chúng là các tầng buffer khác nhau.",
      "File này cung cấp cùng API board.h với board.c, được CMake chọn thay thế; không khởi tạo GPS, I2C, SPI hoặc servo."
    ]
  },
  {
    "path": "src/board.c",
    "group": "Phần cứng",
    "profiles": [
      "recovered"
    ],
    "context": "Init trước scheduler; read/write HAL từ task; UART/USB RX callback trong ISR.",
    "role": "Board layer rộng hơn cho bản recovered: UART FC/GPS, I2C, SPI và timer PWM servo.",
    "data": [
      "g_fc_uart USART1 PA9/PA10; g_gps_uart USART3 PD8/PD9, GPS baud 230400.",
      "g_i2c1 PB6/PB7 external; g_i2c2 PB10/PB11 internal; g_spi2 PC2/PC3/PD3.",
      "CS: BMI088 gyro PD5, accel PD4, BMI270 PA15. g_tim1 CH3 PE13 pitch, CH4 PE14 yaw.",
      "Ba stream buffer RX 4096 byte: FC, GPS, USB; hai biến RX byte và cờ g_servo_started."
    ],
    "functions": [
      [
        "gpio_init()",
        "Bật clock GPIOA–E, CS output high; UART AF7 push-pull; I2C AF4 open-drain/pull-up; SPI AF5; servo AF1 TIM1. Cấu hình mode/chân chưa tự tạo tín hiệu PWM."
      ],
      [
        "uart_init()",
        "Khởi tạo USART1 và USART3 8N1; copy cấu hình FC sang GPS rồi đổi Instance/baud; đặt NVIC priority 6."
      ],
      [
        "i2c_init()",
        "Khởi tạo I2C1/2 địa chỉ 7 bit, timing 0x10c0ecff, analog filter bật. Đây là giá trị cấu hình bus, không phải địa chỉ sensor."
      ],
      [
        "spi_init()",
        "SPI2 master 8 bit, mode 3 (CPOL high, CPHA second edge), MSB first, prescaler 128; chip-select điều khiển bằng GPIO."
      ],
      [
        "pwm_init()",
        "TIM1 prescaler 239: với timer clock 240 MHz, mỗi tick 1 µs. ARR=(1000000/330)-1; cấu hình CH3/4 PWM1, pulse=0. Tần số xấp xỉ 330 Hz do phép chia nguyên; chưa HAL_TIM_PWM_Start()."
      ],
      [
        "board_init(), board_start_io()",
        "Init GPIO, tạo ba stream rồi init UART/I2C/SPI/PWM. Sau scheduler, start_io bắt đầu UART RX từng byte và bật IRQ1/3."
      ],
      [
        "stream_read(), board_fc_read(), board_gps_read(), board_usb_read(), board_millis()",
        "Các wrapper nhận stream buffer với timeout và cung cấp thời gian HAL."
      ],
      [
        "board_fc_write(), board_usb_write(), board_usb_reset_rx()",
        "UART TX ở đây dùng HAL_UART_Transmit() polling đồng bộ, có timeout; USB TX dùng usb_device; reset RX USB xóa byte chờ và tính drop."
      ],
      [
        "board_usb_rx_from_isr(), HAL_UART_RxCpltCallback(), HAL_UART_ErrorCallback()",
        "Đưa RX vào stream và yêu cầu yield khi cần; UART phân biệt Instance1/3 và đăng ký nhận byte tiếp theo. Bản này bỏ qua return code của send/rearm và thiếu phần lớn counter/retry của bridge_board.c."
      ],
      [
        "board_i2c_read(), board_i2c_write()",
        "Đọc/ghi thanh ghi sensor bằng HAL_I2C_Mem_*, chuyển địa chỉ 7 bit sang address<<1 theo API HAL, timeout 20 ms."
      ],
      [
        "chip_select_port(), board_spi_read()",
        "Chọn CS theo ID board; kéo CS low, gửi reg|0x80 rồi nhận dữ liệu, kéo CS high. Đây là thao tác SPI cơ bản; sensor đặc thù còn cần quy trình riêng."
      ],
      [
        "board_servo_set(), board_servo_stop()",
        "Clamp yaw 800–2200 µs, pitch 1100–2200 µs; cập nhật compare CH4/CH3 và start PWM lần đầu. stop dừng cả hai channel."
      ],
      [
        "board_i2c_external(), board_i2c_internal(), board_fc_uart_handle(), board_gps_uart_handle()",
        "Trả địa chỉ handle ngoại vi để sensor task hoặc interrupt handler sử dụng."
      ],
      [
        "board_fatal()",
        "Tắt IRQ rồi đứng vô hạn. Khác boot_board.c, phiên bản này không bật LED lỗi hay gọi dừng PWM."
      ]
    ],
    "flow": "sensor task dùng GPS/I2C; control dùng PWM; mav_bridge dùng FC UART/USB qua API board.h.",
    "notes": [
      "Khi build bridge, file này không được link. Hành vi UART TX ở đây khác M2: polling có thể giữ CPU trong lời gọi HAL.",
      "SPI được khởi tạo nhưng sensor_task.c hiện chưa đọc IMU BMI088/BMI270; I2C2 cũng chưa được sensor task hiện tại dùng."
    ]
  },
  {
    "path": "src/mavlink_bridge_task.c",
    "group": "Giao tiếp",
    "profiles": [
      "bridge",
      "recovered"
    ],
    "context": "Task mav_bridge: priority 5, stack 1536 words = 6144 byte.",
    "role": "Điều phối bridge hai chiều, phân tích MAVLink từ FC, cập nhật target và chèn heartbeat tracker.",
    "data": [
      "g_to_fc/g_to_usb: hai ring byte TX 4096; g_protocol: trạng thái parser persistent.",
      "g_bridge_status và g_bridge_io_status được định nghĩa ở đây; driver/ISR cập nhật bộ đếm IO.",
      "USB generation, rx_loss và thời gian giúp nhận biết thay phiên USB và mất byte UART."
    ],
    "functions": [
      [
        "mavlink_bridge_task()",
        "Init parser; init USB và bắt đầu UART RX sau scheduler. Mỗi vòng kiểm tra USB session/configuration, clear ring và USB RX nếu đổi session; reset parser khi UART loss/error tăng, expire frame dở, nhận FC/USB, flush TX, cập nhật diagnostics rồi vTaskDelay(1 tick)."
      ],
      [
        "receive_fc()",
        "Đọc tối đa 128 byte; từng byte đưa vào g_to_usb nếu USB configured, đồng thời đưa vào parser. Khi parser trả bản tin hợp lệ, gọi handle_fc_message(). Thử chèn heartbeat tại ranh giới wire frame."
      ],
      [
        "handle_fc_message()",
        "Chỉ xử lý SYSID=TRACKER_TARGET_SYSTEM_ID=1: HEARTBEAT cập nhật timestamp, GLOBAL_POSITION_INT decode lat/lon/alt và cập nhật target. Các message khác vẫn đi qua đường chuyển tiếp."
      ],
      [
        "receive_usb()",
        "Đọc tối đa 128 byte USB và enqueue vào g_to_fc. Chiều này chuyển byte nguyên trạng, không có parser lệnh STOP trong file."
      ],
      [
        "flush_outputs()",
        "Peek ring, gửi tối đa 128 byte FC hoặc 64 byte USB. Chỉ consume khi board API nhận dữ liệu. Trong bridge, driver copy nên ring có thể tái sử dụng byte trước khi TX hoàn tất."
      ],
      [
        "update_diagnostics()",
        "Loop/frame/drop/pending counters cập nhật mỗi vòng. Time/heap/stack lấy khoảng mỗi 500 ms. tracker_heartbeats đếm bản tin đã enqueue, chưa đảm bảo host đã nhận."
      ],
      [
        "mavlink_bridge_task_start()",
        "Tạo task mav_bridge priority 5 và trả kết quả xTaskCreate."
      ]
    ],
    "flow": "FC bytes → USB queue; FC message hợp lệ → app_state.target. USB bytes → FC queue.",
    "notes": [
      "vTaskDelay(1) là ngủ một tick sau công việc; chu kỳ cả vòng gồm thời gian xử lý và chờ khác, không đảm bảo chính xác 1 ms.",
      "USB disconnected vẫn có thể parse FC để cập nhật target, nhưng byte FC không giữ lại để gửi ở phiên USB sau."
    ]
  },
  {
    "path": "src/bridge_protocol.c",
    "group": "Giao tiếp",
    "profiles": [
      "bridge",
      "recovered"
    ],
    "context": "Hàm thường chạy trong mav_bridge, không tạo task/ISR và không gọi HAL trực tiếp.",
    "role": "Thuật toán ring buffer, nhận diện khung MAVLink, kiểm tra parser và tạo heartbeat.",
    "data": [
      "bridge_buffer_t chứa byte TX nguyên gốc; bridge_protocol_t giữ trạng thái một khung đang nhận và thống kê."
    ],
    "functions": [
      [
        "bridge_buffer_enqueue()",
        "Nếu không đủ chỗ cho toàn đoạn thì từ chối và cộng dropped_bytes; nếu đủ thì copy tối đa hai đoạn khi vòng qua cuối mảng, cập nhật write_index/count."
      ],
      [
        "bridge_buffer_peek(), bridge_buffer_consume(), bridge_buffer_clear()",
        "Peek trả đoạn liên tục tối đa limit và con trỏ, chưa xóa. Consume dịch read_index sau khi gửi được nhận. Clear loại byte đang chờ, cộng drop rồi reset các index."
      ],
      [
        "bridge_protocol_init(), bridge_protocol_reset(), bridge_protocol_expire()",
        "Init zero và at_boundary=true. Reset bỏ tiến trình frame dở, tăng resets và at_boundary=false; khi chưa nhận xong frame mà im 250 ms thì expire reset."
      ],
      [
        "bridge_protocol_receive()",
        "Nhận từng byte. Nhận diện MAVLink1/2, cộng payload length vào tổng frame 8/12 byte overhead, cộng 13 byte nếu v2 signed. Bỏ local parsing khi có incompat flags không hỗ trợ. Gọi mavlink_frame_char_buffer(); tại cuối khung mới trả true nếu hợp lệ, đếm valid/bad và mở boundary."
      ],
      [
        "bridge_protocol_heartbeat()",
        "Nếu đang ở boundary, ring đủ chỗ và đã ít nhất 1000 ms từ lần enqueue trước: pack heartbeat SYS1/COMP191, serialize byte rồi enqueue to_usb."
      ]
    ],
    "flow": "Byte nhận vừa được chuyển tiếp bởi task vừa được thuật toán đọc; chỉ bản tin hợp lệ mới cấp cho cập nhật trạng thái.",
    "notes": [
      "CRC lỗi vẫn có thể được chuyển tiếp nguyên byte vì đường forwarding ở task độc lập với kết quả parse.",
      "Code tính độ dài khung signed để giữ ranh giới; không thiết lập khóa hoặc chính sách xác thực MAVLink signing. Heartbeat có thể bị hoãn khi ring đầy hoặc đang giữa frame."
    ]
  },
  {
    "path": "src/usb_device.c",
    "group": "USB",
    "profiles": [
      "bridge",
      "recovered"
    ],
    "context": "Init/TX/connected trong task; CDC RX/TX complete/reset callbacks qua USB IRQ.",
    "role": "Lớp USB CDC ứng dụng: cổng serial ảo, buffer sở hữu của driver và trạng thái phiên USB.",
    "data": [
      "g_usb_device: handle middleware; RX/TX buffer mỗi cái 64 byte.",
      "g_tx_busy/g_tx_length theo dõi TX đang chạy; g_generation tăng khi init/deinit/reset phiên.",
      "g_line_coding[7]: baud, stop bits, parity, data bits được host đọc/ghi cho cổng ảo."
    ],
    "functions": [
      [
        "usb_device_init()",
        "Assert scheduler running; USBD_Init với descriptor, đăng ký lớp CDC và callback interface, USBD_Start."
      ],
      [
        "cdc_init(), cdc_deinit(), abort_tx()",
        "Bỏ TX đang chờ và đếm aborted nếu có, tăng generation; init gắn buffer RX/TX vào CDC."
      ],
      [
        "cdc_control()",
        "Xử lý SET_LINE_CODING/GET_LINE_CODING. Thay line coding cổng USB ảo không thay baud USART1 phần cứng."
      ],
      [
        "cdc_receive()",
        "Chuyển byte cho board_usb_rx_from_isr(), gắn lại buffer và đăng ký nhận packet kế tiếp."
      ],
      [
        "cdc_transmit_complete()",
        "Cộng byte TX hoàn tất, bỏ busy để task có thể gửi đoạn tiếp."
      ],
      [
        "usb_device_transmit()",
        "Chỉ nhận đoạn 1–64 byte khi configured, class data tồn tại và TX rảnh. Trong critical section, copy vào buffer riêng rồi yêu cầu CDC transmit. true nghĩa được nhận để gửi."
      ],
      [
        "usb_device_connected(), usb_device_reset_from_isr()",
        "Đọc configured/generation có bảo vệ; reset abort TX, tăng generation và reset_events."
      ]
    ],
    "flow": "mav_bridge → usb_device_transmit → middleware CDC → HAL PCD; USB RX đi chiều ngược lại.",
    "notes": [
      "CONFIGURED nghĩa host hoàn tất cấu hình USB; không chứng minh có chương trình đang đọc ttyACM hoặc đã nhận dữ liệu.",
      "Driver sở hữu bản copy tránh trỏ vào vùng ring đã bị task tái sử dụng."
    ]
  },
  {
    "path": "src/usbd_conf.c",
    "group": "USB",
    "profiles": [
      "bridge",
      "recovered"
    ],
    "context": "HAL MSP/init từ task; HAL PCD callbacks trong IRQ; USBD_LL gọi theo đường middleware.",
    "role": "Adapter giữa middleware USB Device và phần cứng USB2_OTG_FS của STM32.",
    "data": [
      "g_usb_pcd: handle bộ điều khiển USB; pData nối hai chiều giữa HAL PCD và USBD handle.",
      "PCD Full Speed, embedded PHY, DMA=0, VBUS sensing=0, SOF=0; cấu hình FIFO RX/TX.",
      "Một vùng static cấp bộ nhớ cho USBD_CDC_HandleTypeDef."
    ],
    "functions": [
      [
        "HAL_PCD_MspInit(), HAL_PCD_MspDeInit()",
        "Cấu hình PA11/PA12 AF10, clock USB2_OTG_FS, USB voltage detector và NVIC priority 6. Deinit tắt IRQ/clock và tháo GPIO."
      ],
      [
        "HAL_PCD_SetupStageCallback(), DataOutStageCallback(), DataInStageCallback()",
        "Đưa yêu cầu control, dữ liệu host→device hoặc hoàn thành device→host sang USBD_LL tương ứng."
      ],
      [
        "HAL_PCD_ResetCallback(), DisconnectCallback(), ConnectCallback(), SuspendCallback(), ResumeCallback()",
        "Chuyển sự kiện cho middleware; reset/disconnect còn báo usb_device_reset_from_isr() để đổi generation và bỏ TX dở."
      ],
      [
        "HAL_PCD_SOFCallback(), ISOOUTIncompleteCallback(), ISOINIncompleteCallback()",
        "Các adapter sự kiện SOF/isochronous được cung cấp; cấu hình hiện tại tắt SOF và dùng CDC."
      ],
      [
        "USBD_LL_Init(), DeInit(), Start(), Stop()",
        "Gắn PCD vào USBD, khởi tạo controller/FIFO, chuyển kết quả HAL thành USBD_OK/FAIL."
      ],
      [
        "USBD_LL_OpenEP(), CloseEP(), FlushEP(), StallEP(), ClearStallEP(), IsStallEP()",
        "Quản lý endpoint, vùng giao tiếp logic của USB."
      ],
      [
        "USBD_LL_SetUSBAddress(), Transmit(), PrepareReceive(), GetRxDataSize()",
        "Đặt địa chỉ USB, gửi/đăng ký nhận packet qua HAL và đọc số byte đã nhận."
      ],
      [
        "USBD_static_malloc(), USBD_static_free(), USBD_LL_Delay()",
        "Cấp một vùng static cho CDC thay heap; free không làm gì; delay dùng HAL_Delay()."
      ]
    ],
    "flow": "OTG_FS_IRQHandler → HAL_PCD_IRQHandler → callbacks ở đây → USB middleware → CDC callbacks ở usb_device.c.",
    "notes": [
      "USBD_static_malloc bỏ qua size, thiết kế dành cho cấu hình một lớp CDC hiện tại.",
      "Middleware xử lý USB control trong ngắt; phần MAVLink/target được đưa sang task qua stream."
    ]
  },
  {
    "path": "src/usbd_desc.c",
    "group": "USB",
    "profiles": [
      "bridge",
      "recovered"
    ],
    "context": "Hàm descriptor được middleware gọi khi host yêu cầu thông tin USB.",
    "role": "Cung cấp danh tính và chuỗi mô tả USB để máy tính nhận ra thiết bị.",
    "data": [
      "tracker_usb_descriptors: bảng con trỏ hàm callback descriptor.",
      "VID 0x1b8c, PID 0x0036; USB version 2.00; language ID 0x0409.",
      "Device/language/string/serial descriptor buffers; string chuyển sang Unicode định dạng USB."
    ],
    "functions": [
      [
        "device_descriptor(), language_descriptor()",
        "Trả dữ liệu mô tả thiết bị, VID/PID, class và ngôn ngữ, cùng chiều dài."
      ],
      [
        "make_string(), manufacturer_descriptor(), product_descriptor()",
        "Dùng USBD_GetString tạo chuỗi USB: manufacturer MicoAir, product MicoAir H743 FreeRTOS Tracker."
      ],
      [
        "hex_to_unicode(), serial_descriptor()",
        "Lấy UID MCU, chuyển thành 12 ký tự hex trong descriptor serial dài 26 byte."
      ],
      [
        "configuration_descriptor(), interface_descriptor()",
        "Trả chuỗi tên MAVLink CDC và MAVLink Bridge. Đây là string descriptor; cấu hình endpoint CDC do thư viện CDC cung cấp."
      ]
    ],
    "flow": "Host enumerate → yêu cầu descriptor → các callback này trả danh tính.",
    "notes": [
      "File này không parse MAVLink hoặc tạo task; /dev/ttyACM* do hệ điều hành host tạo khi nhận lớp CDC."
    ]
  },
  {
    "path": "src/app_state.c",
    "group": "Trạng thái",
    "profiles": [
      "bridge",
      "recovered"
    ],
    "context": "Chỉ gọi từ task; sử dụng FreeRTOS mutex.",
    "role": "Giữ bản dữ liệu chung mới nhất giữa task giao tiếp, sensor và control.",
    "data": [
      "g_state: target, station, heading và FC heartbeat cùng timestamp/valid.",
      "g_state_mutex: bảo vệ copy/update tránh control đọc một struct đang bị ghi dở."
    ],
    "functions": [
      [
        "app_state_init()",
        "Zero trạng thái và tạo mutex trước scheduler."
      ],
      [
        "update_position()",
        "Ghi lat/lon đơn vị 1e-7 độ, altitude mm, timestamp ms; valid nếu lat hoặc lon khác 0."
      ],
      [
        "app_state_update_target(), app_state_update_station()",
        "Take mutex tối đa 10 ms, ghi vị trí target/station rồi Give. Nếu hết thời gian lấy lock thì không cập nhật."
      ],
      [
        "app_state_update_heartbeat(), app_state_update_heading()",
        "Ghi timestamp heartbeat hoặc heading độ và cờ valid, cùng cách khóa mutex 10 ms."
      ],
      [
        "app_state_snapshot()",
        "Nếu con trỏ hợp lệ, đợi mutex bằng portMAX_DELAY, copy toàn g_state cho caller rồi nhả mutex. Caller tính toán trên bản copy."
      ]
    ],
    "flow": "mav_bridge ghi target/heartbeat; sensors ghi station/heading; control đọc snapshot.",
    "notes": [
      "valid ở đây không tự hết hạn theo thời gian; control kiểm tra tuổi timestamp riêng.",
      "Đánh dấu vị trí (0,0) không hợp lệ là quy ước hiện tại, không phải kiểm tra chất lượng GPS tổng quát."
    ]
  },
  {
    "path": "src/gps_ubx.c",
    "group": "Sensor và tracking",
    "profiles": [
      "recovered"
    ],
    "context": "Hàm parser thường được sensor task gọi từng byte; không tạo task.",
    "role": "Ráp và giải mã bản tin GPS UBX-NAV-PVT.",
    "data": [
      "State machine: sync1 → sync2 → class → ID → length low/high → payload → checksum A/B.",
      "payload tối đa 92 byte; parser chỉ xuất vị trí cho class 0x01, ID 0x07, length 92."
    ],
    "functions": [
      [
        "checksum(), read_i32()",
        "Tính hai checksum tích lũy UBX và đọc int32 little-endian từ payload."
      ],
      [
        "gps_ubx_init()",
        "Zero parser, bắt đầu tìm sync byte 0xb5."
      ],
      [
        "gps_ubx_parse()",
        "Nhận từng byte; tìm 0xb5/0x62, đọc header/length, từ chối length>92, ráp payload/checksum. Khi NAV-PVT đúng checksum: lấy fix_type, số vệ tinh, lon/lat, hMSL mm; valid khi fix_type>=3 và flags.gnssFixOK set; trả true cho message hoàn chỉnh."
      ]
    ],
    "flow": "USART3 → GPS stream → sensor_task → gps_ubx_parse → station position.",
    "notes": [
      "true nghĩa giải mã được NAV-PVT; caller còn phải kiểm tra position.valid.",
      "File này không gửi lệnh cấu hình GPS và không parse NMEA; module phải đang phát UBX-NAV-PVT ở baud phù hợp."
    ]
  },
  {
    "path": "src/sensor_task.c",
    "group": "Sensor và tracking",
    "profiles": [
      "recovered"
    ],
    "context": "Task sensors: priority 4, stack 768 words = 3072 byte.",
    "role": "Đọc GPS trạm và compass QMC5883L rồi cập nhật app_state.",
    "data": [
      "parser UBX và position nằm trên stack của task; mảng nhận GPS 128 byte.",
      "QMC5883L địa chỉ I2C 0x0d, thanh ghi data/status/control; compass_ready ghi kết quả init."
    ],
    "functions": [
      [
        "read_i16()",
        "Đọc giá trị signed 16 bit little-endian cho trục compass."
      ],
      [
        "compass_init()",
        "Qua I2C1 external: ghi reset 0x80 vào control2, cấu hình control1 0x1d."
      ],
      [
        "compass_update()",
        "Kiểm tra data-ready, đọc 6 byte X/Y/Z; tính heading=atan2(Y,X) thành độ và đưa về 0–360, cập nhật timestamp."
      ],
      [
        "sensor_task()",
        "Init parser, delay 100 ms, init compass một lần. Lặp đọc GPS tối đa 128 byte/chờ 10 ms; NAV-PVT valid cập nhật station. Khi đủ ít nhất 20 ms từ lần kiểm tra compass trước thì thử đọc compass."
      ],
      [
        "sensor_task_start()",
        "Tạo sensors priority 4, stack 768 words."
      ]
    ],
    "flow": "GPS UART + compass I2C → station/heading → control đọc.",
    "notes": [
      "Hiện chưa đọc gyro/accelerometer/barometer/IST8310; việc board có handle SPI/I2C2 không có nghĩa đã có task IMU.",
      "Heading chưa áp dụng calibration hard/soft iron, hướng lắp trục hoặc bù nghiêng; comment nguồn ghi rõ cần bổ sung. Compass init fail hiện không có retry trong vòng lặp.",
      "Mốc 20 ms là kiểm tra elapsed, không phải vòng sensor có chu kỳ cố định 20 ms."
    ]
  },
  {
    "path": "src/tracker_math.c",
    "group": "Sensor và tracking",
    "profiles": [
      "recovered"
    ],
    "context": "Hàm tính toán được control gọi; không truy cập HAL/FreeRTOS.",
    "role": "Từ tọa độ trạm/target tính hướng ngắm và đổi góc sang độ rộng xung servo.",
    "data": [
      "EARTH_RADIUS_M=6371000; DEG_TO_RAD/RAD_TO_DEG.",
      "Yaw pulse 800–2200 µs; pitch 1100–2200 µs; tracker_solution_t chứa góc và khoảng cách."
    ],
    "functions": [
      [
        "clampf(), wrap_180()",
        "Giới hạn số thực và đưa góc về khoảng -180 đến +180 độ."
      ],
      [
        "tracker_solution_compute()",
        "Từ lat/lon e7 và altitude mm: đổi sang rad/m, haversine tính khoảng cách ngang, atan2 tính bearing 0–360 và elevation, hypot dạng sqrt tính slant distance. Từ chối con trỏ NULL hoặc tọa độ (0,0); kiểm tra góc hữu hạn."
      ],
      [
        "tracker_solution_to_servo()",
        "Yaw tương đối=bearing-heading, wrap ±180 rồi ánh xạ tuyến tính sang 800–2200 µs. Elevation clamp 0–90 rồi ánh xạ sang 1100–2200 µs."
      ]
    ],
    "flow": "station + target → bearing/elevation → relative yaw/pitch → yaw_us/pitch_us.",
    "notes": [
      "Đây là geometry và ánh xạ tuyến tính; chưa có PID, encoder feedback hay hiệu chuẩn cơ khí đầy đủ.",
      "Hướng tham chiếu heading phải phù hợp bearing địa lý; compass chưa có hiệu chỉnh declination/calibration trong sensor task hiện tại."
    ]
  },
  {
    "path": "src/tracker_control_task.c",
    "group": "Sensor và tracking",
    "profiles": [
      "recovered"
    ],
    "context": "Task control: priority 6, stack 768 words = 3072 byte; lịch 50 Hz/20 ms.",
    "role": "Kiểm tra dữ liệu còn mới, tính góc tracking rồi quyết định PWM hoặc dừng output.",
    "data": [
      "state snapshot local; tracker_solution_t local; yaw_us/pitch_us tính mỗi vòng.",
      "Timeout FC heartbeat/target 2000 ms, station GPS 3000 ms, heading 1000 ms từ tracker_config.h."
    ],
    "functions": [
      [
        "state_is_fresh()",
        "Yêu cầu mọi cờ valid và tuổi các timestamp không vượt timeout tương ứng."
      ],
      [
        "tracker_control_task()",
        "Dừng servo lúc vào task. Mỗi mốc 20 ms: lấy snapshot, kiểm tra freshness, compute solution và convert pulse. Nếu build cho phép output thì board_servo_set(); nếu lỗi hoặc dữ liệu cũ thì stop. Khi macro output=0 hiện tại, luôn stop cả khi phép tính thành công."
      ],
      [
        "tracker_control_task_start()",
        "Tạo control priority 6 và stack 768 words; lịch dùng vTaskDelayUntil()."
      ]
    ],
    "flow": "Snapshot → freshness → geometry → pulse → PWM nếu được bật.",
    "notes": [
      "TRACKER_ENABLE_SERVO_OUTPUT mặc định 0. Có code tính tracking không đồng nghĩa servo đang chạy.",
      "Timeout nằm trong task này; chưa có task giám sát độc lập, lệnh STOP hoặc watchdog phần cứng trong src hiện tại."
    ]
  },
  {
    "path": "include/board.h",
    "group": "Header API",
    "profiles": [
      "boot",
      "bridge",
      "recovered"
    ],
    "context": "Được compiler đọc; các hàm chạy theo implementation board layer được chọn.",
    "role": "Hợp đồng truy cập phần cứng dùng chung giữa task và board layer.",
    "data": [
      "BOARD_FC_BAUD_RATE mặc định 57600U nếu CMake chưa định nghĩa; GPS 230400U, servo 330U.",
      "ID chip-select BMI088 gyro=1, accel=2, BMI270=3."
    ],
    "functions": [
      [
        "API init/time/IO",
        "board_init/start_io/poll_io/millis; read FC/GPS/USB, write FC/USB; reset USB RX và callback dữ liệu USB từ ISR."
      ],
      [
        "API sensor/servo",
        "board_i2c_read/write, board_spi_read, servo_set/stop; getters handle và board_fatal."
      ]
    ],
    "flow": "Task include board.h → compiler kiểm tra lời gọi → linker chọn implementation từ bridge_board.c hoặc board.c.",
    "notes": [
      "Header khai báo API rộng; bridge không cung cấp GPS/I2C/SPI/servo vì không có caller trong profile đó. board_poll_io chỉ được gọi trong bridge.",
      "boot chỉ dùng phần HAL/board_fatal của hợp đồng; HAL type definitions đến từ stm32h7xx_hal.h."
    ]
  },
  {
    "path": "include/boot_task.h",
    "group": "Header API",
    "profiles": [
      "boot",
      "bridge"
    ],
    "context": "Kiểu dữ liệu và khai báo cho main, boot task và boot board.",
    "role": "Các mốc boot, struct chẩn đoán và API bắt đầu boot task/PWM.",
    "data": [
      "boot_stage_t: MAIN=1, HAL_READY=2, CLOCK_READY=3, TASK_CREATED=4, TASK_RUNNING=5, PWM_RUNNING=6.",
      "boot_status_t: boot_stage, iterations, ticks, HAL millis, heap free/min, stack high-water words.",
      "extern volatile g_boot_status: khai báo biến, vùng nhớ thật được định nghĩa trong boot_task.c."
    ],
    "functions": [
      [
        "boot_board_init(), boot_board_led_pwm_start(), boot_task_start()",
        "Khai báo các hàm do boot_board.c và boot_task.c thực hiện."
      ]
    ],
    "flow": "main ghi boot_stage trước scheduler; boot_diag ghi stage và diagnostics sau scheduler.",
    "notes": [
      "Đọc p g_boot_status khi halt MCU để lấy snapshot. volatile không thay cho mutex/atomic và không làm toàn struct được đọc đồng thời."
    ]
  },
  {
    "path": "include/bridge_protocol.h",
    "group": "Header API",
    "profiles": [
      "bridge",
      "recovered"
    ],
    "context": "Dùng bởi bridge task và implementation thuật toán.",
    "role": "Kiểu ring byte và trạng thái parser MAVLink, cùng API buffer/protocol.",
    "data": [
      "bridge_buffer_t: data[4096], read_index/write_index/count/dropped_bytes.",
      "bridge_protocol_t: rx_message/rx_status ráp một bản tin; frame index/length/version/flags/boundary; thời gian và counters.",
      "BRIDGE_FRAME_GAP_MS=250; includes mavlink/common/mavlink.h cho kiểu thư viện."
    ],
    "functions": [
      [
        "buffer enqueue/peek/consume/clear",
        "API hàng đợi byte chờ gửi."
      ],
      [
        "protocol init/reset/expire/receive/heartbeat",
        "API ráp bản tin, reset khung dở và tạo heartbeat."
      ]
    ],
    "flow": "g_to_fc/g_to_usb có kiểu buffer; g_protocol có kiểu parser.",
    "notes": [
      "Mỗi kiểu giải quyết một việc: giữ byte chờ truyền và nhớ tiến trình hiểu một bản tin."
    ]
  },
  {
    "path": "include/bridge_status.h",
    "group": "Header API",
    "profiles": [
      "bridge",
      "recovered"
    ],
    "context": "Kiểu chẩn đoán dùng bởi task, driver và debugger.",
    "role": "Tách thống kê do task quản lý và thống kê sự kiện IO/ISR.",
    "data": [
      "bridge_status_t: IO/configured, iterations/time/heap/stack, valid/bad/reset/target/heartbeat, link change, pending/drop hai chiều.",
      "bridge_io_status_t: UART RX/drop/error/restart, TX accepted/completed/start error; USB RX/drop, TX accepted/completed/aborted và reset.",
      "extern volatile g_bridge_status/g_bridge_io_status định nghĩa trong mavlink_bridge_task.c."
    ],
    "functions": [
      [
        "Không có hàm",
        "File này chỉ định nghĩa struct và khai báo biến extern."
      ]
    ],
    "flow": "Driver/ISR ghi IO counters; task tổng hợp protocol/queue/runtime; debugger xem cả hai.",
    "notes": [
      "TX accepted khác TX completed. Halt CPU khi muốn so sánh snapshot nhất quán.",
      "Phần lớn IO counters được bridge_board.c hỗ trợ; board.c recovered chưa cập nhật đầy đủ cùng mức M2."
    ]
  },
  {
    "path": "include/usb_device.h",
    "group": "Header API",
    "profiles": [
      "bridge",
      "recovered"
    ],
    "context": "API task và callback reset từ ISR.",
    "role": "Giao diện nhỏ của USB CDC ứng dụng.",
    "data": [
      "Chỉ expose bool, size_t, uint8_t; giấu handle USBD và buffer bên usb_device.c."
    ],
    "functions": [
      [
        "usb_device_init(), usb_device_transmit()",
        "Bắt đầu USB sau scheduler; nhận đoạn dữ liệu TX khi driver sẵn sàng."
      ],
      [
        "usb_device_connected(), usb_device_reset_from_isr()",
        "Lấy configured/generation, hoặc báo reset từ đường IRQ."
      ]
    ],
    "flow": "Bridge task gọi API này; usbd_conf.c báo reset phiên.",
    "notes": [
      "Caller phải tôn trọng ngữ cảnh task/ISR và giới hạn đoạn TX 64 byte của implementation."
    ]
  },
  {
    "path": "include/usbd_conf.h",
    "group": "Cấu hình thư viện",
    "profiles": [
      "bridge",
      "recovered"
    ],
    "context": "Middleware đọc macro ở thời điểm compile.",
    "role": "Cấu hình thư viện USB Device và ánh xạ các dịch vụ bộ nhớ/log/delay.",
    "data": [
      "Một configuration, một lớp hỗ trợ, max string descriptor 64; self-powered=0.",
      "Debug/log, LPM, BOS và register callback tùy chọn tắt.",
      "USBD_malloc/free → static allocator; memset/memcpy chuẩn; Delay → HAL_Delay."
    ],
    "functions": [
      [
        "USBD_static_malloc(), USBD_static_free()",
        "Prototype implementation trong usbd_conf.c."
      ]
    ],
    "flow": "USB middleware include usbd_conf.h → dùng cấu hình của ứng dụng.",
    "notes": [
      "USBD_MAX_NUM_INTERFACES=1 là giá trị macro hiện tại của middleware; endpoint/interface descriptors của lớp CDC nằm trong thư viện.",
      "Không phải cấu hình thuật toán MAVLink."
    ]
  },
  {
    "path": "include/usbd_desc.h",
    "group": "Header API",
    "profiles": [
      "bridge",
      "recovered"
    ],
    "context": "Khai báo descriptor callback table cho USB init.",
    "role": "Expose bảng descriptor USB cho usb_device.c.",
    "data": [
      "extern USBD_DescriptorsTypeDef tracker_usb_descriptors; type đến từ usbd_def.h."
    ],
    "functions": [
      [
        "Không có hàm",
        "Biến được định nghĩa trong usbd_desc.c; USBD_Init nhận địa chỉ bảng này."
      ]
    ],
    "flow": "usb_device_init → &tracker_usb_descriptors → descriptor callbacks.",
    "notes": [
      "extern chỉ thông báo biến tồn tại; không cấp vùng nhớ mới ở mỗi file include."
    ]
  },
  {
    "path": "include/app_state.h",
    "group": "Header API",
    "profiles": [
      "bridge",
      "recovered"
    ],
    "context": "Kiểu dữ liệu/API trao đổi giữa task.",
    "role": "Mô hình trạng thái tracker và các hàm cập nhật/snapshot.",
    "data": [
      "tracker_position_t: lat/lon e7, altitude mm, timestamp ms, valid.",
      "tracker_state_snapshot_t: target (máy bay), station (trạm anten), heading độ/timestamp/valid, heartbeat FC/timestamp."
    ],
    "functions": [
      [
        "app_state_init(), app_state_update_*(), app_state_snapshot()",
        "Khai báo API mutex-protected trong app_state.c; caller không truy cập trực tiếp g_state qua header."
      ]
    ],
    "flow": "Giao tiếp cung cấp target; sensor cung cấp station/heading; control dùng snapshot.",
    "notes": [
      "Ví dụ latitude_e7=110000000 nghĩa 11°; altitude_mm=23700 nghĩa 23.7 m.",
      "Header không tự tạo mutex, không tự cập nhật hay hết hạn dữ liệu."
    ]
  },
  {
    "path": "include/gps_ubx.h",
    "group": "Header API",
    "profiles": [
      "recovered"
    ],
    "context": "Kiểu/parser API cho sensor task.",
    "role": "Tách trạng thái đang ráp UBX và vị trí đã giải mã.",
    "data": [
      "gps_ubx_parser_t: state, class/ID, length/offset, checksum A/B, checksum A đã nhận và payload[92].",
      "gps_ubx_position_t: lat/lon e7, altitude_msl_mm, fix_type, satellites, valid."
    ],
    "functions": [
      [
        "gps_ubx_init(), gps_ubx_parse()",
        "Init parser, nhận một byte; trả true khi NAV-PVT checksum đúng đã được giải mã vào position."
      ]
    ],
    "flow": "Byte đến rời rạc được tích lũy trong parser; position chỉ xuất khi hoàn chỉnh.",
    "notes": [
      "altitude_msl_mm là cao độ so với mực nước biển, đơn vị mm; không phải cao tương đối so với điểm home.",
      "valid là điều kiện fix của parser hiện tại; sensor task dùng valid để quyết định update station."
    ]
  },
  {
    "path": "include/tracker_tasks.h",
    "group": "Header API",
    "profiles": [
      "bridge",
      "recovered"
    ],
    "context": "main include để tạo task.",
    "role": "Danh sách hàm khởi tạo ba task ứng dụng.",
    "data": [
      "Không khai báo TCB hoặc stack thủ công; xTaskCreate trong các file .c tạo chúng qua FreeRTOS."
    ],
    "functions": [
      [
        "mavlink_bridge_task_start()",
        "Implementation mavlink_bridge_task.c; bridge và recovered."
      ],
      [
        "sensor_task_start(), tracker_control_task_start()",
        "Implementation sensor_task.c và tracker_control_task.c; recovered."
      ]
    ],
    "flow": "main → *_task_start → xTaskCreate → scheduler chạy hàm task.",
    "notes": [
      "Các hàm *_start trả bool thành công tạo task; không phải chính vòng xử lý vô hạn của task."
    ]
  },
  {
    "path": "include/tracker_config.h",
    "group": "Cấu hình ứng dụng",
    "profiles": [
      "bridge",
      "recovered"
    ],
    "context": "Macro ứng dụng, được quyết định khi compile.",
    "role": "ID MAVLink, lịch control, timeout và công tắc đầu ra servo.",
    "data": [
      "TRACKER_SYSTEM_ID=1, COMPONENT_ID=191, TARGET_SYSTEM_ID=1.",
      "CONTROL_RATE_HZ=50, telemetry timeout=2000 ms, station GPS=3000 ms, heading=1000 ms.",
      "TRACKER_ENABLE_SERVO_OUTPUT=0."
    ],
    "functions": [
      [
        "Không có hàm",
        "Macro được bridge_protocol, bridge task và control task sử dụng."
      ]
    ],
    "flow": "ID → heartbeat/filter target; rate/timeout/output switch → control.",
    "notes": [
      "Đây là cấu hình compile-time, chưa có parameter service thay đổi lúc chạy.",
      "Macro output=0 giữ servo tắt trong control task; cần xác nhận chiều quay, giới hạn và stop trước khi bật."
    ]
  },
  {
    "path": "include/tracker_math.h",
    "group": "Header API",
    "profiles": [
      "recovered"
    ],
    "context": "Kiểu/API tính toán cho control.",
    "role": "Hợp đồng đầu vào/đầu ra cho geometry và đổi xung servo.",
    "data": [
      "tracker_solution_t: bearing_deg/elevation_deg, horizontal_distance_m/slant_distance_m.",
      "Đầu vào vị trí dùng int32 e7/mm; đầu ra xung yaw/pitch uint16 microsecond."
    ],
    "functions": [
      [
        "tracker_solution_compute()",
        "Tính góc/khoảng cách từ vị trí station và target."
      ],
      [
        "tracker_solution_to_servo()",
        "Dùng solution và heading trạm đổi thành yaw_us/pitch_us."
      ]
    ],
    "flow": "Control include header → gọi implementation tracker_math.c.",
    "notes": [
      "Tên _deg/_m/_mm/_e7/_us ghi rõ đơn vị, tránh trộn độ/radian hoặc mm/m."
    ]
  },
  {
    "path": "include/FreeRTOSConfig.h",
    "group": "Cấu hình thư viện",
    "profiles": [
      "boot",
      "bridge",
      "recovered"
    ],
    "context": "Cấu hình kernel/port ở compile time.",
    "role": "Quy định scheduler, tick, heap, đồng bộ, task timer và mức ngắt gọi API RTOS.",
    "data": [
      "Preemption=1, time slicing=1, tickless=0; tick 1000 Hz (1 ms), 8 mức task priority 0–7, số tick không dùng 16 bit.",
      "Heap 96 KiB; minimum stack 256 words; mutex, counting semaphore, task notification bật; recursive mutex/queue sets tắt.",
      "Software timer daemon priority 2, stack 512 words, queue 8; configUSE_TIMERS=1 tạo daemon khi scheduler bắt đầu.",
      "Malloc hook, tick hook, stack overflow check level 2 bật; FPU bật, MPU tắt; SVC/PendSV ánh xạ sang port."
    ],
    "functions": [
      [
        "configASSERT(condition)",
        "Nếu điều kiện false gọi tracker_assert_failed() trong main.c."
      ],
      [
        "INCLUDE_* và config*",
        "Chọn API/feature kernel được cung cấp, như delay, delayUntil, tick, stack watermark, scheduler state."
      ]
    ],
    "flow": "Kernel/port trong .deps dùng macro này; task và ISR phải tuân thủ cấu hình.",
    "notes": [
      "Task priority: số lớn ưu tiên cao. NVIC priority: số nhỏ ưu tiên cao. Hai thang không so sánh trực tiếp.",
      "configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY=5: các IRQ gọi FromISR phải có mức số 5–15 với cấu hình 4 bit hiện tại; UART/USB dùng 6. SysTick/PendSV mức thấp 15.",
      "Bật timer service không có nghĩa LED PWM được timer task tạo; TIM15/TIM1 là timer phần cứng riêng."
    ]
  },
  {
    "path": "include/stm32h7xx_hal_conf.h",
    "group": "Cấu hình thư viện",
    "profiles": [
      "boot",
      "bridge",
      "recovered"
    ],
    "context": "HAL STM32 đọc macro và include ở compile time.",
    "role": "Chọn module HAL và giá trị clock/callback/assert dùng trong driver.",
    "data": [
      "Bật HAL Cortex/DMA/EXTI/Flash/GPIO/I2C/PCD/PWR/RCC/SPI/TIM/UART.",
      "HSE_VALUE=8 MHz, HSI=64 MHz, CSI=4 MHz, LSI=32 kHz, LSE=32768 Hz; VDD_VALUE=3300 mV, tick IRQ priority=15.",
      "Các USE_HAL_*_REGISTER_CALLBACKS=0 dùng callback được override bằng tên hàm HAL thay vì đăng ký con trỏ lúc chạy."
    ],
    "functions": [
      [
        "assert_param(), assert_failed()",
        "Nếu USE_FULL_ASSERT được bật thì assert_param gọi assert_failed(file,line); cấu hình CMake hiện chưa bật macro đó nên assert_param không kiểm tra."
      ],
      [
        "Các #include HAL module",
        "Làm các kiểu/hàm HAL của module đã bật có sẵn cho stm32h7xx_hal.h."
      ]
    ],
    "flow": "board.h → stm32h7xx_hal.h → HAL configuration → driver types/functions.",
    "notes": [
      "USE_RTOS=0 là macro cấu hình của HAL; scheduler vẫn là FreeRTOS được main khởi động.",
      "Định nghĩa HSE_VALUE không bật oscillator; thao tác bật và cấu hình PLL thực hiện trong main. Bật module DMA trong HAL không đồng nghĩa đường UART/USB đang dùng DMA."
    ]
  }
];
const WORKFLOWS = [
  {
    "key": "boot",
    "title": "Khởi động",
    "steps": [
      "CMSIS startup → main",
      "HAL + nguồn + clock",
      "Tạo buffer / mutex / task",
      "vTaskStartScheduler",
      "Task bridge bắt đầu USB / UART RX"
    ],
    "detail": "Boot và bridge còn có boot_diag bắt đầu PWM LED. Với recovered, control priority 6 có thể chạy và block trước khi bridge priority 5 được chạy. File C không tự chạy chỉ vì được include."
  },
  {
    "key": "fc",
    "title": "FC → USB",
    "steps": [
      "USART1 RX",
      "IRQ → HAL callback",
      "Stream RX g_fc_rx",
      "mav_bridge đọc byte",
      "Ring g_to_usb → USB TX"
    ],
    "detail": "Cùng byte đó còn đi qua bridge_protocol: bản tin hợp lệ đúng SYSID cập nhật target/heartbeat trong app_state. Byte sai CRC vẫn đi đường chuyển tiếp. Chỉ driver chấp nhận đoạn TX thì task mới consume ring."
  },
  {
    "key": "usb",
    "title": "USB → FC",
    "steps": [
      "OTG_FS IRQ",
      "PCD → USBD → CDC RX",
      "Stream RX g_usb_rx",
      "mav_bridge đọc byte",
      "Ring g_to_fc → UART TX"
    ],
    "detail": "Chiều USB→FC chuyển nguyên byte. Trong bridge, TX interrupt copy tối đa 128 byte vào buffer riêng. Callback TX complete mới xác nhận gửi xong; không parse lệnh STOP tại task này."
  },
  {
    "key": "sensor",
    "title": "Sensor → PWM",
    "steps": [
      "GPS UBX + compass I2C",
      "Task sensors",
      "app_state station / heading",
      "Task control đọc snapshot",
      "Freshness → geometry → pulse"
    ],
    "detail": "Chỉ recovered. Control 50 Hz kiểm tra dữ liệu còn mới. TRACKER_ENABLE_SERVO_OUTPUT=0 hiện giữ output tắt. Source chưa có PID hoặc fusion IMU; compass cần hiệu chuẩn/bù hướng."
  }
];
const TASKS = [
  {
    "n": "boot_diag",
    "p": "1",
    "s": "256 / 1024",
    "schedule": "500 ms · delayUntil",
    "profiles": [
      "boot",
      "bridge"
    ]
  },
  {
    "n": "mav_bridge",
    "p": "5",
    "s": "1536 / 6144",
    "schedule": "Xử lý rồi delay 1 tick",
    "profiles": [
      "bridge",
      "recovered"
    ]
  },
  {
    "n": "sensors",
    "p": "4",
    "s": "768 / 3072",
    "schedule": "GPS wait ≤10 ms; compass elapsed ≥20 ms",
    "profiles": [
      "recovered"
    ]
  },
  {
    "n": "control",
    "p": "6",
    "s": "768 / 3072",
    "schedule": "20 ms · delayUntil",
    "profiles": [
      "recovered"
    ]
  },
  {
    "n": "IDLE (hệ thống)",
    "p": "0",
    "s": "256 / 1024",
    "schedule": "Khi task ready cao hơn đều block",
    "profiles": [
      "boot",
      "bridge",
      "recovered"
    ]
  },
  {
    "n": "Timer daemon (hệ thống)",
    "p": "2",
    "s": "512 / 2048",
    "schedule": "Đợi timer command / expiry",
    "profiles": [
      "boot",
      "bridge",
      "recovered"
    ]
  }
];
const PROFILES = [
  {key:"boot",title:"M1 · boot",info:"4 file C: boot/task diagnostics và LED PWM. Chưa có USB CDC hoặc giao tiếp FC."},
  {key:"bridge",title:"M2 · bridge",info:"11 file C: boot diagnostics, UART1/USB bridge, parser và target. Không link board.c hoặc sensor/control."},
  {key:"recovered",title:"recovered",info:"13 file C: bridge + GPS/compass + geometry/control, dùng board.c. Servo output mặc định tắt; phần sensor/control cần xác nhận riêng."}
];

export default function FreeRTOSGuide() {
  const theme = useHostTheme();
  const dispatch = useCanvasAction();
  const [profile, setProfile] = useCanvasState("freertos-guide-profile", "bridge");
  const [selected, setSelected] = useCanvasState("freertos-guide-file", "src/bridge_board.c");
  const [flow, setFlow] = useCanvasState("freertos-guide-flow", "fc");
  const chosen = FILES.find(f => f.path === selected) || FILES[0];
  const chosenProfile = PROFILES.find(p => p.key === profile) || PROFILES[1];
  const availableWorkflows = WORKFLOWS.filter(w => w.key === "boot" || (profile !== "boot" && (w.key !== "sensor" || profile === "recovered")));
  const chosenFlow = availableWorkflows.find(w => w.key === flow) || availableWorkflows[0];
  const activeTasks = TASKS.filter(t => t.profiles.includes(profile));
  const activeFiles = FILES.filter(f => f.path.startsWith("src/") && f.profiles.includes(profile));
  const selectProfile = (nextProfile: string) => {
    setProfile(nextProfile);
    if (!chosen.profiles.includes(nextProfile)) {
      setSelected(nextProfile === "boot" ? "src/boot_board.c" : nextProfile === "bridge" ? "src/bridge_board.c" : "src/board.c");
    }
    if (nextProfile === "boot") setFlow("boot");
    else if (flow === "sensor" && nextProfile === "bridge") setFlow("fc");
  };
  return (
    <Stack gap={20} style={{padding:24, maxWidth:1200, margin:"0 auto", color:theme.text.primary}}>
      <Stack gap={6}>
        <H1>Đọc source FreeRTOS theo từng file</H1>
        <Text tone="secondary">MicoAir H743 · 16 file C + 14 header · đối chiếu CMake và source ngày 28/09/2026; kiểm tra lại cấu trúc và luồng chính ngày 01/10/2026.</Text>
        <Text size="small" tone="tertiary">Tài liệu giải thích source, không phải trạng thái kết nối board trực tiếp. Chọn profile để xem phạm vi build; chọn file để đọc biến, hàm và luồng phối hợp.</Text>
        <Row gap={8} wrap>
          {PROFILES.map(p => <Button key={p.key} variant={profile===p.key?"primary":"secondary"} onClick={()=>selectProfile(p.key)}>{p.title}</Button>)}
          <Button variant="ghost" onClick={()=>dispatch({type:"openFile",path:GUIDE})}>Mở bản Markdown đầy đủ</Button>
        </Row>
        <Text>{chosenProfile.info}</Text>
      </Stack>

      <Grid columns="repeat(auto-fit, minmax(310px, 1fr))" gap={24}>
        <Stack gap={10}>
          <H2>Luồng thực thi và dữ liệu</H2>
          <Row gap={6} wrap>{availableWorkflows.map(w=><Button key={w.key} variant={chosenFlow.key===w.key?"primary":"ghost"} onClick={()=>setFlow(w.key)}>{w.title}</Button>)}</Row>
          <ol style={{margin:0,paddingLeft:24,lineHeight:1.9}}>
            {chosenFlow.steps.map((step,i)=><li key={i}>{step}</li>)}
          </ol>
          <Text tone="secondary">{chosenFlow.detail}</Text>
        </Stack>
        <Stack gap={10}>
          <H2>Task trong profile {profile}</H2>
          <Table headers={["Task","Priority","Stack words / byte","Lịch hoặc thời điểm chạy"]}
            rows={activeTasks.map(t=>[t.n,t.p,t.s,t.schedule])} framed={false}/>
          <Text size="small" tone="secondary">Task: số priority lớn hơn chạy trước khi ready. IRQ NVIC: số nhỏ hơn ưu tiên cao hơn. UART/USB dùng NVIC 6; ISR không phải một task priority 6.</Text>
        </Stack>
      </Grid>

      <Divider/>
      <Grid columns="minmax(210px, 1fr) minmax(0, 3fr)" gap={28}>
        <Stack gap={12}>
          <H2>Tra cứu từng file</H2>
          <Select value={chosen.path} onChange={setSelected} options={FILES.map(f=>({value:f.path,label:f.path}))}/>
          <Text size="small" tone="secondary">Header mô tả API/kiểu/macro. File C chứa implementation; có file tạo task, có file chỉ chứa helper/driver.</Text>
          <Button variant="secondary" onClick={()=>dispatch({type:"openFile",path:ROOT+"/freertos/"+chosen.path})}>Mở source đang chọn</Button>
          <CollapsibleSection title={activeFiles.length+" file C trong "+profile}>
            <Stack gap={4}>{activeFiles.map(f=><Button key={f.path} variant="ghost" onClick={()=>setSelected(f.path)}>{f.path}</Button>)}</Stack>
          </CollapsibleSection>
          <CollapsibleSection title="Thứ tự đọc M2" defaultOpen>
            <Text size="small">main → FreeRTOSConfig → board.h → bridge_board → interrupts → mavlink_bridge_task → bridge_protocol → usb_device → usbd_conf → usbd_desc → app_state → bridge_status.</Text>
          </CollapsibleSection>
          <CollapsibleSection title="Đọc sensor / tracking">
            <Text size="small">board.c → gps_ubx → sensor_task → app_state → tracker_config → tracker_math → tracker_control_task.</Text>
          </CollapsibleSection>
        </Stack>
        <Stack gap={16}>
          <Stack gap={5}>
            <Text size="small" tone="tertiary">{chosen.group} · profile {chosen.profiles.join(", ")}</Text>
            <H2>{chosen.path}</H2>
            <Text weight="semibold">{chosen.role}</Text>
            <Text tone="secondary">{chosen.context}</Text>
            {!chosen.profiles.includes(profile) && <Callout>File này thuộc profile {chosen.profiles.join(", ")}. Profile {profile} đang chọn không sử dụng phần chức năng này.</Callout>}
          </Stack>
          <Card>
            <CardHeader>Biến, kiểu dữ liệu và cấu hình</CardHeader>
            <CardBody><Stack gap={8}>{chosen.data.map((item,i)=><Text key={i}>{item}</Text>)}</Stack></CardBody>
          </Card>
          <Stack gap={10}>
            <H3>Hàm hoặc API chính</H3>
            {chosen.functions.map(([name,desc])=><CollapsibleSection key={name} title={name} defaultOpen><Text>{desc}</Text></CollapsibleSection>)}
          </Stack>
          <Stack gap={8}>
            <H3>Luồng phối hợp</H3>
            <Text>{chosen.flow}</Text>
            <H3>Điều cần hiểu đúng</H3>
            {chosen.notes.map((note,i)=><Text key={i} tone="secondary">{note}</Text>)}
          </Stack>
        </Stack>
      </Grid>

      <Divider/>
      <Grid columns="repeat(auto-fit, minmax(300px, 1fr))" gap={24}>
        <Stack gap={8}>
          <H2>Bốn tầng dữ liệu</H2>
          <Text>1. Ngoại vi / HAL handle và byte RX.</Text>
          <Text>2. Stream buffer RX: ISR ghi, task đọc.</Text>
          <Text>3. Ring TX: byte nguyên gốc đang chờ gửi.</Text>
          <Text>4. Parser và app_state: bản tin đang ráp và thông tin đã giải mã.</Text>
          <Text tone="secondary">Driver accepted dữ liệu TX khác với callback completed xác nhận gửi xong. Buffer TX riêng giữ byte ổn định tới completion.</Text>
        </Stack>
        <Stack gap={8}>
          <H2>Từ vựng cần nhớ</H2>
          <Text><Code>static</Code> cấp file giữ tên riêng trong file; biến tồn tại suốt chương trình.</Text>
          <Text><Code>extern</Code> khai báo biến định nghĩa ở nơi khác; không tạo bản sao mới.</Text>
          <Text><Code>volatile</Code> không thay thế mutex/atomic hoặc bảo đảm snapshot nhiều trường.</Text>
          <Text><Code>typedef struct</Code> định nghĩa kiểu; cần khai báo instance mới có vùng nhớ.</Text>
          <Text><Code>Handle HAL</Code> là struct cấu hình/trạng thái driver; phần cứng thật là peripheral MCU.</Text>
        </Stack>
      </Grid>
      <Text size="small" tone="tertiary">Nguồn: freertos/src, freertos/include, freertos/CMakeLists.txt; port/kernel/startup trong .deps. common/attitude và common/tracker_control chưa được CMake FreeRTOS link. Phần recovered có GPS/compass/geometry; chưa có PID, fusion IMU, lệnh STOP hoặc task watchdog độc lập trong các file hiện tại.</Text>
    </Stack>
  );
}
