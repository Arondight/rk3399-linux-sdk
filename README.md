# rk3399-linux-sdk

## About

RockChip RK3399 Linux SDK (RK3399_LINUX_SDK_RELEASE_V2.5.1_20201203).

## Build Images

Install and run [Ubuntu 20.04 LTS](https://mirrors.ustc.edu.cn/ubuntu-releases/20.04.6/ubuntu-20.04.6-desktop-amd64.iso), upgrade whole system.

```shell
sudo apt update
sudo apt upgrade
sudo reboot
```

Clone this repo.

```shell
git clone https://github.com/Arondight/rk3399-linux-sdk.git
cd ./rk3399-linux-sdk/
```

Install extra build requires.

```shell
sudo apt install python
sudo apt install ./debian/ubuntu-build-service/packages/live-build_3.0.5-1linaro1_all.deb
```

Read documention [Rockchip RK3399 Linux SDK Quick Start](docs/Rockchip_RK3399_Quick_Start_Linux_EN.pdf) to build images, and find these images in `./IMAGE/*/IMAGES/`.

> <details>
>   <summary>
>     For a successful build you may need to set up a proxy to download packages from GitHub (or other sites).
>   </summary>
>
> ```shell
> export http_proxy='http://localhost:10809'
> export https_proxy="$http_proxy"
> ```
>
> </details>
