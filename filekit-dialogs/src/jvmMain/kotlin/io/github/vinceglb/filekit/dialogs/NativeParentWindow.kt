package io.github.vinceglb.filekit.dialogs

/**
 * Opt-in marker for a [FileKitDialogSettings.parentWindow] that supplies its own
 * native window handle instead of relying on an AWT peer.
 *
 * By default FileKit derives the parent's native handle from the AWT peer of a
 * [java.awt.Window] (`Native.getWindowPointer` on Windows, `Native.getWindowID`
 * on Linux/X11). That only works for windows actually backed by AWT. Backends
 * that own a native window without an AWT peer — Tao, SWT, LWJGL, or a raw
 * HWND / X11 window — can instead pass a [java.awt.Window] that *also*
 * implements this interface; FileKit then uses [nativeHandle] directly and never
 * touches the AWT peer.
 *
 * The handle is platform-specific:
 * - Windows: the `HWND` as a pointer value.
 * - Linux (X11): the X11 `Window` XID.
 * - macOS: unused — the native file panels are application-modal and ignore the parent.
 *
 * This is purely additive: callers passing an ordinary [java.awt.Window] are
 * unaffected and keep the exact same behaviour.
 */
public interface NativeParentWindow {
    public val nativeHandle: Long
}
