# LGTV Companion Command line documentation

## Disclaimer:
This is a powerful tool so please use these commands responsibly! The author of this app shall not be liable for any damage incurred. ❤️
## Usage of the command line parameters
"LGTV companion.exe" accepts command line arguments which can be used to control aspects of the application and any attached WebOS-device. The general format for the command line is:

*LGTV Companion.exe -[command] [[Argument 1] [Argument 2] ... [Argument X]] [[Device1|Name] [Device2|Name]...[DeviceX|Name]]

All command line follow the above structure, except as noted specifically. All commands must be prefixed with a hyphen (-) and all following arguments of the command line shall be separated by the space character. It is however possible to escape the control characters (hyphen and space) by using double quotes ("). 

The device argument is always optional, but when it is used it shall be specified by using either the index (e g Device1) or the friendly name (e g OLED42C2). If you omit the device argument the application will apply the command to all configured devices. Please note that device ID's are assigned by the order they have been added, i e the topmost in the device configuration drop down in the main UI is Device1, the seconds topmost is Device2 etc. 

Multiple commands can be sent at once - every new hyphen (which is not escaped by double quotes) indicates the start of a new command.

Please note! The double quotes (") are an important component of the LG JSON API, so whenever you want to send JSON to a device please escape the double quotes with \\. 

Please see the many examples below to learn more. Don't worry, it's not super complicated! 👍 

Please also note that feature sets vary between models and model years and it's not guaranteed that all commands below will work for all models/series of devices.
## Scripting support
LGTV Companion supports bidirectional communication and support for external scripting, as outlined [here](https://github.com/JPersson77/LGTVCompanion/blob/master/Docs/Scripting.md). This can be used in a lot of ways, to automate tasks and customise the behaviour of your WebOS-device.

## The two command line interfaces
Please note that both "LGTV Companion.exe" and "LGTVcli.exe" support all of the commands below (some exceptions are noted below). There are however a few notable differences in the usage.

"LGTV Companion.exe" is a multi-threaded windows application and will send your commands to the devices as quickly as possible and without waiting for a response. 

"LGTVcli.exe" on the other hand is single-threaded application which will relay the response from the devices and present them to the user in a JSON object, and in the correct order. So as a rule of thumb, if you just need to fire-and-forget a command you can use "LGTV Companion.exe" but if you are interested in the response then "LGTVcli.exe" should be used. 

## Formatting the output of LGTVcli (LGTVcli only)
Almost all console (STDOUT) output of LGTVCli is in the form of a JSON container. For easy usage there are a few command line options for formatting the output. 
- *-output [default | friendly | key] [key]*	- format the output
- *-od* 	- shorter version of -output default
- *-of* 	- shorter version of -output friendly
- *-ok* [key] 	- shorter version of -output key 

All other commands following the -output command will obey the formatting. Please note that the default output is compact and with no formatting. The friendly output adds indentation and line breaks to make the output human-friendly. When using the "key" argument you must also supply the name of the key to filter for and then only the value of the respectivev key is output.
#### Examples:
*Power on the device with id Device1 and apply friendly formatting to the output.*
```
"LGTVcli.exe" -of -poweron Device1 "OLED 42"
```
*Get the value of the backlight for Device1 and filter the output for the backlight key (the below will output only the value, f e 100)*
```
"LGTVcli.exe" -ok backlight -get_system_settings picture [\"backlight\"]
```

## Power commands (LGTV Companion and LGTVcli)
- *-poweroff* 	- power off the device(s).
- *-poweron* 	- power on
- *-screenoff* 	- blank the screen, i e disable the emitters.
- *-screenon* 	- unblank the screen, and power on.
#### Examples:
*Power on the device with id Device1 and the device named "OLED 42". Please note how the double quotes are used to escape the space (which is a control character) in the friendly device name.*
```
"LGTV Companion.exe" -poweron Device1 "OLED 42"
```
*Power off all configured devices*
```
"LGTV Companion.exe" -poweroff
```
## Application commands (LGTV Companion only)
- *-autodisable* 	- temporarily disable the automatic management, i.e. to stop processing power events for device(s). This is effective until next restart of the service. 
- *-autoenable* 	- temporarily enable the application's automatic management of a device. This is effective until next restart of the service.
- *-clearlog* 		- clear the application log. This takes no further arguments
- *-idle* 			- enable user idle mode. This is a global setting and takes no further arguments.
- *-unidle* 		- disable user idle mode. This is a global setting and takes no further arguments.

#### Examples: 
*Power on Device1 and then disable automatic management of Device1*
```
"LGTV Companion.exe" -poweron Device1 -autodisable Device1
```
## HDMI input commands (LGTV Companion and LGTVcli)
- *-sethdmi1 | sethdmi2 | sethdmi3 | sethdmi4* - set active HDMI input to 1, 2, 3 or 4
- *-sethdmi [1 | 2 | 3 | 4]* - equivalent to the above but the active input is supplied using an argument.
- *-set_input_type [hdmi_input] [icon] [label]* - Set the type of the input. This can be used to switch f e between PC and non-PC modes
>[hdmi_input]: HDMI_1, HDMI_2, HDMI_3, HDMI_4  
>[icon]: HDMI_1, HDMI_2, HDMI_3, HDMI_4, satellite, settopbox, dvd, bluray, hometheater, gameconsole,streamingbox, camera, mobile, pc  
>[label]: "HDMI 1", "HDMI 2", "HDMI 3", "HDMI 4", Satellite, "Set-Top Box", "DVD Player", "Blu-ray Player", "Home Theatre", "Game Console", "Streaming Box", "Generic Camera", "Mobile Device", PC  
- *-gamemode_hdmiX [off | on]* 	- set Game Optimizer for HDMI-input X (X = 1, 2, 3 or 4)
- *-uhddeepcolor_hdmiX [off | on]* 	- set UHD Deep Color for HDMI-input X (X = 1, 2, 3 or 4) 
- *-gameoptimization_hdmiX [off | on]* 	- set game optimization (Instant game response) for HDMI-input X (X = 1, 2, 3 or 4)
- *-freesyncoled_hdmiX [off | on]* 	- set OLED freesync for HDMI-input X (X = 1, 2, 3 or 4) 
- *-hdmipcmode_hdmiX [off | on]* 	- set PC-mode for HDMI-input X (X = 1, 2, 3 or 4) 
#### Examples: 
*Set active HDMI-input to 1 for Device2 and then set active HDMI-input to 2 for Device2*
```
"LGTV Companion.exe" -sethdmi1 Device1 -sethdmi 2 Device2
```
*Set HDMI-input to PC-mode for Device1 and then set HDMI-input 2 to default for Device2*
```
"LGTV Companion.exe" -set_input_type HDMI_1 pc PC Device1 -set_input_type HDMI_2 HDMI_2 "HDMI 2" Device2
```
*Enable game optimiser for HDMI-input 1*
```
"LGTV Companion.exe" -gamemode_hdmi1 on Device1
```
## Audio commands (LGTV Companion and LGTVcli)
- *-mute* 	- mute the built-in-speakers of a device
- *-unmute* - unmute the built-in-speakers of a device
- *-soundmode [aiSoundPlus | aiSound | standard | news | music | movie | sports | game]* 	- set sound mode
- *-soundoutput [tv_speaker | external_arc | external_optical | bt_soundbar | mobile_phone | lineout | headphone | tv_speaker_bluetooth | tv_external_speaker | tv_speaker_headphone | wisa_speaker]* 	- set sound output 
- *-autovolume [off | on]* 	- set automatic volume
#### Examples: 
*Mute Device1*
```
"LGTV Companion.exe" -mute Device1
```
## Button commands (LGTV Companion and LGTVcli)
- *-button [button]* 	- virtual remote key press 

>[button]: LEFT, RIGHT, UP, DOWN, RED, GREEN, YELLOW, BLUE, CHANNELUP, CHANNELDOWN, VOLUMEUP, VOLUMEDOWN, PLAY, PAUSE, STOP, REWIND, FASTFORWARD, ASTERISK, BACK, EXIT, ENTER, AMAZON, NETFLIX, 3D_MODE, AD *(Audio Description)*, ADVANCE_SETTING, ALEXA, AMAZON, ASPECT_RATIO, CC *(Closed Captions)*, DASH *(Live TV)*, EMANUAL, EZPIC, EZ_ADJUST *(CAREFUL! EzAdjust Service Menu. Default code is 0413)*, EYE_Q, GUIDE, HCEC, HOME (Dashboard), INFO, IN_START *(CAREFUL! InStart Service Menu. Default code is 0413)*, INPUT_HUB, IVI, LIST, LIVE_ZOOM, MAGNIFIER_ZOOM, MENU, MUTE, MYAPPS, NETFLIX, POWER, PROGRAM, QMENU, RECENT, RECLIST, RECORD, SAP, SCREEN_REMOTE, SEARCH, SOCCER, TELETEXT, TEXTOPTION, TIMER, TV, TWIN, UPDOWN *(Always Ready app)* USP, YANDEX, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9
#### Examples: 
*Display the info-panel on device 1*
```
"LGTV Companion.exe" -button INFO device1
```
*Display the service menu on device 1. Enter defautl code 0413 if you are certain you want to enter the srvice menu. Please use caution in the service menu!*
```
"LGTV Companion.exe" -button IN_START device1
```
## Retrieving system settings (LGTVcli only)
- *-get_system_settings [category] [Json-array of keys]* 	- get one or many system settings in a specified category. Supported values below:
```
"twinTv": [
                "status",
                "role",
                "systemMode"
        ],
        "network": [
                "deviceName",
                "wolwowlOnOff",
                "bleAdvertisingOnOff"
        ],
        "option": [
                "audioGuidance",
                "country",
                "zipcode",
                "livePlus",
                "firstTvSignalStatus",
                "addressInfo",
                "phlCitySelection",
                "smartServiceCountryCode3",
                "syncMode",
                "storeMode",
                "localeCountryGroup",
                "japanCitySelection",
                "countryBroadcastSystem",
                "yourMomentsVersion",
                "wallPaperSettings"
        ],
        "time": [
                "onTimerVolume",
                "timeZone"
        ],
        "picture": [
                "brightness",
                "backlight",
                "contrast",
                "color",
                "energySaving"
        ],
        "" : [
                "eulaStatus",
                "eulaInfoNetwork",
                "mobileSetupStatus",
                "localeInfo"
        ],
        "other": [
                "simplinkEnable",
                "ueiEnable",
                "gameWallpaper"
        ],
        "sound": [
                "avSync",
                "avSyncSpdif",
                "avSyncBypassInput",
                "eArcSupport",
                "soundOutput",
                "soundOutputDigital",
                "soundMode",
                "tvSetupConfiguration"
        ],
        "lock": [
                "parentalGuidance",
                "ziggoRaiting"
        ],
        "general": [
                "alwaysOn",
                "tvOnScreen",
                "tvInstallMethod",
                "powerOffBySCA3SystemChanged",
                "SCA3SystemCountry",
                "homeAutoLaunch",
                "lifeOnScreenMode"
        ]
```

#### Examples: 
*Get the current values for contrast, color and backlight for device 1*
```
"LGTVcli.exe" -get_system_settings picture [\"color\",\"contrast\",\"backlight\"] device1
```

## Picture commands (LGTV Companion and LGTVcli)
- *-picturemode [mode]* 	- set picture mode

>[mode]: cinema, eco, expert1, expert2, game, normal, photo, sports, technicolor, vivid, hdrEffect, filmMaker, hdrCinema, hdrCinemaBright, hdrExternal, hdrGame, hdrStandard, hdrTechnicolor, hdrVivid, hdrFilmMaker, dolbyHdrCinema, dolbyHdrCinemaBright, dolbyHdrDarkAmazon, dolbyHdrGame, dolbyHdrStandard, dolbyHdrVivid, dolbyStandard
- *-backlight [0 -> 100]* - set backlight.
- *-contrast [0 -> 100]*	 - set contrast
- *-brightness [0 -> 100]* 	-set brightness
- *-color [0 -> 100]* 			- set Color
- *-sharpness [0 -> 50]*		-set sharpness
- *-colorgamut [auto|extended|wide|srgb|native]*		-set the color gamut
- *-dynamiccontrast [off|low|medium|high]* - set the dynamic contrast
- *-gamma [low|medium|high1|high2]*		-set gamma
- *-colortemperature [-50 -> 50]*	- Set the color temperature
- *-whitebalancecolortemperature [cool|medium|warm1|warm2|warm3]* -set white balance color temperature 
- *-eyecomfortmode [off|on]* 	- set eye comfort mode
- *-dynamiccolor [off|on|low|medium|high]* 	-set dynamic Color
- *-peakbrightness [off|low|medium|high]*	-set peak brightness
- *-smoothgradation [off|low|medium|high]*	- set smooth gradation
- *-energysaving [auto|off|min|med|max|screen_off]* - set energy saving mode
- *-hdrdynamictonemapping [on|off|HGIG]*		- set HDR dynamic tone mapping
- *-blacklevel [low|medium|high|limited|full]*	-set black level
- *-dolbyprecisiondetail [off|on]*	- set dolby precision detail
- *-ai_brightness [off|on]*	- set AI brightness
- *-ai_genre [off|on] * 	-set AI genre
- *-ai_picture [off|on]*	- set AI Picture Pro
- *-arcperapp [value]*	- set aspect ratio
>[value]: _21x9, _16x9, _4x3, _14x9, _32x9, _32x12, just_scan, original, full_wide, limited, zoom, zoom2, cinema_zoom, vertZoom, allDirZoom, twinZoom
- *-justscan [off|on|auto]*	- set Just Scan
- *-alldirzoomhratio [0 -> 10]*	- set all-direction zoom horizontal ratio
- *-alldirzoomvratio" [0 -> 9]* 	- set all-direction zoom vertical ratio
- *-alldirzoomhposition [-10 -> 9]* 	- set all-direction zoom horizontal position 
- *-alldirzoomvposition [-9 -> 9]* 	-set all-direction zoom vertical position
- *-vertzoomvposition [-8 -> 9]* 	- set vertical zoom position
- *-vertzoomvratio [0 -> 9]* 	- set vertical zoom ratio
- *-trumotionmode [off|smooth|clear|clearPlus|cinemaClear|natural|user]* 	-set TruMotion
- *-trumotionjudder [0 -> 10]* 	- set TruMotion judder
- *-trumotionblur [0 ->10]* 	- set TruMotion blur
- *-motionprooled [off|low|medium|high]* 	- set OLED Motion Pro
- *-motionpro [off|on]* 	- set Motion Pro 
- *-realcinema [off|on]* 	- set Real Cinema
- *-lowleveladjustment [-30 -> 30]* 	- Fine Tune Dark Areas
- *-blackstabilizer [-30 -> 30]* 	- set Black Stabilizer
- *-whitestabilizer [-30 -> 30]* 	- set White Stabilizer
- *-bluelight [off|level1|level2]* 	- set Reduce Blue Light
- *-gameoptimization [off|on]* 	- set VRR and G-Sync
- *-inputoptimization [auto|on|standard|boost]* 	- Prevent Input Delay
- *-freesync [off|on]* 	- set AMD Freesync Premium 
- *-adjustingluminance [-50 -> 50]* 	- set luminance adjustment 
- *-whitebalanceblue [-50 -> 50]* 	- set white balance blue 
- *-whitebalancegreen [-50 -> 50]* 	- set white balance green 
- *-whitebalancered [-50 -> 50]* 	- set white balance red 

#### Examples: 
*Set the picture mode to "Normal" for device1 and device2*
```
"LGTV Companion.exe" -picturemode normal device1 device2
```
*Set contrast to 80 for device1*
```
"LGTV Companion.exe" -contrast 80 device1
```
## Network and misc other settings (LGTV Companion and LGTVcli)
- *-wol* [true|false] 	- enable or disable the Wake-On-Lan setting, a k a "On with Mobile" or "Turn on via Wi-Fi"
- *-freesyncinfo* 	- show the freesync information panel (the 7 x tap on green button), i e current FPS in freesync/gsync mode
#### Examples: 
*Set the wake-on-lan setting to ON for all devices*
```
"LGTV Companion.exe" -wol true
```
*Show the freesync info on device1*
```
"LGTV Companion.exe" -freesyncinfo device1
```

# Commands for sending generic requests to the device, for advanced users only (LGTV Companion and LGTVcli)
In addition to the above commands it is also possible to send various arbitrary requests to WebOS devices. 
- *-request [endpoint]* 	- Send a json request to an endpoint (with no params)
		[endpoint] below are examples of publically published endpoints:

		audio/volumeUp
		audio/volumeDown
		com.webos.service.ime/sendEnterKey
		com.webos.service.ime/deleteCharacters
		com.webos.service.tv.display/set3DOn
		com.webos.service.tv.display/set3DOff
		media.controls/play
		media.controls/stop
		media.controls/pause
		media.controls/rewind
		media.controls/fastForward
		media.viewer/close
		system/turnOff
		system/turnOn
		system.notifications/closeToast
		tv/channelDown
		tv/channelUp
		webapp/closeWebApp
		com.webos.service.tvpower/power/turnOffScreen
		com.webos.service.tvpower/power/turnOnScreen

- *-request_with_param [endpoint] [param]* 	- Send a request to an endpoint with a JSON payload (param)
		[endpoint] [param] below are examples of publically published endpoints and the format of the required JSON parameters:

		audio/setMute {"mute":true|false}
		audio/setVolume {"volume":volume}
		com.webos.applicationManager/launch {"id": id, "params": params} 
		com.webos.service.ime/insertText {"text":"text...","replace":true|false}
		system.notifications/createToast {"message":"text...","iconData":"base64_encoded_string","iconExtension":"icon_extension"}	
		system.notifications/createAlert (not final implementation)
		system.notifications/closeAlert (not final implementation)
		system.launcher/close {"id":"application_id"}
		system.launcher/launch {"id":"application_id"}|{"id":"application_id","params": params}|{"id":"application_id","contentId": contentId} (-start_app and -start_app_with_param wraps this)
		system.launcher/open {"target":"url"}
		tv/openChannel {"channelId": channel}
		tv/switchInput {"inputId": input}
		tv/executeOneShot {"method":"method_value","format":"format_value","width":width,"height":height,"path": "path"}
            method_value: DISPLAY (SCREEN), SCREEN_WITH_SOURCE_VIDEO, VIDEO, GRAPHIC, SOURCE (SCALER)
            format_value: BMP, JPG, PNG, RGB, RGBA, YUV422
		com.webos.service.apiadapter/audio/changeSoundOutput {"output":"output"} (also see -soundoutput)

The following endpoints are used to query information from WebOS devices, using -request (LGTVcli only).

		config/getConfigs 
		com.webos.service.attachedstoragemanager/listDevices 
		com.webos.service.tvpower/power/getPowerState 
		com.webos.service.networkinput/getPointerInputSocket 
		com.webos.service.apiadapter/audio/getSoundOutput 
		tv/getChannelList 
		tv/getChannelProgramInfo 
		tv/getCurrentChannel 
		tv/getExternalInputList 
		settings/getSystemSettings 
		system.launcher/getAppState 
		system/getSystemInfo 
		com.webos.service.update/getCurrentSWInformation 
		com.webos.applicationManager/listLaunchPoints 
		com.webos.applicationManager/listApps 
		com.webos.service.appstatus/getAppStatus 
		com.webos.applicationManager/getForegroundAppInfo 
		audio/getStatus 
		audio/getVolume 
		api/getServiceList 

- *-start_app [id]* 	- Launch application (with no params). Application IDs can be found by querying the device using com.webos.applicationManager/listApps
- *-start_app_with_param [id] [param]* 	-Launch application with JSON payload (params)
- *-close_app [id]* 	- close Application
#### Examples: 
*Increase volume one step for device1*
```
"LGTV Companion.exe" -request audio/volumeUp device1
```
*Open the built in web brower and go to the github page for this app on device 1*
```
"LGTV Companion.exe" -request_with_params system.launcher/open {"target":"https://github.com/JPersson77/LGTVCompanion"} device1
```
*launch the screensaver on device1*
```
"LGTV Companion.exe" -start_app com.webos.app.screensaver device1
```
*close the screensaver on device1*
```
"LGTV Companion.exe" -close_app com.webos.app.screensaver device1
```
## picture system settings (LGTV Companion and LGTVcli)
- *-settings_picture [payload]* 	- Send a generic JSON payload containing settings for the "picture" system settings category (see examples of applicable settings below)

		{"adjustingLuminance":[0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]}
		{"adjustingLuminance10pt":[0,0,0,0,0,0,0,0,0,0]}
		{"ambientLightCompensation":"off",}
		{"applyToAllInput":"done"}
		{"backlight":"80"}
		{"blackLevel":{"ntsc":"auto","ntsc443":"auto","pal":"auto","pal60":"auto","palm":"auto","paln":"auto","secam":"auto","unknown":"auto"}}
		{"brightness":"50"}
		{"color":"50"}
		{"colorFilter":"off"}
		{"colorGamut":"auto"}
		{"colorManagementColorSystem":"red"}
		{"colorManagementHueBlue":"0"}
		{"colorManagementHueCyan":"0"}
		{"colorManagementHueGreen":"0"}
		{"colorManagementHueMagenta":"0"}
		{"colorManagementHueRed":"0"}
		{"colorManagementHueYellow":"0"}
		{"colorManagementLuminanceBlue":"0"}
		{"colorManagementLuminanceCyan":"0"}
		{"colorManagementLuminanceGreen":"0"}
		{"colorManagementLuminanceMagenta":"0"}
		{"colorManagementLuminanceRed":"0"}
		{"colorManagementLuminanceYellow":"0"}
		{"colorManagementSaturationBlue":"0"}
		{"colorManagementSaturationCyan":"0"}
		{"colorManagementSaturationGreen":"0"}
		{"colorManagementSaturationMagenta":"0"}
		{"colorManagementSaturationRed":"0"}
		{"colorManagementSaturationYellow":"0"}
		{"colorTemperature":"-50"}
		{"contrast":"80"}
		{"dolbyPrecisionDetail":"off"}
		{"dynamicColor":"off"}
		{"dynamicContrast":"off"}
		{"edgeEnhancer":"on"}
		{"expertPattern":"off"}
		{"externalPqlDbType":"none"}
		{"gamma":"high2"}
		{"grassColor":"0"}
		{"hPosition":"0"}
		{"hSharpness":"10"}
		{"hSize":"0"}
		{"hdrDynamicToneMapping":"on"}
		{"localDimming":"low"}
		{"motionEyeCare":"off"}
		{"motionPro":"off"}
		{"motionProOLED":"off"}
		{"mpegNoiseReduction":"off"}
		{"noiseReduction":"off"}
		{"peakBrightness":"off"}
		{"pictureTempKey":"off"}
		{"realCinema":"on"}
		{"sharpness":"10"}
		{"skinColor":"0"}
		{"skyColor":"0"}
		{"smoothGradation":"off"}
		{"superResolution":"off"}
		{"tint":"0"}
		{"truMotionBlur":"10"}
		{"truMotionJudder":"0"}
		{"truMotionMode":"user"}
		{"vPosition":"0"}
		{"vSharpness":"10"}
		{"vSize":"0"}
		{"whiteBalanceApplyAllInputs":"off"}
		{"whiteBalanceBlue":[0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]}
		{"whiteBalanceBlue10pt":[0,0,0,0,0,0,0,0,0,0]}
		{"whiteBalanceBlueGain":"0"}
		{"whiteBalanceBlueOffset":"0"}
		{"whiteBalanceCodeValue":"21"}
		{"whiteBalanceCodeValue10pt":"9"}
		{"whiteBalanceColorTemperature":"warm2"}
		{"whiteBalanceGreen":[0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]}
		{"whiteBalanceGreen10pt":[0,0,0,0,0,0,0,0,0,0]}
		{"whiteBalanceGreenGain":"0"}
		{"whiteBalanceGreenOffset":"0"}
		{"whiteBalanceIre":"100"}
		{"whiteBalanceIre10pt":"100"}
		{"whiteBalanceLuminance":"130"}
		{"whiteBalanceMethod":"2"}
		{"whiteBalancePattern":"outer"}
		{"whiteBalancePoint":"high"}
		{"whiteBalanceRed":[0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]}
		{"whiteBalanceRed10pt":[0,0,0,0,0,0,0,0,0,0]}
		{"whiteBalanceRedGain":"0"}
		{"whiteBalanceRedOffset":"0"}
		{"xvycc":"auto"}
#### Examples: 
*turn on Real Cinema for device1 
```
"LGTV Companion.exe" -settings_picture {"realCinema":"on"} device1
```
*set contrast to 50, brightness to 50 and contrast to 85 for device 2*
```
"LGTV Companion.exe" -settings_picture {"color":"50","brightness":"50","contrast":"85"} device2
```
## Other system settings (LGTV Companion and LGTVcli)

- *-settings_other [payload]* 	- Send a generic JSON payload containing settings for the "other" system settings category (see examples of applicable settings below)  

		{"activeArtisticDisplayScreenSaver":false}
		{"amazonHotkeyIsActive":true}
		{"appReturn":""}
		{"battery25PercentMode":"off"}
		{"batteryInstopProtect":"on"}
		{"blackStabilizer":13}
		{"blueLight":"off"}
		{"care365":{"accountName":"","accountNumber":"","userAgreementLocation":"","userAgreementVersion":"","value":"off"}
		{"colorimetry":"auto"}
		{"colorimetryHDMI1":"auto"}
		{"colorimetryHDMI2":"auto"}
		{"colorimetryHDMI3":"auto"}
		{"colorimetryHDMI4":"auto"}
		{"cursorAutoRemover":"on"}
		{"darkMode":"off"}
		{"dolbyVSVDBVer":"v2"}
		{"dolbyVSVDBVerHDMI1":"v2"}
		{"dolbyVSVDBVerHDMI2":"v2"}
		{"dolbyVSVDBVerHDMI3":"v2"}
		{"dolbyVSVDBVerHDMI4":"v2"}
		{"enableQuickGame":"on"}
		{"eotf":"auto"}
		{"eotfHDMI1":"auto"}
		{"eotfHDMI2":"auto"}
		{"eotfHDMI3":"auto"}
		{"eotfHDMI4":"auto"}
		{"epgRowCount":"1"}
		{"fitLogUsbDump":"off"}
		{"flickerPatternCtrl":false}
		{"freesync":"off"}
		{"freesyncLCDHDMI1":"off"}
		{"freesyncLCDHDMI2":"off"}
		{"freesyncLCDHDMI3":"off"}
		{"freesyncLCDHDMI4":"off"}
		{"freesyncOLEDHDMI1":"off"}
		{"freesyncOLEDHDMI2":"off"}
		{"freesyncOLEDHDMI3":"off"}
		{"freesyncOLEDHDMI4":"off"}
		{"freesyncSupport":"off"}
		{"freeviewTnCPopup":"off"}
		{"gameAdjustContrast":95}
		{"gameBlackLevel":50}
		{"gameColorDepth":55}
		{"gameDashboardStatusList":["fps","vrr_aiGameSound_whiteStabilizer","blackStabilizer","lowLatency"]}
		{"gameGenre":"Standard"}
		{"gameMode":{"hdmi1":"off","hdmi2":"off","hdmi3":"off","hdmi4":"off"}}
		{"gameOptimization":"on"}
		{"gameOptimizationHDMI1":"on"}
		{"gameOptimizationHDMI2":"on"}
		{"gameOptimizationHDMI3":"on"}
		{"gameOptimizationHDMI4":"on"}
		{"gameScreenPosition":"middle"}
		{"gameScreenRatio":"16:9"}
		{"gameScreenSize":"full"}
		{"gameSettingModified":{"FPS":false,"RPG":false,"RTS":false,"Sports":false,"Standard":false,"USER":false}}
		{"gameSharpness":10}
		{"gameUIColor":"violet"}
		{"gameWallpaper":{"folderUpdateVersion":0,"imgSrc":""}}
		{"hdmiPcMode":{"hdmi1":false,"hdmi2":false,"hdmi3":false,"hdmi4":false}}
		{"homeAppLaunched":"off"}
		{"homeEffectVersion":[{"id":"Christmas","version":1.0},{"id":"Halloween","version":1.0}]}
		{"illuminanceThreshold":0}
		{"inputOptimization":"auto"}
		{"isFirstCapture":"true"}
		{"isfUpdated":"false"}
		{"lgLogoDisplay":"on"}
		{"lightingBrightness":8}
		{"lightingEnable":"off"}
		{"lightingMode":"dynamic"}
		{"lowLevelAdjustment":0}
		{"lowPowerMode":"off"}
		{"masterLuminanceLevel":"540nit"}
		{"masteringColor":"auto"}
		{"masteringColorHDMI1":"auto"}
		{"masteringColorHDMI2":"auto"}
		{"masteringColorHDMI3":"auto"}
		{"masteringColorHDMI4":"auto"}
		{"masteringPeak":"auto"}
		{"masteringPeakHDMI1":"auto"}
		{"masteringPeakHDMI2":"auto"}
		{"masteringPeakHDMI3":"auto"}
		{"masteringPeakHDMI4":"auto"}
		{"maxCLL":"auto"}
		{"maxCLLHDMI1":"auto"}
		{"maxCLLHDMI2":"auto"}
		{"maxCLLHDMI3":"auto"}
		{"maxCLLHDMI4":"auto"}
		{"maxFALL":"auto"}
		{"maxFALLHDMI1":"auto"}
		{"maxFALLHDMI2":"auto"}
		{"maxFALLHDMI3":"auto"}
		{"maxFALLHDMI4":"auto"}
		{"netflixHotkeyIsActive":true}
		{"newKey":"on"}
		{"oledCareMode":"off"}
		{"oledCareRecommendation":"off"}
		{"playbackThreshold":200}
		{"pseudoTouchMode":"on"}
		{"quickSettingsMenuList":	{["QuickSettings_picture_button","QuickSettings_soundMode_button","QuickSettings_soundOut_button","QuickSettings_game_button","QuickSettings_multiview_button","QuickSettings_ocp_button","QuickSettings_network_button","QuickSettings_menu_button"]}}
		{"screenRemoteAutoShow":"true"}
		{"screenRemoteExpanded":"false"}
		{"screenRemotePosition":"right"}
		{"simplinkAutoPowerOn":"on"}
		{"simplinkEnable":"off"}
		{"soundSyncModeColor":"auto"}
		{"soundSyncModeDisplayMode":"bar"}
		{"soundSyncModeFrequency":"mid"}
		{"soundSyncModeStaticColor":35}
		{"staticModeColor1":35}
		{"staticModeColor2":1}
		{"staticModeColor3":12}
		{"staticModeColor4":0}
		{"supportAirplay":false}
		{"supportBnoModel":false}
		{"touchRemoteLaunchMode":"edgeSwipe"}
		{"ueiEnable":"off"}
		{"uhdDeepColor":"off"}
		{"uhdDeepColor8kHDMI1":"off"}
		{"uhdDeepColor8kHDMI2":"off"}
		{"uhdDeepColor8kHDMI3":"off"}
		{"uhdDeepColor8kHDMI4":"off"}
		{"uhdDeepColorAutoStatusHDMI1":"none"}
		{"uhdDeepColorAutoStatusHDMI2":"none"}
		{"uhdDeepColorAutoStatusHDMI3":"none"}
		{"uhdDeepColorAutoStatusHDMI4":"none"}
		{"uhdDeepColorHDMI1":"off"}
		{"uhdDeepColorHDMI2":"off"}
		{"uhdDeepColorHDMI3":"off"}
		{"uhdDeepColorHDMI4":"off"}
		{"weatherAllowed":false}
		{"whiteStabilizer":13}

#### Examples: 
*Set black stabilizer to 13 for device1*
```
"LGTV Companion.exe" -settings_other {"blackStabilizer":13} device1
```

## Option system settings (LGTV Companion and LGTVcli)
- *-settings_options [payload]* 	- Send a generic JSON payload containing settings for the "options" system settings category (see examples of applicable settings below)  

		{"IPControlSecureKey":""}
		{"_3dModeEstreamer":"off"}
		{"additionalAudioSelection":"none"}
		{"addressInfo":["not_defined","not_defined","not_defined","not_defined"]}
		{"adjustments":{"blackAndWhite":"off","colorInversion":"off"}}
		{"animationGuide":"on"}
		{"appInstallDevice":{"deviceId":"","driveId":""}}
		{"appUpdateMode":"manual"}
		{"artisticDisplayTimer":"off"}
		{"audioGuidance":"off"}
		{"audioGuidancePitch":"medium"}
		{"audioGuidanceSpeed":"medium"}
		{"audioGuidanceVolume":"medium"}
		{"autoComplete":false}
		{"autoSmartServiceCountry":"on"}
		{"avatar":"off"}
		{"backupPsm":{"backupPsm2d":"hdrStandard","backupPsm3d":"hdrStandard"}}
		{"backupPsmDolby":{"backupPsm2d":"dolbyHdrDark","backupPsm3d":"dolbyHdrDark"}}
		{"baloonHelp":"on"}
		{"bannerPosition":"none"}
		{"broadcastInfoNoti":"on"}
		{"cameraResourcePermission":[]}
		{"channelplus":"off"}
		{"channelplusPopup":"off"}
		{"cicNumber":[{"country":"default","number":"none","shortName":"default"}]}
		{"country":"other"}
		{"countryGroup":"UNDEFINED"}
		{"countryRegion":"other"}
		{"curDemoFile":"undefined"}
		{"curvature":{"curvatureList":[{"disable":false,"selected":true,"type":"flat","user":false,"value":"0%"}{"disable":false,"selected":false,"type":"curvature1","user":false,"value":"50%"}{"disable":false,"selected":false,"type":"curvature2","user":false,"value":"100%"}{"disable":true,"selected":false,"type":"curvature3","user":false,"value":"100%"}],"valueList":["0%","5%","10%","15%","20%","25%","30%","35%","40%","45%","50%","55%","60%","65%","70%","75%","80%","85%","90%","95%","100%"]}}
		{"dataService":"mheg"}
		{"dbgLogUpload":false}
		{"demoFileList":"undefined"}
		{"demoMode":"on"}
		{"displayMusicWidget":true}
		{"eStreamerPosition":"all"}
		{"emergencyAlert":"on"}
		{"emergencyInformationAtsc30":"on"}
		{"emergencyInformationLanguageAtsc30":"eng"}
		{"enableIpControl":"off"}
		{"enableSDDP":"off"}
		{"enableToastPopup":"off"}
		{"enabling3dSettingsMenu":"off"}
		{"epgPipMode":"off"}
		{"estreamerMinimalMode":"off"}
		{"estreamerStatus":"off"}
		{"faultLogUpload":false}
		{"firstTvSignalStatus":"undefined"}
		{"focusedItemEnlarged":"off"}
		{"freeviewMode":"off"}
		{"freeviewplay":"off"}
		{"googleAssistantTTS":"on"}
		{"graphicSharpnessLevel":0}
		{"hbbTV":"off"}
		{"hbbTvDeviceId":"on"}
		{"hbbTvDnt":"off"}
		{"hddEcoMode":"on"}
		{"helpOnSettings":"on"}
		{"highContrast":"off"}
		{"hybridCast":"off"}
		{"inputDevicesSupportStatus":{"keyboard":true,"motionSensor":true,"pointer":true,"touch":true,"voice":true}}
		{"interactive-service":"off"}
		{"interactive-service-hdmi":"off"}
		{"interactive-service-id":""}
		{"interactivity":"off"}
		{"irBlaster":"off"}
		{"ismMethod":"normal"}
		{"japanCitySelection":"Tokyo"}
		{"lifeOnScreenEnergySaving":"auto"}
		{"lifeOnScreenNotification":true}
		{"lifeOnScreenOnTimer":[]}
		{"lifeOnScreenUsingMotionSensor":false}
		{"lineView":"on"}
		{"liveMenuLaunched":false}
		{"livePlus":"off"}
		{"localeCountryGroup":"UNDEFINED"}
		{"logoLight":"low"}
		{"magicNum1":{"id":"","override":false,"params":{}}
		{"magicNum2":{"id":"","override":false,"params":{}}
		{"magicNum3":{"id":"","override":false,"params":{}}
		{"magicNum4":{"id":"","override":false,"params":{}}
		{"magicNum5":{"id":"","override":false,"params":{}}
		{"magicNum6":{"id":"","override":false,"params":{}}
		{"magicNum7":{"id":"","override":false,"params":{}}
		{"magicNum8":{"id":"","override":false,"params":{}}
		{"magicNum8":{"id":"com.webos.app.self-diagnosis","override":true,"params":{"from":"magicNum"}}
		{"magicNumFvp":false}
		{"magicNumHelpShow":true}
		{"menuLanguage":"eng"}
		{"menuTransparency":"on"}
		{"mhegGuide":"off"}
		{"miracastOverlayAdRecovery":"off"}
		{"miracastOverlayStatus":"off"}
		{"modeSelectFlag":"off"}
		{"motionRecognition":"off"}
		{"motionSensorSensitivity":"medium"}
		{"motionSensorSensitivityForAOD":"medium"}
		{"multiChannelAudio":"on"}
		{"multiViewStatus":"off"}
		{"ohtv":"on"}
		{"orbit":"off"}
		{"password_ipcontrol":"828"}
		{"phlCitySelection":"0"}
		{"pointerAlignment":"off"}
		{"pointerShape":"auto"}
		{"pointerSize":"medium"}
		{"pointerSpeed":"normal"}
		{"powerOnLight":"off"}
		{"promotionOriginEnd":"undefined"}
		{"promotionOriginStart":"undefined"}
		{"promotionPeriodEnd":"0"}
		{"promotionPeriodStart":"0"}
		{"promotionStreamer":"off"}
		{"pstreamerUser":"off"}
		{"quickStartMode":"off"}
		{"restoreCurve":"on"}
		{"screenOff":"off"}
		{"screenOffTime":"5"}
		{"screenRotation":"off"}
		{"searchAppTTS":"off"}
		{"serviceCountryForMagicNum":""}
		{"setId":1}
		{"smartServiceCountryCode2":"other"}
		{"smartServiceCountryCode3":"other"}
		{"smartSoundDemo":"on"}
		{"speakToTv":"off"}
		{"standByLight":"on"}
		{"storeHDR":"on"}
		{"storeLogo":"0"}
		{"storeMode":"home"}
		{"storeMode2":"on"}
		{"storeModeVideo":"off"}
		{"storeUsbAlarm":"off"}
		{"subdivisionCodeOfServiceCountry":""}
		{"subtitleLanguageFirst":"eng"}
		{"subtitleLanguageSecond":"eng"}
		{"supplementaryAudio":"off"}
		{"syncMode":"off"}
		{"syncModeTvCondition":"none"}
		{"teletextLanguageFirst":"eng"}
		{"teletextLanguageSecond":"eng"}
		{"turnOnByVoice":"off"}
		{"usbBuiltInVideo":"on"}
		{"virtualKeyboardLanguage":["en-US"]}
		{"virtualSetTop":"off"}
		{"voiceRecognitionLanguage":"eng"}
		{"vsn":"N/A"}
		{"wakeUpword":"LGTV"}
		{"wallPaperSettings":{"artisticDisplayTheme":"default","artisticDisplayThemeVersion":0,"homeImageVersion":0,"imageLimit":0,"isFullView":false}}
		{"watchedListCollection":"on"}
		{"webOSPromotionVideo":"on"}
		{"yourMomentsVersion":"0"}
		{"zipcode":"not_defined"}

#### Examples: 
*Enable quickstart+ for device1*
```
"LGTV Companion.exe" -settings_options {"quickStartMode":"on"} device1
```
