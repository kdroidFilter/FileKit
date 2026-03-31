package io.github.vinceglb.filekit.dialogs.platform.jni

internal object NativeXdgPortalBridge {
    private const val LIBRARY_NAME = "filekit_xdg_portal"

    private val loaded = NativeLibraryLoader.load(LIBRARY_NAME, NativeXdgPortalBridge::class.java)

    val isLoaded: Boolean get() = loaded

    @JvmStatic
    external fun nativeIsAvailable(): Boolean

    /**
     * Opens the XDG FileChooser portal and blocks until the user responds.
     *
     * @return array of file URIs (e.g. "file:///home/user/file.txt"), or null if cancelled
     */
    @JvmStatic
    external fun nativeOpenFile(
        parentWindow: String,
        title: String,
        multiple: Boolean,
        openDirectory: Boolean,
        extensions: Array<String>?,
        currentFolder: String?,
    ): Array<String>?

    /**
     * Opens the XDG FileChooser save dialog and blocks until the user responds.
     *
     * @return array with a single file URI, or null if cancelled
     */
    @JvmStatic
    external fun nativeSaveFile(
        parentWindow: String,
        title: String,
        currentName: String,
        currentFolder: String?,
    ): Array<String>?

    fun isAvailable(): Boolean {
        if (!loaded) return false
        return try {
            nativeIsAvailable()
        } catch (_: Exception) {
            false
        }
    }
}
