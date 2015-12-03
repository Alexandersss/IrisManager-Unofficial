#---------------------------------------------------------------------------------
# Clear the implicit built in rules
#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------
ifeq ($(strip $(PSL1GHT)),)
$(error "Please set PSL1GHT in your environment. export PSL1GHT=<path>")
endif

#---------------------------------------------------------------------------------
#  TITLE, APPID, CONTENTID, ICON0 SFOXML before ppu_rules.
#---------------------------------------------------------------------------------
TC_ADD		:=	`date +%d%H%M`
ICON0		:=	ICON0.PNG
ICON1		:=	ICON1.PAM
PIC1		:=	PIC1.PNG
SFOXML		:=	sfo.xml
EXPLOREPLG	:=	explore_plugin_450.sprx
EXPLOREPL2	:=	explore_plugin_430.sprx
EXPLOREPL3	:=	explore_plugin_446.sprx
LIBFS		:=	libfs_patched.sprx

# usage:  make BUILD_STEALTH=yes
ifndef BUILD_STEALTH
TITLE		:=	IrisManager - v2.68U Final ( Unofficial OpenModding )
APPID		:=	IMANGER4U
else
TITLE		:=	LEMMINGS� Trial Version
APPID		:=	NPUA80034
endif
CONTENTID	:=	UP0001-$(APPID)_00-0000000000000000
PKGFILES	:=	release

WITH_GAMES_DIR	?=	GAMEZ

SCETOOL_FLAGS	?=	--self-app-version=0001000000000000  --sce-type=SELF --compress-data=TRUE --self-add-shdrs=TRUE --skip-sections=FALSE --key-revision=1 \
					--self-auth-id=1010000001000003 --self-vendor-id=01000002 --self-fw-version=0003004000000000

include $(PSL1GHT)/ppu_rules

# aditional scetool flags (--self-ctrl-flags, --self-cap-flags...)
SCETOOL_FLAGS	+=	--self-ctrl-flags 4000000000000000000000000000000000000000000000000000000000000002
SCETOOL_FLAGS	+=	--self-cap-flags 00000000000000000000000000000000000000000000007B0000000100000000

#---------------------------------------------------------------------------------
# TARGET � il nome dell'output
# BUILD � la directory in cui verranno posizionati i file oggetto e file intermedi
# SOURCES  � una lista di directory che contiene il codice sorgente
# INCLUDES � una lista di directory che contengono i file di intestazione aggiuntivi
#---------------------------------------------------------------------------------
TARGET		:=	$(notdir $(CURDIR))
BUILD		:=	build
SOURCES		:=	source source/ftp
SOURCES		+=  source/payload341 source/payload355 source/payload355dex source/payload421 source/payload421dex source/payload441dex
SOURCES		+=  source/payload430 source/payload430dex source/payload431 source/payload440 source/payload441 
SOURCES		+=  source/payload446 source/payload446dex source/payload450
DATA		:=	datas
SHADERS		:=	shaders
INCLUDES	:=	include include/ftp
INCLUDES	+=  include/payload341 include/payload355 include/payload355dex include/payload421 include/payload421dex
INCLUDES	+=  include/payload430 include/payload430dex include/payload431 include/payload440 include/payload441 include/payload441dex
INCLUDES	+=  include/payload446 include/payload446dex include/payload450

#---------------------------------------------------------------------------------
# eventuali librerie aggiuntive che vogliamo collegare con il progetto
#---------------------------------------------------------------------------------
LIBS		:=	 -lfreetype -lz -ltiny3d -lnetctl -lnet -lsysfs -lpngdec -ljpgdec -lsimdmath -lgcm_sys -lio -lsysutil -lrt -llv2 -lsysmodule \
			-laudioplayer -lmod -lspu_sound -laudio -lm $(PORTLIBS)/modules/spu_soundmodule.bin.a


#---------------------------------------------------------------------------------
# opzioni per la generazione del codice
#---------------------------------------------------------------------------------

