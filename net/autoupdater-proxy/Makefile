include $(TOPDIR)/rules.mk

PKG_NAME:=autoupdater-proxy
PKG_VERSION:=1
PKG_RELEASE:=1

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/autoupdater-proxy
  SECTION:=net
  CATEGORY:=Network
  TITLE:=Cgi script for proxying updates via neighbours
  # Pretty much a hack, but we don't have a cgi meta directory
  DEPENDS:=+gluon-status-page
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
endef

define Build/Compile
	mkdir -p $(PKG_BUILD_DIR)/luadest
	$(CP) ./luasrc/* $(PKG_BUILD_DIR)/luadest/
#	$(call GluonSrcDiet,./luasrc,$(PKG_BUILD_DIR)/luadest/)
endef

define Package/autoupdater-proxy/install
	$(INSTALL_DIR) $(1)/lib/gluon/status-page/www/cgi-bin/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/luadest/lib/gluon/status-page/www/cgi-bin/fwproxy $(1)/lib/gluon/status-page/www/cgi-bin/fwproxy
endef

$(eval $(call BuildPackage,autoupdater-proxy))
