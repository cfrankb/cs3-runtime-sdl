APP_NAME = cs3-runtime
EXECUTABLE = ../build/${APP_NAME}
DATAPATH = ../data/*
APPDIR = $(APP_NAME).AppDir
APP_DATA = $(APPDIR)/usr/share/data
APP_ID = com.cfrankb.$(APP_NAME)
APP_ICON = data/${APP_NAME}.png
DESKTOP_FILE = data/$(APP_ID).desktop
APP_DESKTOP_FILE = $(APPDIR)/usr/share/applications/$(APP_ID).desktop


# Rule to create the AppImage
appimage: clean_appimage $(EXECUTABLE)
	# Create the basic AppDir structure
	mkdir -p $(APPDIR)/usr/share/data
	mkdir -p $(APPDIR)/usr/bin
	mkdir -p $(APPDIR)/usr/lib
	mkdir -p $(APPDIR)/usr/share/applications
	mkdir -p $(APPDIR)/usr/share/metainfo
	mkdir -p $(APPDIR)/usr/share/icons/hicolor/256x256/apps # Or other sizes

	# Copy your executable into the AppDir
	cp $(EXECUTABLE) $(APPDIR)/usr/bin/

	# Copy essential assets (e.g., res/ folder)
	cp -r ${DATAPATH} ${APP_DATA} # Put assets next to executable, or in a specific data folder
	cp data/com.cfrankb.cs3-runtime.appdata.xml $(APPDIR)/usr/share/metainfo

	# Create a .desktop file for your application (important for desktop integration)
	# This file describes your application to the desktop environment.
	# It should point to your executable relative to the AppDir's usr/bin.
	cp $(DESKTOP_FILE) $(APP_DESKTOP_FILE)

	# Copy your application icon (used by the .desktop file)
	cp $(APP_ICON) $(APPDIR)/usr/share/icons/hicolor/256x256/apps/$(APP_NAME).png
	cp $(APP_ICON) $(APPDIR)/ # Also copy to the root of AppDir for the AppImage icon

	# Run linuxdeploy to bundle dependencies and create the AppImage
	# --appdir: Path to your AppDir
	# --executable: Path to your main executable inside the AppDir
	# --output appimage: Specify AppImage as output format
	# --desktop-file: Path to your .desktop file inside the AppDir
	# --icon: Path to your icon file inside the AppDir
	# --library-path: Hint to linuxdeploy where to find specific libraries (optional, but good for SDL)
	LD_LIBRARY_PATH=local/std/lib:$LD_LIBRARY_PATH bin/linuxdeploy-x86_64.AppImage --appimage-extract-and-run --appdir $(APPDIR) \
	              --executable $(APPDIR)/usr/bin/${APP_NAME} \
	              --output appimage \
	              --desktop-file $(APP_DESKTOP_FILE) \
	              --icon-file $(APP_ICON)
	ls -la .
	#--appimage-extract

clean_appimage:
	rm -rf $(APPDIR) $(APP_NAME)-*.AppImage
