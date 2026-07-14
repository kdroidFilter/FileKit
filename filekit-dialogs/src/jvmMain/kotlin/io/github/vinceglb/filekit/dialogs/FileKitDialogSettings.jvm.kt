package io.github.vinceglb.filekit.dialogs

import java.awt.Window

/**
 * JVM implementation of [FileKitDialogSettings].
 *
 * @property title The title of the dialog.
 * @property parentWindow The parent window for the dialog. Its native handle is
 * derived from the AWT peer — only works for windows backed by AWT.
 * @property macOS Specific settings for macOS when running on JVM.
 * @property parentWindowHandle Raw native parent window handle, for apps whose
 * windows are not backed by AWT (Tao, SWT, LWJGL, a raw native window). When
 * set, it takes precedence over [parentWindow] and no AWT peer is touched.
 * Platform-specific: `HWND` on Windows, X11 `Window` XID on Linux. Unused on
 * macOS (its file panels are application-modal and ignore the parent).
 */
public actual data class FileKitDialogSettings(
    public val title: String? = null,
    public val parentWindow: Window? = null,
    public val macOS: FileKitMacOSSettings = FileKitMacOSSettings(),
    public val parentWindowHandle: Long? = null,
) {
    public actual companion object {
        /**
         * Creates a default instance of [FileKitDialogSettings].
         */
        public actual fun createDefault(): FileKitDialogSettings = FileKitDialogSettings()
    }
}

/**
 * Settings specific to macOS file dialogs on JVM.
 *
 * @property resolvesAliases Whether aliases should be resolved.
 * @property canCreateDirectories Whether the user can create directories in the save panel.
 */
public class FileKitMacOSSettings(
    public val resolvesAliases: Boolean? = null,
    public val canCreateDirectories: Boolean = true,
)
