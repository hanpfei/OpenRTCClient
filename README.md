这个仓库包含了 WebRTC 的源码，构建 WebRTC SDK 所需的大部分构建工具，如 LLVM，以及一套主要用 Python 编写的简单构建系统。

这套构建系统支持在 Mac 平台上为 Mac、iOS 和 Android 编译 WebRTC SDK，支持在 Windows 平台上为 Windows 编译 WebRTC SDK，支持在 Linux 平台上为 Linux 和 Android 编译 WebRTC SDK。

构建系统中预埋了大部分构建过程所需的工具，WebRTC 依赖的许多 Git Repo 也以源码的形式提交进了 webrtc/ 目录下，因而通过这套构建系统无需执行 `gclient` 等命令来同步依赖。这套构建系统用一个统一的命令 `webrtc_build` 作为入口，以一种统一的方式支持各个主机平台上不同目标系统平台 WebRTC SDK 的构建。

在运行 `webrtc_build` 命令之前，首先需要为它准备构建环境，包括 Python3 安装，IDE 和构建工具安装等，如通过原生的 WebRTC 构建系统来构建那样。当在不同主机平台上为不同目标系统平台构建 WebRTC SDK 时，所要做的环境准备也不尽相同。

## 在 Mac 平台上为 Mac 编译 WebRTC SDK 准备

1. 安装 Git。

2. 安装 Xcode 9 及以上，建议安装最新版 Xcode。

3. 安装 Python3。

4. 安装 depot_tools，并将其路径添加进 PATH 环境变量。

(1). 下载 depot_tools，如在 `~/bin` 目录下：
```
bin % git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
```

(2). 修改 `~/.zshrc` （假设用的 shell 为 zshell，如果用 bash，修改 `~/.bashrc`），添加如下行：
```
export PATH=~/bin/depot_tools:$PATH
```

