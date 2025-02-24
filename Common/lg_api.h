﻿#pragma once
// Handshake
#define LG_HANDSHAKE_NOTPAIRED		L"{\"type\":\"register\",\"id\":\"register_0\",\"payload\":{\"forcePairing\":false,\"pairingType\":\"PROMPT\",\"manifest\":{\"manifestVersion\":1,\"appVersion\":\"1.1\",\"signed\":{\"created\":\"20140509\",\"appId\":\"com.lge.test\",\"vendorId\":\"com.lge\",\"localizedAppNames\":{\"\":\"LG Remote App\",\"ko-KR\":\"리모컨 앱\",\"zxx-XX\":\"ЛГ Rэмotэ AПП\"},\"localizedVendorNames\":{\"\":\"LG Electronics\"},\"permissions\":[\"TEST_SECURE\",\"CONTROL_INPUT_TEXT\",\"CONTROL_MOUSE_AND_KEYBOARD\",\"READ_INSTALLED_APPS\",\"READ_LGE_SDX\",\"READ_NOTIFICATIONS\",\"SEARCH\",\"WRITE_SETTINGS\",\"WRITE_NOTIFICATION_ALERT\",\"CONTROL_POWER\",\"READ_CURRENT_CHANNEL\",\"READ_RUNNING_APPS\",\"READ_UPDATE_INFO\",\"UPDATE_FROM_REMOTE_APP\",\"READ_LGE_TV_INPUT_EVENTS\",\"READ_TV_CURRENT_TIME\"],\"serial\":\"2f930e2d2cfe083771f68e4fe7bb07\"},\"permissions\":[\"LAUNCH\",\"LAUNCH_WEBAPP\",\"APP_TO_APP\",\"CLOSE\",\"TEST_OPEN\",\"TEST_PROTECTED\",\"CONTROL_AUDIO\",\"CONTROL_DISPLAY\",\"CONTROL_INPUT_JOYSTICK\",\"CONTROL_INPUT_MEDIA_RECORDING\",\"CONTROL_INPUT_MEDIA_PLAYBACK\",\"CONTROL_INPUT_TV\",\"CONTROL_POWER\",\"CONTROL_TV_SCREEN\",\"READ_APP_STATUS\",\"READ_CURRENT_CHANNEL\",\"READ_INPUT_DEVICE_LIST\",\"READ_NETWORK_STATE\",\"READ_RUNNING_APPS\",\"READ_TV_CHANNEL_LIST\",\"WRITE_NOTIFICATION_TOAST\",\"READ_POWER_STATE\",\"READ_COUNTRY_INFO\",\"READ_SETTINGS\"],\"signatures\":[{\"signatureVersion\":1,\"signature\":\"eyJhbGdvcml0aG0iOiJSU0EtU0hBMjU2Iiwia2V5SWQiOiJ0ZXN0LXNpZ25pbmctY2VydCIsInNpZ25hdHVyZVZlcnNpb24iOjF9.hrVRgjCwXVvE2OOSpDZ58hR+59aFNwYDyjQgKk3auukd7pcegmE2CzPCa0bJ0ZsRAcKkCTJrWo5iDzNhMBWRyaMOv5zWSrthlf7G128qvIlpMT0YNY+n/FaOHE73uLrS/g7swl3/qH/BGFG2Hu4RlL48eb3lLKqTt2xKHdCs6Cd4RMfJPYnzgvI4BNrFUKsjkcu+WD4OO2A27Pq1n50cMchmcaXadJhGrOqH5YmHdOCj5NSHzJYrsW0HPlpuAx/ECMeIZYDh6RMqaFM2DXzdKX9NmmyqzJ3o/0lkk/N97gfVRLW5hA29yeAwaCViZNCP8iC9aO0q9fQojoa7NQnAtw==\"}]}}}"
#define LG_HANDSHAKE_PAIRED			L"{\"type\":\"register\",\"id\":\"register_0\",\"payload\":{\"forcePairing\":false,\"pairingType\":\"PROMPT\",\"client-key\":\"#CLIENTKEY#\",\"manifest\":{\"manifestVersion\":1,\"appVersion\":\"1.1\",\"signed\":{\"created\":\"20140509\",\"appId\":\"com.lge.test\",\"vendorId\":\"com.lge\",\"localizedAppNames\":{\"\":\"LG Remote App\",\"ko-KR\":\"리모컨 앱\",\"zxx-XX\":\"ЛГ Rэмotэ AПП\"},\"localizedVendorNames\":{\"\":\"LG Electronics\"},\"permissions\":[\"TEST_SECURE\",\"CONTROL_INPUT_TEXT\",\"CONTROL_MOUSE_AND_KEYBOARD\",\"READ_INSTALLED_APPS\",\"READ_LGE_SDX\",\"READ_NOTIFICATIONS\",\"SEARCH\",\"WRITE_SETTINGS\",\"WRITE_NOTIFICATION_ALERT\",\"CONTROL_POWER\",\"READ_CURRENT_CHANNEL\",\"READ_RUNNING_APPS\",\"READ_UPDATE_INFO\",\"UPDATE_FROM_REMOTE_APP\",\"READ_LGE_TV_INPUT_EVENTS\",\"READ_TV_CURRENT_TIME\"],\"serial\":\"2f930e2d2cfe083771f68e4fe7bb07\"},\"permissions\":[\"LAUNCH\",\"LAUNCH_WEBAPP\",\"APP_TO_APP\",\"CLOSE\",\"TEST_OPEN\",\"TEST_PROTECTED\",\"CONTROL_AUDIO\",\"CONTROL_DISPLAY\",\"CONTROL_INPUT_JOYSTICK\",\"CONTROL_INPUT_MEDIA_RECORDING\",\"CONTROL_INPUT_MEDIA_PLAYBACK\",\"CONTROL_INPUT_TV\",\"CONTROL_POWER\",\"CONTROL_TV_SCREEN\",\"READ_APP_STATUS\",\"READ_CURRENT_CHANNEL\",\"READ_INPUT_DEVICE_LIST\",\"READ_NETWORK_STATE\",\"READ_RUNNING_APPS\",\"READ_TV_CHANNEL_LIST\",\"WRITE_NOTIFICATION_TOAST\",\"READ_POWER_STATE\",\"READ_COUNTRY_INFO\",\"READ_SETTINGS\"],\"signatures\":[{\"signatureVersion\":1,\"signature\":\"eyJhbGdvcml0aG0iOiJSU0EtU0hBMjU2Iiwia2V5SWQiOiJ0ZXN0LXNpZ25pbmctY2VydCIsInNpZ25hdHVyZVZlcnNpb24iOjF9.hrVRgjCwXVvE2OOSpDZ58hR+59aFNwYDyjQgKk3auukd7pcegmE2CzPCa0bJ0ZsRAcKkCTJrWo5iDzNhMBWRyaMOv5zWSrthlf7G128qvIlpMT0YNY+n/FaOHE73uLrS/g7swl3/qH/BGFG2Hu4RlL48eb3lLKqTt2xKHdCs6Cd4RMfJPYnzgvI4BNrFUKsjkcu+WD4OO2A27Pq1n50cMchmcaXadJhGrOqH5YmHdOCj5NSHzJYrsW0HPlpuAx/ECMeIZYDh6RMqaFM2DXzdKX9NmmyqzJ3o/0lkk/N97gfVRLW5hA29yeAwaCViZNCP8iC9aO0q9fQojoa7NQnAtw==\"}]}}}"

