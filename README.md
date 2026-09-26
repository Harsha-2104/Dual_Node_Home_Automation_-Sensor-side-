# LoRa-Based Home Automation & Energy Monitoring

**Context:** This is the work That I have done as a part of my Embedded & IoT Internship at Gaibi Sahib Technologies (Dec 2025 – Mar 2026)

## 📖 About This Repository
This repository contains the firmware and simulation files for a dual-node Internet of Things (IoT) architecture. It is designed for remote energy monitoring and automated appliance control. 

**What is inside:**
* `STM32_Node/`: C firmware for the STM32 Blue Pill sensor node (data acquisition and RF transmission).

**What are part of this but not here:**
* `ESP32_Gateway/`: C/C++ firmware for the ESP32 gateway (RF reception and network bridging).
* `Simulations/`: Pre-deployment circuit validation files for Wokwi and Proteus.

**The Goal:** To build a long-range wireless energy monitoring system that bypasses the need for local Wi-Fi at the sensor level, using sub-GHz LoRa RF to transmit data to a centralized Wi-Fi gateway.

**The Outcome:** Successfully established a local sensor network where an STM32 processes analog readings from an SCT-013 current transformer and reliably transmits the structured data payload via LoRa SX1278 transceivers to an ESP32 gateway.

## 🛠️ How to Use This Repository

### 1. Hardware Prerequisites
* **Nodes:** STM32 Blue Pill, ESP32
* **Modules:** 2x LoRa SX1278 (433MHz), SCT-013 Non-invasive split-core current transformer
* **Programmer:** ST-Link V2 for STM32

### 2. Wiring Configuration
**STM32 Node (SPI)**
* `SCK`: [Insert Pin] | `MISO`: [Insert Pin] | `MOSI`: [Insert Pin] | `NSS/CS`: [Insert Pin] | `DIO0`: [Insert Pin]
* SCT-013 connected to ADC pin [Insert Pin] with a standard burden resistor circuit.

**ESP32 Gateway (SPI)**
* `SCK`: [Insert Pin] | `MISO`: [Insert Pin] | `MOSI`: [Insert Pin] | `NSS/CS`: [Insert Pin] | `DIO0`: [Insert Pin]

### 3. Build & Flash Instructions
**For the Sensor Node:**
1. Open the `STM32_Node` directory in **STM32CubeIDE**.
2. Verify the SPI and ADC configurations in the `.ioc` file.
3. Build the project and flash via ST-Link.

**For the Gateway Node:**
1. Open the `ESP32_Gateway` directory in Arduino IDE or ESP-IDF.
2. Update the `config.h` file with your local Wi-Fi credentials.
3. Compile and upload via micro-USB.

## 🚀 Next Steps / Future Scope
* Implement a structured JSON payload for multi-node support.
* Integrate the ESP32 output with a cloud dashboard (e.g., Node-RED, AWS IoT).
