## SoF2Plus Multiprotocol

This project enables running multiprotocol servers in SoF2 between SoF2 Silver and SoF2 Gold.

It does so by translating certain fields in both entitystates and playerstates before transmission to SOF2 1.00 counterparts.
Triggering multiprotocol can be done with a cvar: ```net_multiprotocol 1``` 

When ```net_multiprotocol``` is not set, the engine acts like a regular SoF2 1.03 engine.

Engine also comes with a quite good in-game autodownloader speed (the one which is activated by ```cl_allowdownload 1 / sv_allowdownload 1```. By setting the cvar ```sv_dlRate 0```, the clients can get up to 1MB/s speeds directly in game.
You can also combine it with a functionality called Smart Download, toggled by ```sv_smartDownload```. When Smart Download is turned on, the client will only be served the .pk3 where the current map is in. So, when running custommaps (a good tip is to have maps in separate pk3's), the client can utilize the server download speed to download a single pk3 - the one currently being played.

As ```sv_smartDownload``` does remove all packages from ```sv_referencedPaks``` except for the current map, there is a possibility to enforce additional packages to be sent to the client with a cvar ```sv_smartAdditionalPaks```. That is a string, space delimited without extension and it will only be served to Gold clients (as SoF2 Silver notoriously didn't have cl_allowDownload enabled by default). But also referencing paks for Silver clients would require some minor adjustments, but it wasn't in the scope of this project.

When building a "debug" release:
* Linux: the engine does depend on libbacktrace (which should be available as a standard package in new GCC's).
* Windows: the engine should be linked with WinDbg. Makefile has not been adjusted on Windows as the build process for Windows was done over Visual Studio.

Debug release brings crash stack trace logging. It will be automatically logged into your fs_game/crashdumps folder. If the game module is built also with debug symbols, you will end up with a clear stack trace from the engine start up until the actual crash logging function call.

If your distro doesn't have libbacktrace available out-of-box, you can build it yourself:
```
git clone https://github.com/ianlancetaylor/libbacktrace.git
cd libbacktrace
./configure
make
sudo make install
```



If you wish to make your own server mod compatible with Multiprotocol, there are some parts which need to be considered:

* The engine has a built-in spoof for public gametypes (so that the client is not required to have a library for the gametype you're trying to play if it's a custom one). Setting it is mandatory on every mod. Cvar is ```g_publicGametype```and it can only be set programmatically by using ```trap_Cvar_Set```. Ensure that is set up in G_InitGame module (if you don't have any custom gametypes or don't need spoofing, just use ```trap_Cvar_Set("g_publicGametype", g_gametype.string);```

* You need to ensure that ```sv_pure``` is set to 0 - it simply doesn't make sense having it turned on in a multiprotocol state.

* Your mod has to disable the weapons which do not exist in Silver: 

```
trap_Cvar_Set("disable_pickup_weapon_silvertalon", "1");
trap_Cvar_Set("disable_pickup_weapon_MP5", "1");
trap_Cvar_Set("disable_pickup_weapon_SIG551", "1");
```


