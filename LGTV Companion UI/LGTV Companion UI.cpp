/*
DESCRIPTION
    <<<< LGTV Companion >>>>

    The application (UI and service) controls your WebOS (LG) Television.

    Using this application allows for your WebOS display to shut off and turn on
    in reponse to to the PC shutting down, rebooting, entering low power modes as well as 
    and when user is afk (idle). Like a normal PC-monitor. Or, alternatively this application 
    can be used as a command line tool to turn your displays on or off.

BACKGROUND
    With the rise in popularity of using OLED TVs as PC monitors, it is apparent that standard
    functionality of PC-monitors is missing. Particularly turning the display on or off in 
    response to power events in windows. With OLED monitors this is particularly important to 
    prevent "burn-in", or more accurately pixel-wear.

INSTALLATION AND USAGE
    0) Important prerequisites:
        a) Power ON all TVs
        b) Ensure that the TV can be woken via the network. For the CX line of displays this is 
          accomplished by navigating to Settings (cog button on remote)->All Settings->Connection->
          Mobile Connection Management->TV On with Mobile and enable 'Turn On via Wi-Fi'.
        c) Open the admin interface of your router, and set a static DHCP lease for your WebOS 
          devices, i.e. to ensure that the displays always have the same IP-address on your LAN.

    1) Download the setup package and install. This will install and start the service 
    (LGTVsvc.exe), and also install the user interface (LGTV Companion.exe).

    2) Open the user interface from the Windows start menu.

    3) Click the 'Scan' button to let the application try and automatically find network attached WebOs 
    devices (TVs)

    4) Optionally, click the drop down button to manually add, remove, configure the parameters of respective 
    devices, this includes the network IP-address, and the physical address, i.e. the MAC(s). This information
    can easily be found in the network settings of the TV.

    5) In the main application window, use the checkboxes to select what power events (shutdown, restart, 
    suspend, resume, idle) the respective devices shall respond to.

    6) Optionally, tweak additional settings, by clicking on the hamburger icon. Note that enabling logging can 
    be very useful if you are facing any issues.
    
    IMPORTANT NOTICE: if your OS is not localised in english, you must in the 'additional settings'
    dialog click the correct checkboxes to indicate what words refer to the system restarting/rebooting
    (as opposed to shutting down). This is needed because there is no better (at least known to me) 
    way for a service to know if the system is being restarted or shut down than looking at a certain event 
    in the event log. But the event log is localised, and this approach saves me from having to build a language 
    table for all languages in the world. Note that if you don't do this on a non-english OS the application
    will not be able to determine if the system is being restarted or shut down. The difference is of course 
    that the displays should not power off when the system is restarted.

    7) Click Apply, to save the configuration file and automatically restart the service. 
    
    8) At this point your respective WebOS TV will display a pairing dialog which you need to accept.
    
    All systems are now GO!

    9) Please go ahead and use the drop down menu again and select 'Test', to ensure that the displays 
    properly react to power on/off commands.


LIMITATIONS
    The OLED displays cannot be turned on via network when an automatic pixel refresh is being performed. You can hear 
    an internal relay click after the display is actually powered down, at which point it can be turned on again at any 
    time by the application.

    The WebOS displays can only be turned on/off when both the PC and the display is connected to a network. 


SYSTEM REQUIREMENTS
    - The application must be run in a modern windows environment, i.e. any potato running Windows 10 is fine.
    - A LAN

COMMAND LINE ARGUMENTS

    LGTV Companion.exe -[poweron|poweroff|screenon|screenoff|autoenable|autodisable|sethdmi1|sethdmi2|sethdmi3|sethdmi4] [Device1|Name] [Device2|Name] ... [DeviceX|Name]

    -poweron        - power on a device.
    -screenon       - power on a device
    -poweroff       - power off a device.
    -screenoff      - disable emitters, i.e. enter power saving mode where screen is blanked.
    -sethdmi1       - set HDMI input 1
    -sethdmi2       - set HDMI input 2
    -sethdmi3       - set HDMI input 3
    -sethdmi4       - set HDMI input 4
    -autoenable     - temporarily enable the automatic management of a device, i.e. to respond to power events. This
                      is effective until next restart of the service. (I personally use this for my home automation system).
    -autodisable    - temporarily disable the automatic management of a device, i.e. to respond to power events. This
                      is effective until next restart of the service. 

    [DeviceX|Name]  - device identifier. Either use Device1, Device2, ..., DeviceX or the friendly device name as found 
                      in the User Interface, for example OLED48CX.

    Example usage: LGTV Companion.exe -poweron Device1 Device2 "LG OLED48CX" -autodisable Device4 

    This command will power on device 1, device 2 and the device named LG OLED48CX, and additionally device4 is set to 
    temporarily not respond to automatic power events (on/off).

ADDITIONAL NOTES
    N/A

CHANGELOG
    v 1.0.0             - Initial release
    
    v 1.2.0             - Update logic for managing power events.
                        - Removal of redundant options
                        - More useful logging and error messages
                        - fixed proper command line interpretation of arguments enclosed in "quotes"
                        - Minor bug fixes
                        - Installer/upgrade bug fixed

    v 1.3.0             - Output Host IP in log (for debugging purposes) and additional logging
                        - Display warning in UI when TV is configured on a subnet different from PC
                        - Add service dependencies
                        - Set service shutdown priority
                        - Implementation of option to automatically check for new application version (off by default)
                        - Bug fixes and optimisations

    v 1.4.0             - Additional device network options for powering on via WOL

    v 1.5.0             - Implementation of more options for controlling power on/off logic
                        - Option to utilise capability for blanking TV (screen off)when user is idle
                        - Extended command line parameters to include blanking of screen.

    v 1.6.0             - Implementation of option for setting HDMI input on system start / resume from low power mode
                        - Command line parameter to set HDMI input added
    
    Todo:                 Option to  manage on active display outputs only

LICENSE
    Copyright (c) 2021-2022 Jörgen Persson
    
    Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation 
    files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, 
    modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the 
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE 
    WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR 
    COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, 
    ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

THANKS TO
    - Niels Lohmann - JSON for Modern CPP https://github.com/nlohmann/json
    - Boost libs - Boost and Beast https://www.boost.org/
    - Maassoft for initial inspo and understanding re the WebOS comms - https://github.com/Maassoft
    - Mohammed Boujemaoui - Author of WinToast https://github.com/mohabouje/WinToast
    - Etienne Dechanmps - valuable contributions

COPYRIGHT
    Copyright (c) 2021-2022 Jörgen Persson
*/

#include "LGTV Companion UI.h"

using namespace std;
using json = nlohmann::json;


// Global Variables:
HINSTANCE                       hInstance;  // current instance
HWND                            hMainWnd;

wstring                         CommandLineParameters;
wstring                         DataPath;
json                            jsonPrefs;                          //contains the user preferences in json
PREFS                           Prefs;
vector <SESSIONPARAMETERS>      Devices;                 //CSession objects manage network connections with Display
HBRUSH                          hBackbrush; 
HFONT                           hEditfont; 
HFONT                           hEditMediumfont;
HFONT                           hEditSmallfont;
HMENU                           hPopupMeuMain;
HICON                           hOptionsIcon;
HANDLE                          hPipe = INVALID_HANDLE_VALUE;
WSADATA                         WSAData;

//Application entry point
int APIENTRY wWinMain(_In_ HINSTANCE Instance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(nCmdShow);
 
    
    hInstance = Instance;
    MSG msg;
    wstring WindowTitle;

    WindowTitle = APPNAME_FULL;
    WindowTitle += L" v";
    WindowTitle += APP_VERSION;

    if (lpCmdLine)
        CommandLineParameters = lpCmdLine;
    
    //make lowercase command line parameters
    transform(CommandLineParameters.begin(), CommandLineParameters.end(), CommandLineParameters.begin(), ::tolower);
    
    // if the app is already running as another process, send the command line parameters to that process and exit 
    if (MessageExistingProcess(CommandLineParameters))
        return false;

    // read the configuration file and init prefs ad devices
    try {
        if(ReadConfigFile())
            ReadDeviceConfig();
    }
    catch (std::exception const& e) {

        wstring s = L"Error when reading configuration. Service is terminating. Error: ";
        s += widen(e.what());
        SvcReportEvent(EVENTLOG_ERROR_TYPE, s);
        MessageBox(NULL, L"Error when reading the configuration file.\n\nCheck the event log. Application terminated.", APPNAME_FULL, MB_OK | MB_ICONERROR);
        return false;
    }

    //parse and execute command line parameters when applicable and then exit
    if (Devices.size() > 0 && CommandLineParameters.size() > 0)
    {
            CommunicateWithService(narrow(CommandLineParameters));
            if (hPipe != INVALID_HANDLE_VALUE)
                CloseHandle(hPipe);
            return false;
    }

    hBackbrush = CreateSolidBrush(0x00ffffff);
    hEditfont = CreateFont(26, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, VARIABLE_PITCH, TEXT("Calibri"));
    hEditMediumfont = CreateFont(20, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, VARIABLE_PITCH, TEXT("Calibri"));
    hEditSmallfont = CreateFont(18, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, VARIABLE_PITCH, TEXT("Calibri"));
    hPopupMeuMain = LoadMenu(hInstance, MAKEINTRESOURCE(IDR_BUTTONMENU));

    INITCOMMONCONTROLSEX icex;           // Structure for control initialization.
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
 //   icex.dwICC = ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES | ICC_TREEVIEW_CLASSES | ICC_UPDOWN_CLASS | ICC_USEREX_CLASSES | ICC_BAR_CLASSES ;
    icex.dwICC = ICC_LISTVIEW_CLASSES  | ICC_UPDOWN_CLASS | ICC_STANDARD_CLASSES  | ICC_USEREX_CLASSES ;
    InitCommonControlsEx(&icex);

    hOptionsIcon = (HICON)LoadImageW(hInstance, MAKEINTRESOURCE(IDI_ICON2), IMAGE_ICON, 25, 25, LR_DEFAULTCOLOR | LR_DEFAULTSIZE | LR_SHARED);
    
    WSAStartup(MAKEWORD(2, 2), &WSAData);

    // create main window (dialog)
    hMainWnd = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_MAIN), NULL, (DLGPROC)WndProc);
    SetWindowText(hMainWnd, WindowTitle.c_str());
    ShowWindow(hMainWnd, SW_SHOW);
    UpdateWindow(hMainWnd);

    // spawn thread to check for updated version of the app.
    if (Prefs.AutoUpdate)
    {
        thread thread_obj(VersionCheckThread, hMainWnd);
        thread_obj.detach();
    }


    // message loop:
    while (GetMessage(&msg, NULL, 0, 0))
    {
       if (!IsDialogMessage(hMainWnd, &msg))
       {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
       }
    }

    //clean up
    DeleteObject(hBackbrush);
    DeleteObject(hEditfont);
    DeleteObject(hEditMediumfont);
    DeleteObject(hEditSmallfont);
    DestroyMenu(hPopupMeuMain);
    DestroyIcon(hOptionsIcon);
    WSACleanup();

    return (int) msg.wParam;
}