// Ssap's
#define LG_URI_SCREENON				"com.webos.service.tvpower/power/turnOnScreen"
#define LG_URI_SCREENOFF			"com.webos.service.tvpower/power/turnOffScreen"
#define LG_URI_POWEROFF				"system/turnOff"
#define LG_URI_CREATEALERT			"system.notifications/createAlert"
#define LG_URI_CLOSEALERT			"system.notifications/closeAlert"
#define LG_URI_GETPOWERSTATE		"com.webos.service.tvpower/power/getPowerState"
#define LG_URI_GETFOREGROUNDAPP		"com.webos.applicationManager/getForegroundAppInfo"
#define LG_URI_GETINPUTSOCKET		"com.webos.service.networkinput/getPointerInputSocket"
#define LG_URI_LAUNCH				"system.launcher/launch"
#define LG_URI_CLOSE				"system.launcher/close"
#define LG_URI_SETMUTE				"audio/setMute"
#define LG_URI_SETVOLUME			"audio/setVolume"
#define LG_URI_GET_SYSTEM_SETTINGS	"settings/getSystemSettings"
#define LG_LUNA_SET_SYSTEM_SETT		"luna://com.webos.settingsservice/setSystemSettings"
#define LG_LUNA_SET_DEVICE_INFO		"luna://com.webos.service.eim/setDeviceInfo"
#define LG_LUNA_SET_CURVE_PRESET	"luna://com.webos.service.rollingscreen/changeCurve"
#define LG_LUNA_ADJUST_CURVE_PRESET	"luna://com.webos.service.rollingscreen/updateCurvature"
#define LG_LUNA_SET_TPC				"luna://com.webos.service.oledepl/setTemporalPeakControl"
#define LG_LUNA_SET_GSR				"luna://com.webos.service.oledepl/setGlobalStressReduction"

