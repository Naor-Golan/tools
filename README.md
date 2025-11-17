# Tzofeh

Keylogger for Windows and Linux systems built on C/C++

## Features
- Captures live keystrokes while being updated immediatly
- Persistent for both Linux and Windows
- Encrypted keystroke capture with decrypter
- Allows exfil of data by sharing the file on lan (`IP_ADDR:63333`)

## Installation
Windows and Linux each have their respective install instructions in their own folders.
<br>
<br>
The python decrypter is used for decrypting the file once you extracted it from the target machine as mentioned in the features.
<br>
<br>
The log files will be automatically named as the MAC address and IP of the machine. Rename it to log.enc once ready.
<br>
<br>
To use the decrypter place the log.enc file in the same directory as the decrypt.py file and run it:
   ```bash
   python decrypt.py