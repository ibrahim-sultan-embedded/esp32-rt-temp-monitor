# ESP32 FreeRTOS Temperature Monitor

Real-time embedded system using ESP32 and FreeRTOS for temperature monitoring with a DS18B20 sensor, featuring multi-tasking architecture, inter-task communication, and UART CLI interface.

---

## 🚀 Features

- 🌡️ Real-time temperature monitoring (DS18B20 - 1-Wire)
- ⚙️ FreeRTOS-based multi-tasking system
- 🔁 Inter-task communication using FreeRTOS Queue (producer-consumer)
- 💻 UART CLI interface (interactive commands)
- 📊 Moving average temperature calculation
- 🚨 Threshold-based alert system
- 🧠 State machine (NORMAL / WARNING / CRITICAL)
- 🔧 Modular architecture (drivers + components)

---

## 🧠 System Architecture
The system is designed using a modular real-time architecture with clear separation between data acquisition, processing, and user interaction.
### Tasks

- **Sensor Task** – Reads temperature from sensor and sends data to queue  
- **Processing Task** – Calculates average, evaluates threshold, updates system state  
- **UART Task** – Handles CLI input/output  

### Data Flow

Sensor Task → Queue → Processing Task → System State

↓
UART CLI

## 🧪 CLI Commands

| Command        | Description                     |
|----------------|---------------------------------|
| `help`         | Show available commands         |
| `temp`         | Read current temperature        |
| `status`       | Show system status              |
| `set <ms>`     | Set sampling period             |
| `get`          | Get current period              |
| `avg`          | Show average temperature        |
| `set_th <val>` | Set temperature threshold       |

---

## 🧠 State Machine

- **NORMAL** – Temperature below threshold  
- **WARNING** – Temperature ≥ threshold  
- **CRITICAL** – Temperature ≥ threshold + 5°C  

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

- Designed real-time embedded system using FreeRTOS tasks and queues  
- Implemented producer-consumer architecture using Queue  
- Developed custom UART CLI with command parsing  
- Implemented state machine for system monitoring  
- Integrated DS18B20 sensor using 1-Wire protocol  
- Tested and validated on real hardware  

---

## 📌 Future Improvements

- WiFi integration (IoT dashboard)  
- Data logging  
- OTA updates  
- Web interface  

---

## 👨‍💻 Author

Ibrahim Sultan