CFLAGS		=	-O2 -Wall -mcpu=cell --std=gnu99 $(MACHDEP) $(INCLUDE)
CFLAGS		+=  `$(PORTLIBS)/bin/freetype-config --cflags`
CFLAGS		+= -D__MKDEF_MANAGER_DIR__="\"$(APPID)\"" -D__MKDEF_MANAGER_FULLDIR__="\"dev_hdd0/game/$(APPID)\""
CFLAGS		+=  -DUSE_MEMCPY_SYSCALL
CFLAGS		+=  -DUSE_DISC_CALLBACK
CFLAGS		+=	-D'__MKDEF_GAMES_DIR="$(WITH_GAMES_DIR)"'


CXXFLAGS	=	$(CFLAGS)


LDFLAGS		=	$(MACHDEP) -Wl,-Map,$(notdir $@).map

#---------------------------------------------------------------------------------
# elenco di directory che contengono le librerie, questo deve essere il livello 
# superiore che contiene include e lib
#---------------------------------------------------------------------------------
LIBDIRS	:=

#---------------------------------------------------------------------------------
# non c'� alcuna reale necessit� di modificare niente oltre questo punto se 
# non � necessario aggiungere ulteriore regole per le estensioni di file diversi
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT	:=	$(CURDIR)/$(TARGET)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
					$(foreach dir,$(DATA),$(CURDIR)/$(dir)) \
					$(foreach dir,$(SHADERS),$(CURDIR)/$(dir))

export DEPSDIR	:=	$(CURDIR)/$(BUILD)

export BUILDDIR	:=	$(CURDIR)/$(BUILD)

