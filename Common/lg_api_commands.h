R"(
{
  "wol": {
    "Category": "network",
    "Setting": "wolwowlOnOff",
    "Argument": "true false",
    "LogMessage": "wake-on-lan enabled [#ARG#]"
  },
  "picturemode": {
    "Category": "picture",
    "Setting": "pictureMode",
    "Argument": "cinema eco expert1 expert2 game normal photo sports technicolor vivid hdrEffect filmMaker hdrCinema hdrCinemaBright hdrExternal hdrGame hdrStandard hdrTechnicolor hdrVivid hdrFilmMaker dolbyHdrCinema dolbyHdrCinemaBright dolbyHdrDarkAmazon dolbyHdrGame dolbyHdrStandard dolbyHdrVivid dolbyStandard",
    "LogMessage": "set picture mode [#ARG#]"
  },
  "backlight": {
    "Category": "picture",
    "Setting": "backlight",
    "Max": 100,
    "Min": 0,
    "LogMessage": "set backlight [#ARG#]"
  },
  "contrast": {
    "Category": "picture",
    "Setting": "contrast",
    "Max": 100,
    "Min": 0,
    "LogMessage": "set contrast [#ARG#]"
  },
  "brightness": {
    "Category": "picture",
    "Setting": "brightness",
    "Max": 100,
    "Min": 0,
    "LogMessage": "set brightness [#ARG#]"
  },
  "color": {
    "Category": "picture",
    "Setting": "color",
    "Max": 100,
    "Min": 0,
    "LogMessage": "set color [#ARG#]"
  },
  "sharpness": {
    "Category": "picture",
    "Setting": "sharpness",
    "Max": 50,
    "Min": 0,
    "LogMessage": "set sharpness [#ARG#]"
  },
  "colorgamut": {
    "Category": "picture",
    "Setting": "colorGamut",
    "Argument": "auto extended wide srgb native",
    "LogMessage": "set color gamut [#ARG#]"
  },
  "dynamiccontrast": {
    "Category": "picture",
    "Setting": "dynamicContrast",
    "Argument": "off low medium high",
    "LogMessage": "set dynamic contrast [#ARG#]"
  },
  "gamma": {
    "Category": "picture",
    "Setting": "gamma",
    "Argument": "low medium high1 high2",
    "LogMessage": "set gamma [#ARG#]"
  },
  "colortemperature": {
    "Category": "picture",
    "Setting": "colorTemperature",
    "Max": 50,
    "Min": -50,
    "LogMessage": "set color temperature [#ARG#]"
  },
  "whitebalancecolortemperature": {
    "Category": "picture",
    "Setting": "whiteBalanceColorTemperature",
    "Argument": "cool medium warm1 warm2 warm3",
    "LogMessage": "set white balance color temperature [#ARG#]"
  },
  "eyecomfortmode": {
    "Category": "picture",
    "Setting": "eyeComfortMode",
    "Argument": "off on",
    "LogMessage": "set eye comfort mode [#ARG#]"
  },
  "dynamiccolor": {
    "Category": "picture",
    "Setting": "dynamicColor",
    "Argument": "off on low medium high",
    "LogMessage": "set dynamic color [#ARG#]"
  },
  "peakbrightness": {
    "Category": "picture",
    "Setting": "peakBrightness",
    "Argument": "off low medium high",
    "LogMessage": "set peak brightness [#ARG#]"
  },
  "smoothgradation": {
    "Category": "picture",
    "Setting": "smoothGradation",
    "Argument": "off low medium high",
    "LogMessage": "set smooth gradation [#ARG#]"
  },
  "energysaving": {
    "Category": "picture",
    "Setting": "energySaving",
    "Argument": "auto off min med max screen_off",
    "LogMessage": "set energy saving [#ARG#]"
  },
  "hdrdynamictonemapping": {
    "Category": "picture",
    "Setting": "hdrDynamicToneMapping",
    "Argument": "on off HGIG",
    "LogMessage": "set HDR dynamic tone mapping [#ARG#]"
  },
  "blacklevel": {
    "Category": "picture",
    "Setting": "blackLevel",
    "Argument": "low medium high limited full",
    "LogMessage": "set black level [#ARG#]"
  },
  "dolbyprecisiondetail": {
    "Category": "picture",
    "Setting": "dolbyPrecisionDetail",
    "Argument": "off on",
    "LogMessage": "set black level [#ARG#]"
  },
  "ai_brightness": {
    "Category": "aiPicture",
    "Setting": "ai_Brightness",
    "Argument": "off on",
    "LogMessage": "set AI brightness [#ARG#]"
  },
  "ai_genre": {
    "Category": "aiPicture",
    "Setting": "ai_Genre",
    "Argument": "off on",
    "LogMessage": "set AI genre [#ARG#]"
  },
  "ai_picture": {
    "Category": "aiPicture",
    "Setting": "ai_Picture",
    "Argument": "off on",
    "LogMessage": "set AI Picture Pro [#ARG#]"
  },
  "arcperapp": {
    "Category": "aspectRatio",
    "Setting": "arcPerApp",
    "Argument": "_21x9 _16x9 _4x3 _14x9 _32x9 _32x12 just_scan original full_wide limited zoom zoom2 cinema_zoom vertZoom allDirZoom twinZoom",
    "LogMessage": "set aspect ratio [#ARG#]"
  },
  "justscan": {
    "Category": "aspectRatio",
    "Setting": "justScan",
    "Argument": "off on auto",
    "LogMessage": "set Just Scan [#ARG#]"
  },
  "alldirzoomhratio": {
    "Category": "aspectRatio",
    "Setting": "allDirZoomHRatio",
    "Max": 10,
    "Min": 0,
    "LogMessage": "set all-direction zoom horizontal ratio [#ARG#]"
  },
  "alldirzoomvratio": {
    "Category": "aspectRatio",
    "Setting": "allDirZoomVRatio",
    "Max": 9,
    "Min": 0,
    "LogMessage": "set all-direction zoom vertical ratio [#ARG#]"
  },
  "alldirzoomhposition": {
    "Category": "aspectRatio",
    "Setting": "allDirZoomHPosition",
    "Max": 9,
    "Min": -10,
    "LogMessage": "set all-direction zoom horizontal position [#ARG#]"
  },
  "alldirzoomvposition": {
    "Category": "aspectRatio",
    "Setting": "allDirZoomVPosition",
    "Max": 9,
    "Min": -9,
    "LogMessage": "set all-direction zoom vertical position [#ARG#]"
  },
  "vertzoomvposition": {
    "Category": "aspectRatio",
    "Setting": "vertZoomVPosition",
    "Max": 9,
    "Min": -8,
    "LogMessage": "set vertical zoom position [#ARG#]"
  },
  "vertzoomvratio": {
    "Category": "aspectRatio",
    "Setting": "vertZoomVRatio",
    "Max": 9,
    "Min": 0,
    "LogMessage": "set vertical zoom ratio [#ARG#]"
  },
  "trumotionmode": {
    "Category": "picture",
    "Setting": "truMotionMode",
    "Argument": "off smooth clear clearPlus cinemaClear natural user",
    "LogMessage": "set TruMotion [#ARG#]"
  },
  "trumotionjudder": {
    "Category": "picture",
    "Setting": "truMotionJudder",
    "Max": 10,
    "Min": 0,
    "LogMessage": "set TruMotion judder [#ARG#]"
  },
  "trumotionblur": {
    "Category": "picture",
    "Setting": "truMotionBlur",
    "Max": 10,
    "Min": 0,
    "LogMessage": "set TruMotion blur [#ARG#]"
  },
  "motionprooled": {
    "Category": "picture",
    "Setting": "motionProOLED",
    "Argument": "off low medium high",
    "LogMessage": "set OLED Motion Pro [#ARG#]"
  },
  "motionpro": {
    "Category": "picture",
    "Setting": "motionPro",
    "Argument": "off on",
    "LogMessage": "set Motion Pro [#ARG#]"
  },
  "realcinema": {
    "Category": "picture",
    "Setting": "realCinema",
    "Argument": "off on",
    "LogMessage": "set Real Cinema [#ARG#]"
  },
  "uhddeepcolor_hdmi1": {
    "Category": "other",
    "Setting": "uhdDeepColorHDMI1",
    "Argument": "off on",
    "LogMessage": "set UHD Deep Color for HDMI-input 1 [#ARG#]"
  },
  "uhddeepcolor_hdmi2": {
    "Category": "other",
    "Setting": "uhdDeepColorHDMI2",
    "Argument": "off on",
    "LogMessage": "set UHD Deep Color for HDMI-input 2 Cinema [#ARG#]"
  },
  "uhddeepcolor_hdmi3": {
    "Category": "other",
    "Setting": "uhdDeepColorHDMI3",
    "Argument": "off on",
    "LogMessage": "set UHD Deep Color for HDMI-input 3 [#ARG#]"
  },
  "uhddeepcolor_hdmi4": {
    "Category": "other",
    "Setting": "uhdDeepColorHDMI4",
    "Argument": "off on",
    "LogMessage": "set UHD Deep Color for HDMI-input 4 [#ARG#]"
  },
  "lowleveladjustment": {
    "Category": "other",
    "Setting": "lowLevelAdjustment",
    "Max": 30,
    "Min": -30,
    "LogMessage": "set Fine Tune Dark Areas [#ARG#]"
  },
  "blackstabilizer": {
    "Category": "other",
    "Setting": "blackStabilizer",
    "Max": 30,
    "Min": -30,
    "LogMessage": "set Black Stabilizer [#ARG#]"
  },
  "whitestabilizer": {
    "Category": "other",
    "Setting": "whiteStabilizer",
    "Max": 30,
    "Min": -30,
    "LogMessage": "set White Stabilizer [#ARG#]"
  },
  "bluelight": {
    "Category": "other",
    "Setting": "blueLight",
    "Argument": "off level1 level2",
    "LogMessage": "set Reduce Blue Light [#ARG#]"
  },
  "gamemode_hdmi1": {
    "Category": "other",
    "Setting": "gameMode_hdmi1",
    "Argument": "off on",
    "LogMessage": "set Game Optimizer for HDMI-input 1 [#ARG#]"
  },
  "gamemode_hdmi2": {
    "Category": "other",
    "Setting": "gameMode_hdmi2",
    "Argument": "off on",
    "LogMessage": "set Game Optimizer for HDMI-input 2 [#ARG#]"
  },
  "gamemode_hdmi3": {
    "Category": "other",
    "Setting": "gameMode_hdmi3",
    "Argument": "off on",
    "LogMessage": "set Game Optimizer for HDMI-input 3 [#ARG#]"
  },
  "gamemode_hdmi4": {
    "Category": "other",
    "Setting": "gameMode_hdmi4",
    "Argument": "off on",
    "LogMessage": "set Game Optimizer for HDMI-input 4 [#ARG#]"
  },
  "gameoptimization": {
    "Category": "other",
    "Setting": "gameOptimization",
    "Argument": "off on",
    "LogMessage": "set VRR and G-Sync [#ARG#]"
  },
  "inputoptimization": {
    "Category": "other",
    "Setting": "inputOptimization",
    "Argument": "auto on standard boost",
    "LogMessage": "set Prevent Input Delay [#ARG#]"
  },
  "gameoptimization_hdmi1": {
    "Category": "other",
    "Setting": "gameOptimizationHDMI1",
    "Argument": "off on",
    "LogMessage": "set game optimization for HDMI-input 1 [#ARG#]"
  },
  "gameoptimization_hdmi2": {
    "Category": "other",
    "Setting": "gameOptimizationHDMI2",
    "Argument": "off on",
    "LogMessage": "set game optimization for HDMI-input 2 [#ARG#]"
  },
  "gameoptimization_hdmi3": {
    "Category": "other",
    "Setting": "gameOptimizationHDMI3",
    "Argument": "off on",
    "LogMessage": "set game optimization for HDMI-input 3 [#ARG#]"
  },
  "gameoptimization_hdmi4": {
    "Category": "other",
    "Setting": "gameOptimizationHDMI4",
    "Argument": "off on",
    "LogMessage": "set game optimization for HDMI-input 4 [#ARG#]"
  },
  "freesync": {
    "Category": "other",
    "Setting": "freesync",
    "Argument": "off on",
    "LogMessage": "set AMD Freesync Premium [#ARG#]"
  },
  "freesyncoled_hdmi1": {
    "Category": "other",
    "Setting": "freesyncOLEDHDMI1",
    "Argument": "off on",
    "LogMessage": "set OLED freesync for HDMI-input 1 [#ARG#]"
  },
  "freesyncoled_hdmi2": {
    "Category": "other",
    "Setting": "freesyncOLEDHDMI2",
    "Argument": "off on",
    "LogMessage": "set OLED freesync for HDMI-input 2 [#ARG#]"
  },
  "freesyncoled_hdmi3": {
    "Category": "other",
    "Setting": "freesyncOLEDHDMI3",
    "Argument": "off on",
    "LogMessage": "set OLED freesync for HDMI-input 3 [#ARG#]"
  },
  "freesyncoled_hdmi4": {
    "Category": "other",
    "Setting": "freesyncOLEDHDMI4",
    "Argument": "off on",
    "LogMessage": "set OLED freesync for HDMI-input 4 [#ARG#]"
  },
  "hdmipcmode_hdmi1": {
    "Category": "other",
    "Setting": "hdmiPcMode_hdmi1",
    "Argument": "off on",
    "LogMessage": "set PC-mode for HDMI-input 1 [#ARG#]"
  },
  "hdmipcmode_hdmi2": {
    "Category": "other",
    "Setting": "hdmiPcMode_hdmi2",
    "Argument": "off on",
    "LogMessage": "set PC-mode for HDMI-input 2 [#ARG#]"
  },
  "hdmipcmode_hdmi3": {
    "Category": "other",
    "Setting": "hdmiPcMode_hdmi3",
    "Argument": "off on",
    "LogMessage": "set PC-mode for HDMI-input 3 [#ARG#]"
  },
  "hdmipcmode_hdmi4": {
    "Category": "other",
    "Setting": "hdmiPcMode_hdmi4",
    "Argument": "off on",
    "LogMessage": "set PC-mode for HDMI-input 4 [#ARG#]"
  },
  "adjustingluminance": {
    "Category": "picture",
    "Setting": "adjustingLuminance",
    "Max": 50,
    "Min": -50,
    "LogMessage": "set luminance adjustment [#ARG#]"
  },
  "whitebalanceblue": {
    "Category": "picture",
    "Setting": "whiteBalanceBlue",
    "Max": 50,
    "Min": -50,
    "LogMessage": "set white balance blue [#ARG#]"
  },
  "whitebalancegreen": {
    "Category": "picture",
    "Setting": "whiteBalanceGreen",
    "Max": 50,
    "Min": -50,
    "LogMessage": "set white balance green [#ARG#]"
  },
  "whitebalancered": {
    "Category": "picture",
    "Setting": "whiteBalanceRed",
    "Max": 50,
    "Min": -50,
    "LogMessage": "set white balance red [#ARG#]"
  },
  "soundmode": {
    "Category": "sound",
    "Setting": "soundMode",
    "Argument": "aiSoundPlus aiSound standard news music movie sports game",
    "LogMessage": "set sound mode [#ARG#]"
  },
  "soundoutput": {
    "Category": "sound",
    "Setting": "soundOutput",
    "Argument": "tv_speaker external_arc external_optical bt_soundbar mobile_phone lineout headphone tv_speaker_bluetooth tv_external_speaker tv_speaker_headphone wisa_speaker",
    "LogMessage": "set sound output [#ARG#]"
  },
  "autovolume": {
    "Category": "sound",
    "Setting": "autoVolume",
    "Argument": "off on",
    "LogMessage": "set automatic volume [#ARG#]"
  }
}
)"