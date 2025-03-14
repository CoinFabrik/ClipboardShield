# Configuration

# Introduction

The purpose of ClipboardShield is to allow setting policies to control access to the Windows clipboard based on the identity of the program trying to paste from it. When a process attempts to paste from the clipboard, ClipboardShield intercepts that request and compares the operation to the rules configured into it, and can either allow or deny the request accordingly, depending on which program last copied to the clipboard. Besides a global default policy, ClipboardShield supports two different kinds of rules: one to control access based on the program that last wrote to the clipboard, and the other to control access based on the program that’s trying to read the clipboard.

The configuration is stored as a text file in `%ProgramData%\CoinFabrik Clipboard Shield\config.txt`.

# Basic example

```
policy allow

source-deny c:\windows\system32\notepad.exe  
	allow c:\windows\system32\notepad.exe

source-deny C:\Program Files\KeePass\KeePass.exe  
	allow C:\Program Files\Mozilla Firefox\firefox.exe  
	allow C:\Program Files\Google\Chrome\Application\chrome.exe

destination-allow C:\Program Files\Mozilla Firefox\firefox.exe  
	deny C:\Program Files\Google\Chrome\Application\chrome.exe
```

## Default policy

The policy directive defines a global policy for the entire system. `allow` is the default, and allows any process to read the clipboard at any time, unless forbidden by another rule. The alternative, `deny` forbids any process from reading the clipboard, unless otherwise allowed by another rule.

## Rule groups

Rule groups define policies around a single executable that acts as either the reader or writer in a clipboard operation. The main directive can be either `source-allow`, `source-deny`, `destination-allow`, or `destination-deny`, and must be followed by the path to the executable. Besides the main directive, a rule group can contain zero or more subrules that control exceptions to the main directive. Note that subrules must be indented with either tabs or spaces.

### source-allow/source-deny

This directive controls clipboard operations based on the last program that copied to the clipboard. The path specified in its subrules refer to the program that is trying to paste from the clipboard at a given time. In the above example, the second rule group means, "after KeePass has copied to the clipboard, no process can read the clipboard other than Firefox and Chrome".

### destination-allow/destination-deny

This directive is the reverse of `source-*`. It controls clipboard operations based on the program that is trying to paste from the clipboard at a given time. The path specified in its subrules refer to the program that last copied the clipboard when the paste occurred. In the above example, the third rule group means, "when Firefox attempts to paste from the clipboard, it is allowed to do so unless Chrome last copied to it".

# Sample use cases

## Use case 1: Complete lock-down

```
policy deny
```

This will prevent any process from reading the clipboard from any other process. A process can still copy from itself to itself. Thus, for example, you can copy and paste text within the same Notepad window, but not from one Notepad window to another.

## Use case 2: Password manager protection

```
source-deny C:\Program Files\KeePass\KeePass.exe  
	allow C:\Program Files\Mozilla Firefox\firefox.exe  
	allow C:\Program Files\Google\Chrome\Application\chrome.exe
```

In this case, we’d like to prevent random processes from just peeking into the passwords KeePass puts on the clipboard. We can add more programs that are allowed to paste KeePass passwords by adding more allow subrules. It’s also possible to remove all subrules to not allow anyone to past KeePass passwords, but that’s not as useful.

## Use case 3: Preventing command overrides

```
destination-deny C:\Windows\System32\cmd.exe  
	allow C:\Windows\System32\notepad.exe
```

Suppose we use cmd to control important computers and we don’t want random processes being able to override commands that we copy-and-paste between our notes. We can use the above rule to forbid cmd from reading the clipboard when anything other than Notepad has copied to the clipboard.

# Grammar

```
config: [ "policy" {"allow" | "deny"} NEWLINE ] ( rule-group )*

rule-group: main-directive SPACE PATH NEWLINE ( subrule )*

main-directive: "source-allow" | "source-deny" | "destination-allow" | "destination-deny"

subrule: SPACE { "allow" | "deny" } SPACE PATH NEWLINE
```
