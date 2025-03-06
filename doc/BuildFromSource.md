# Building from source

# Requirements

* Visual Studio 2022  
* Boost 1.76 or newer

# Instructions

1. `git clone https://github.com/CoinFabrik/ClipboardShield.git`
2. Open `Src\\ClipboardFirewall.sln`
3. Build projects in Release in the following order:  
   1. Batch build for all platforms:  
      1. CFManualInjector  
      2. ClipboardFirewallDll  
      3. InjectDll  
   2. Build code\_generation and trayicon. Run hash\_checker to validate that the hashes were generated correctly. If the program detects an error, rebuild code\_generation.  
   3. Build ClipboardFirewallService and DriverInstaller for x64.

# Installation

1. Create a directory (%DST%) somewhere to keep the executable files. E.g. `C:\\Program Files\\CoinFabrik\\Clipboard Firewall`. Copy to that directory:  
   1. `Src\\bin\\ClipboardFirewallDll32.dll`
   2. `Src\\bin\\CFManualInjector32.exe`
   3. `Src\\bin\\InjectDll32.exe`
   4. `Src\\bin\\trayicon.exe`
   5. `Src\\bin64\\ClipboardFirewallDll64.dll`
   6. `Src\\bin64\\ClipboardFirewallService64.exe`
   7. `Src\\bin64\\CFManualInjector64.exe`
   8. `Src\\bin64\\InjectDll64.exe`
2. Move `driver\_binary\\x64\\DeviarePD.sys` to `Src\\bin64\\Drivers\\Vista\\DeviarePD.sys`
3. (admin) From inside `Src\\bin64` run: `DriverInstaller64.exe \-installdriver`
4. Create `%ProgramData%\\CoinFabrik Clipboard Shield\\config.txt`. See `doc/sample/c/ProgramData/CoinFabrik Clipboard Shield/config.txt` for an example.  
5. (admin) From inside %DST% run: `ClipboardFirewallService64.exe \-install`
6. (admin) Start the service CoinFabrik Clipboard Shield: `net start ClipboardShield`
7. Run `trayicon.exe` to monitor the state of the service.