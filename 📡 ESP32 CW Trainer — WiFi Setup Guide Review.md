---START FILE---

```markdown

# 📡 ESP32 CW Trainer — WiFi Setup Guide
## Complete Print-Ready Documentation

**Version:** 1.1 (Revised)  
**Original Design By:** Doc. Duane Erby(KK4AM) and Dan Martin (KD0SMP)  
**Last Updated:** 2024  
**Document Type:** Setup & Operation Manual

---

## 📋 Table of Contents

1. [Introduction](#introduction)
2. [Quick Start Guide](#quick-start)
3. [Hardware Configuration](#hardware-configuration)
4. [AP Mode Setup](#ap-mode-setup)
5. [STA Mode Setup](#sta-mode-setup)
6. [Changing Modes](#changing-modes)
7. [Troubleshooting](#troubleshooting)
8. [Theory of Operation](#theory-of-operation)
9. [Technical Specifications](#technical-specifications)
10. [Safety & Maintenance](#safety-maintenance)
11. [Quick Reference Card](#quick-reference-card)
12. [Revision History](#revision-history)

---

## 🎯 Introduction

The ESP32 CW Trainer is a Morse Code training device implementing the Koch method. It operates as either a WiFi access point or a station on an existing WiFi network. The ESP32 supports dual access modes that can be operated simultaneously.

### Functional Modes

**1. CW Trainer Mode (Default)**
- Koch method training device
- ESP32 sends CW characters
- User recognizes and responds to characters

**2. Iambic Keyer Mode**
- Six preset message memories
- Selected via GPIO pin 18

---

## ⚡ Quick Start Guide

### For Immediate Use (AP Mode - No WiFi Network Needed)

1. **Power on** the ESP32 with GPIO 19 open/unconnected
2. **Connect** your phone/computer to WiFi:
   - Network: `ESP32_Morse_Trainer`
   - Password: `morse1234`
3. **Browse to:** `http://192.168.4.1`
4. **Start training!**

### Blue LED Indicator
- **Solid:** AP is active
- **Brief blinks:** Device connecting/connected
- **Off:** Not in AP mode or no power

---

## 🔧 Hardware Configuration

### GPIO Pin Assignments

| GPIO Pin | Function | Logic HIGH (Float/Open) | Logic LOW (Ground) |
|----------|----------|------------------------|-------------------|
| **18** | Functional Mode | CW Trainer | Iambic Keyer |
| **19** | WiFi Boot Mode | AP Mode | STA Mode |
| **2** | Status LED | Solid when AP active | N/A |

### Important Notes

⚠️ **GPIO 19 is read ONLY at boot time**
- Changing the switch requires a reboot to take effect
- Plan your mode before powering on

⚠️ **Thermal Considerations**
- AP mode is more power-intensive (ESP32 simulates full router)
- Maximum 4 simultaneous connections recommended
- Provide adequate ventilation
- Monitor board temperature during extended use
- Consider adding heatsink for prolonged operation

---

## 📶 AP Mode Setup (Default)

### What is AP Mode?
The ESP32 creates its own WiFi network. No internet connection or existing WiFi required. Perfect for field operation or standalone use.

### Network Information

| Parameter | Value |
|-----------|-------|
| **SSID** | ESP32_Morse_Trainer |
| **Password** | morse1234 |
| **IP Address** | 192.168.4.1 |
| **mDNS Address** | http://cwtrainer.local |
| **Max Connections** | 4 devices |

### Step-by-Step Setup

**Step 1: Configure Hardware**
- Ensure GPIO 19 is open (not connected to ground)
- Power on the ESP32

**Step 2: Connect Your Device**
- On your phone, tablet, or computer
- Open WiFi settings
- Find network: `ESP32_Morse_Trainer`
- Enter password: `morse1234`
- Wait for connection confirmation

**Step 3: Access Web Interface**
- Open any web browser
- Enter: `http://192.168.4.1`
- Alternative (if mDNS supported): `http://cwtrainer.local`
- The training interface will load