//   Process messages for the main window.
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    wstring str;
    switch (message)
    {
    case WM_INITDIALOG:
    {
        SendDlgItemMessage(hWnd, IDC_COMBO, WM_SETFONT, (WPARAM)hEditfont, MAKELPARAM(TRUE, 0));
        SendDlgItemMessage(hWnd, IDC_CHECK_ENABLE, WM_SETFONT, (WPARAM)hEditSmallfont, MAKELPARAM(TRUE, 0));
        SendDlgItemMessage(hWnd, IDC_NEWVERSION, WM_SETFONT, (WPARAM)hEditSmallfont, MAKELPARAM(TRUE, 0));
        SendDlgItemMessage(hWnd, IDC_DONATE, WM_SETFONT, (WPARAM)hEditSmallfont, MAKELPARAM(TRUE, 0));
        SendDlgItemMessage(hWnd, IDC_SPLIT, WM_SETFONT, (WPARAM)hEditSmallfont, MAKELPARAM(TRUE, 0));
        SendDlgItemMessage(hWnd, IDOK, WM_SETFONT, (WPARAM)hEditSmallfont, MAKELPARAM(TRUE, 0));

        SendMessage(GetDlgItem(hWnd, IDC_COMBO), (UINT)CB_RESETCONTENT, (WPARAM)0, (LPARAM)0);
        SendMessage(GetDlgItem(hWnd, IDC_COMBO), (UINT)CB_SETCURSEL, (WPARAM)-1, (LPARAM)0);

        if (Devices.size() > 0)
        {
            for (const auto& item : Devices)
            {
                SendMessage(GetDlgItem(hWnd, IDC_COMBO), (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)widen(item.Name).c_str());
            }
            SendMessage(GetDlgItem(hWnd, IDC_COMBO), (UINT)CB_SETCURSEL, (WPARAM)0, (LPARAM)0);

            CheckDlgButton(hWnd, IDC_CHECK_ENABLE, Devices[0].Enabled ? BST_CHECKED : BST_UNCHECKED);
            EnableWindow(GetDlgItem(hWnd, IDC_COMBO), true);
            EnableWindow(GetDlgItem(hWnd, IDC_CHECK_ENABLE), true);
 
            SetDlgItemText(hWnd, IDC_SPLIT, L"C&onfigure");

        }
        else
            SetDlgItemText(hWnd, IDC_SPLIT, L"&Scan");
        SendMessageW(GetDlgItem(hWnd, IDC_OPTIONS), BM_SETIMAGE, IMAGE_ICON, (LPARAM)hOptionsIcon);
    }break;
    case APP_NEW_VERSION:
    {
        ShowWindow(GetDlgItem(hWnd, IDC_NEWVERSION), SW_SHOW);
    }break;
    case APP_MESSAGE_ADD:
    {
        HWND hDeviceWnd = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_DEVICE), hWnd, (DLGPROC)DeviceWndProc);
        SetWindowText(hDeviceWnd, DEVICEWINDOW_TITLE_ADD);
        SetWindowText(GetDlgItem(hDeviceWnd, IDOK), L"&Add");
        CheckDlgButton(hDeviceWnd, IDC_RADIO1, BST_CHECKED);
        EnableWindow(GetDlgItem(hDeviceWnd, IDC_SUBNET), false);
        SetWindowText(GetDlgItem(hDeviceWnd, IDC_SUBNET), WOL_DEFAULTSUBNET);
 

        CheckDlgButton(hDeviceWnd, IDC_HDMI_INPUT_NUMBER_CHECKBOX, BST_UNCHECKED);
        EnableWindow(GetDlgItem(hDeviceWnd, IDC_HDMI_INPUT_NUMBER), false);
        SetWindowText(GetDlgItem(hDeviceWnd, IDC_HDMI_INPUT_NUMBER), L"1");

        CheckDlgButton(hDeviceWnd, IDC_SET_HDMI_INPUT_CHECKBOX, BST_UNCHECKED);
        EnableWindow(GetDlgItem(hDeviceWnd, IDC_SET_HDMI_INPUT_NUMBER), false);
        SetWindowText(GetDlgItem(hDeviceWnd, IDC_HDMI_INPUT_NUMBER), L"1");

        EnableWindow(GetDlgItem(hDeviceWnd, IDOK), false);

        EnableWindow(hWnd, false);
        ShowWindow(hDeviceWnd, SW_SHOW);
    }break;
    case APP_MESSAGE_MANAGE:
    {
        int sel = (int)(SendMessage(GetDlgItem(hWnd, IDC_COMBO), (UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0));
        if (sel == CB_ERR)
            break;

        HWND hDeviceWnd = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_DEVICE), hWnd, (DLGPROC)DeviceWndProc);
        SetWindowText(hDeviceWnd, DEVICEWINDOW_TITLE_MANAGE);
        SetWindowText(GetDlgItem(hDeviceWnd, IDOK), L"&Save");
        SetWindowText(GetDlgItem(hDeviceWnd, IDC_DEVICENAME), widen(Devices[sel].Name).c_str());
        SetWindowText(GetDlgItem(hDeviceWnd, IDC_DEVICEIP), widen(Devices[sel].IP).c_str());

        CheckDlgButton(hDeviceWnd, IDC_HDMI_INPUT_NUMBER_CHECKBOX, Devices[sel].HDMIinputcontrol);
        EnableWindow(GetDlgItem(hDeviceWnd, IDC_HDMI_INPUT_NUMBER), Devices[sel].HDMIinputcontrol?true:false);
        SetWindowText(GetDlgItem(hDeviceWnd, IDC_HDMI_INPUT_NUMBER), widen(std::to_string(Devices[sel].OnlyTurnOffIfCurrentHDMIInputNumberIs)).c_str());
        SendDlgItemMessage(hDeviceWnd, IDC_HDMI_INPUT_NUMBER_SPIN, UDM_SETPOS, (WPARAM)NULL, (LPARAM)Devices[sel].OnlyTurnOffIfCurrentHDMIInputNumberIs);

        CheckDlgButton(hDeviceWnd, IDC_SET_HDMI_INPUT_CHECKBOX, Devices[sel].SetHDMIInputOnResume);
        EnableWindow(GetDlgItem(hDeviceWnd, IDC_SET_HDMI_INPUT_NUMBER), Devices[sel].SetHDMIInputOnResume ? true : false);
        SetWindowText(GetDlgItem(hDeviceWnd, IDC_SET_HDMI_INPUT_NUMBER), widen(std::to_string(Devices[sel].SetHDMIInputOnResumeToNumber)).c_str());
        SendDlgItemMessage(hDeviceWnd, IDC_SET_HDMI_INPUT_SPIN, UDM_SETPOS, (WPARAM)NULL, (LPARAM)Devices[sel].SetHDMIInputOnResumeToNumber);


        str = L"";
        for (const auto& item : Devices[sel].MAC)
        {
            str += widen(item);
            if (item != Devices[sel].MAC.back())
                str += L"\r\n";
        }
        SetWindowText(GetDlgItem(hDeviceWnd, IDC_DEVICEMACS), str.c_str());
        EnableWindow(GetDlgItem(hDeviceWnd, IDOK), false);

        switch (Devices[sel].WOLtype)
        {
            case 1:
            {
                CheckDlgButton(hDeviceWnd, IDC_RADIO1, BST_CHECKED);
                EnableWindow(GetDlgItem(hDeviceWnd, IDC_SUBNET), false);
            }break;
            case 2:
            {
                CheckDlgButton(hDeviceWnd, IDC_RADIO2, BST_CHECKED);
                EnableWindow(GetDlgItem(hDeviceWnd, IDC_SUBNET), false);
            }break;
            case 3:
            {
                CheckDlgButton(hDeviceWnd, IDC_RADIO3, BST_CHECKED);
                EnableWindow(GetDlgItem(hDeviceWnd, IDC_SUBNET), true);

            }break;
            default:break;
        }
        if(Devices[sel].Subnet != "")
            SetWindowText(GetDlgItem(hDeviceWnd, IDC_SUBNET), widen(Devices[sel].Subnet).c_str());
        else
            SetWindowText(GetDlgItem(hDeviceWnd, IDC_SUBNET), WOL_DEFAULTSUBNET);

        EnableWindow(GetDlgItem(hDeviceWnd, IDOK), false);

        EnableWindow(hWnd, false);
        ShowWindow(hDeviceWnd, SW_SHOW);
    }break;
    case APP_MESSAGE_SCAN:
    {
        bool RemoveCurrentDevices = (wParam == 0) ? true : false;
        bool ChangesWereMade = false;
        int DevicesAdded = 0;
 
        HDEVINFO DeviceInfoSet = SetupDiGetClassDevs(NULL, NULL, NULL, DIGCF_ALLCLASSES);
        SP_DEVINFO_DATA DeviceInfoData;

        memset(&DeviceInfoData, 0, sizeof(SP_DEVINFO_DATA));
        DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
        int DeviceIndex = 0;

        while (SetupDiEnumDeviceInfo(DeviceInfoSet, DeviceIndex, &DeviceInfoData)) 
        {
            PDEVPROPKEY     pDevPropKey;
            DEVPROPTYPE PropType;
            DWORD required_size = 0;
            DeviceIndex++;

            pDevPropKey = (PDEVPROPKEY)(&DEVPKEY_Device_FriendlyName);

            SetupDiGetDeviceProperty(DeviceInfoSet, &DeviceInfoData, pDevPropKey, &PropType, nullptr, 0, &required_size, 0);
            if(required_size > 2)
            {
                vector<BYTE> unicode_buffer(required_size, 0);
                SetupDiGetDeviceProperty(DeviceInfoSet, &DeviceInfoData, pDevPropKey, &PropType, unicode_buffer.data(), required_size, nullptr, 0);
                wstring FriendlyName;
                FriendlyName = ((PWCHAR)unicode_buffer.data()); //NAME
                if (FriendlyName.find(L"[LG]", 0) != wstring::npos)
                {
                    /*
                    pDevPropKey = (PDEVPROPKEY)(&PKEY_DeviceDisplay_ModelNumber);
                    SetupDiGetDeviceProperty(DeviceInfoSet, &DeviceInfoData, pDevPropKey, &PropType, nullptr, 0, &required_size, 0);
                    if (required_size > 2)
                    {
                        vector<BYTE> unicode_buffer4(required_size, 0);
                        SetupDiGetDeviceProperty(DeviceInfoSet, &DeviceInfoData, pDevPropKey, &PropType, unicode_buffer4.data(), required_size, nullptr, 0);
                        FriendlyName = ((PWCHAR)unicode_buffer4.data()); //Shorter friendly name
                    }
                    */
                    pDevPropKey = (PDEVPROPKEY)(&PKEY_PNPX_IpAddress);
                    SetupDiGetDeviceProperty(DeviceInfoSet, &DeviceInfoData, pDevPropKey, &PropType, nullptr, 0, &required_size, 0);
                    if (required_size > 2)
                    {
                        vector<BYTE> unicode_buffer2(required_size, 0);
                        SetupDiGetDeviceProperty(DeviceInfoSet, &DeviceInfoData, pDevPropKey, &PropType, unicode_buffer2.data(), required_size, nullptr, 0);
                        wstring IP;
                        IP = ((PWCHAR)unicode_buffer2.data()); //IP
                        pDevPropKey = (PDEVPROPKEY)(&PKEY_PNPX_PhysicalAddress);
                        SetupDiGetDeviceProperty(DeviceInfoSet, &DeviceInfoData, pDevPropKey, &PropType, nullptr, 0, &required_size, 0);
                        if (required_size >= 6)
                        {
                            vector<BYTE> unicode_buffer3(required_size, 0);
                            SetupDiGetDeviceProperty(DeviceInfoSet, &DeviceInfoData, pDevPropKey, &PropType, unicode_buffer3.data(), required_size, nullptr, 0);
                            
                            stringstream ss;
                            string MAC;
                            ss << std::hex << std::setfill('0');
                            for (int i = 0; i < 6; i++) // TODO, check here what happens if more than one MAC in vector.
                            {
                                ss << std::setw(2) << static_cast<unsigned> (unicode_buffer3[i]);
                                if (i < 5)
                                    ss <<  ":";  
                            }
                            MAC = ss.str();
                            transform(MAC.begin(), MAC.end(), MAC.begin(), ::toupper);
                            if (RemoveCurrentDevices)
                            {
                                Devices.clear();
                                RemoveCurrentDevices = false;
                                ChangesWereMade = true;
                            }
                            bool DeviceExists = false;
                            for (auto& item : Devices)
                                for (auto& m : item.MAC)
                                    if (m == MAC)
                                    {
                                        if (narrow(IP) != item.IP)
                                        {
                                            item.IP = narrow(IP);
                                            ChangesWereMade = true;
                                        }
                                        DeviceExists = true;
                                    }

                            if (!DeviceExists)
                            {
                                SESSIONPARAMETERS temp;
                                temp.Name = narrow(FriendlyName);
                                temp.IP = narrow(IP);
                                temp.MAC.push_back(MAC);
                                temp.Subnet = narrow(WOL_DEFAULTSUBNET);
                                temp.WOLtype = WOL_IPSEND;
                                Devices.push_back(temp);
                                ChangesWereMade = true;
                                DevicesAdded++;
                            }
                        }
                    }
                }
            }          
        }
        if (ChangesWereMade)
        {
            SendMessage(hWnd, (UINT)WM_INITDIALOG, (WPARAM)0, (LPARAM)0);
            EnableWindow(GetDlgItem(hWnd, IDOK), true);
        }
        if (DeviceInfoSet) 
            SetupDiDestroyDeviceInfoList(DeviceInfoSet);
 
        wstringstream mess;
        mess << DevicesAdded;
        mess << L" new devices found.";
        MessageBox(hWnd, mess.str().c_str(), L"Scan results", MB_OK | MB_ICONINFORMATION);

    }break;  
    case APP_MESSAGE_REMOVE:
    {
        int sel = CB_ERR;
        if (wParam == 1) //remove all
        {
            Devices.clear();
            CheckDlgButton(hWnd, IDC_CHECK_ENABLE, BST_UNCHECKED);
            EnableWindow(GetDlgItem(hWnd, IDC_CHECK_ENABLE), false);
            SendMessage(GetDlgItem(hWnd, IDC_COMBO), (UINT)CB_RESETCONTENT, (WPARAM)0, (LPARAM)0);
            SendMessage(GetDlgItem(hWnd, IDC_COMBO), (UINT)CB_SETCURSEL, (WPARAM)-1, (LPARAM)0);
            SetDlgItemText(hWnd, IDC_SPLIT, L"&Scan");
        }
        else
        {
            int sel = (int)(SendMessage(GetDlgItem(hWnd, IDC_COMBO), (UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0));
            if (sel == CB_ERR)
                break;
            Devices.erase(Devices.begin() + sel);

            int ind = (int)SendMessage(GetDlgItem(hWnd, IDC_COMBO), (UINT)CB_DELETESTRING, (WPARAM)sel, (LPARAM)0);
            if (ind > 0)
            {
                int j = sel < ind ? sel : sel - 1;
                SendMessage(GetDlgItem(hWnd, IDC_COMBO), (UINT)CB_SETCURSEL, (WPARAM)j, (LPARAM)0);
                CheckDlgButton(hWnd, IDC_CHECK_ENABLE, Devices[j].Enabled ? BST_CHECKED : BST_UNCHECKED);
                EnableWindow(GetDlgItem(hWnd, IDC_CHECK_ENABLE), true);
                 SetDlgItemText(hWnd, IDC_SPLIT, L"C&onfigure");
            }
            else
            {
                CheckDlgButton(hWnd, IDC_CHECK_ENABLE, BST_UNCHECKED);
                EnableWindow(GetDlgItem(hWnd, IDC_CHECK_ENABLE), false);
                 SendMessage(GetDlgItem(hWnd, IDC_COMBO), (UINT)CB_SETCURSEL, (WPARAM)-1, (LPARAM)0);
                SetDlgItemText(hWnd, IDC_SPLIT, L"&Scan");
            }
        }
        EnableWindow(GetDlgItem(hWnd, IDOK), true);
  
    }break;

    case APP_MESSAGE_TURNON:
    {
        int sel = (int)(SendMessage(GetDlgItem(hWnd, IDC_COMBO), (UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0));
        if (sel == CB_ERR)
            break;
        string s = "-poweron ";
        s += Devices[sel].DeviceId;
        CommunicateWithService(s);
    }break;
    case APP_MESSAGE_TURNOFF:
    {
        int sel = (int)(SendMessage(GetDlgItem(hWnd, IDC_COMBO), (UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0));
        if (sel == CB_ERR)
            break;
        string s = "-poweroff ";
        s += Devices[sel].DeviceId;
        CommunicateWithService(s);    
    }break;
    case APP_MESSAGE_APPLY:
    {
        if (Devices.size() > 0)
        {
            //check if devices are on other subnets and warn user
            vector<string> HostIPs = GetOwnIP();
            int found = 0;
            if (HostIPs.size() > 0)
            {
                for (auto& Dev : Devices)
                {
                    for (auto& HostIP : HostIPs)
                    {
                        if (Dev.IP != "" && HostIP != "")
                        {
                            vector<string> DevIPcat = stringsplit(Dev.IP, ".");
                            DevIPcat.pop_back();
                            vector<string> HostIPcat = stringsplit(HostIP, ".");
                            HostIPcat.pop_back();
                            if (HostIPcat == DevIPcat)
                                found++;
                        }
                    }
                }
                if (found < Devices.size())
                {
                    int mb = MessageBox(hWnd, L"One or several devices have been configured to a subnet different from the PC. Please note that this might cause problems with waking up the TV. Please check the documentation and the configuration.\n\n Do you want to continue anyway?", L"Warning", MB_YESNO | MB_ICONEXCLAMATION);
                    if (mb == IDNO)
                    {
                        EnableWindow(hWnd, true);
                        return 0;
                    }
                }
            }
        }
        WriteConfigFile();

        //restart the service
        SERVICE_STATUS_PROCESS status;
        DWORD bytesNeeded;
        SC_HANDLE serviceDbHandle = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
        if (serviceDbHandle)
        {
            SC_HANDLE serviceHandle = OpenService(serviceDbHandle, L"LGTVsvc", SERVICE_QUERY_STATUS|SERVICE_STOP|SERVICE_START);
            if (serviceHandle)
            {
                QueryServiceStatusEx(serviceHandle, SC_STATUS_PROCESS_INFO, (LPBYTE)&status, sizeof(SERVICE_STATUS_PROCESS), &bytesNeeded);
                ControlService(serviceHandle, SERVICE_CONTROL_STOP, (LPSERVICE_STATUS)&status);
                while (status.dwCurrentState != SERVICE_STOPPED)
                {
                    Sleep(status.dwWaitHint);
                    if (QueryServiceStatusEx(serviceHandle, SC_STATUS_PROCESS_INFO, (LPBYTE)&status, sizeof(SERVICE_STATUS_PROCESS), &bytesNeeded))
                    {
                        if (status.dwCurrentState == SERVICE_STOPPED)
                            break;

                    }
                }

                StartService(serviceHandle, NULL, NULL);

                CloseServiceHandle(serviceHandle);
            }
            else
                MessageBox(hWnd, L"The LGTV Companion service is not installed. Please reinstall the application", L"Error", MB_OK | MB_ICONEXCLAMATION);
            CloseServiceHandle(serviceDbHandle);

        }
        //restart the daemon
        TCHAR buffer[MAX_PATH] = { 0 };
        GetModuleFileName(NULL, buffer, MAX_PATH);
        wstring path = buffer;
        wstring exe;
        std::wstring::size_type pos = path.find_last_of(L"\\/");
        path = path.substr(0, pos+1);
        exe = path;
        exe += L"LGTVdaemon.exe";
        ShellExecute(NULL, L"open", exe.c_str(), L"-hide", path.c_str(), SW_SHOWNORMAL);
        
        EnableWindow(hWnd, true);
        EnableWindow(GetDlgItem(hWnd, IDOK), false);

    }break;
    case WM_COMMAND:
    {
        switch (HIWORD(wParam))
        {
        case BN_CLICKED:
        {
            switch (LOWORD(wParam))
            {
            case IDC_CHECK_ENABLE:
             {
                int sel = (int)(SendMessage(GetDlgItem(hWnd, IDC_COMBO), (UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0));
                if (sel == CB_ERR)
                    break;
                Devices[sel].Enabled = IsDlgButtonChecked(hWnd, IDC_CHECK_ENABLE) == BST_CHECKED ? true : false;
                EnableWindow(GetDlgItem(hWnd, IDOK), true);
 
            }break;
            case IDOK:
            {
                EnableWindow(hWnd, false);
                //EnableWindow(GetDlgItem(hWnd, IDOK), false);
                PostMessage(hWnd, APP_MESSAGE_APPLY, (WPARAM) NULL, NULL);

            }break;
            case IDC_SPLIT:
            {
                if (Devices.size() > 0)
                    SendMessage(hWnd, APP_MESSAGE_MANAGE, NULL, NULL);
                else
                    SendMessage(hWnd, APP_MESSAGE_SCAN, NULL, NULL);
            }break;
            case IDC_OPTIONS:
            {
                HWND hOptionsWnd = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_OPTIONS), hWnd, (DLGPROC)OptionsWndProc);
                SetWindowText(hOptionsWnd, L"Additional options");
                EnableWindow(hWnd, false);
                ShowWindow(hOptionsWnd, SW_SHOW);
            }break;
            default:break;
            }
        }break;
        case CBN_SELCHANGE:
        {
            if (Devices.size() > 0)
            {
                int sel = (int)(SendMessage(GetDlgItem(hWnd, IDC_COMBO), (UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0));
                if (sel == CB_ERR)
                    break;

                CheckDlgButton(hWnd, IDC_CHECK_ENABLE, Devices[sel].Enabled ? BST_CHECKED : BST_UNCHECKED);
                EnableWindow(GetDlgItem(hWnd, IDC_CHECK_ENABLE), true);
            }
        }break;
  
        default:break;
        }
    }break;

    case WM_NOTIFY:
    {
        switch (((NMHDR*)lParam)->code)
        {
        case NM_CLICK:
        {
            //download new version
            if (wParam == IDC_NEWVERSION)
            {
                 ShellExecute(0, 0, NEWRELEASELINK, 0, 0, SW_SHOW);
            }
            //care to support your coder
            if (wParam == IDC_DONATE)
            {
                if (MessageBox(hWnd, L"This is free software, but your support is appreciated and there is a donation page set up over at PayPal. PayPal allows you to use a credit- or debit card or your PayPal balance to make a donation, even without a PayPal account.\n\nClick 'Yes' to continue to the PayPal donation web page!", L"Donate via PayPal?", MB_YESNO | MB_ICONQUESTION) == IDYES)
                    ShellExecute(0, 0, DONATELINK, 0, 0, SW_SHOW);
            }
        }
        case BCN_DROPDOWN:
        {
            NMBCDROPDOWN* pDropDown = (NMBCDROPDOWN*)lParam;
            if (pDropDown->hdr.hwndFrom == GetDlgItem(hWnd, IDC_SPLIT))
            {
                POINT pt;
                MENUITEMINFO mi;
                pt.x = pDropDown->rcButton.right;
                pt.y = pDropDown->rcButton.bottom;
                ClientToScreen(pDropDown->hdr.hwndFrom, &pt);

                mi.cbSize = sizeof(MENUITEMINFO);
                mi.fMask = MIIM_STATE;
                if (Devices.size() > 0)
                    mi.fState = MFS_ENABLED;
                else
                    mi.fState = MFS_DISABLED;

                SetMenuItemInfo(hPopupMeuMain, ID_M_MANAGE, false, &mi);
                SetMenuItemInfo(hPopupMeuMain, ID_M_REMOVE, false, &mi);
                SetMenuItemInfo(hPopupMeuMain, ID_M_REMOVEALL, false, &mi);

                if (Devices.size() > 0 && !IsWindowEnabled(GetDlgItem(hWnd, IDOK)))
                    mi.fState = MFS_ENABLED;
                else
                    mi.fState = MFS_DISABLED;

                SetMenuItemInfo(hPopupMeuMain, ID_M_TEST, false, &mi);
                SetMenuItemInfo(hPopupMeuMain, ID_M_TURNON, false, &mi);
                SetMenuItemInfo(hPopupMeuMain, ID_M_TURNOFF, false, &mi);

                switch (TrackPopupMenu(GetSubMenu(hPopupMeuMain, 0), TPM_TOPALIGN | TPM_RIGHTALIGN | TPM_RETURNCMD, pt.x, pt.y, 0, hWnd, NULL))
                {
                 case ID_M_REMOVE:
                {
                     if (Devices.size() > 0)
                         if (MessageBox(hWnd, L"You are about to remove this device.\n\nDo you want to continue?", L"Remove device", MB_YESNO | MB_ICONQUESTION) == IDYES)
                             SendMessage(hWnd, (UINT)APP_MESSAGE_REMOVE, (WPARAM)0, (LPARAM)lParam);
                }break;
                 case ID_M_REMOVEALL:
                 {
                     if (Devices.size() > 0)
                         if (MessageBox(hWnd, L"You are about to remove ALL devices.\n\nDo you want to continue?", L"Remove all devices", MB_YESNO | MB_ICONQUESTION) == IDYES)
                             SendMessage(hWnd, (UINT)APP_MESSAGE_REMOVE, (WPARAM)1, (LPARAM)lParam);
                 }break;

                case ID_M_ADD:
                {
                    SendMessage(hWnd, (UINT)APP_MESSAGE_ADD, (WPARAM)wParam, (LPARAM)lParam);
                }break;
                case ID_M_MANAGE:
                {
                    SendMessage(hWnd, (UINT)APP_MESSAGE_MANAGE, (WPARAM)wParam, (LPARAM)lParam);
                }break;
                case ID_M_SCAN:
                {
                    
                    if (Devices.size() > 0)
                    {
                        int ms = MessageBoxW(hWnd, L"Scanning will discover and add network attached LG devices.\n\nDo you want to replace the current devices with any discovered devices?\n\nYES = clear current devices before adding, \n\nNO = add to current list of devices.", L"Scanning", MB_YESNOCANCEL | MB_ICONEXCLAMATION);
                        
                        if (ms == IDCANCEL)
                            break;

                        SendMessage(hWnd, (UINT)APP_MESSAGE_SCAN, (WPARAM)ms == IDYES ? 0 : 1, (LPARAM)lParam);
                    }
                    else
                        SendMessage(hWnd, (UINT)APP_MESSAGE_SCAN, (WPARAM)0, (LPARAM)lParam);
                 }break;
                case ID_M_TEST:
                {
                    if (Devices.size() > 0)
                    {
                        if (IsWindowEnabled(GetDlgItem(hWnd, IDOK)))
                            if (MessageBox(hWnd, L"Please apply unsaved changes before attempting to control the device", L"Information", MB_OK | MB_ICONEXCLAMATION) == IDOK)
                                break;
                        int ms = MessageBoxW(hWnd, L"You are about to test the ability to control this device?\n\nPlease click YES to power off the device. Then wait about 5 seconds, or until you hear an iinternal relay of the TV clicking, and press ENTER on your keyboard to power on the device again.", L"Test device", MB_YESNO | MB_ICONQUESTION);
                        switch (ms)
                        {
                        case IDYES:
                            SendMessage(hWnd, (UINT)APP_MESSAGE_TURNOFF, (WPARAM)wParam, (LPARAM)lParam);
                            MessageBoxW(hWnd, L"Please press ENTER on your keyboard to power on the device again.", L"Test device", MB_OK | MB_ICONEXCLAMATION);
                            SendMessage(hWnd, (UINT)APP_MESSAGE_TURNON, (WPARAM)wParam, (LPARAM)lParam);
                            break;
                        default:break;
                        }
                    }
                }break;
                case ID_M_TURNON:
                {                     
                    if (Devices.size() > 0)
                    {
                        if (IsWindowEnabled(GetDlgItem(hWnd, IDOK)))
                            if (MessageBox(hWnd, L"Please apply unsaved changes before attempting to control the device", L"Information", MB_OK | MB_ICONEXCLAMATION) == IDOK)
                                break;
                        SendMessage(hWnd, (UINT)APP_MESSAGE_TURNON, (WPARAM)wParam, (LPARAM)lParam);
                    }
                }break;
                case ID_M_TURNOFF:
                {
                    if (Devices.size() > 0)
                    {
                        if (IsWindowEnabled(GetDlgItem(hWnd, IDOK)))
                            if (MessageBox(hWnd, L"Please apply unsaved changes before attempting to control the device", L"Information", MB_OK | MB_ICONEXCLAMATION) == IDOK)
                                break;
                        SendMessage(hWnd, (UINT)APP_MESSAGE_TURNOFF, (WPARAM)wParam, (LPARAM)lParam);
                    }
                }break;

                default:break;
                }
            }
        }	break;

        default:break;
        }
    }break;
    case WM_CTLCOLORSTATIC:
    {

        HDC hdcStatic = (HDC)wParam;
        if ((HWND)lParam == GetDlgItem(hWnd, IDC_CHECK_ENABLE) )
        {
            SetBkMode(hdcStatic, TRANSPARENT);
        }
        return(INT_PTR)hBackbrush;

    }break;
    case WM_COPYDATA:
    {
        wstring CmdLineExternal;
         if (!lParam)
            return true;

        COPYDATASTRUCT* pcds = (COPYDATASTRUCT*)lParam;

        if (pcds->dwData == NOTIFY_NEW_COMMANDLINE)
        {
            if (pcds->cbData == 0)
                return true;
            CmdLineExternal = (WCHAR*)pcds->lpData;
            if (CmdLineExternal.size() == 0)
                return true;
            CommunicateWithService(narrow(CmdLineExternal));
        }
        return true;
    }break;
    case WM_PAINT:
    {
        RECT rc = { 0 };
        GetClientRect(hWnd, &rc);
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        FillRect(hdc, &rc, (HBRUSH)hBackbrush);
        EndPaint(hWnd, &ps);
    }break;
    case WM_CLOSE:
    {
        if (IsWindowEnabled(GetDlgItem(hWnd, IDOK)))
        {
            int mess = MessageBox(hWnd, L"You have made changes which are not yet applied.\n\nDo you want to apply the changes before exiting?", L"Unsaved configuration", MB_YESNOCANCEL | MB_ICONEXCLAMATION);
            if (mess == IDCANCEL)
                break;
            if (mess == IDYES)
                SendMessage(hWnd, WM_COMMAND, MAKEWPARAM(IDOK, BN_CLICKED), NULL);
        }
        DestroyWindow(hWnd);
    }break;
    case WM_DESTROY:
    {
        PostQuitMessage(0);
        break;
    }
    default:
        return 0;
    }
    return 0;
}

//   Process messages for the add/manage devices window  
LRESULT CALLBACK DeviceWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_INITDIALOG:
    {
        SendDlgItemMessage(hWnd, IDC_DEVICENAME, WM_SETFONT, (WPARAM)hEditfont, MAKELPARAM(TRUE, 0));
        SendDlgItemMessage(hWnd, IDC_DEVICEIP, WM_SETFONT, (WPARAM)hEditfont, MAKELPARAM(TRUE, 0));
        SendDlgItemMessage(hWnd, IDC_HDMI_INPUT_NUMBER, WM_SETFONT, (WPARAM)hEditMediumfont, MAKELPARAM(TRUE, 0));
        SendDlgItemMessage(hWnd, IDC_HDMI_INPUT_NUMBER_SPIN, UDM_SETRANGE, (WPARAM)NULL, MAKELPARAM(4, 1));
        SendDlgItemMessage(hWnd, IDC_DEVICEMACS, WM_SETFONT, (WPARAM)hEditMediumfont, MAKELPARAM(TRUE, 0));
        SendDlgItemMessage(hWnd, IDC_SUBNET, WM_SETFONT, (WPARAM)hEditMediumfont, MAKELPARAM(TRUE, 0));
        SendDlgItemMessage(hWnd, IDC_SET_HDMI_INPUT_NUMBER, WM_SETFONT, (WPARAM)hEditMediumfont, MAKELPARAM(TRUE, 0));
        SendDlgItemMessage(hWnd, IDC_SET_HDMI_INPUT_SPIN, UDM_SETRANGE, (WPARAM)NULL, MAKELPARAM(4, 1));

    }break;
    case WM_NOTIFY:
    {
        switch (((NMHDR*)lParam)->code)
        {
        case NM_CLICK:
        {
            // explain the WOL options
            if (wParam == IDC_SYSLINK4)
            {
                MessageBox(hWnd,
                    L"Devices are powered on by means of sending wake-on-Lan magic packets. Depending on your network environment, TV firmware and operating system you may need to adjust these settings.\n\n"
                    "The \"Device IP-address\" approach sends unicast packets directly to the device IP. IP neighbor table overrides are used to make this work even if ARP doesn't. This is believed to be the most reliable option.\n\n"
                    "If the \"Device IP-address\" option doesn't work for you, you might want to try the other options. You are invited to file an issue on GitHub so that we can get a better understanding of what works and what doesn't work in the wild.\n\n"
                    "Anecdotal evidence seems to suggest broadcast-based approaches can intermittently fail to wake up the TV (see e.g. GitHub issue #19), which is why they are not recommended.\n\n"
                    "Between the two broadcast options, the subnet approach is the most likely to work. The global network broadcast approach is prone to issues when multiple network interfaces are present (VPN, etc.), because the packet might be sent using the wrong interface.\n\n"
                    "The current subnet mask of the subnet(s) of your PC can be found by using the \"IPCONFIG /all\" command in the command prompt.",
                    L"Information", MB_OK | MB_ICONINFORMATION);
            }
        }break;
        }
    }break;
    case WM_COMMAND:
    {
        switch (HIWORD(wParam))
        {
        case BN_CLICKED:
        {
            switch (LOWORD(wParam))
            {
            case IDC_HDMI_INPUT_NUMBER_CHECKBOX:
            {
                EnableWindow(GetDlgItem(hWnd, IDC_HDMI_INPUT_NUMBER), IsDlgButtonChecked(hWnd, IDC_HDMI_INPUT_NUMBER_CHECKBOX) == BST_CHECKED);
                EnableWindow(GetDlgItem(hWnd, IDOK), true);
            }break;
            case IDC_SET_HDMI_INPUT_CHECKBOX:
            {
                EnableWindow(GetDlgItem(hWnd, IDC_SET_HDMI_INPUT_NUMBER), IsDlgButtonChecked(hWnd, IDC_SET_HDMI_INPUT_CHECKBOX) == BST_CHECKED);
                EnableWindow(GetDlgItem(hWnd, IDOK), true);
            }break;
            case IDC_RADIO1:
            case IDC_RADIO2:
            {
                EnableWindow(GetDlgItem(hWnd, IDC_SUBNET), false);
                EnableWindow(GetDlgItem(hWnd, IDOK), true);

            }break;
            case IDC_RADIO3:
            {
                EnableWindow(GetDlgItem(hWnd, IDC_SUBNET), true);
                EnableWindow(GetDlgItem(hWnd, IDOK), true);


            }break;
            case IDOK:
            {
                HWND hParentWnd = GetParent(hWnd);
                if (hParentWnd)
                {
                    vector<string> maclines;
                    wstring edittext = GetWndText(GetDlgItem(hWnd, IDC_DEVICEMACS));
                    
                    //verify the user supplied information
                    if (edittext != L"")
                    {
                        transform(edittext.begin(), edittext.end(), edittext.begin(), ::toupper);
                        
                        //remove unecessary characters
                        char CharsToRemove[] = ":- ,;.\n";
                        for (int i = 0; i < strlen(CharsToRemove); ++i)
                            edittext.erase(remove(edittext.begin(), edittext.end(), CharsToRemove[i]), edittext.end());
                        
                        //check on HEX
                        if (edittext.find_first_not_of(L"0123456789ABCDEF\r") != std::string::npos)
                        {
                            MessageBox(hWnd, L"One or several MAC addresses contain illegal caharcters.\n\nPlease correct before continuing.", L"Error", MB_OK | MB_ICONERROR);
                            return 0;
                        }
                        //verify length of MACs
                        maclines = stringsplit(narrow(edittext), "\r");
                        for (auto& mac : maclines)
                        {
                            if (mac.length() != 12)
                            {
                                MessageBox(hWnd, L"One or several MAC addresses is incorrect.\n\nPlease correct before continuing.", L"Error", MB_OK | MB_ICONERROR);
                                maclines.clear();
                                return 0;
                            }
                            else
                                for (int ind = 4; ind >= 0; ind--)
                                    mac.insert(ind * 2 + 2, ":");
                        }

                    }

                    if (maclines.size() > 0 && GetWndText(GetDlgItem(hWnd, IDC_DEVICENAME)) != L"" && GetWndText(GetDlgItem(hWnd, IDC_DEVICEIP)) != L"")
                    {
                        if (GetWndText(hWnd) == DEVICEWINDOW_TITLE_MANAGE) //configuring existing device
                        {
                            int sel = (int)(SendMessage(GetDlgItem(hParentWnd, IDC_COMBO), (UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0));
                            if (sel == CB_ERR)
                                break;

                            Devices[sel].MAC = maclines;
                            Devices[sel].Name = narrow(GetWndText(GetDlgItem(hWnd, IDC_DEVICENAME)));
                            Devices[sel].IP = narrow(GetWndText(GetDlgItem(hWnd, IDC_DEVICEIP)));
                            Devices[sel].Subnet = narrow(GetWndText(GetDlgItem(hWnd, IDC_SUBNET)));

                            Devices[sel].HDMIinputcontrol = IsDlgButtonChecked(hWnd, IDC_HDMI_INPUT_NUMBER_CHECKBOX) == BST_CHECKED;
                            Devices[sel].OnlyTurnOffIfCurrentHDMIInputNumberIs = atoi(narrow(GetWndText(GetDlgItem(hWnd, IDC_HDMI_INPUT_NUMBER))).c_str());
                            
                            Devices[sel].SetHDMIInputOnResume = IsDlgButtonChecked(hWnd, IDC_SET_HDMI_INPUT_CHECKBOX) == BST_CHECKED;
                            Devices[sel].SetHDMIInputOnResumeToNumber = atoi(narrow(GetWndText(GetDlgItem(hWnd, IDC_SET_HDMI_INPUT_NUMBER))).c_str());

                            if (IsDlgButtonChecked(hWnd, IDC_RADIO3))
                                Devices[sel].WOLtype = WOL_SUBNETBROADCAST;
                            else if (IsDlgButtonChecked(hWnd, IDC_RADIO2))
                                Devices[sel].WOLtype = WOL_IPSEND;
                            else
                                Devices[sel].WOLtype = WOL_NETWORKBROADCAST;

                            int ind = (int)SendMessage(GetDlgItem(hParentWnd, IDC_COMBO), (UINT)CB_DELETESTRING, (WPARAM)sel, (LPARAM)0);
                            SendMessage(GetDlgItem(hParentWnd, IDC_COMBO), (UINT)CB_INSERTSTRING, (WPARAM)sel, (LPARAM)widen(Devices[sel].Name).c_str());
                            SendMessage(GetDlgItem(hParentWnd, IDC_COMBO), (UINT)CB_SETCURSEL, (WPARAM)sel, (LPARAM)0);
                            EnableWindow(GetDlgItem(hParentWnd, IDOK), true);


                        }
                        else //adding a new device
                        {
                            SESSIONPARAMETERS sess;
                            stringstream strs;
                            strs << "Device";
                            strs << Devices.size() + 1;
                            sess.DeviceId = strs.str();
                            sess.MAC = maclines;
                            sess.Name = narrow(GetWndText(GetDlgItem(hWnd, IDC_DEVICENAME)));
                            sess.IP = narrow(GetWndText(GetDlgItem(hWnd, IDC_DEVICEIP)));

                            sess.Subnet = narrow(GetWndText(GetDlgItem(hWnd, IDC_SUBNET)));

                            if (IsDlgButtonChecked(hWnd, IDC_RADIO3))
                                sess.WOLtype = WOL_SUBNETBROADCAST;
                            else if (IsDlgButtonChecked(hWnd, IDC_RADIO2))
                                sess.WOLtype = WOL_IPSEND;
                            else
                                sess.WOLtype = WOL_IPSEND;
                           
                            sess.HDMIinputcontrol = IsDlgButtonChecked(hWnd, IDC_HDMI_INPUT_NUMBER_CHECKBOX) == BST_CHECKED;
                            sess.OnlyTurnOffIfCurrentHDMIInputNumberIs = atoi(narrow(GetWndText(GetDlgItem(hWnd, IDC_HDMI_INPUT_NUMBER))).c_str());
                            sess.SetHDMIInputOnResume = IsDlgButtonChecked(hWnd, IDC_SET_HDMI_INPUT_CHECKBOX) == BST_CHECKED;
                            sess.SetHDMIInputOnResumeToNumber = atoi(narrow(GetWndText(GetDlgItem(hWnd, IDC_SET_HDMI_INPUT_NUMBER))).c_str());

                            Devices.push_back(sess);

                            int index = (int)SendMessage(GetDlgItem(hParentWnd, IDC_COMBO), (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)widen(sess.Name).c_str());
                            if (index != CB_ERR)
                            {
                                SendMessage(GetDlgItem(hParentWnd, IDC_COMBO), (UINT)CB_SETCURSEL, (WPARAM)index, (LPARAM)0);
                                CheckDlgButton(hParentWnd, IDC_CHECK_ENABLE, BST_CHECKED);
                                EnableWindow(GetDlgItem(hParentWnd, IDC_CHECK_ENABLE), true);

                                SetDlgItemText(hParentWnd, IDC_SPLIT, L"C&onfigure");
                                EnableWindow(GetDlgItem(hParentWnd, IDOK), true);

                            }
                            else {
                                MessageBox(hWnd, L"Failed to add a new device.\n\nUnknown error.", L"Error", MB_OK | MB_ICONERROR);
                            }
                        }

                        EndDialog(hWnd, 0);

                        EnableWindow(GetParent(hWnd), true);
                    }
                    else
                    {
                        MessageBox(hWnd, L"The configuration is incorrect or missing information.\n\nPlease correct before continuing.", L"Error", MB_OK | MB_ICONERROR);
                        return 0;
                    }

                }
            }break;

            case IDCANCEL:
            {
                if (IsWindowEnabled(GetDlgItem(hWnd, IDOK)))
                {
                    if (MessageBox(hWnd, L"You have made changes to the configuration.\n\nDo you want to discard the changes?", L"Error", MB_YESNOCANCEL | MB_ICONQUESTION) == IDYES)
                    {
                        EndDialog(hWnd, 0);
                        EnableWindow(GetParent(hWnd), true);
                    }
                }
                else
                {
                    EndDialog(hWnd, 0);
                    EnableWindow(GetParent(hWnd), true);

                }
            }break;
            default:break;
            }
        }break;
        case EN_CHANGE:
        {
            switch (LOWORD(wParam))
            {
            case IDC_DEVICENAME:
            case IDC_DEVICEIP:
            case IDC_HDMI_INPUT_NUMBER:
            case IDC_SET_HDMI_INPUT_NUMBER:
            case IDC_DEVICEMACS:
            case IDC_SUBNET:

            {
                EnableWindow(GetDlgItem(hWnd, IDOK), true);

            }break;
            default:break;
            }
        }break;
        default:break;
        }
    }break;
    
    case WM_CTLCOLORSTATIC:
    {

        HDC hdcStatic = (HDC)wParam;
        if ((HWND)lParam == GetDlgItem(hWnd, IDC_CHECK_ENABLE))
        {
            SetBkMode(hdcStatic, TRANSPARENT);
        }
        return(INT_PTR)hBackbrush;

    }break;
 
    case WM_PAINT:
    {
        RECT rc = { 0 };

        GetClientRect(hWnd, &rc);

        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        FillRect(hdc, &rc, (HBRUSH)hBackbrush);

        EndPaint(hWnd, &ps);
    }break;
    
    case WM_CLOSE:
    {
        EndDialog(hWnd, 0);
    }break;

    default:break; 
    }
    return 0;
}

