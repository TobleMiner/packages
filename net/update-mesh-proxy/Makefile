include $(TOPDIR)/rules.mk

PKG_NAME:=update-mesh-proxy
PKG_VERSION:=1
PKG_RELEASE:=1

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/update-mesh-proxy
  SECTION:=gluon
  CATEGORY:=Gluon
  TITLE:=Cgi script for proxying updates via neighbours
  # Pretty much a hack, but we don't have a cgi meta directory
  DEPENDS:=+gluon-status-page-api
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(call Build/Compile/Default)
	$(call GluonSrcDiet,./luasrc,$(PKG_BUILD_DIR)/luadest/)
endef

define Package/update-mesh-proxy/install
	$(CP) $(PKG_BUILD_DIR)/luadest/* $(1)/
endef

$(eval $(call BuildPackage,update-mesh-proxy))
