LIST=screen_firmware_update
EARLY_DIRS=
LATE_DIRS=

# Bring in configuration macros which includes qconf-override.mk
ifndef QCONFIG
QCONFIG=qconfig.mk
endif
include $(QCONFIG)

#===== include make definitions and macros
include $(PLATFORM_ROOT)/core_amss_def.mk

# include makefile to stage the protected files PROTECTED_MAKE searches for protected folder and stages to $(INSTALL_ROOT_PHDR). 
# defined at the top of core bsp as: INSTALL_ROOT_PHDR=$(PROTECTED_CORE_INC)
include $(PROTECTED)
# Note: $(PROTECTED) includes recurse.mk so DO NOT INCLUDE recurse.mk in this file.
