package io.github.vinceglb.filekit.dialogs.platform.xdg

import io.github.vinceglb.filekit.PlatformFile
import io.github.vinceglb.filekit.dialogs.FileKitDialogSettings
import io.github.vinceglb.filekit.dialogs.platform.PlatformFilePicker
import io.github.vinceglb.filekit.dialogs.platform.jni.NativeLinuxBridge
import io.github.vinceglb.filekit.dialogs.platform.jni.NativeXdgPortalBridge
import io.github.vinceglb.filekit.path
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.awt.Window
import java.io.File
import java.net.URI

// https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.FileChooser.html
internal class XdgFilePickerPortal : PlatformFilePicker {
    fun isAvailable(): Boolean = NativeXdgPortalBridge.isAvailable()

    override suspend fun openFilePicker(
        fileExtensions: Set<String>?,
        directory: PlatformFile?,
        dialogSettings: FileKitDialogSettings,
    ): File? = withContext(Dispatchers.IO) {
        NativeXdgPortalBridge.nativeOpenFile(
            parentWindow = getWindowIdentifier(dialogSettings.parentWindow) ?: "",
            title = dialogSettings.title ?: "",
            multiple = false,
            openDirectory = false,
            extensions = fileExtensions?.toTypedArray(),
            currentFolder = directory?.path,
        )?.firstOrNull()?.toFile()
    }

    override suspend fun openFilesPicker(
        fileExtensions: Set<String>?,
        directory: PlatformFile?,
        dialogSettings: FileKitDialogSettings,
    ): List<File>? = withContext(Dispatchers.IO) {
        NativeXdgPortalBridge.nativeOpenFile(
            parentWindow = getWindowIdentifier(dialogSettings.parentWindow) ?: "",
            title = dialogSettings.title ?: "",
            multiple = true,
            openDirectory = false,
            extensions = fileExtensions?.toTypedArray(),
            currentFolder = directory?.path,
        )?.map { it.toFile() }
    }

    override suspend fun openDirectoryPicker(
        directory: PlatformFile?,
        dialogSettings: FileKitDialogSettings,
    ): File? = withContext(Dispatchers.IO) {
        NativeXdgPortalBridge.nativeOpenFile(
            parentWindow = getWindowIdentifier(dialogSettings.parentWindow) ?: "",
            title = dialogSettings.title ?: "",
            multiple = false,
            openDirectory = true,
            extensions = null,
            currentFolder = directory?.path,
        )?.firstOrNull()?.toFile()
    }

    override suspend fun openFileSaver(
        suggestedName: String,
        extension: String?,
        directory: PlatformFile?,
        dialogSettings: FileKitDialogSettings,
    ): File? = withContext(Dispatchers.IO) {
        val currentName = when {
            extension != null -> "$suggestedName.$extension"
            else -> suggestedName
        }

        NativeXdgPortalBridge.nativeSaveFile(
            parentWindow = getWindowIdentifier(dialogSettings.parentWindow) ?: "",
            title = dialogSettings.title ?: "",
            currentName = currentName,
            currentFolder = directory?.path,
        )?.firstOrNull()?.toFile()
    }

    private fun getWindowIdentifier(parentWindow: Window?): String? {
        if (parentWindow == null) return null
        val windowId = NativeLinuxBridge.getWindowId(parentWindow) ?: return null
        return "X11:$windowId"
    }
}

private fun String.toFile(): File = File(
    this
        .replace(" ", "%20")
        .replace("[", "%5B")
        .replace("]", "%5D")
        .let { URI(it) },
)
