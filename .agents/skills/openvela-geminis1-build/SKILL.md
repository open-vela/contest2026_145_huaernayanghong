---
name: openvela-geminis1-build
description: "OpenVela / 全志 R528-S3 (Gemini-S1 开发板) 编译与打包官方指引。涵盖环境依赖安装、Repo 代码同步、2.8寸/7寸屏编译命令以及 lichee 固件打包流程。"
---

# 开发编译环境准备 (OpenVela Gemini-S1 / 全志 R528-S3)

本指南严格根据润芯微官方开发编译文档整理，涵盖开发环境准备、代码获取、开发板编译与镜像打包全流程。

---

## 1. 依赖软件安装

### 1.1 系统要求
* **推荐系统**：Ubuntu 22.04 LTS (x86_64)
* **硬件配置建议**：16GB+ 内存，80GB+ 可用硬盘空间

### 1.2 安装基础依赖包
```bash
sudo apt-get update && sudo apt-get install -y \
  git-core gnupg flex bison gperf build-essential zip curl zlib1g-dev \
  libc6-dev-i386 libncurses5 lib32ncurses5-dev x11proto-core-dev libx11-dev \
  lib32z-dev ccache libgl1-mesa-dev libxml2-utils xsltproc unzip mtools \
  u-boot-tools python3 python-is-python3 python3-pip dosfstools pkg-config \
  libssl-dev libarchive-zip-perl libkmod-dev libmpc-dev libncurses-dev \
  libasound2-dev libmp3lame-dev libpulse-dev libv4l-dev libusb-1.0-0-dev \
  libxext-dev gcc-multilib g++-multilib genromfs texinfo xxd
```

### 1.3 安装与配置 Repo 工具
```bash
mkdir -p ~/bin
curl https://mirrors.tuna.tsinghua.edu.cn/git/git-repo > ~/bin/repo
chmod a+x ~/bin/repo
export PATH=~/bin:$PATH
export REPO_URL='https://mirrors.tuna.tsinghua.edu.cn/git/git-repo'
```

### 1.4 安装 Python 依赖模块
```bash
pip install kconfiglib pyelftools cxxfilt
```

---

## 2. 代码下载

可通过 Gitee、GitHub 或 GitCode 获取 OpenVela 代码：

```bash
# 初始化清单仓库 (以 Gitee 为例，使用 trunk 分支及 trunk-5.4.xml 发布标签)
repo init -u https://gitee.com/open-vela/manifest.git -b trunk -m tags/trunk-5.4.xml

# 同步代码
repo sync -c -j8
```

---

## 3. 编译

Gemini-S1 开发板提供两种屏幕方案配置：

### 3.1 2.8 寸屏 (SPI 屏幕)
* **配置目录**：`vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay/`
* **清理编译**：
  ```bash
  ./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay/ distclean -j8
  ```
* **日常编译**：
  ```bash
  ./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay/ -j8
  ```

### 3.2 7 寸屏 (MIPI 屏幕)
* **配置目录**：`vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh/`
* **清理编译**：
  ```bash
  ./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh/ -j8 distclean
  ```
* **日常编译**：
  ```bash
  ./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh/ -j8
  ```

---

## 4. 打包

完成固件编译后，进入打包目录生成全志烧录固件：

```bash
cd vendor/allwinnertech/lichee/
source envsetup.sh
lunch_nuttx
```
在交互式菜单提示中，输入对应编号选择 `r528s3-gemini-s1`（通常为 `2`）：
```
2
```

执行打包命令：
```bash
pack
```

* **打包产物输出路径**：
  `vendor/allwinnertech/lichee/out/r528s3/gemini-s1_nand`
