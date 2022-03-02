# SteamVR-VoidScene
Minimal SteamVR scene application serving as a performance-friendly environment to use overlay applications in.

# What it does
- Run as the active VR game/scene application, submitting a static 1x1 pixel texture as the eye textures each frame
- Make SteamVR perform better than when not running any scene application
- Nothing else

# Why
Even with Home disabled, the blank default space in SteamVR takes more GPU time than running a SteamVR scene application that does nothing but submit eye textures. This can have a noticeable performance impact when playing demanding flatscreen games via a desktop overlay for example.

SteamVR-VoidScene attempts to minimize the basic load in order to provide more headroom for everything else in these scenarios.

# Usage
Execute SteamVR-VoidScene.exe. There is no visual indication of it running on the desktop, but it will appear as the active SteamVR application.

SteamVR-VoidScene will exit when quit via SteamVR, by starting another SteamVR scene application or when closing SteamVR itself.

## Configuration
Settings are stored in the global SteamVR settings file (steamvr.vrsettings). The application will add an *"elvissteinjr.void_scene"* section with options that can then be adjusted.

It will look like this:
    
    "elvissteinjr.void_scene" : {
      "BackgroundColor" : "#000000",
	  "UseDebugCommand" : false
    }
	
**BackgroundColor**:  
Color of the submitted eye texture/background. RGB color in hex-notation (#RRGGBB).  
**UseDebugCommand**:  
Use SteamVR debug command to fully suspend rendering and minimize load even more. Either *true* or *false*. 

### Command-Line
Settings can also be changed by running SteamVR-VoidScene with extra command-line arguments. This allows them to be adjusted while SteamVR is running.

**--set-background-color**:  
Set *BackgroundColor* setting on launch. Example: *SteamVR-VoidScene --set-background-color #386386*  
**--set-use-debug-command**:  
Set *UseDebugCommand* setting on launch. Example: *SteamVR-VoidScene --set-use-debug-command true*

These arguments change the settings permanently, meaning the changes persist when running it without any arguments afterwards.

### About UseDebugCommand
This setting enables the use of the *Latency Testing Toggle* SteamVR debug command to fully suspend rendering while also avoiding the loading pop-up and view fade behavior that typically appears when scene applications stop rendering.  
While fairly effective, keep in mind this option is a hack and may stop working at any time or not work at all with certain configurations or hardware.

Keep the background color at #000000 when using this setting to avoid visual artifacts. If you still need a different background color, consider using something like [Desktop+](https://github.com/elvissteinjr/DesktopPlus) which allows setting a background color using overlays.  
SteamVR will not update most frame stats while the command is active.

SteamVR-VoidScene needs to exit cleanly to reset the effects of the debug command. They don't persist over multiple SteamVR sessions, but if the application is killed, any other subsequently launched SteamVR scene application will not be able to render.  
Normal application exits are fine, including launching a different VR scene application to replace it.

# Building
Just compile main.cpp and link it with OpenVR, Direct3D 11, and DXGI.

For example: *g++ -o SteamVR-VoidScene.exe main.cpp openvr_api.dll -ld3d11 -ldxgi*  
Or: *cl main.cpp openvr_api.lib d3d11.lib dxgi.lib shlapi.lib user32.lib /link /out:SteamVR-VoidScene.exe*

# License
SteamVR-VoidScene is licensed under the MIT License, see [LICENSE.txt](LICENSE.txt) for more information.

For the OpenVR API header and library, see [LICENSE-OpenVR.txt](LICENSE-OpenVR.txt).