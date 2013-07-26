on run args
  set thePluginName to (item 1 of args)
  set theInstallerName to (item 2 of args)
  tell application "Finder"
    tell disk theInstallerName
      open
      set current view of container window to icon view
      set toolbar visible of container window to false
      set statusbar visible of container window to false
      set the bounds of container window to {200, 100, 480, 324}
      set opts to the icon view options of container window
      set background picture of opts to file ".background:background.png"
      set arrangement of opts to not arranged
      set icon size of opts to 80
      #setting positions don't seem to work
      #set position of item thePluginName of container window to {48, 60}
      #set position of item "Plugins" of container window to {248, 60}
      update without registering applications
      delay 3
      eject
    end tell
  end tell
end run
