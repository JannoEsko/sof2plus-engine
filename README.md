## SoF2Plus MP-MV

**SoF2Plus MP-MV** (Multiprotocol-Multiversion) is an enhanced engine based on [sof2plus-engine on GitHub](https://github.com/sof2plus/sof2plus-engine) for *Soldier of Fortune II: Double Helix* that enables seamless interoperability between the **Silver (1.00)** and **Gold (1.03)** protocol versions.  

Originally designed to support one-way Gold → Silver communication and only being compatible with the modern ABI from SoF2Plus, the project has since evolved into a fully bidirectional and modular multiprotocol engine with modern and legacy mods compatibility.

---

### 🧩 Overview

SoF2Plus allows servers to run **multiprotocol environments**, where both Silver and Gold clients can connect and play together.  
Multiprotocol logic works transparently by translating key network structures — such as `entityState` and `playerState` — before transmission.

Enable multiprotocol support using:
```
net_multiprotocol 1
```

When `net_multiprotocol` is **not set**, the engine behaves exactly like the standard engine, based on which mod you run and to what `net_runningLegacy` is set to.

---

### 🚀 Major Features

#### 🔁 Multiprotocol Networking
- Full **bidirectional** compatibility:
  - Gold ↔ Silver (1.03 ↔ 1.00)
  - Works across both modern and legacy game modules.
- Automatic translation of networked states between protocols.
- Configurable through:
  ```
  net_multiprotocol 1
  ```

#### ⚙️ Dual ABI System
SoF2Plus can load and run game modules built for **either modern or legacy ABIs**.

Controlled by:
```
sv_gameModernABI 1   # Use new ABI (1fxplus-compatible)
sv_gameModernABI 0   # Use legacy ABI (for older mods)
```

#### 🎮 Game Compatibility
- Supports both Silver and Gold game environments.
- Runtime detection via:
  ```
  net_runningLegacy 1   # Running Silver/1.00 game module
  net_runningLegacy 0   # Running Gold/1.03 game module
  ```

##### Compatibility table for SoF2Plus engine
| Engine architecture | ABI | Mod type | Compatibility | Comment |
|:--------------------:|:---:|:---------:|:-------------:|:--------:|
| x86 | Original | QVM | ✅ | |
| x86 | Original | Shared Library | ✅ | Runtime patches must be removed if present |
| x86 | Modern | QVM | ✅ | Requires modern ABI usage in the Game module |
| x86 | Modern | Shared Library | ✅ | Requires modern ABI usage in the Game module |
| x86_64 | Original | QVM | ✅ | There's a translation layer for 64-bit pointers to 32-bit handles for the game module, experimental, but so far promising |
| x86_64 | Original | Shared Library | ✅ | Runtime patches must be removed if present |
| x86_64 | Modern | QVM | ❌ | QVMs are 32-bit and considering the usage of modern ABI needs partial rebuild of the mod, you should rather convert it to a shared library instead |
| x86_64 | Modern | Shared Library | ✅ | Requires modern ABI usage in the Game module |

The known issues from the original SoF2 executable (for example `q3dirtrav`, `q3infoboom`, `update` query, etc.) have been fixed within the engine.  
For any additional needs, it is recommended to modify the engine source code directly rather than applying runtime patches in the game module.

---

### 🧩 Cvars and Configuration Changes
Some cvars which existed in the original SoF2MP/sof2ded or which were previously a part of SoF2Plus have also had changes:

| Old Name | New Name | Description/Changes |
|-----------|-----------|-------------|
| `net_port` | `gold_net_port` | Port for Gold/1.03 protocol |
| `legacy_net_port` | `silver_net_port` | Port for Silver/1.00 protocol |
| `sv_clientMod` | `sv_goldClientMod` | Defines Gold-side client mod name |
| `sv_legacyClientMod` | `sv_silverClientMod` | Defines Silver-side client mod name |
| `g_spoofGametype` | `g_spoofGametype` | `1` = show public gametype name, `0` = disable spoofing, now changeable through console or config files |
| `g_publicGametype` | `g_publicGametype` | The gametype name visible to clients when spoofing is active, now changeable through console or config files |

---

### ⚡ Smart and Fast Downloads

SoF2Plus has a **significantly faster in-game autodownloader**, improving the experience for players so that they're able to download .pk3's directly from the server. The exploits which plagued the original engine autodownloader (mainly q3dirtrav) has also been fixed.

Enable it via:
```
cl_allowDownload 1   # That's the cvar for the clients to enable
sv_allowDownload 1   # The server's cvar
sv_dlRate 0          # 0 = unlimited (up to ~1 MB/s)
```

#### Smart Download
Limit downloads to only the `.pk3` containing the current map:
```
sv_smartDownload 1
```

As `sv_smartDownload` does remove all packages from `sv_referencedPaks` except for the current map, there is a possibility to enforce additional packages to be sent to the client with a cvar `sv_smartAdditionalPaks`.
```
sv_smartAdditionalPaks "pak1 pak2 pak3"
```
Additional packages is a space delimited list, where the package names shouldn't contain extensions (.pk3) and do note that they will only be served to Gold clients as it is today.
This does make `sv_pure` usage useless, but running pure server in a multiprotocol state nevertheless wouldn't properly work.

---

### 🪳 Debug Builds and Crash Logging

When running a **Debug** release, SoF2Plus provides **automatic stack trace logging** on crashes. Crash logs are dumped into `fs_game/crashdumps/`, fs_game will depend on the cvar you set it to. If `fs_game` is not set, it is defaulted to `fs_homepath`.

For libraries, if your game module includes debug symbols, you’ll get a detailed trace from engine startup to the crash event. For QVM's, no such functionality is yet implemented.
#### Linux
Requires `libbacktrace` - included internally on modern GCC toolchains. If you're missing it, you can build it as per below:
```bash
git clone https://github.com/ianlancetaylor/libbacktrace.git
cd libbacktrace
./configure
make
sudo make install
```

#### Windows
Debug builds for MSVC are linked with **WinDbg**, for MinGW they're linked with **libbacktrace** 

---

### 🔧 Making Your Mod Compatible with Multiprotocol

If your intention is to develop a new mod, then a good starting base is the [SDK for SoF2Plus, available on GitHub](https://github.com/sof2plus/sof2plus-game)

If you want to add new features to a mod which already has a lot of features, then feel free to use [1fxplus, available on GitHub](https://github.com/JannoEsko/1fxplus)

If you want to use the original 1fxmod for your server, then I've put together a 1fxmod version which removes the workarounds done for the original `sof2ded` here - [1fxmod-sof2plus on GitHub](https://github.com/JannoEsko/1fxmod-sof2plus)

If you want to use your own QVM mod, then check out the details on `net_runningLegacy` and `sv_gameModernABI` above.

If you want to use your own shared library mod, if it was built on an old GCC, you should first rebuild it in a modern context. Otherwise, it should work and you should check out the details on `net_runningLegacy` and `sv_gameModernABI` above.


* Ensure that you disable the weapons which do not exist in either mod (e.g. Silver Talon, SIG551 and MP5 do not exist in Silver; M67, F1, L2A2 and MDN11 do not exist in Gold).
* Ensure that you've set up gametype spoofing if needed
* Ensure that `sv_pure` is set to 0
* Ensure that `sv_goldClientMod` and `sv_silverClientMod` is set up properly.

Your own mod should then also take care of serving scoreboards properly to the clients. 1fxplus has code available how to make scoreboards work on RPM and ROCmod.


---

### Building the engine

There is a GitHub workflow which automatically builds the engine on every commit / PR to master branch. Occasionally, I also generate a Release tag given that enough functionality has been delivered so it deserves a release. The releases can be found [over here](https://github.com/JannoEsko/sof2plus-engine/releases).

If you wish to build the engine by yourself, then you have 2 options - either use the Visual Studio MSVC14 solution files available in `misc/msvc14/` folder or build it with CMake. 

#### Preparations for the build

If you're building with Visual Studio, then there's not much to prepare.

If you're building with CMake over Linux or MSYS / MinGW, along the usual buildchain tools, you need to ensure you have `libbacktrace` installed. Please see above how to install `libbacktrace` if it is missing. 

#### CMake

In the root folder of the repository, run the below command (depending on the architecture and build type you want):

```
cmake -S . -B {your-preferred-folder} -DCMAKE_BUILD_TYPE={release-type} {additional-options}
```

Your preferred folder is truly up to you, for example, you can set it to `build-x86-debug`, which would mean that you're intending to build a debug version in x86 architecture. Release type is either Debug or Release. Valid additional options to use are `FORCE_32BIT` (default is off) and `USE_INTERNAL_LIBS` (default is on). If `USE_INTERNAL_LIBS` is turned off, you need to have zlib installed as well.

**Please note that when building x86, using option `FORCE_32BIT` is required**

So for example, to build a x86 Debug engine, you'd run the command:
```
cmake -S . -B build-x86-debug -DCMAKE_BUILD_TYPE=Debug -DFORCE_32BIT=On
```

If you'd want to build a x64 Release engine, you'd run the command:
```
cmake -S . -B build-x86_64-debug -DCMAKE_BUILD_TYPE=Release 
```

After that, head to the generated folder and just type `make`. That will build the engine based on the defined configuration.