**Step 4: Verify Operation**
- Blue LED on ESP32 should be solid
- LED blinks briefly when devices connect
- Web interface should be responsive

### AP Mode Advantages
✅ No existing network required  
✅ Works anywhere  
✅ Simple setup  
✅ Predictable IP address  
✅ Isolated from internet (secure)

### AP Mode Limitations
⚠️ No internet access  
⚠️ Higher power consumption  
⚠️ Limited range (typical WiFi)  
⚠️ Thermal considerations with multiple connections

---

## 🌐 STA Mode Setup

### What is STA Mode?
The ESP32 joins your existing WiFi network (home router, phone hotspot, etc.). Behaves like any other WiFi device on your network.

### Prerequisites

Before starting, ensure you have:
- ✓ Active WiFi network (router or mobile hotspot)
- ✓ Network name (SSID) 
- ✓ Network password
- ✓ Second device available for initial configuration
- ✓ Serial monitor cable (optional but recommended)

### First-Time Setup

**Step 1: Prepare Your Network**
- Turn on your home WiFi router OR
- Enable mobile hotspot on your phone
- Confirm network is broadcasting
- Have SSID and password ready

**Step 2: Configure ESP32 Hardware**
- Connect GPIO 19 to GND (use jumper wire or switch)
- Power on the ESP32
- Wait 5-10 seconds for boot

**Step 3: Connect to Setup Portal**
- On your configuration device (phone/laptop)
- Open WiFi settings
- Find network: `CW_Trainer_Setup`
- Connect using password: `morse1234`
- Captive portal should open automatically
- If portal doesn't open, browse to: `http://192.168.4.1`

**Step 4: Enter Your Network Credentials**
- Portal will display available networks
- Select your network from the list
- Enter your network password carefully
- Click Submit/Connect
- Wait 15-30 seconds

**Step 5: Verify Connection**
- ESP32 connects to your network
- Check Serial Monitor (115200 baud) for confirmation:
```
  ✓ Connected to [your_network_name]!
  IP Address: 192.168.x.x
```
- Note the IP address shown

**Step 6: Access the Trainer**
- Reconnect your device to your normal WiFi
- Open browser and enter the IP address shown, OR
- Use: `http://cwtrainer.local` (if mDNS supported)
- Training interface will load

### Setup Portal Configuration

| Parameter | Value |
|-----------|-------|
| **Setup SSID** | CW_Trainer_Setup |
| **Setup Password** | morse1234 |
| **Portal IP** | 192.168.4.1 |
| **Portal Timeout** | 3 minutes |

### Subsequent Boots (After Initial Setup)

1. Power on ESP32 (GPIO 19 still grounded)
2. ESP32 automatically connects (15-20 seconds)
3. Check Serial Monitor for assigned IP
4. Browse to IP address or `http://cwtrainer.local`
5. No portal configuration needed

**Credentials are saved permanently** until you change networks or reset the ESP32.

### Changing to a Different Network

1. Power off ESP32
2. Reboot ESP32 in STA mode (GPIO 19 grounded)
3. If old network unavailable, setup portal reopens after 3 minutes
4. Connect to `CW_Trainer_Setup` again
5. Enter new network credentials
6. Submit and verify connection

### STA Mode Advantages
✅ Uses existing network infrastructure  
✅ Lower power consumption  
✅ Less thermal load  
✅ Can access from multiple devices easily  
✅ Better range (depends on router)

### STA Mode Limitations
⚠️ Requires existing WiFi network  
⚠️ IP address may change (use mDNS)  
⚠️ Network dependent  
⚠️ More complex initial setup

---

## 🔄 Changing Between Modes

### Switching from AP to STA Mode

1. **Power off** the ESP32
2. **Connect** GPIO 19 to GND (ground)
3. **Power on** the ESP32
4. **Verify** mode via Serial Monitor:
```
   WiFi mode: STA (phone wifi or home wifi browser access)
```
5. **Follow** STA mode setup procedure

### Switching from STA to AP Mode