// Various
#define LG_URI_PAYLOAD_SETHDMI		"{\"id\":\"com.webos.app.hdmi#ARG#\"}"
#define LG_URI_PAYLOAD_CLOSEALERT	"{\"alertId\":\"com.webos.service.apiadapter-#ARG#\"}"

// for the webOS-client
#define	JSON_GETPOWERSTATE			"{\"type\":\"request\",\"id\":\"getPowerState\",\"uri\":\"ssap://com.webos.service.tvpower/power/getPowerState\",\"payload\":{}}"
#define	JSON_UNBLANK				"{\"type\":\"request\",\"id\":\"unblankScreen\",\"uri\":\"ssap://com.webos.service.tvpower/power/turnOnScreen\",\"payload\":{}}"
#define	JSON_BLANK					"{\"type\":\"request\",\"id\":\"blankScreen\",\"uri\":\"ssap://com.webos.service.tvpower/power/turnOffScreen\",\"payload\":{}}"
#define	JSON_POWERTOGGLE			"{\"type\":\"request\",\"id\":\"powerToggle\",\"uri\":\"ssap://system/turnOff\",\"payload\":{}}"
#define	JSON_MUTE					"{\"type\":\"request\",\"id\":\"mute\",\"uri\":\"ssap://audio/setMute\",\"payload\":{\"mute\":\"true\"}}"
#define	JSON_UNMUTE					"{\"type\":\"request\",\"id\":\"unmute\",\"uri\":\"ssap://audio/setMute\",\"payload\":{}}"
#define	JSON_GETAUDIOSTATUS			"{\"type\":\"request\",\"id\":\"getAudioStatus\",\"uri\":\"ssap://audio/getStatus\",\"payload\":{}}"
#define	JSON_GETFOREGROUNDAPP		"{\"type\":\"request\",\"id\":\"getForegroundApp\",\"uri\":\"ssap://com.webos.applicationManager/getForegroundAppInfo\",\"payload\":{}}"
#define	JSON_GETBUTTONSOCKET		"{\"type\":\"request\",\"id\":\"getButtonSocket\",\"uri\":\"ssap://com.webos.service.networkinput/getPointerInputSocket\",\"payload\":{}}"
#define	JSON_LUNA_CLOSE_ALERT		"{\"type\":\"request\",\"id\":\"closeLunaAlert\",\"uri\":\"ssap://system.notifications/closeAlert\",\"payload\":{\"alertId\":\"com.webos.service.apiadapter-#ARG#\"}}"
#define JSON_LUNA_SET_WOL			"{\"id\":\"luna_request\",\"payload\":{\"buttons\":[{\"label\":\"\",\"onClick\":\"luna://com.webos.settingsservice/setSystemSettings\",\"params\":{\"category\":\"network\",\"settings\":{\"wolwowlOnOff\":\"true\"}}}],\"message\":\" \",\"onclose\":{\"params\":{\"category\":\"network\",\"settings\":{\"wolwowlOnOff\":\"true\"}},\"uri\":\"luna://com.webos.settingsservice/setSystemSettings\"},\"onfail\":{\"params\":{\"category\":\"network\",\"settings\":{\"wolwowlOnOff\":\"true\"}},\"uri\":\"luna://com.webos.settingsservice/setSystemSettings\"}},\"type\":\"request\",\"uri\":\"ssap://system.notifications/createAlert\"}"