#---------------------------------------------------------------------------------
# creare automaticamente un elenco di file oggetto per il nostro progetto
#---------------------------------------------------------------------------------
CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
sFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.S)))
BINFILES	:= $(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.bin)))
VCGFILES	:=	$(foreach dir,$(SHADERS),$(notdir $(wildcard $(dir)/*.vcg)))
FCGFILES	:=	$(foreach dir,$(SHADERS),$(notdir $(wildcard $(dir)/*.fcg)))

VPOFILES	:=	$(VCGFILES:.vcg=.vpo)
FPOFILES	:=	$(FCGFILES:.fcg=.fpo)

#---------------------------------------------------------------------------------
# usa CXX per il collegamento con C++ projects, CC per standard C
#---------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
	export LD	:=	$(CC)
else
	export LD	:=	$(CXX)
endif

export OFILES	:=	$(addsuffix .o,$(BINFILES)) \
					$(addsuffix .o,$(VPOFILES)) \
					$(addsuffix .o,$(FPOFILES)) \
					$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) \
					$(sFILES:.s=.o) $(SFILES:.S=.o)

#---------------------------------------------------------------------------------
# costruisce un elenco di percorsi di inclusione
#---------------------------------------------------------------------------------
export INCLUDE	:=	$(foreach dir,$(INCLUDES), -I$(CURDIR)/$(dir)) \
					$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
					$(LIBPSL1GHT_INC) \
					-I$(CURDIR)/$(BUILD) -I$(PORTLIBS)/include -I$(PORTLIBS)/modules

#---------------------------------------------------------------------------------
# costruisce un elenco di percorsi di libreria
#---------------------------------------------------------------------------------
export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib) \
					$(LIBPSL1GHT_LIB) -L$(PORTLIBS)/lib

export OUTPUT	:=	$(CURDIR)/$(TARGET)
.PHONY: $(BUILD) clean


#---------------------------------------------------------------------------------
$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

#---------------------------------------------------------------------------------
clean:
	@echo ----------------------------------------
	@echo Rimozione file temporaneai in corso...
	@echo ----------------------------------------
	@rm -fr $(BUILD) $(OUTPUT).elf $(OUTPUT).self EBOOT.BIN

#---------------------------------------------------------------------------------
run:
	ps3load $(OUTPUT).self

#---------------------------------------------------------------------------------
pkg: $(BUILD) #$(OUTPUT).pkg
	@$(MAKE) --no-print-directory -C $(CURDIR)/loader -f $(CURDIR)/loader/Makefile npdrm
	$(VERB) echo building pkg ... $(notdir $@)
	$(VERB) mkdir -p $(BUILDDIR)/pkg/USRDIR
	$(VERB) cp $(ICON0) $(BUILDDIR)/pkg/ICON0.PNG
	$(VERB) cp $(EXPLOREPLG) $(BUILDDIR)/pkg/explore_plugin_450.sprx
	$(VERB) cp $(EXPLOREPL2) $(BUILDDIR)/pkg/explore_plugin_430.sprx
	$(VERB) cp $(EXPLOREPL3) $(BUILDDIR)/pkg/explore_plugin_446.sprx
	$(VERB) cp $(LIBFS) $(BUILDDIR)/pkg/libfs_patched.sprx
	$(VERB) cp -f $(CURDIR)/loader/EBOOT.BIN $(BUILDDIR)/pkg/USRDIR/EBOOT.BIN
	$(VERB) cp -f $(CURDIR)/$(TARGET).self $(BUILDDIR)/pkg/USRDIR/iris_manager.self
	$(VERB) $(SFO) --title "$(TITLE)" --appid "$(APPID)" -f $(SFOXML) $(BUILDDIR)/pkg/PARAM.SFO
	$(VERB) if [ -n "$(PKGFILES)" -a -d "$(PKGFILES)" ]; then cp -rf $(PKGFILES)/* $(BUILDDIR)/pkg/; fi
	$(VERB) $(PKG) --contentid $(CONTENTID) $(BUILDDIR)/pkg/ $(TARGET).pkg >> /dev/null

#---------------------------------------------------------------------------------

pkg2: $(BUILD)
	@$(MAKE) --no-print-directory -C $(CURDIR)/loader -f $(CURDIR)/loader/Makefile npdrm
	$(VERB) echo building pkg ... $(notdir $@)
	$(VERB) mkdir -p $(BUILDDIR)/pkg2/USRDIR
	$(VERB) cp $(ICON0) $(BUILDDIR)/pkg2/ICON0.PNG
	$(VERB) cp $(ICON1) $(BUILDDIR)/pkg2/ICON1.PAM
	$(VERB) cp $(PIC1) $(BUILDDIR)/pkg2/PIC1.PNG
	$(VERB) cp -f $(CURDIR)/loader/EBOOT.BIN $(BUILDDIR)/pkg2/USRDIR/EBOOT.BIN
	$(VERB) cp -f $(CURDIR)/$(TARGET).self $(BUILDDIR)/pkg2/USRDIR/iris_manager.self
	$(VERB) $(SFO) --title "$(TITLE)" --appid "$(APPID)" -f $(SFOXML) $(BUILDDIR)/pkg2/PARAM.SFO
	$(VERB) if [ -n "$(PKGFILES)" -a -d "$(PKGFILES)" ]; then cp -rf $(PKGFILES)/* $(BUILDDIR)/pkg2/; fi
	$(VERB) $(PKG) --contentid $(CONTENTID) $(BUILDDIR)/pkg2/ $(TARGET)_animated.pkg >> /dev/null
#---------------------------------------------------------------------------------

npdrm: $(BUILD)
	@$(SELF_NPDRM) $(SCETOOL_FLAGS) --np-content-id=$(CONTENTID) --encrypt $(BUILDDIR)/$(basename $(notdir $(OUTPUT))).elf $(BUILDDIR)/../EBOOT.BIN

#---------------------------------------------------------------------------------

else

DEPENDS	:=	$(OFILES:.o=.d)

#---------------------------------------------------------------------------------
# obiettivi principali
#---------------------------------------------------------------------------------
$(OUTPUT).self: $(OUTPUT).elf
$(OUTPUT).elf:	$(OFILES)

#---------------------------------------------------------------------------------
# Questo link regola in dati binari con estensione. bin
#---------------------------------------------------------------------------------
%.bin.o	:	%.bin
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(bin2o)

#---------------------------------------------------------------------------------
%.vpo.o	:	%.vpo
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(bin2o)

#---------------------------------------------------------------------------------
%.fpo.o	:	%.fpo
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(bin2o)

-include $(DEPENDS)

#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------