1. **Power off** the ESP32
2. **Disconnect** GPIO 19 from GND (leave floating/open)
3. **Power on** the ESP32
4. **Verify** mode via Serial Monitor:
```
   WiFi mode: AP (ESP32 hotspot access for your browser)
```
5. **Connect** to `ESP32_Morse_Trainer` network

### Verification Methods

**Method 1: Serial Monitor**
- Connect USB cable to computer
- Open Serial Monitor (115200 baud)
- Send command: `help`
- View status output showing current mode

**Method 2: WiFi Scan**
- Scan for WiFi networks on your device
- If `ESP32_Morse_Trainer` visible = AP mode active
- If not visible = likely STA mode

**Method 3: LED Indicator**
- Blue LED solid = AP mode active
- No blue LED = STA mode or not powered

---

## 🐛 Troubleshooting

### Cannot Connect to ESP32 in AP Mode

**Problem:** `ESP32_Morse_Trainer` network not visible

**Solutions:**
- ✓ Verify GPIO 19 is floating (not grounded)
- ✓ Check power supply is adequate (5V, minimum 500mA)
- ✓ Reboot ESP32
- ✓ Check blue LED is solid (indicates AP active)
- ✓ Move closer to ESP32 (improve signal)
- ✓ Disconnect other devices (max 4 connections)

**Problem:** Connected to WiFi but cannot browse to `192.168.4.1`

**Solutions:**
- ✓ Verify your device obtained IP address (check WiFi settings)
- ✓ Try `http://cwtrainer.local` instead
- ✓ Disable mobile data on phone (forces WiFi use)
- ✓ Clear browser cache
- ✓ Try different browser
- ✓ Check firewall settings on device

### Cannot Connect to ESP32 in STA Mode

**Problem:** Setup portal doesn't appear

**Solutions:**
- ✓ Verify GPIO 19 is grounded
- ✓ Look for `CW_Trainer_Setup` network (not `ESP32_Morse_Trainer`)
- ✓ Wait 3 minutes for portal timeout if previously configured
- ✓ Manually browse to `192.168.4.1`
- ✓ Reboot ESP32

**Problem:** ESP32 won't connect to my network

