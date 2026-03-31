#include <jni.h>
#include <jawt.h>
#include <jawt_md.h>

JNIEXPORT jlong JNICALL
Java_io_github_vinceglb_filekit_dialogs_platform_jni_NativeLinuxBridge_nativeGetWindowId(
    JNIEnv *env, jclass cls, jobject awtComponent)
{
    JAWT awt;
    awt.version = JAWT_VERSION_1_4;

    if (JAWT_GetAWT(env, &awt) == JNI_FALSE) {
        return 0;
    }

    JAWT_DrawingSurface *ds = awt.GetDrawingSurface(env, awtComponent);
    if (ds == NULL) {
        return 0;
    }

    jint lock = ds->Lock(ds);
    if ((lock & JAWT_LOCK_ERROR) != 0) {
        awt.FreeDrawingSurface(ds);
        return 0;
    }

    JAWT_DrawingSurfaceInfo *dsi = ds->GetDrawingSurfaceInfo(ds);
    if (dsi == NULL) {
        ds->Unlock(ds);
        awt.FreeDrawingSurface(ds);
        return 0;
    }

    JAWT_X11DrawingSurfaceInfo *x11dsi = (JAWT_X11DrawingSurfaceInfo *)dsi->platformInfo;
    jlong windowId = (jlong)x11dsi->drawable;

    ds->FreeDrawingSurfaceInfo(dsi);
    ds->Unlock(ds);
    awt.FreeDrawingSurface(ds);

    return windowId;
}
