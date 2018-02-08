include $(TOPDIR)/rules.mk

PKG_NAME:=libgluonutil
PKG_VERSION:=1
PKG_RELEASE:=1

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)
PKG_BUILD_DEPENDS := libubox libubus

include $(INCLUDE_DIR)/package.mk

define Package/libgluonutil
  SECTION:=libs
  CATEGORY:=Libraries
  TITLE:=libgluonutil
  DEPENDS:=+libubox +libubus
endef

define Package/libgluonutil/description
	C equivalent of util.lua from gluon package
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Configure
endef

define Build/Compile
	CFLAGS="$(TARGET_CFLAGS)" CPPFLAGS="$(TARGET_CPPFLAGS)" $(MAKE) -C $(PKG_BUILD_DIR) $(TARGET_CONFIGURE_OPTS)
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/include/libgluonutil-0
	$(CP) $(PKG_BUILD_DIR)/libgluonutil.h $(1)/usr/include/libgluonutil-0
	$(INSTALL_DIR) $(1)/usr/lib
	$(CP) $(PKG_BUILD_DIR)/libgluonutil.so.0 $(1)/usr/lib/
	ln -s $(PKG_BUILD_DIR)/libgluonutil.so.0 $(1)/usr/lib/libgluonutil.so
endef

define Package/libgluonutil/install
	$(INSTALL_DIR) $(1)/usr/lib
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/libgluonutil.so.0 $(1)/usr/lib/
endef

$(eval $(call BuildPackage,libgluonutil))