**Solutions:**
- ✓ Verify network password is correct (case-sensitive)
- ✓ Ensure network is 2.4GHz (ESP32 doesn't support 5GHz)
- ✓ Check router isn't using MAC address filtering
- ✓ Verify network allows new device connections
- ✓ Try mobile hotspot instead to isolate issue
- ✓ Check signal strength (see table below)

**Problem:** Cannot find ESP32 on network after connection

**Solutions:**
- ✓ Check Serial Monitor for assigned IP address
- ✓ Try `http://cwtrainer.local` (requires mDNS support)
- ✓ Use router admin page to find ESP32's IP
- ✓ Disable VPN if active
- ✓ Ensure device is on same network segment

### Signal Strength Issues (STA Mode)

| RSSI Value | Status | Action |
|------------|--------|--------|
| **Better than -75 dBm** | Good | Normal operation |
| **-75 to -85 dBm** | Weak | May drop occasionally, move closer |
| **Worse than -85 dBm** | Unreliable | Switch to AP mode or relocate |

**Check signal strength:**
1. Connect Serial Monitor (115200 baud)
2. Send command: `help` (shows available commands)
3. Send command: `wifi show`
4. Read RSSI value

### Web Interface Issues

**Problem:** Interface loads but doesn't respond

**Solutions:**
- ✓ Refresh browser (Ctrl+F5 or Cmd+Shift+R)
- ✓ Check JavaScript is enabled
- ✓ Try different browser
- ✓ Clear browser cache and cookies
- ✓ Check ESP32 isn't overheating (thermal throttling)

**Problem:** `cwtrainer.local` doesn't work

**Solutions:**
- ✓ Not all devices support mDNS (Bonjour)
- ✓ Use IP address instead
- ✓ iOS/Mac usually support mDNS
- ✓ Android: requires Bonjour app
- ✓ Windows: requires Bonjour service

### Serial Monitor Issues

**Problem:** Cannot connect to Serial Monitor

**Solutions:**
- ✓ Install CP210x or CH340 USB drivers
- ✓ Select correct COM port
- ✓ Set baud rate to 115200
- ✓ Try different USB cable (some are power-only)
- ✓ Check device manager for port conflicts

**Problem:** Garbage characters in Serial Monitor

**Solutions:**
- ✓ Set baud rate to 115200
- ✓ Set line ending to "Both NL & CR"
- ✓ Check USB cable quality

### Resetting the ESP32

**Soft Reset (preserves WiFi credentials):**
- Press RESET button on ESP32 board

**Hard Reset (clears WiFi credentials):**
- Method 1: Flash new firmware
- Method 2: Use factory reset command via Serial Monitor
- Method 3: Erase flash memory with esptool:
  ```bash
  esptool.py --port [COM_PORT] erase_flash
```

### Emergency Recovery

If ESP32 is completely unresponsive:

1. **Power cycle:** Disconnect power, wait 10 seconds, reconnect
2. **Check power supply:** Measure voltage (should be 5V ±0.25V)
3. **Re-flash firmware:** Upload fresh firmware via USB
4. **Check for hardware damage:** Look for burnt components, broken traces
5. **Test with minimal configuration:** Disconnect all external connections except power

---

## 📚 Theory of Operation

### Understanding WiFi Modes

The ESP32 can operate in three WiFi modes:
1. **AP (Access Point)** - Creates its own network
2. **STA (Station)** - Joins existing network
3. **AP+STA** - Both simultaneously (advanced use)

This CW Trainer uses modes 1 and 2 selectively, not simultaneously.

### Access Point (AP) Mode Explained

**What it does:**
- ESP32 acts as a WiFi router (Soft AP)
- Creates local WiFi network
- Assigns IP addresses to connecting devices
- Runs web server for user interface

**Network topology:**
```
[Your Phone/Computer] <--WiFi--> [ESP32 (192.168.4.1)]
                                      |
                                [CW Trainer Software]
```

**Technical details:**
- Network subnet: 192.168.4.0/24
- ESP32 IP: 192.168.4.1 (gateway + server)
- DHCP range: 192.168.4.2 - 192.168.4.10
- Max clients: 4 (hardware limitation)
- Security: WPA2-PSK
- No internet gateway (isolated network)

**Use cases:**
- Field operation without infrastructure
- Demonstrations
- Private training sessions
- When internet isolation is desired
- Emergency communications

**Limitations:**
- Higher CPU usage (routing overhead)
- Increased power consumption
- Thermal concerns with multiple connections
- Limited to WiFi range of ESP32
- No internet access for connected devices

### Station (STA) Mode Explained

**What it does:**
- ESP32 joins existing WiFi network
- Behaves like phone or laptop
- Receives IP from network DHCP
- Runs web server accessible from network

**Network topology:**
```
[Your Phone/Computer] <--WiFi--> [Your Router] <--WiFi--> [ESP32 (192.168.x.x)]
                                                                |
                                                      [CW Trainer Software]
```

**Technical details:**
- IP address: Assigned by DHCP (varies)
- Can use mDNS for consistent addressing (cwtrainer.local)
- Requires 2.4GHz WiFi (ESP32 limitation)
- Supports WPA/WPA2 security
- Lower power consumption than AP mode

**Use cases:**
- Home operation
- Office training
- Shared access among multiple users
- When permanent IP/hostname needed
- Leveraging existing network infrastructure

**Limitations:**
- Requires existing WiFi network
- IP address may change (use mDNS)
- Subject to network configuration/restrictions
- Initial setup more complex
- Depends on router range/signal quality

### Captive Portal Mechanism (STA Mode)

When first booting in STA mode without saved credentials, the ESP32:

1. Starts temporary AP: `CW_Trainer_Setup`
2. Waits for device connection
3. Serves captive portal webpage
4. Scans for available WiFi networks
5. Presents network list to user
6. Accepts credentials
7. Attempts connection to target network
8. Saves credentials to flash memory
9. Provides feedback (success/failure)

**Portal timeout:** 3 minutes
- After timeout, ESP32 retries saved credentials in background
- Reboot reopens portal if connection fails

### mDNS (Multicast DNS)

**What is mDNS?**
- Allows use of `cwtrainer.local` instead of IP address
- Works without DNS server
- Broadcasts hostname on local network

**Compatibility:**
- ✓ iOS/iPadOS: Native support
- ✓ macOS: Native support (Bonjour)
- ✓ Windows: Requires Bonjour service
- ✓ Android: Requires third-party app
- ✓ Linux: Requires Avahi daemon

**Benefits:**
- Consistent address regardless of IP changes
- Easier to remember than IP address
- Professional appearance

### Security Considerations

**Default Password:**
⚠️ Default password `morse1234` should be changed for security
- Modify in firmware before uploading
- Or implement web-based password change feature

**Network Security:**
- AP mode: WPA2-PSK encrypted
- STA mode: Uses target network's security
- Credentials stored in ESP32 flash (encrypted)
- No credentials exposed in source code (runtime entry)

**Best Practices:**
- Change default password
- Use strong WiFi passwords
- Keep firmware updated
- Don't expose to untrusted networks
- Monitor connected devices in AP mode

### Power Consumption

**Typical current draw (5V supply):**
- Idle (STA mode): ~80mA
- Active training (STA): ~120mA
- AP mode (1 client): ~180mA
- AP mode (4 clients): ~250-300mA

**Battery operation considerations:**
- Use STA mode for longer runtime
- Consider power bank (2000mAh = ~6-10 hours)
- AP mode significantly reduces battery life
- Sleep modes not implemented (trainer requires continuous operation)

---

## 📊 Technical Specifications

### Hardware Requirements

**ESP32 Development Board:**
- Chip: ESP32-WROOM-32
- Flash: Minimum 4MB
- RAM: 520KB
- CPU: Dual-core Xtensa 32-bit LX6 (240MHz)

**Power Requirements:**
- Input voltage: 5V via USB or Vin pin
- Operating voltage: 3.3V (regulated on board)
- Current consumption: 80-300mA (mode dependent)
- Recommended power supply: 5V 1A minimum

**GPIO Connections:**

| Pin     | Function    | Type   | Notes                    |
| ------- | ----------- | ------ | ------------------------ |
| GPIO 2  | Status LED  | Output | Internal blue LED        |
| GPIO 18 | Mode select | Input  | Pull-up, read at runtime |
| GPIO 19 | Boot mode   | Input  | Pull-up, read at boot    |

**Optional Connections:**
- Paddle inputs (for Iambic Keyer mode)
- Audio output (for CW tone)
- Serial console (USB, 115200 baud)

### Software Specifications

**Firmware:**
- Platform: Arduino/ESP-IDF
- Web server: Async HTTP
- WiFi: ESP32 native WiFi stack
- DNS: Captive portal implementation
- mDNS: ESP32 mDNS responder

**Network Parameters:**

| Parameter         | AP Mode               | STA Mode          |
| ----------------- | --------------------- | ----------------- |
| **SSID**          | ESP32_Morse_Trainer   | User's network    |
| **Password**      | morse1234             | User's password   |
| **IP Address**    | 192.168.4.1 (fixed)   | DHCP assigned     |
| **Subnet**        | 192.168.4.0/24        | Network dependent |
| **Gateway**       | 192.168.4.1 (self)    | Router IP         |
| **DNS**           | 192.168.4.1 (captive) | Router DNS        |
| **mDNS hostname** | cwtrainer.local       | cwtrainer.local   |

**Web Interface:**
- Protocol: HTTP (port 80)
- Technologies: HTML5, CSS3, JavaScript
- Update method: AJAX/WebSocket
- Browser compatibility: Modern browsers (Chrome, Firefox, Safari, Edge)

**Serial Console:**
- Baud rate: 115200
- Data bits: 8
- Parity: None
- Stop bits: 1
- Flow control: None
- Line ending: CR+LF

### Performance Specifications

**WiFi Performance:**
- Standard: IEEE 802.11 b/g/n
- Frequency: 2.4GHz only (5GHz not supported)
- Range: ~50m indoor, ~100m outdoor (typical)
- Data rate: Up to 150Mbps (PHY rate)

**AP Mode Limits:**
- Maximum clients: 4 simultaneous
- DHCP pool: 8 addresses (192.168.4.2-9)
- Channel: Configurable (default auto)
- Beacon interval: 100ms

**STA Mode:**
- Connection time: 10-20 seconds (typical)
- Reconnect attempts: Unlimited
- Retry interval: 30 seconds

**Web Server:**
- Maximum connections: 4 simultaneous
- Request timeout: 30 seconds
- Response buffer: 8KB
- WebSocket support: Yes (if implemented)

### Operating Environment

**Temperature:**
- Operating: 0°C to 70°C
- Storage: -40°C to 85°C
- ⚠️ Board may heat in AP mode with multiple connections

**Humidity:**
- Operating: 10% to 90% non-condensing
- Storage: 5% to 95% non-condensing

**Physical:**
- Dimensions: ~55mm × 28mm (typical dev board)
- Weight: ~10g
- Mounting: Breadboard compatible, or case

---

## ⚠️ Safety & Maintenance

### Thermal Management

**Normal Operation:**
- STA mode: Minimal heat generation
- AP mode (1 client): Warm to touch (normal)
- AP mode (4 clients): Hot to touch (monitor closely)

**Warning Signs:**
- Board too hot to touch comfortably
- Unexpected reboots or crashes
- WiFi disconnections under load
- Sluggish web interface response

**Cooling Solutions:**
- Ensure adequate ventilation
- Do not enclose in sealed container
- Consider adding heatsink to ESP32 chip
- Use fan for extended AP mode operation
- Limit to 2 clients if thermal issues occur
- Take breaks in heavy use scenarios

### Electrical Safety

**Power Supply:**
- ✓ Use quality 5V USB power supply (1A minimum)
- ✓ Avoid cheap unregulated supplies
- ✗ Do not exceed 5.5V on Vin pin
- ✗ Do not reverse polarity
- ✗ Do not connect 3.3V and 5V simultaneously from different sources

**GPIO Protection:**
- GPIO pins are 3.3V logic ONLY
- Do not connect to 5V signals directly
- Use level shifters for 5V interfacing
- Respect maximum current per pin (12mA recommended, 40mA absolute max)

**ESD Protection:**
- ESP32 is sensitive to static discharge
- Ground yourself before handling
- Use ESD strap when working on circuits
- Store in anti-static bag when not in use

### Maintenance

**Regular Checks:**
- Inspect for physical damage
- Check USB connector for looseness
- Verify LED still functions
- Test both AP and STA modes periodically

**Software Maintenance:**
- Keep firmware updated
- Clear flash if experiencing persistent issues
- Backup configuration (if feature exists)
- Document any custom modifications

**Cleaning:**
- Power off before cleaning
- Use compressed air for dust removal
- Isopropyl alcohol (90%+) for stubborn dirt
- Do not use water or wet cleaners
- Allow to dry completely before powering on

### Long-Term Storage

**Before storing:**
1. Power off device
2. Disconnect all cables
3. Store in anti-static bag
4. Keep in cool, dry location
5. Document any configuration details

**After long storage:**
1. Inspect for corrosion or damage
2. Check power supply voltage before connecting
3. Allow board to acclimate to room temperature
4. Test basic functionality before full deployment

### When to Seek Help

Contact experienced Arduino/ESP32 users if:
- Board doesn't power on at all
- Smoke or burning smell occurs (discontinue use immediately)
- Repeated firmware upload failures
- Physical damage to components
- Cannot resolve persistent software issues

**Resources:**
- ESP32 Arduino Forums
- Local amateur radio club (for CW trainer questions)
- ESP32 subreddit
- GitHub issues (if firmware is open source)

---

## 📞 Quick Reference Card

*Cut out and keep this section for quick access*

---

### Connection Quick Reference

**AP MODE:**
- Network: `ESP32_Morse_Trainer`
- Password: `morse1234`
- URL: `http://192.168.4.1` or `http://cwtrainer.local`
- GPIO 19: Open (floating)

**STA MODE SETUP:**
- Network: `CW_Trainer_Setup`
- Password: `morse1234`
- Portal: `http://192.168.4.1`
- GPIO 19: Grounded

**STA MODE OPERATION:**
- Network: Your WiFi network
- URL: Check Serial Monitor for IP, or `http://cwtrainer.local`
- GPIO 19: Grounded

---

### GPIO Pin Quick Reference

| Pin  | HIGH (Float) | LOW (Ground) |
| ---- | ------------ | ------------ |
| 18   | CW Trainer   | Iambic Keyer |
| 19   | AP Mode      | STA Mode     |

---

### Serial Commands Quick Reference

- Baud rate: `115200`
- Command: `help` - Show all commands
- Command: `wifi show` - Display WiFi status and signal strength

---

### Troubleshooting Quick Checks

1. ✓ Is power supply adequate? (5V, 1A)
2. ✓ Is GPIO 19 in correct position for desired mode?
3. ✓ Did you reboot after changing GPIO 19?
4. ✓ Is blue LED behaving correctly?
5. ✓ Is your target network 2.4GHz (not 5GHz)?
6. ✓ Did you wait 15-20 seconds for connection?

---

### Signal Strength Reference

| RSSI           | Status      |
| -------------- | ----------- |
| > -75 dBm      | Good        |
| -75 to -85 dBm | Weak        |
| < -85 dBm      | Use AP mode |

---

## 📝 Revision History

| Version | Date     | Changes                                                      |
| ------- | -------- | ------------------------------------------------------------ |
| 1.0     | Original | Initial release by Doc. Duane Irby and Dan Martin (KD0SMP)   |
| 1.1     | 2024     | Enhanced documentation: Added troubleshooting, expanded theory section, safety information, technical specifications, formatting improvements |

---

## 📚 References & Credits

**Original Design:**
- Doc. Duane Irby
- Dan Martin (KD0SMP)

**Reference Materials:**
- https://www.upesy.com/blogs/tutorials/how-create-a-wifi-acces-point-with-esp32
- https://www.come-star.com/blog/wifi-ap-vs-sta-mode-ap-sta-comparison/
- ESP32 Arduino Core Documentation
- ESP-IDF Programming Guide

**Additional Resources:**
- Arduino Forum - ESP32 Section
- Random Nerd Tutorials - ESP32 Projects
- ESP32.net Community Resources

---

## 📄 License & Disclaimer

**Usage:** This documentation is provided for amateur radio and educational purposes.

**Disclaimer:** Use this device and documentation at your own risk. The authors and contributors assume no liability for any damage, injury, or loss resulting from the use of this equipment or information.

**Amateur Radio:** This device is intended for use by licensed amateur radio operators in accordance with local regulations.

**FCC Notice (USA):** This is a hobbyist project. Ensure compliance with FCC Part 97 regulations for amateur radio use.

---

## 📧 Support & Community

**For Questions:**
- Consult local amateur radio club
- ESP32 Arduino community forums
- GitHub repository (if available)

**For Contributions:**
- Improvements to documentation welcome
- Firmware enhancements via pull request (if open source)
- Share your experiences with other operators

---

**73 de [Your Call Sign] - Happy CW Training! 📻**

---

*End of Document - ESP32 CW Trainer WiFi Setup Guide v1.1*

---

**Print Notes:**
- Print double-sided for paper conservation
- Consider printing Quick Reference Card on cardstock
- Recommended: Bind or place in protective sheet protectors
- Total pages: Approximately 15-18 (depending on printer settings)
```

---END FILE---

**You now have a complete markdown file!** 

Save this as `ESP32_CW_Trainer_Setup_Guide.md` and you can:
- View it in any markdown editor
- Upload to GitHub/GitLab
- Convert to PDF using Pandoc
- Share with other radio operators
- Print for physical reference

73! 📻
```