//   Process messages for the options window
LRESULT CALLBACK OptionsWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_INITDIALOG:
    {
        LVCOLUMN lvC;
        LVITEM lvi;
        DWORD status = ERROR_SUCCESS;
        EVT_HANDLE hResults = NULL;
        wstring path = L"System";
        wstring query = L"Event/System[EventID=1074]";
        vector<wstring> str;

        SendDlgItemMessage(hWnd, IDC_TIMEOUT, WM_SETFONT, (WPARAM)hEditMediumfont, MAKELPARAM(TRUE, 0));
        SendDlgItemMessage(hWnd, IDC_LIST, WM_SETFONT, (WPARAM)hEditMediumfont, MAKELPARAM(TRUE, 0));
        SendDlgItemMessage(hWnd, IDC_SPIN, UDM_SETRANGE , (WPARAM)NULL, MAKELPARAM(100, 1));
        SendDlgItemMessage(hWnd, IDC_SPIN, UDM_SETPOS, (WPARAM)NULL, (LPARAM)Prefs.PowerOnTimeout);
        SendDlgItemMessage(hWnd, IDC_EDIT_BLANK, WM_SETFONT, (WPARAM)hEditMediumfont, MAKELPARAM(TRUE, 0));
        SendDlgItemMessage(hWnd, IDC_SPIN2, UDM_SETRANGE, (WPARAM)NULL, MAKELPARAM(240, 1));
        SendDlgItemMessage(hWnd, IDC_SPIN2, UDM_SETPOS, (WPARAM)NULL, (LPARAM)Prefs.BlankScreenWhenIdleDelay);

        for (auto& item : Prefs.EventLogRestartString)
            str.push_back(widen(item));
        for (auto& item : Prefs.EventLogShutdownString)
            str.push_back(widen(item));
        hResults = EvtQuery(NULL, path.c_str(), query.c_str(), EvtQueryChannelPath | EvtQueryReverseDirection);
        if (hResults)
        {
            EVT_HANDLE hEv[100];
            DWORD dwReturned = 0;

            if (EvtNext(hResults, 100, hEv, INFINITE, 0, &dwReturned))
            {
                for (DWORD i = 0; i < dwReturned; i++) 
                {
                    DWORD dwBufferSize = 0;
                    DWORD dwBufferUsed = 0;
                    DWORD dwPropertyCount = 0;
                    LPWSTR pRenderedContent = NULL;
                      
                    if (!EvtRender(NULL, hEv[i], EvtRenderEventXml, dwBufferSize, pRenderedContent, &dwBufferUsed, &dwPropertyCount))
                    {
                        if (ERROR_INSUFFICIENT_BUFFER == GetLastError())
                        {
                            dwBufferSize = dwBufferUsed;
                            pRenderedContent = (LPWSTR)malloc(dwBufferSize);
                            if (pRenderedContent)
                            {
                                EvtRender(NULL, hEv[i], EvtRenderEventXml, dwBufferSize, pRenderedContent, &dwBufferUsed, &dwPropertyCount);
                            }
                        }
                    }

                    if (pRenderedContent)
                    {
                        wstring s = pRenderedContent;
                        free(pRenderedContent);
                            
                        wstring strfind = L"<Data Name='param5'>";
                        size_t f = s.find(strfind);
                        if (f != wstring::npos)
                        {
                            size_t e = s.find(L"<", f + 1);
                            if (e != wstring::npos)
                            {
                                wstring sub = s.substr(f + strfind.length(), e - (f + strfind.length()));
                                if (std::find(str.begin(), str.end(), sub) == str.end())
                                {
                                    str.push_back(sub);
                                }
                            }
                        }
                    }
                    EvtClose(hEv[i]);
                    hEv[i] = NULL;
                }
            }
            EvtClose(hResults);
        }
        lvC.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
        lvC.iSubItem = 0; lvC.cx = 140;	lvC.fmt = LVCFMT_LEFT;
        lvC.pszText = (LPWSTR)L"Eventlog 1074";
        ListView_InsertColumn(GetDlgItem(hWnd, IDC_LIST), 0, &lvC);
        ListView_SetExtendedListViewStyle(GetDlgItem(hWnd, IDC_LIST), LVS_EX_FULLROWSELECT | LVS_EX_CHECKBOXES | LVS_EX_DOUBLEBUFFER);
        memset(&lvi, 0, sizeof(LVITEM));
        lvi.mask = LVIF_TEXT | LVIF_PARAM | LVIF_STATE;
        lvi.state = 0;
        lvi.stateMask = 0;
        lvi.iItem = 1;
        lvi.iSubItem = 0;
        lvi.lParam = (LPARAM)0;
        int i = 0;

        for (auto& item : str)
        {
            lvi.iItem = i;
            int row = ListView_InsertItem(GetDlgItem(hWnd, IDC_LIST), &lvi);
            ListView_SetItemText(GetDlgItem(hWnd, IDC_LIST), row, 0, (LPWSTR)item.c_str());
            if (std::find(Prefs.EventLogRestartString.begin(), Prefs.EventLogRestartString.end(), narrow(item)) != Prefs.EventLogRestartString.end())
            {
                ListView_SetCheckState(GetDlgItem(hWnd, IDC_LIST), row, true);
            }

            //				ListView_SetItemState(GetDlgItem(hWnd, IDC_OPT_LANGUAGES_LIST), i, UINT((int(true) + 1) << 12), LVIS_STATEIMAGEMASK);
            i++;
        }
        CheckDlgButton(hWnd, IDC_LOGGING, Prefs.Logging ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hWnd, IDC_AUTOUPDATE, Prefs.AutoUpdate ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hWnd, IDC_CHECK_BLANK, Prefs.BlankScreenWhenIdle ? BST_CHECKED : BST_UNCHECKED);
        EnableWindow(GetDlgItem(hWnd, IDC_EDIT_BLANK), Prefs.BlankScreenWhenIdle);

        EnableWindow(GetDlgItem(hWnd, IDOK), true);
    }break;
    case WM_COMMAND:
    {
        switch (HIWORD(wParam))
        {
        case BN_CLICKED:
        {
            switch (LOWORD(wParam))
            {
            case IDC_CHECK_BLANK:
            {
                if (Prefs.version < 2 && !Prefs.ResetAPIkeys)
                {
                    int mess = MessageBox(hWnd, L"Enabling this option will enforce re-pairing of all your devices.\n\n Do you want to enable this option?", L"Device pairing", MB_YESNO | MB_ICONQUESTION);
                    if (mess == IDNO)
                    {
                        CheckDlgButton(hWnd, IDC_CHECK_BLANK, BST_UNCHECKED);
                        EnableWindow(GetDlgItem(hWnd, IDC_EDIT_BLANK), false);

                    }
                    if (mess == IDYES)
                    {
                        CheckDlgButton(hWnd, IDC_CHECK_BLANK, BST_CHECKED);
                        EnableWindow(GetDlgItem(hWnd, IDC_EDIT_BLANK), true);

                        Prefs.ResetAPIkeys = true;
                        EnableWindow(GetDlgItem(hWnd, IDOK), true);
                    }
                }
                else
                {
                    if(IsDlgButtonChecked(hWnd, IDC_CHECK_BLANK))
                        EnableWindow(GetDlgItem(hWnd, IDC_EDIT_BLANK), true);
                    else
                        EnableWindow(GetDlgItem(hWnd, IDC_EDIT_BLANK), false);

                    EnableWindow(GetDlgItem(hWnd, IDOK), true);
                }

            }break;
            case IDC_LOGGING:
            case IDC_AUTOUPDATE:
            {
                EnableWindow(GetDlgItem(hWnd, IDOK), true);
            }break;

            case IDOK:
            {
                
                HWND hParentWnd = GetParent(hWnd);
                if (hParentWnd)
                {
                    Prefs.PowerOnTimeout = atoi(narrow(GetWndText(GetDlgItem(hWnd, IDC_TIMEOUT))).c_str());
                    Prefs.Logging = IsDlgButtonChecked(hWnd, IDC_LOGGING);
                    
                    bool TempAutoUpdate = IsDlgButtonChecked(hWnd, IDC_AUTOUPDATE);
                    if (TempAutoUpdate && !Prefs.AutoUpdate)
                    {
                        thread thread_obj(VersionCheckThread, hMainWnd);
                        thread_obj.detach();
                    }
                    Prefs.AutoUpdate = IsDlgButtonChecked(hWnd, IDC_AUTOUPDATE);
                    Prefs.BlankScreenWhenIdle = IsDlgButtonChecked(hWnd, IDC_CHECK_BLANK) == BST_CHECKED;
                    Prefs.BlankScreenWhenIdleDelay = atoi(narrow(GetWndText(GetDlgItem(hWnd, IDC_EDIT_BLANK))).c_str());

                    int count = ListView_GetItemCount(GetDlgItem(hWnd, IDC_LIST));
                    Prefs.EventLogRestartString.clear();
                    Prefs.EventLogShutdownString.clear();
                    for (int i = 0; i < count; i++)
                    {
                        vector<wchar_t> bufText(256);
                        wstring st;
                        ListView_GetItemText(GetDlgItem(hWnd, IDC_LIST), i, 0, &bufText[0], (int)bufText.size());
                        st = &bufText[0];
                        if (ListView_GetCheckState(GetDlgItem(hWnd, IDC_LIST), i))
                        {
                            Prefs.EventLogRestartString.push_back(narrow(st));
                        }
                        else
                        {
                            Prefs.EventLogShutdownString.push_back(narrow(st));
                        }
                    }
                    EndDialog(hWnd, 0);
                    EnableWindow(GetParent(hWnd), true);
                    EnableWindow(GetDlgItem(GetParent(hWnd), IDOK), true);
                }
            }break;

              case IDCANCEL:
            {
                  Prefs.ResetAPIkeys = false;

                  EndDialog(hWnd, 0);
                  EnableWindow(GetParent(hWnd), true);

            }break;
            default:break;
            }
        }break;
        case EN_CHANGE:
        {
            switch (LOWORD(wParam))
            {
            case IDC_TIMEOUT:
            {
                EnableWindow(GetDlgItem(hWnd, IDOK), true);
            }break;
            default:break;
            }
        }break;
        default:break;
        }
    }break;
    case WM_NOTIFY:
    {
        switch (((NMHDR*)lParam)->code)
        {
        case NM_CLICK:
        {
            //show log
            if (wParam == IDC_SYSLINK)
            {
                wstring str = DataPath;
                str += L"log.txt";
                ShellExecute(NULL, L"open", str.c_str(), NULL, DataPath.c_str(), SW_SHOW);
            }
            // explain the restart words
            if (wParam == IDC_SYSLINK3)
            {
                MessageBox(hWnd, L"This application rely on events in the windows event log to determine whether a reboot or shutdown was initiated by the user.\n\nThese events are localised in the language of your operating system, and the user must therefore assist with manually indicating which strings refers to the system restarting.\n\nPlease put a checkmark for every string which refers to 'restart'", L"Information", MB_OK | MB_ICONINFORMATION);
            }
        }break;
        case LVN_ITEMCHANGED:
        {
            if (wParam == IDC_LIST)
            {
                LPNMLISTVIEW pnmv = (LPNMLISTVIEW)lParam;
                if (pnmv->uChanged & LVIF_STATE) // item state has been changed  
                {
                    bool bPrevState = (((pnmv->uOldState & LVIS_STATEIMAGEMASK) >> 12) - 1) == 1 ? true : false;   // Old check box state
                    bool bChecked = (((pnmv->uNewState & LVIS_STATEIMAGEMASK) >> 12) - 1) == 1 ? true : false;

                    if (bPrevState == bChecked) // No change in check box
                        break;

                    EnableWindow(GetDlgItem(hWnd, IDOK), true);
                }
            }
        }break;
        default:break;
        }
    }break;

    case WM_CTLCOLORSTATIC:
    {

        HDC hdcStatic = (HDC)wParam;
        if ((HWND)lParam == GetDlgItem(hWnd, IDC_CHECK_ENABLE) )
        {
            SetBkMode(hdcStatic, TRANSPARENT);
        }
        return(INT_PTR)hBackbrush;

    }break;

    case WM_PAINT:
    {
        RECT rc = { 0 };

        GetClientRect(hWnd, &rc);

        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        FillRect(hdc, &rc, (HBRUSH)hBackbrush);

        EndPaint(hWnd, &ps);
    }break;

    case WM_CLOSE:
    {
        EndDialog(hWnd, 0);
    }break;

    default:break; 
    }
    return 0;
}

