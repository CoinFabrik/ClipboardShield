# 🛡️ Clipboard Shield

Clipboard Shield is a project designed to monitor and protect the system clipboard from unauthorized access.

🔐 Safeguard your privacy by preventing malicious applications from accessing sensitive data copied to the clipboard.

## ⭐️ Key Features
- ✔️ Protection of clipboard content against unauthorized access.
- ✔️ Implementation of hooks for event detection.
- ✔️ Inter-process communication (IPC) for security management.
- ✔️ Use of custom threads to optimize performance.
- ✔️ Logging capabilities for security auditing.
- ✔️ Windows service integration for continuous protection.

## 🚀 **Demo**
🔧 [Coming Soon] – A demo video or screenshots will be added.


## 🔨 Building from Source

### **Requirements**
- Visual Studio 2022
- Boost 1.76 or newer

### **Instructions**
1. Clone the repository:
   ```sh
   git clone https://github.com/CoinFabrik/ClipboardShield.git
   ```
2. Open `Src\ClipboardFirewall.sln` in Visual Studio.
3. Build projects in **Release** in the following order:
   - **Batch build for all platforms:**
     1. CFManualInjector
     2. ClipboardFirewallDll
     3. InjectDll
   - Build **code_generation** and **trayicon**.
   - Run `hash_checker` to validate the hashes. If errors occur, rebuild **code_generation**.
   - Build **ClipboardFirewallService** and **DriverInstaller** for x64.

## ⚙️ Installation from Binary

1. Download the latest binary release from GitHub Releases.
2. Extract Binary
1. Create a directory (`%DST%`) to store the executable files (e.g., `C:\Program Files\CoinFabrik\Clipboard Firewall`). Copy the following files:
   - `Src\bin\ClipboardFirewallDll32.dll`
   - `Src\bin\CFManualInjector32.exe`
   - `Src\bin\InjectDll32.exe`
   - `Src\bin\trayicon.exe`
   - `Src\bin64\ClipboardFirewallDll64.dll`
   - `Src\bin64\ClipboardFirewallService64.exe`
   - `Src\bin64\CFManualInjector64.exe`
   - `Src\bin64\InjectDll64.exe`
2. Move `driver_binary\x64\DeviarePD.sys` to `Src\bin64\Drivers\Vista\DeviarePD.sys`.
3. (Admin) From inside `Src\bin64`, run:
   ```sh
   DriverInstaller64.exe -installdriver
   ```
4. Create the config file at `%ProgramData%\CoinFabrik Clipboard Shield\config.txt` (refer to `/doc/sample/c/ProgramData/CoinFabrik Clipboard Shield/config.txt`).
5. (Admin) From inside `%DST%`, run:
   ```sh
   ClipboardFirewallService64.exe -install
   ```
6. (Admin) Start the service:
   ```sh
   net start ClipboardShield
   ```
7. Run `trayicon.exe` to monitor the service status.

## Usage

## 🧪 Testing
To verify the security measures:
- Use a clipboard monitoring tool to check if unauthorized access is blocked.
- Run a script that attempts to read the clipboard and confirm that access is denied.
- Check the log files for any recorded access attempts

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

