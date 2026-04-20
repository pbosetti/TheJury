return {
    LrSdkVersion = 13.0,
    LrSdkMinimumVersion = 13.0,
    LrToolkitIdentifier = 'com.pbosetti.thejury',
    LrPluginName = 'The Jury',
    LrPluginInfoUrl = 'https://github.com/pbosetti/TheJury',
    LrPluginInfoProvider = 'PluginInfoProvider.lua',
    LrInitPlugin = 'PluginInit.lua',
    LrShutdownPlugin = 'PluginShutdown.lua',
    LrEnablePlugin = 'PluginEnable.lua',
    LrDisablePlugin = 'PluginDisable.lua',
    LrMetadataProvider = 'MetadataDefinition.lua',
    LrLibraryMenuItems = {
        {
            title = 'PPA Critique...',
            file = 'CritiqueMenu.lua',
            enabledWhen = 'photosSelected',
        },
        {
            title = 'Show Critique Details...',
            file = 'ShowCritiqueDetails.lua',
            enabledWhen = 'photosSelected',
        },
    },
    VERSION = {
        major = 0,
        minor = 2,
        revision = 0,
        build = 0,
    },
}
