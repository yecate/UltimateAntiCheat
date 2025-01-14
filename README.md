# UltimateAntiCheat: An Educational Anti-Cheat built in C++ (x64)

Research project: make an anti-cheat system to detect and/or prevent things like memory editing, debugging, unsigned code/certs, injected modules, etc. 

This project is meant to serve as a very basic anti-cheat program for educational purposes, and is not intended to be commercial software. This is a limited example and includes only basic protections, in production we would aim to have many more detection methods along with a sophisticated server-side design.  

If there is anything not working for you (throws exceptions, can't build, etc) please raise an issue and I will answer it ASAP. If you have code suggestions or techniques you'd like to see added, or want assistance with adding anti-cheat to your program, please feel free to give me an email. More techniques and better design will be added to the project, time permitting.

## Current Detections and protective features:
- Loaded Unsigned Modules
- Debugger detection (hardware, PEB, exceptions, kernelmode)
- Integrity checks on program memory (.text section checks)
- Parent process check
- Remapping sections & re-re-mapping checks (anti-tamper)
- TLS Callback (anti-DLL injection)
- Internal module name randomly renaming (process manipulation)
- Exported function name randomly renaming (process manipulation)
- Encrypted Shellcode payload execution
- Data obfuscation via templated type class
- VTable hooking (code must be fixed)
- Blocks DLL & symbol enumeration within certain tools such as Cheat Engine (modules will show up as undefined or unknown, making it more difficult to manually inspect)

## Requirements
- For remapping to work in this project, /O2 must be enabled for optimization. Choosing other options might cause the program to throw exceptions.

## Updates
- April 3, '24: Added WINAPI hook checking, blacklisted process checking
- March 31, '24: Detection methods have been moved to Detections class, techniques/prevention methods have been moved into Preventions class. Thus we have a set of detections and a set of preventions. Added detection looping to make the program feel more like a commercial AC. Code structure is closer now to where I had aimed originally. There's still much that can be added, stay tuned.
- March 25, '24: Beginning to move tests into their proper places and making use of the API class. Future updates will improve code structure and add threaded & looping support to make the program feel more like a commercial AC product and less like a techniques testing suite
- March 24, '24: A full re-upload was done along with various bug fixes. The program should compile without issues in x64 (x86 not supported yet, apologies). I plan on continuing work on this project in the near future. As it's been some time since making this project, lots more can be added soon with new knowledge and techniques acquired in the time between then and now. Stay tuned for further updates!

![SampleOutput](https://github.com/AlSch092/UltimateAntiCheat/assets/94417808/eba3b526-0003-47aa-833e-79b64f51be36)
