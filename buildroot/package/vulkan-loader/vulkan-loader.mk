################################################################################
#
# vulkan-loader
#
################################################################################

VULKAN_LOADER_VERSION = 1.3.214
VULKAN_LOADER_SITE = $(call github,KhronosGroup,Vulkan-Loader,v$(VULKAN_LOADER_VERSION))
VULKAN_LOADER_LICENSE = Apache-2.0
VULKAN_LOADER_LICENSE_FILES = LICENSE.txt
VULKAN_LOADER_INSTALL_STAGING = YES

VULKAN_LOADER_DEPENDENCIES = vulkan-headers

VULKAN_LOADER_CONF_OPTS = -DASSEMBLER_WORKS=FALSE

ifeq ($(BR2_PACKAGE_XORG7),)
VULKAN_LOADER_CONF_OPTS += \
	-DBUILD_WSI_XCB_SUPPORT=OFF -DBUILD_WSI_XLIB_SUPPORT=OFF
else
VULKAN_LOADER_CONF_OPTS += \
	-DBUILD_WSI_XCB_SUPPORT=ON -DBUILD_WSI_XLIB_SUPPORT=ON
VULKAN_LOADER_DEPENDENCIES += libxcb xlib_libX11 xlib_libXrandr
endif

ifeq ($(BR2_PACKAGE_WAYLAND),)
VULKAN_LOADER_CONF_OPTS += -DBUILD_WSI_WAYLAND_SUPPORT=OFF
else
VULKAN_LOADER_CONF_OPTS += -DBUILD_WSI_WAYLAND_SUPPORT=ON
VULKAN_LOADER_DEPENDENCIES += wayland
endif

$(eval $(cmake-package))