//   If the application is already running, send the command line parameters to that other process
bool MessageExistingProcess(wstring CmdLine)
{

    wstring WindowTitle;
    WindowTitle = APPNAME_FULL;
    WindowTitle += L" v";
    WindowTitle += APP_VERSION;
    wstring sWinSearch = WindowTitle;
    HWND Other_hWnd = FindWindow(NULL, sWinSearch.c_str());
    if (Other_hWnd)
    {
        COPYDATASTRUCT cds;
        cds.cbData = CmdLine == L"" ? 0 : (DWORD)(CmdLine.size() * sizeof(WCHAR) + sizeof(WCHAR));
        cds.lpData = CmdLine == L"" ? NULL : (PVOID)CmdLine.data();
        cds.dwData = NOTIFY_NEW_COMMANDLINE;
        SendMessage(Other_hWnd, WM_COPYDATA, NULL, (LPARAM)&cds);

        return true;
    }

    return false;
}
//   Read the configuration file into a json object and populate the preferences struct
bool ReadConfigFile()
{
    WCHAR szPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_COMMON_APPDATA, NULL, 0, szPath)))
    {
        wstring path = szPath;
        path += L"\\";
        path += APPNAME_FULL;
        path += L"\\";
        CreateDirectory(path.c_str(), NULL);

        DataPath = path;

        path += L"config.json";

        ifstream i(path.c_str());
        if (i.is_open())
        {
            json j;
            i >> jsonPrefs;
            i.close();

            j = jsonPrefs[JSON_PREFS_NODE][JSON_EVENT_RESTART_STRINGS];
            if (!j.empty() && j.size() > 0)
            {
                Prefs.EventLogRestartString.clear();
                for (auto& elem : j.items())
                    Prefs.EventLogRestartString.push_back(elem.value().get<string>());
            }

            j = jsonPrefs[JSON_PREFS_NODE][JSON_EVENT_SHUTDOWN_STRINGS];
            if (!j.empty() && j.size() > 0)
            {
                Prefs.EventLogShutdownString.clear();
                for (auto& elem : j.items())
                    Prefs.EventLogShutdownString.push_back(elem.value().get<string>());
            }

            j = jsonPrefs[JSON_PREFS_NODE][JSON_VERSION];
            if (!j.empty() && j.is_number())
                Prefs.version = j.get<int>();

            j = jsonPrefs[JSON_PREFS_NODE][JSON_PWRONTIMEOUT];
            if (!j.empty() && j.is_number())
                Prefs.PowerOnTimeout = j.get<int>();

            j = jsonPrefs[JSON_PREFS_NODE][JSON_LOGGING];
            if (!j.empty() && j.is_boolean())
                Prefs.Logging = j.get<bool>();
 
            j = jsonPrefs[JSON_PREFS_NODE][JSON_AUTOUPDATE];
            if (!j.empty() && j.is_boolean())
                Prefs.AutoUpdate = j.get<bool>();

            j = jsonPrefs[JSON_PREFS_NODE][JSON_IDLEBLANK];
            if (!j.empty() && j.is_boolean())
                Prefs.BlankScreenWhenIdle = j.get<bool>();

            j = jsonPrefs[JSON_PREFS_NODE][JSON_IDLEBLANKDELAY];
            if (!j.empty() && j.is_number())
                Prefs.BlankScreenWhenIdleDelay = j.get<int>();

            return true;
        }
    }
    return false;
}
//   Convert UTF-8 to wide
wstring widen(string sInput) {

    if (sInput == "")
        return L"";

    // Calculate target buffer size
    long len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, sInput.c_str(), (int)sInput.size(), NULL, 0);
    if (len == 0)
        return L"";

    // Convert character sequence
    wstring out(len, 0);
    if (len != MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, sInput.c_str(), (int)sInput.size(), &out[0], (int)out.size()))
        return L"";

    return out;
}
//   Convert wide to UTF-8
string narrow(wstring sInput) {

    // Calculate target buffer size
    long len = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, sInput.c_str(), (int)sInput.size(),
        NULL, 0, NULL, NULL);
    if (len == 0)
        return "";

    // Convert character sequence
    string out(len, 0);
    if (len != WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, sInput.c_str(), (int)sInput.size(), &out[0], (int)out.size(), NULL, NULL))
        return "";

    return out;
}
//   Add an event in the event log. Type can be: EVENTLOG_SUCCESS, EVENTLOG_ERROR_TYPE, EVENTLOG_INFORMATION_TYPE
void SvcReportEvent(WORD Type, wstring string)
{
    HANDLE hEventSource;
    LPCTSTR lpszStrings[2];

    hEventSource = RegisterEventSource(NULL, APPNAME_FULL);

    if (hEventSource)
    {
        wstring s;
        switch (Type)
        {
        case EVENTLOG_SUCCESS:
            s = string;
            s += L" succeeded";
            break;
        case EVENTLOG_INFORMATION_TYPE:
        default:
            s = string;
            break;
        }


        lpszStrings[0] = APPNAME_FULL;
        lpszStrings[1] = s.c_str();

        ReportEvent(hEventSource,        // event log handle
            Type,               // event type
            0,                   // event category
            0,                   // event identifier
            NULL,                // no security identifier
            2,                   // size of lpszStrings array
            0,                   // no binary data
            lpszStrings,         // array of strings
            NULL);               // no binary data

        DeregisterEventSource(hEventSource);
    }
}
//   Read device configuration and populate device object
void ReadDeviceConfig()
{
    if (jsonPrefs.empty())
        return;

    //Clear current sessions.  
    Devices.clear();

    //Iterate nodes
    for (const auto& item : jsonPrefs.items())
    {
        json j;
        if (item.key() == JSON_PREFS_NODE)
            break;

        SESSIONPARAMETERS params;

        params.DeviceId = item.key();

        if (item.value()["Name"].is_string())
            params.Name = item.value()["Name"].get<string>();

        if (item.value()["IP"].is_string())
            params.IP = item.value()["IP"].get<string>();

        if (item.value()["HDMIinputcontrol"].is_boolean())
            params.HDMIinputcontrol = item.value()["HDMIinputcontrol"].get<bool>();
        
        if (item.value()["OnlyTurnOffIfCurrentHDMIInputNumberIs"].is_number())
            params.OnlyTurnOffIfCurrentHDMIInputNumberIs = item.value()["OnlyTurnOffIfCurrentHDMIInputNumberIs"].get<int>();

         if (item.value()["Enabled"].is_boolean())
            params.Enabled = item.value()["Enabled"].get<bool>();

         if (item.value()["SessionKey"].is_string())
            params.SessionKey = item.value()["SessionKey"].get<string>();

        if (item.value()["Subnet"].is_string())
            params.Subnet = item.value()["Subnet"].get<string>();

        if (item.value()["WOL"].is_number())
            params.WOLtype = item.value()["WOL"].get<int>();

        if (item.value()["SetHDMIInputOnResume"].is_boolean())
            params.SetHDMIInputOnResume = item.value()["SetHDMIInputOnResume"].get<bool>();

        if (item.value()["SetHDMIInputOnResumeToNumber"].is_number())
            params.SetHDMIInputOnResumeToNumber = item.value()["SetHDMIInputOnResumeToNumber"].get<int>();

        j = item.value()["MAC"];
        if (!j.empty() && j.size() > 0)
        {
            for (auto& m : j.items())
            {
                params.MAC.push_back(m.value().get<string>());
            }
        }
        Devices.push_back(params);
    }
    return;
}
//   Get string from control
wstring GetWndText(HWND hWnd)
{
    int len = GetWindowTextLength(hWnd) + 1;
    vector<wchar_t> buf(len);
    GetWindowText(hWnd, &buf[0], len);
    wstring text = &buf[0];
    return text;
}
//   Split a string into a vecor of words
vector<string> stringsplit(string str, string token) {
    vector<string>res;
    while (str.size()) {
        int index = (int)str.find(token);
        if (index != string::npos) {
            res.push_back(str.substr(0, index));
            str = str.substr(index + token.size());
            if (str.size() == 0)res.push_back(str);
        }
        else {
            res.push_back(str);
            str = "";
        }
    }
    return res;
}
//   Send the commandline to the service
void CommunicateWithService(string cmdline)
{

    DWORD dwWritten;

    if (cmdline == "")
        return;

    hPipe = CreateFile(PIPENAME, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);

    if (hPipe != INVALID_HANDLE_VALUE)
    {
        WriteFile(hPipe,
            cmdline.c_str(),
            (DWORD)cmdline.length() + 1,   // = length of string + terminating '\0' !!!
            &dwWritten,
            NULL);
    }
    else
        MessageBox(hMainWnd, L"Failed to connect to named pipe. Service may be stopped.", L"Error", MB_OK| MB_ICONEXCLAMATION);

    if (hPipe != INVALID_HANDLE_VALUE)
        CloseHandle(hPipe);
}
//   Write the configuration file
void WriteConfigFile(void)
{
    json prefs;
    wstring path = DataPath;
    json p;

    CreateDirectory(DataPath.c_str(), NULL);
    path += L"config.json";

    // do we need to upgrade the api key version
    if (Prefs.ResetAPIkeys)
    {
        for (auto& k : Devices)
        {
            k.SessionKey = "";    
        }
        Prefs.ResetAPIkeys = false;
        Prefs.version = 2;
    }
    else
    {
        //load sessionkeys from config.json and add it to the device list
        ifstream i(path.c_str());
        if (i.is_open())
        {
            i >> p;
            i.close();

            for (const auto& item : p.items())
            {
                if (item.key() == JSON_PREFS_NODE)
                    break;

                json j;
                string key = "";

                if (item.value()["SessionKey"].is_string())
                    key = item.value()["SessionKey"].get<string>();

                j = item.value()["MAC"];
                if (!j.empty() && j.size() > 0)
                {
                    for (auto& m : j.items())
                    {
                        for (auto& k : Devices)
                        {
                            for (auto& l : k.MAC)
                            {
                                if (l == m.value().get<string>())
                                {
                                    k.SessionKey = key;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    prefs[JSON_PREFS_NODE][JSON_VERSION] = (int)Prefs.version;
    prefs[JSON_PREFS_NODE][JSON_PWRONTIMEOUT] = (int)Prefs.PowerOnTimeout;
    prefs[JSON_PREFS_NODE][JSON_LOGGING] = (bool)Prefs.Logging;
    prefs[JSON_PREFS_NODE][JSON_AUTOUPDATE] = (bool)Prefs.AutoUpdate;
    prefs[JSON_PREFS_NODE][JSON_IDLEBLANK] = (bool)Prefs.BlankScreenWhenIdle;
    prefs[JSON_PREFS_NODE][JSON_IDLEBLANKDELAY] = (int)Prefs.BlankScreenWhenIdleDelay;

    for (auto& item : Prefs.EventLogRestartString)
        prefs[JSON_PREFS_NODE][JSON_EVENT_RESTART_STRINGS].push_back(item);

    for (auto& item : Prefs.EventLogShutdownString)
        prefs[JSON_PREFS_NODE][JSON_EVENT_SHUTDOWN_STRINGS].push_back(item);

    //Iterate nodes
    int deviceid = 1;
    for (auto& item : Devices)
    {
        stringstream dev;
        dev << "Device";
        dev << deviceid;
        item.DeviceId = dev.str();

        prefs[dev.str()]["Name"] = item.Name;

        if(item.Name !="")
            prefs[dev.str()]["Name"] = item.Name;
        if (item.IP != "")
            prefs[dev.str()]["IP"] = item.IP;
        if (item.SessionKey != "")
            prefs[dev.str()]["SessionKey"] = item.SessionKey;
        else
            prefs[dev.str()]["SessionKey"] = "";

        prefs[dev.str()]["HDMIinputcontrol"] = (bool)item.HDMIinputcontrol;
        prefs[dev.str()]["OnlyTurnOffIfCurrentHDMIInputNumberIs"] = item.OnlyTurnOffIfCurrentHDMIInputNumberIs;

        prefs[dev.str()]["SetHDMIInputOnResume"] = (bool)item.SetHDMIInputOnResume;
        prefs[dev.str()]["SetHDMIInputOnResumeToNumber"] = item.SetHDMIInputOnResumeToNumber;


        if (item.Subnet != "")
            prefs[dev.str()]["Subnet"] = item.Subnet;
 
        prefs[dev.str()]["WOL"] = item.WOLtype;

        prefs[dev.str()]["Enabled"] = (bool)item.Enabled;

        for (auto& m : item.MAC)
            prefs[dev.str()]["MAC"].push_back(m);

 


        deviceid++;
    }
   
    if (!prefs.empty())
    {
        ofstream i(path.c_str());
        if (i.is_open())
        {
            i << setw(4) << prefs << endl;
            i.close();
        }
        jsonPrefs = prefs;
    }
    
}
// Discover the local host ip, e.g 192.168.1.x
vector <string> GetOwnIP(void)
{
    vector <string> IPs;
    char host[256];
    if (gethostname(host, sizeof(host)) != SOCKET_ERROR)
    {
        struct hostent* phent = gethostbyname(host);
        if (phent != 0)
        {
            for (int i = 0; phent->h_addr_list[i] != 0; ++i)
            {
                string ip;
                struct in_addr addr;
                memcpy(&addr, phent->h_addr_list[i], sizeof(struct in_addr));
                ip = inet_ntoa(addr);
                if(ip != "127.0.0.1")
                    IPs.push_back(ip);
            }
        }
    }
    return IPs;
}

void VersionCheckThread(HWND hWnd)
{
    IStream* stream;
    char buff[100];
    string s;
    unsigned long bytesRead;

    if (URLOpenBlockingStream(0, VERSIONCHECKLINK, &stream, 0, 0))
        return;// error

    while (true)
    {
        stream->Read(buff, 100, &bytesRead);

        if (0U == bytesRead)
            break;
        s.append(buff, bytesRead);

    };

    stream->Release();

    size_t find = s.find("\"tag_name\":", 0);
    if (find != string::npos)
    {
        size_t begin = s.find_first_of("0123456789", find);
        if (begin != string::npos)
        {
            size_t end = s.find("\"", begin);
            string lastver = s.substr(begin, end - begin);

            vector <string> local_ver = stringsplit(narrow(APP_VERSION), ".");
            vector <string> remote_ver = stringsplit(lastver, ".");

            if (local_ver.size() < 3 || remote_ver.size() < 3)
                return;
            int local_ver_major = atoi(local_ver[0].c_str());
            int local_ver_minor = atoi(local_ver[1].c_str());
            int local_ver_patch = atoi(local_ver[2].c_str());

            int remote_ver_major = atoi(remote_ver[0].c_str());
            int remote_ver_minor = atoi(remote_ver[1].c_str());
            int remote_ver_patch = atoi(remote_ver[2].c_str());


            if ((remote_ver_major > local_ver_major) ||
                (remote_ver_major == local_ver_major) && (remote_ver_minor > local_ver_minor) ||
                (remote_ver_major == local_ver_major) && (remote_ver_minor == local_ver_minor) && (remote_ver_patch > local_ver_patch))
            {
                PostMessage(hWnd, APP_NEW_VERSION, 0, 0);
            }
        }
    }
    return;
}