include $(TOPDIR)/rules.mk

PKG_NAME:=respondd-module-mesh
PKG_VERSION:=1
PKG_RELEASE:=1

PKG_LICENSE:=BSD-2-Clause

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)
PKG_BUILD_DEPENDS := respondd libgluonutil libubus libmnl

include $(INCLUDE_DIR)/package.mk

define Package/respondd-module-mesh
  SECTION:=net
  CATEGORY:=Network
  TITLE:=respondd-module-mesh
  DEPENDS:=+respondd +libmnl +libgluonutil +libubox +libubus
endef

define Package/respondd-module-mesh/description
	Adds support for discovering mac adresses of local mesh devices
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Configure
endef


TARGET_CFLAGS += -I$(STAGING_DIR)/usr/include/libmnl

define Build/Compile
	CFLAGS="$(TARGET_CFLAGS)" CPPFLAGS="$(TARGET_CPPFLAGS)" $(MAKE) -C $(PKG_BUILD_DIR) $(TARGET_CONFIGURE_OPTS)
endef


define Package/respondd-module-mesh/install
	$(INSTALL_DIR) $(1)/usr/lib/respondd
	$(CP) $(PKG_BUILD_DIR)/respondd.so $(1)/usr/lib/respondd/mesh.so
endef

$(eval $(call BuildPackage,respondd-module-mesh))
