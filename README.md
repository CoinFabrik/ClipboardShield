# 🛡️ Clipboard Shield

🔐 Safeguard your privacy by preventing malicious applications from accessing sensitive data copied to the clipboard.

Clipboard Shield is a project designed to monitor and protect the system clipboard from unauthorized access.

## ⭐️ Key Features
- ✔️ Protection of clipboard content against unauthorized access.
- ✔️ Implementation of hooks for event detection.
- ✔️ Inter-process communication (IPC) for security management.
- ✔️ Use of custom threads to optimize performance.
- ✔️ Logging capabilities for security auditing.
- ✔️ Windows service integration for continuous protection.

## 🔨 Building from Source

For users who prefer to build from source, follow the detailed instructions available at the documentation: [Build from Source](doc/BuildFromSource.md).

## ⚙️ Installation from Binary

### Supported Platform

* Tested on: Windows 10
* Requeriments:
  * Windows 10 (64-bit)
  * Administrator privileged (required for installation)
 
### Installation Steps   

1. Download the latest binary release from [GitHub Releases](https://github.com/CoinFabrik/ClipboardShield/releases) 
2. Run `ClipboardShield.exe` to install the application.
3. Launch `trayicon.exe` to monitor the service status. The application should be running in the system tray.

## 📌 Usage Examples
To verify the security measures:
- Use a clipboard monitoring tool to check if unauthorized access is blocked.
- Run a script that attempts to read the clipboard and confirm that access is denied.
- Check the log files for any recorded access attempts

### Sample config.txt
```policy allow

source-deny c:\windows\system32\notepad.exe
    allow c:\windows\system32\notepad.exe

source-deny C:\Program Files\KeePass\KeePass.exe
    allow C:\Program Files\Mozilla Firefox\firefox.exe
    allow C:\Program Files\Google\Chrome\Application\chrome.exe

source-allow C:\Program Files\Mozilla Firefox\firefox.exe
    deny C:\Program Files\Google\Chrome\Application\chrome.exe
```
Explanation:
* By default, any application can copy text to any other application.
* Data copied from Notepad can only be pasted into Notepad.
* Data copied from KeePass can only be pasted into Firefox and Chrome.
* Data copied from Firefox cannot be pasted into Chrome


## 💡 Contribution
If you would like to contribute:
- Fork the repository.
- Create a new **branch** (`feature-new-functionality`).
- Submit a **pull request** with a clear description of the changes.

## 🌐 About CoinFabrik 
We - [CoinFabrik](https://www.coinfabrik.com/) - are a research and development company specialized in Web3, with a strong background in cybersecurity. Founded in 2014, we have worked on over 180 blockchain-related projects, EVM based and also for Solana, Algorand, Stellar and Polkadot. Beyond development, we offer security audits through a dedicated in-house team of senior cybersecurity professionals, currently working on code in Substrate, Solidity, Clarity, Rust, TEAL and Stellar Soroban.

Our team has an academic background in computer science and mathematics, with work experience focused on cybersecurity and software development, including academic publications, patents turned into products, and conference presentations. Furthermore, we have an ongoing collaboration on knowledge transfer and open-source projects with the University of Buenos Aires.

## 📜 License
This project is licensed under the MIT License - see the LICENSE file for details.

