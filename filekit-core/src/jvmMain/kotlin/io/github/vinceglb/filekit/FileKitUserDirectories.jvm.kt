package io.github.vinceglb.filekit

import io.github.vinceglb.filekit.utils.Platform
import io.github.vinceglb.filekit.utils.div
import io.github.vinceglb.filekit.utils.toPath
import kotlinx.io.files.Path
import java.io.File

internal fun resolveJvmUserDirectoryPath(
    type: FileKitUserDirectory,
    platform: Platform,
    envProvider: (String) -> String?,
    linuxUserDirsConfigProvider: () -> String?,
    windowsKnownFolderResolver: (FileKitUserDirectory) -> String?,
): Path? = when (platform) {
    Platform.Linux -> resolveLinuxUserDirectoryPath(
        type = type,
        envProvider = envProvider,
        linuxUserDirsConfigProvider = linuxUserDirsConfigProvider,
    )

    Platform.MacOS -> resolveMacUserDirectoryPath(
        type = type,
        home = envProvider("HOME"),
    )

    Platform.Windows -> resolveWindowsUserDirectoryPath(
        type = type,
        envProvider = envProvider,
        windowsKnownFolderResolver = windowsKnownFolderResolver,
    )
}

internal fun defaultLinuxUserDirsConfig(envProvider: (String) -> String?): String? {
    val home = envProvider("HOME")
        ?.takeIf(String::isNotBlank)
        ?: return null
    val configHome = envProvider("XDG_CONFIG_HOME")
        ?.takeIf(String::isNotBlank)
        ?: "$home/.config"
    val configFile = File(configHome, "user-dirs.dirs")
    return configFile.takeIf(File::exists)?.let { file ->
        runCatching(file::readText).getOrNull()
    }
}

// TODO: Replace with JNI call to Shell32 SHGetKnownFolderPath when Windows JNI is implemented
internal fun resolveKnownFolderPath(@Suppress("UNUSED_PARAMETER") type: FileKitUserDirectory): String? = null

internal fun parseXdgUserDirsConfig(config: String): Map<String, String> =
    buildMap {
        config
            .lineSequence()
            .map(String::trim)
            .filter(String::isNotBlank)
            .filterNot { it.startsWith("#") }
            .forEach { line ->
                val key = line.substringBefore("=", missingDelimiterValue = "").trim()
                val rawValue = line.substringAfter("=", missingDelimiterValue = "").trim()

                if (key.isBlank() || rawValue.isBlank()) {
                    return@forEach
                }

                val value = rawValue
                    .removeSurrounding("\"")
                    .replace("\\\"", "\"")
                    .replace("\\\\", "\\")

                put(key, value)
            }
    }

private fun resolveLinuxUserDirectoryPath(
    type: FileKitUserDirectory,
    envProvider: (String) -> String?,
    linuxUserDirsConfigProvider: () -> String?,
): Path? {
    val home = envProvider("HOME")
        ?.takeIf(String::isNotBlank)
        ?: return null
    val envValue = envProvider(type.xdgEnvKey)
        ?.takeIf(String::isNotBlank)
        ?.let { expandHomeVariable(it, home) }
    if (envValue != null) {
        return envValue.toPath()
    }

    val configuredValue = linuxUserDirsConfigProvider()
        ?.takeIf(String::isNotBlank)
        ?.let(::parseXdgUserDirsConfig)
        ?.get(type.xdgEnvKey)
        ?.takeIf(String::isNotBlank)
        ?.let { expandHomeVariable(it, home) }
    if (configuredValue != null) {
        return configuredValue.toPath()
    }

    return (home.toPath() / type.linuxFallbackDirName)
}

private fun resolveMacUserDirectoryPath(type: FileKitUserDirectory, home: String?): Path? {
    val safeHome = home?.takeIf(String::isNotBlank) ?: return null
    return (safeHome.toPath() / type.macFallbackDirName)
}

private fun resolveWindowsUserDirectoryPath(
    type: FileKitUserDirectory,
    envProvider: (String) -> String?,
    windowsKnownFolderResolver: (FileKitUserDirectory) -> String?,
): Path? {
    val knownFolderPath = windowsKnownFolderResolver(type)
        ?.takeIf(String::isNotBlank)
    if (knownFolderPath != null) {
        return knownFolderPath.toPath()
    }

    val userProfile = envProvider("USERPROFILE")
        ?.takeIf(String::isNotBlank)
        ?: return null
    return (userProfile.toPath() / type.windowsFallbackDirName)
}

private fun expandHomeVariable(path: String, home: String): String =
    path
        .replace("\${HOME}", home)
        .replace("\$HOME", home)

private val FileKitUserDirectory.xdgEnvKey: String
    get() = when (this) {
        FileKitUserDirectory.Downloads -> "XDG_DOWNLOAD_DIR"
        FileKitUserDirectory.Pictures -> "XDG_PICTURES_DIR"
        FileKitUserDirectory.Videos -> "XDG_VIDEOS_DIR"
        FileKitUserDirectory.Music -> "XDG_MUSIC_DIR"
        FileKitUserDirectory.Documents -> "XDG_DOCUMENTS_DIR"
    }

private val FileKitUserDirectory.linuxFallbackDirName: String
    get() = when (this) {
        FileKitUserDirectory.Downloads -> "Downloads"
        FileKitUserDirectory.Pictures -> "Pictures"
        FileKitUserDirectory.Videos -> "Videos"
        FileKitUserDirectory.Music -> "Music"
        FileKitUserDirectory.Documents -> "Documents"
    }

private val FileKitUserDirectory.macFallbackDirName: String
    get() = when (this) {
        FileKitUserDirectory.Downloads -> "Downloads"
        FileKitUserDirectory.Pictures -> "Pictures"
        FileKitUserDirectory.Videos -> "Movies"
        FileKitUserDirectory.Music -> "Music"
        FileKitUserDirectory.Documents -> "Documents"
    }

private val FileKitUserDirectory.windowsFallbackDirName: String
    get() = when (this) {
        FileKitUserDirectory.Downloads -> "Downloads"
        FileKitUserDirectory.Pictures -> "Pictures"
        FileKitUserDirectory.Videos -> "Videos"
        FileKitUserDirectory.Music -> "Music"
        FileKitUserDirectory.Documents -> "Documents"
    }
