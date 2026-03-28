# ESP32 FreeRTOS Temperature Monitor

Real-time embedded system using ESP32 and FreeRTOS to monitor temperature from a DS18B20 sensor, with UART CLI interface, threshold alerts, and average calculations.

---

## 🚀 Features

- 🌡️ Real-time temperature monitoring (DS18B20 - 1-Wire)
- ⚙️ FreeRTOS-based multi-tasking system
- 💻 UART CLI interface (interactive commands)
- 🚨 Threshold-based warning system
- 📊 Average temperature calculation
- 🔧 Modular architecture (drivers + components)

---

## 🧠 System Architecture

- **UART Driver** – Handles serial communication
- **CLI Module** – Parses and executes commands
- **Sensor Driver (DS18B20)** – Reads temperature via 1-Wire
- **Main App (temp_monitor)** – Coordinates tasks and logic
- **FreeRTOS Tasks** – Sensor task + UART task

---

## 📸 Hardware Setup

![hardware](images/hardware.jpg)

---

## 💻 UART Output (Live Data)

![uart](images/uart_output.jpg)

---

## 🚨 Alert System & Average Calculation

![alert](images/alert_output.jpg)

---

## 🧪 CLI Commands

| Command        | Description                     |
|----------------|---------------------------------|
| `help`         | Show available commands         |
| `temp`         | Read current temperature        |
| `status`       | Show system status              |
| `set <ms>`     | Set sampling period             |
| `get`          | Get current period              |
| `avg`          | Show average temperature        |

---

## 🛠️ Technologies

- ESP32
- ESP-IDF (v5.x)
- FreeRTOS
- C Programming
- UART Communication
- 1-Wire Protocol

---

## 🔥 Highlights

- Built and tested on real hardware (ESP32 + DS18B20)
- Implemented custom UART CLI from scratch
- Integrated sensor communication with RTOS tasks
- Designed modular and scalable embedded architecture

---

## 📌 Future Improvements

- WiFi integration (IoT dashboard)
- Data logging
- OTA updates
- Web interface

---

## 👨‍💻 Author

Ibrahim Sultan
