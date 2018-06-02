#!/usr/bin/python

# this script will create/update info plist files based on config.h

kAudioUnitType_MusicDevice      = "aumu"
kAudioUnitType_MusicEffect      = "aumf"
kAudioUnitType_Effect           = "aufx"

import plistlib, os, datetime, fileinput, glob, sys, string, shutil

scriptpath = os.path.dirname(os.path.realpath(__file__))
projectpath = os.path.abspath(os.path.join(scriptpath, os.pardir))

sys.path.insert(0, projectpath + '/../../scripts/')

from parse_config import parse_config, parse_xcconfig

def main():
  config = parse_config(projectpath)
  xcconfig = parse_xcconfig(projectpath + '/../../common-ios.xcconfig')

  CFBundleGetInfoString = config['BUNDLE_NAME'] + " v" + config['FULL_VER_STR'] + " " + config['PLUG_COPYRIGHT']
  CFBundleVersion = config['FULL_VER_STR']
  CFBundlePackageType = "BNDL"
  CSResourcesFileMapped = True
  LSMinimumSystemVersion = xcconfig['DEPLOYMENT_TARGET']

  print "Processing Info.plist files..."

# AUDIOUNIT v3

  if config['PLUG_IS_INSTRUMENT']:
    COMPONENT_TYPE = kAudioUnitType_MusicDevice
  elif config['PLUG_DOES_MIDI']:
    COMPONENT_TYPE = kAudioUnitType_MusicEffect
  else:
    COMPONENT_TYPE = kAudioUnitType_Effect

  NSEXTENSIONPOINTIDENTIFIER  = "com.apple.AudioUnit-UI"

  plistpath = projectpath + "/resources/" + config['BUNDLE_NAME'] + "-iOS-AUv3-Info.plist"
  auv3 = plistlib.readPlist(plistpath)
  auv3['CFBundleExecutable'] = config['BUNDLE_NAME'] + "AppExtension"
  auv3['CFBundleIdentifier'] = config['BUNDLE_DOMAIN'] + "." + config['BUNDLE_MFR'] + ".app." + config['BUNDLE_NAME'] + ".AUv3"
  auv3['CFBundleName'] = config['BUNDLE_NAME'] + "AppExtension"
  auv3['CFBundleDisplayName'] = config['BUNDLE_NAME'] + "AppExtension"
  auv3['CFBundleVersion'] = CFBundleVersion
  auv3['CFBundleShortVersionString'] = CFBundleVersion
  auv3['CFBundlePackageType'] = "XPC!"
  auv3['NSExtension'] = dict(
  NSExtensionAttributes = dict(AudioComponents = [{}]),
                               NSExtensionMainStoryboard = "IPlugEffect-iOS-MainInterface",
                               NSExtensionPointIdentifier = NSEXTENSIONPOINTIDENTIFIER)
  auv3['NSExtension']['NSExtensionAttributes']['AudioComponents'] = [{}]
  auv3['NSExtension']['NSExtensionAttributes']['AudioComponents'][0]['description'] = config['PLUG_NAME']
  auv3['NSExtension']['NSExtensionAttributes']['AudioComponents'][0]['manufacturer'] = config['PLUG_MFR_UID']
  auv3['NSExtension']['NSExtensionAttributes']['AudioComponents'][0]['factoryFunction'] = "IPlugEffectViewController"
  auv3['NSExtension']['NSExtensionAttributes']['AudioComponents'][0]['name'] = config['PLUG_MFR'] + ": " + config['PLUG_NAME']
  auv3['NSExtension']['NSExtensionAttributes']['AudioComponents'][0]['subtype'] = config['PLUG_UID']
  auv3['NSExtension']['NSExtensionAttributes']['AudioComponents'][0]['type'] = COMPONENT_TYPE
  auv3['NSExtension']['NSExtensionAttributes']['AudioComponents'][0]['version'] = config['PLUG_VER_INT']
  auv3['NSExtension']['NSExtensionAttributes']['AudioComponents'][0]['sandboxSafe'] = True
  auv3['NSExtension']['NSExtensionAttributes']['AudioComponents'][0]['tags'] = [{}]

  if config['PLUG_IS_INSTRUMENT']:
    auv3['NSExtension']['NSExtensionAttributes']['AudioComponents'][0]['tags'][0] = "Synth"
  else:
    auv3['NSExtension']['NSExtensionAttributes']['AudioComponents'][0]['tags'][0] = "Effects"

  plistlib.writePlist(auv3, plistpath)

# Standalone APP

  plistpath = projectpath + "/resources/" + config['BUNDLE_NAME'] + "-iOS-Info.plist"
  iOSapp = plistlib.readPlist(plistpath)
  iOSapp['CFBundleExecutable'] = config['BUNDLE_NAME']
  iOSapp['CFBundleIdentifier'] = config['BUNDLE_DOMAIN'] + "." + config['BUNDLE_MFR'] + ".app." + config['BUNDLE_NAME']
  iOSapp['CFBundleName'] = config['BUNDLE_NAME']
  iOSapp['CFBundleVersion'] = CFBundleVersion
  iOSapp['CFBundleShortVersionString'] = CFBundleVersion
  iOSapp['CFBundlePackageType'] = "APPL"
  iOSapp['LSApplicationCategoryType'] = "public.app-category.music"

  plistlib.writePlist(iOSapp, plistpath)

if __name__ == '__main__':
  main()