更多详情，请参考 [Prerequisite Software](https://webrtc.github.io/webrtc-org/native-code/development/prerequisite-sw/)

## 在 Mac 平台上为 iOS 编译 WebRTC SDK 准备

同上 ***在 Mac 平台上为 Mac 编译 WebRTC SDK 准备***。

## 在 Mac 平台上为 Android 编译 WebRTC SDK 准备

首先完成 ***在 Mac 平台上为 Mac 编译 WebRTC SDK 准备***。为 Android 编译 WebRTC 需要一些额外的工具：

1. 安装 JDK 8

当前 WebRTC 的大部分代码的编译通过 Java 11 完成，但部分 Java 工具的执行需要 Java 8 环境。

在 [Oracle 网站](https://www.oracle.com/java/technologies/downloads/#java8-mac) 下载 JDK 8，安装 JDK 8，并修改 `~/.zshrc` （假设用的 shell 为 zshell，如果用 bash，修改 `~/.bashrc`），添加环境变量 `JAVA_8_HOME` 指向 JDK 8 的安装路径，如：
```
export JAVA_8_HOME=/Library/Java/JavaVirtualMachines/jdk1.8.0_311.jdk/Contents/Home
```

2. 安装 JDK 11

在 [Oracle 网站](https://www.oracle.com/java/technologies/downloads/#java11-mac) 下载 JDK 11，安装 JDK 11，并修改 `~/.zshrc` （假设用的 shell 为 zshell，如果用 bash，修改 `~/.bashrc`），添加环境变量 `JAVA_HOME` 指向 JDK 11 的安装路径，如：
```
export JAVA_HOME=/Library/Java/JavaVirtualMachines/jdk-11.0.13.jdk/Contents/Home
```

更多详情，可参考 [Mac OS 安装 Java JDK 11](https://www.jianshu.com/p/6273e7ab0c56)。

3. 安装 Android SDK

可通过 Android Studio 安装 Android SDK，并修改 `~/.zshrc` （假设用的 shell 为 zshell，如果用 bash，修改 `~/.bashrc`），添加环境变量 `ANDROID_SDK` 指向 Android SDK 的安装路径，如：
```
export ANDROID_SDK=~/Library/Android/sdk
```

4. 安装 Android NDK

可通过 Android Studio 安装 Android NDK，也可以直接下载 NDK 安装包，并修改 `~/.zshrc` （假设用的 shell 为 zshell，如果用 bash，修改 `~/.bashrc`），添加环境变量 `ANDROID_NDK` 指向 Android NDK 的安装路径，Android NDK 的安装路径可以是具体的某个版本的 NDK 的安装路径，如：
```
export ANDROID_NDK=~/Library/Android/sdk/ndk/23.1.7779620
```

也可以是 Android Studio 的 NDK 集的路径，如：
```
export ANDROID_NDK=~/Library/Android/sdk/ndk/
```

## 在 Linux 平台上为  Linux 编译 WebRTC SDK 准备

1. 安装 Git。

2. 安装 Python3。

3. 安装 Python 2.7。

构建系统的大部分脚本需要 Python 3 来执行，但有少部分脚本仍然依赖 Python 2.7。

4. 安装 depot_tools，并将其路径添加进 PATH 环境变量。

(1). 下载 depot_tools，如在 `~/bin` 目录下：
```
bin $ git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
```

(2). 修改 `~/.bashrc`，添加如下行：
```
export PATH=~/bin/depot_tools:$PATH
```

5. 执行 `webrtc/build/install-build-deps.sh` 脚本安装其它依赖，如：
```
$ sudo bash webrtc/build/install-build-deps.sh
```

更多详情可参考 [Prerequisite Software](https://webrtc.github.io/webrtc-org/native-code/development/prerequisite-sw/) 和 [Checking out and building Chromium on Linux](https://chromium.googlesource.com/chromium/src/+/master/docs/linux/build_instructions.md) 等。

## 在 Linux 平台上为  Android 编译 WebRTC SDK 准备

首先执行 ***在 Linux 平台上为  Linux 编译 WebRTC SDK 准备***，然后为 Android 构建安装额外的工具。

1. 安装 JDK 8

```
$ sudo apt-get install openjdk-8-jdk
```

修改 `~/.bashrc`，添加环境变量 `JAVA_8_HOME` 指向 JDK 8 的安装路径，如：
```
export JAVA_8_HOME=/usr/lib/jvm/java-8-openjdk-amd64
```

2. 安装 JDK 11

```
$ sudo apt-get install openjdk-11-jdk
```

修改 `~/.bashrc`，添加环境变量 `JAVA_HOME` 指向 JDK 11 的安装路径，如：
```
export JAVA_HOME=/usr/lib/jvm/java-11-openjdk-amd64
```

3. 安装 Android SDK

可通过 Android Studio 安装 Android SDK，修改 `~/.bashrc`，添加环境变量 `ANDROID_SDK` 指向 Android SDK 的安装路径，如：
```
export ANDROID_SDK=~/data/tools/development_tools/android/Sdk
```

4. 安装 Android NDK

可通过 Android Studio 安装 Android NDK，也可以直接下载 NDK 安装包，并修改 `~/.bashrc`，添加环境变量 `ANDROID_NDK` 指向 Android NDK 的安装路径，Android NDK 的安装路径可以是具体的某个版本的 NDK 的安装路径，如：
```
export ANDROID_NDK=~/data/tools/development_tools/android/android-ndk-r21e
```

也可以是 Android Studio 的 NDK 集的路径，如：
```
export ANDROID_NDK=~/Library/Android/sdk/ndk/
```

## 在 Windows 平台上为  Windows 编译 WebRTC SDK 准备

1、 安装 depot_tools，并将 depot_tools 的路径设置到环境变量 `PATH` 中。设置到用户环境变量中就可以了，避免重启。安装请参考：[depot_tools](https://commondatastorage.googleapis.com/chrome-infra-docs/flat/depot_tools/docs/html/depot_tools_tutorial.html#_setting_up)

2、使用管理员权限打开 cmd.ext，并执行 gclient(不带任何参数)，第一次执行时，gclient将安装 msysgit 和 python 在内的 Windows相关的依赖。

3、 下载 [Visual Studio 2019 社区版](https://docs.microsoft.com/zh-cn/visualstudio/releases/2019/release-notes)，用 visual studio 2019 的安装程序执行下面的命令，如：
```
 $ vs_community__de2da9e0d3114609b6724eaa8f0e96c4.exe ^
 --add Microsoft.VisualStudio.Workload.NativeDesktop ^
 --add Microsoft.VisualStudio.Component.VC.ATLMFC ^
 --add Microsoft.VisualStudio.Component.VC.Tools.ARM64 ^
 --add Microsoft.VisualStudio.Component.VC.MFC.ARM64 ^
 --includeRecommended
```

设置环境变量 `vs2019_install` 指向 Visual Studio 2019 的安装路径，如 `set vs2019_install=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community`。

4、在 windows 的程序和功能里找到 window software development kit，选中后点上面的更改，把 debugging tools for windows 前面的勾打上。

5、另外需要添加两个环境变量
```
DEPOT_TOOLS_WIN_TOOLCHAIN=0
WINDOWSSDKDIR=c:\Program Files (x86)\Windows Kits\10
```

其中 `WINDOWSSDKDIR` 需要只想 Windows 10 SDK 的安装路径，请按自己的实际情况来。

## `webrtc_build` 命令的基本使用

1. clone OpenRTCClient，如：
```
 % git clone git@github.com:hanpfei/OpenRTCClient.git
```

2. 进入 OpenRTCClient 目录，并切换到 OpenRTCClient 的 `m98_4758` branch，如:
```
OpenRTCClient %  git checkout -t origin/m98_4758
```

3. 将 OpenRTCClient/build_system 的路径添加进环境变量 PATH 里

`webrtc_build` 命令的用法如下：
```
OpenRTCClient % webrtc_build 

Must provide build_command in command line.

Usage: webrtc_build command[:target] target_os target_cpu [build_type] [build_options_set] [dir] [generator]
	 command:      gen|build|clean|info
	 target_os:    win|mac|ios|android|linux
	 target_cpu:   x86|x64|arm|arm64
	 build_type:   debug|release, by default is release
	 options_set:  build_options_set name you want to build, by default the file name is "default.conf"
	 dir:          output directory, by default "build" dir under current directory
	 generator     cmake|make|vs|xcode

It is strongly recommended that you put "~/OpenRTCClient/build_system"
into your system environment variable PATH so that you can run webrtc_build anywhere
```

4. 执行 gen

如在 OpenRTCClient 目录下执行：
```
webrtc_build gen win x64 debug
```

5. 执行 build

如在 OpenRTCClient 目录下执行：
```
webrtc_build build win x64 debug
```

或者，如对于 Android，在 OpenRTCClient 目录下执行：
```
webrtc_build gen android arm64 debug
webrtc_build build:webrtc android arm64 debug
```

生成的二进制文件将位于 OpenRTCClient/build 目录下，如对于 Windows 来说，位于 `OpenRTCClient/build/win/x64/debug` 目录下。

编译生成的静态库位于 `OpenRTCClient/build/win/x64/debug/obj` 目录下。

对于 iOS：
```
% webrtc_build gen ios arm64 debug
% webrtc_build build:webrtc ios arm64 debug
```
生成的静态库为 `OpenRTCClient/build/ios/arm64/debug/obj/libwebrtc.a`。

## 为 iOS 和 Mac 生成多架构静态库包

为 iOS 和 Mac 生成多架构静态库包可以通过 `webrtc_pack` 命令实现。在这个命令中，将自动通过 `gn` 和 `ninja` 为多个架构编译生成静态库和动态库，并通过 `libtool` 和 `lipo` 等工具生成包含多个架构的目标文件的静态库文件。 

`webrtc_pack` 命令的用法如下：
```
Usage: webrtc_pack target_os build_type
     target_os:    win|mac|ios|android|linux
     build_type:   debug|release, by default is release
```

如，为 iOS 生成 debug 版的包：
```
% webrtc_pack ios debug
```

生成的静态库的包将位于 `build/ios/debug/libwebrtc.a`。
