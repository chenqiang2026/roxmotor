# This is an automatically generated record.
# The area between QNX Internal Start and QNX Internal End is controlled by
# the QNX IDE properties.

ifndef QCONFIG
QCONFIG=qconfig.mk
endif
include $(QCONFIG)

#===== include make definitions and macros
include $(PLATFORM_ROOT)/core_amss_def.mk

PSTAG_64 = .64

#===== NAME - name of the project (default - name of project directory).
NAME=screen_firmware_update

#===== USEFILE - the file containing the usage message for the application. 
USEFILE=

define PINFO
PINFO DESCRIPTION=
endef

#===== EXTRA_SRCVPATH - a space-separated list of directories to search for source files.
EXTRA_SRCVPATH+=$(PROJECT_ROOT)/src

#===== EXTRA_LIBVPATH - a space-separated list of directories to search for library files.
EXTRA_LIBVPATH+=$(INSTALLDIR_S8155_LIB)                                             \
	$(INSTALL_ROOT_nto)/aarch64le/yf/usr/lib64

#===== LIBS - a space-separated list of library items to be included in the link.
LIBS+= gpio_clientS i2c_clientS slog2 dtc_cli

#===== INCVPATH - a space-separated list of directories to search for include files.
INCVPATH+= \
	$(PUBLIC_CORE_INC) \
	$(PROTECTED_CORE_INC) \
	$(AMSS_INC) \
	$(INSTALL_ROOT_nto)/usr/include/amss \
	$(RIM_BUILD_ROOT)/boards/yf/common/include

#===== INSTALLDIR - Subdirectory where the executable or library is to be installed. 
INSTALLDIR=$(INSTALLDIR_S8155_BIN)/../yf/usr/lib64


include $(MKFILES_ROOT)/qmacros.mk
ifndef QNX_INTERNAL
QNX_INTERNAL=$(BSP_ROOT)/.qnx_internal.mk
endif
include $(QNX_INTERNAL)

include $(MKFILES_ROOT)/qtargets.mk

OPTIMIZE_TYPE_g=none
OPTIMIZE_TYPE=$(OPTIMIZE_TYPE_$(filter g, $(VARIANTS)))

