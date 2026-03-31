package io.github.vinceglb.filekit.dialogs.platform.jni

import java.awt.Window

internal object NativeLinuxBridge {
    private const val LIBRARY_NAME = "filekit_linux_window"

    private val loaded = NativeLibraryLoader.load(LIBRARY_NAME, NativeLinuxBridge::class.java)

    val isLoaded: Boolean get() = loaded

    @JvmStatic
    external fun nativeGetWindowId(awtComponent: Window): Long

    fun getWindowId(window: Window): Long? {
        if (!loaded) return null
        return try {
            val id = nativeGetWindowId(window)
            if (id != 0L) id else null
        } catch (_: Exception) {
            null
        }
    }
}
