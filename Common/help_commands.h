R"(
 _     __   _____  _          __    ___   _      ___    __    _      _   ___   _     
| |   / /`_  | |  \ \  /     / /`  / / \ | |\/| | |_)  / /\  | |\ | | | / / \ | |\ | 
|_|__ \_\_/  |_|   \_\/      \_\_, \_\_/ |_|  | |_|   /_/--\ |_| \| |_| \_\_/ |_| \| 

  ~~~~~~~~~~~~~~~~~~~~~~~~  Command Line Interface %%VER%% ~~~~~~~~~~~~~~~~~~~~~~~

This help text contain an overview of available commands. for the full documentation please vist:
https://github.com/JPersson77/LGTVCompanion/blob/master/Docs/Commandline.md

## Format of the command line ##
-[command] [[Argument 1] ... [Argument X]] [[Device1|Name] ... [DeviceX|Name]]

## General usage info ##
Commands are prefixed with a hyphen (-) and arguments are separated by a space character. 
Control characters (hyphen and space) are escaped with double quotes ("). 
The "Device" arguments are optional. 
Multiple commands can be sent at once. 
Commands are case-insensitive, but JSON typically is case-sensitive.

## Formatting the JSON output of subsequent commands
-output [default|friendly|key] [key]            format or filter the output
-od                                             equal to -output default
-of                                             equal to -output friendly
-ok [key]                                       equal to -output key

## HDMI input commands ##
-setHdmi [input]                                set active HDMI input
-setHdmiX                                       equal to -sethdmi X
-set_input_type [hdmi_input] [icon] [label]     set type of input
-gameMode_hdmiX [off|on]                        set Game Optimizer for HDMI-input X
-uhdDeepColor_hdmiX [off|on]                    set UHD Deep Color for HDMI-input X
-gameOptimization_hdmiX [off|on]                set game optimization (Instant game response) for HDMI-input X
-freesyncOled_hdmiX [off|on]                    set OLED freesync for HDMI-input X
-hdmiPcMode_hdmiX [off|on]                      set PC-mode for HDMI-input X

## Power commands ##
-powerOff                                       power off the device(s)
-powerOn                                        power on the device(s)
-screenOff                                      blank the screen, i e disable the emitters
-screenOn                                       unblank the screen (and power on if device is off)

## Button commands ##
-button [button]                                virtual remote button press
-freesyncinfo                                   show freesync info panel with FPS (the 7 x tap on green button)

## Get system, picture or audio settings ##
-get_system_settings [category] [json_keys]     get value(s) of settings in a category

## Audio settings ##
-mute                                           mute sound
-unmute                                         unmute sound
-soundMode [mode]                               set sound mode
-soundOutput [output]                           set sound output
-autoVolume [off|on]                            set automatic volume

## Picture settings ##
-pictureMode [mode]                             set picture mode
-backlight [0 -> 100]                           set backlight / OLED brightness
-contrast [0 -> 100]                            set contrast
-brightness [0 -> 100]                          set brightness
-color [0 -> 100]                               set Color
-sharpness [0 -> 50]                            set sharpness
-colorGamut [auto|extended|wide|srgb|native]    set the color gamut
-dynamicContrast [off|low|medium|high]          set the dynamic contrast
-gamma [low|medium|high1|high2]                 set gamma
-colorTemperature [-50 -> 50]                   set the color temperature
-whitebalanceColorTemperature [value*]          set white balance color temperature
-eyeComfortMode [off|on]                        set eye comfort mode
-dynamicColor [off|on|low|medium|high]          set dynamic Color
-peakBrightness [off|low|medium|high]           set peak brightness
-smoothGradation [off|low|medium|high]          set smooth gradation
-energySaving [value*]                          set energy saving mode
-hdrDynamicTonemapping [on|off|HGIG]            set HDR dynamic tone mapping
-blackLevel [low|medium|high|limited|full]      set black level
-dolbyPrecisionDetail [off|on]                  set Dolby Precision detail
-ai_brightness [off|on]                         set AI brightness
-ai_genre [off|on]                              set AI genre
-ai_picture [off|on]                            set AI Picture Pro
-arcPerApp [value]                              set aspect ratio
-justScan [off|on|auto]                         set Just Scan
-allDirZoomHratio [0 -> 10]                     set all-direction zoom horizontal ratio
-allDirZoomVratio" [0 -> 9]                     set all-direction zoom vertical ratio
-allDirZoomHposition [-10 -> 9]                 set all-direction zoom horizontal position
-allDirZoomVposition [-9 -> 9]                  set all-direction zoom vertical position
-vertZoomVposition [-8 -> 9]                    set vertical zoom position
-vertZoomVratio [0 -> 9]                        set vertical zoom ratio
-truMotionMode [value*]                         set TruMotion
-truMotionJudder [0 -> 10]                      set TruMotion judder
-truMotionBlur [0 ->10]                         set TruMotion blur
-motionProOled [off|low|medium|high]            set OLED Motion Pro
-motionPro [off|on]                             set Motion Pro
-realCinema [off|on]                            set Real Cinema
-lowLevelAdjustment [-30 -> 30]                 set Fine Tune Dark Areas
-blackStabilizer [-30 -> 30]                    set Black Stabilizer
-whiteStabilizer [-30 -> 30]                    set White Stabilizer
-blueLight [off|level1|level2]                  set Reduce Blue Light
-gameOptimization [off|on]                      set VRR and G-Sync
-inputOptimization [auto|on|standard|boost]     set Prevent Input Delay
-freesync [off|on]                              set AMD Freesync Premium

## Network settings ##
-wol [true|false]                               set Wake-On-Lan, a k a "On with Mobile" or "Turn on via Wi-Fi"

## Other settings
-set_curve_preset [preset]						Set the curve preset for compatible devices
-adjust_curve_preset [preset] [0 -> 100]		Adjust value of curve preset for compatible devices
-set_curvature									Set the curvature for compatible devices.
-ambientlight [off|on]							Enable or disable the ambient light for compatible models
-ambientlight_mode [value*]						Set the ambient lighting mode
-ambientlight_brightness [1 -> 10]				Set the ambient light brightness
-ambientlight_staticmodecolor1 [0 -> 42]		Set the color for ambient static mode color 1
-ambientlight_staticmodecolor2 [0 -> 42]		Set the color for ambient static mode color 2
-ambientlight_staticmodecolor3 [0 -> 42]		Set the color for ambient static mode color 3
-ambientlight_staticmodecolor4 [0 -> 42]		Set the color for ambient static mode color 4

## Sending generic reguests ##
-request [endpoint]                             send a request to an endpoint
-request_with_param [endpoint] [payload]        send a request to an endpoint with JSON payload
-start_app [id]                                 launch application
-start_app_with_param [id] [payload]            launch application with JSON payload
-close_app [id]                                 close application
-settings_picture [payload]                     send a JSON payload with settings for the "picture" category 
-settings_other [payload]                       send a JSON payload with settings for the "other" category
-settings_options [payload]                     send a JSON payload with settings for the "options" category
)"