# 💊 Smart Medicine Reminder

> A dual-mode (automatic/manual) medication dispenser built with ESP32, DS3231 RTC, and 8 servo motors.  
> Status: ✅ Project completed – fully functional prototype presented at ideathon exhibition.

![Prototype](Images/prototype_photo.jpg)   <!-- Add your image path here -->

---

📌 Overview
This system helps patients take their medication on time by either **automatically dispensing** pills at preset schedules or allowing **manual on-demand dispensing** via a 5‑button interface. The rotating 6‑compartment tower holds up to 24 tablets, and all feedback is shown on the Arduino IDE Serial Monitor.

✨ Key Features
- Dual-mode operation** – Automatic (RTC scheduled) + Manual (push‑button control)
- High accuracy** – 99.2% dispensing reliability over 100+ cycles
- Precise timing** – DS3231 RTC with ±1.2 min/month drift and battery backup
- 8 servo motors** – 6 for compartments, 1 for tower rotation, 1 for gate
- 5 tactile buttons** – Settings, Select, Up, Down, Exit
- Audible feedback** – Buzzer for dispensing confirmation
- Wokwi simulation** – Test the logic online without hardware

🛠️ Hardware Components
| Component | Quantity | Purpose |
|-----------|----------|---------|
| ESP32 Dev Board | 1 | Main controller |
| DS3231 RTC Module | 1 | Precise timekeeping & scheduling |
| MG995 Servo | 1 | Rotates the pill tower (360°) |
| SG90 Servo | 7 | 6 compartments + 1 gate |
| Push buttons | 5 | User interface |
| Active buzzer | 1 | Audio feedback |
| Power supply (5V/3.3V) | 1 | Powers all components |

🔌 Pin Configuration
| Component | ESP32 GPIO |
|-----------|------------|
| Servo 1 (Compartment 1) | GPIO 4 |
| Servo 2 (Compartment 2) | GPIO 25 |
| Servo 3 (Compartment 3) | GPIO 27 |
| Servo 4 (Compartment 4) | GPIO 26 |
| Servo 5 (Compartment 5) | GPIO 33 |
| Servo 6 (Compartment 6) | GPIO 5 |
| Main Servo (Tower) | GPIO 2 |
| Gate Servo | GPIO 32 |
| RTC SDA | GPIO 21 |
| RTC SCL | GPIO 22 |
| Settings Button | GPIO 13 |
| Select Button | GPIO 14 |
| Up Button | GPIO 15 |
| Down Button | GPIO 18 |
| Exit Button | GPIO 19 |
| Buzzer | GPIO 23 |

> Full circuit diagram is available in the [`/Hardware`](Hardware/) folder.

## 📂 Repository Structure
Smart_medicine_reminder/
├── Software/
│ └── SmartMedicineReminder.ino # Main Arduino code
├── Hardware/
│ └── circuit_simulation.png # Schematic
| └── Blockdiagram.jpeg # Block Diagram
├── Documentation/
│ ├── Report.pdf # Final project report
│ └── Presentation.pptx # Exhibition slides
├── Images/
│ └── prototype_photos.jpg # Photo of working prototype
└── README.md # This file


## 🚀 How to Run (Hardware)
1. **Clone this repository**  
   ```bash
   git clone https://github.com/Emphy0/Smart_medicine_reminder.git
2. Install required libraries in Arduino IDE:
   RTClib by Adafruit
   ESP32Servo by Kevin Harrington
3. Connect the hardware as per the pin configuration table.
4. Open Software/SmartPillDispenser.ino in Arduino IDE.
5. Select board: ESP32 Dev Module → correct COM port.
6. Upload the code.
7. Open Serial Monitor (baud rate: 115200) to see the home screen and interact with the system.

🖥️ Wokwi Simulation (No hardware needed)
Click the badge below to run the complete simulation online:
https://img.shields.io/badge/Simulation-Wokwi-green
Note: The simulation uses the same logic but replaces physical servos with virtual ones.

📊 Performance Results
Metric	                        Achieved	                Target
Dispensing cycle time       	 3.8 seconds            	< 4 seconds
Dispensing accuracy             	99.2%                  	> 99%
Servo positioning error         	±1.5°                   	±2°
RTC timekeeping drift       	±1.2 min/month	        ±2 min/month
Idle power consumption        	  85 mA                	< 100 mA

👥 Authors
Prajwal M – @Emphy0
Nithin S
Keerthana K
Prema S

This project was developed as part of the curriculum.


