# BUILD INSTRUCTIONS AND DEPENDENCIES
To build the projects (UI, Service, Daemon, Console and Installer) in Visual Studio 2022 please ensure that Vcpkg (https://github.com/microsoft/vcpkg) is installed. Vcpkg is a free library Manager for Windows. You should use Vcpkg in one of two ways:

### Manifest
A Vcpkg manifest is included with the source code and the necessary dependencies will be automatically downloaded, configured and installed, if you choose to enable it. To enable the manifest please open the properties for each project in the solution, then 
navigate to the vcpkg section and Select "Yes" for "Use Manifest". Do so for the "Debug" and "Release" project configurations and X64 and ARM64 Platforms.

### Manual
Alternatively, You can manually install the dependencies, with the following commands:

		vcpkg install nlohmann-json:x64-windows-static
		vcpkg install boost-asio:x64-windows-static
		vcpkg install boost-utility:x64-windows-static
		vcpkg install boost-date-time:x64-windows-static
		vcpkg install boost-beast:x64-windows-static
		vcpkg install wintoast:x64-windows-static
		vcpkg install openssl:x64-windows-static
  
  		vcpkg install nlohmann-json:arm64-windows-static
		vcpkg install boost-asio:arm64-windows-static
		vcpkg install boost-utility:arm64-windows-static
		vcpkg install boost-date-time:arm64-windows-static
		vcpkg install boost-beast:arm64-windows-static
		vcpkg install wintoast:arm64-windows-static
		vcpkg install openssl:arm64-windows-static

### Building the setup package
To build the setup package please ensure that the [Heatwave](https://www.firegiant.com/docs/heatwave/) for VS2022 extension is installed.
