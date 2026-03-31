/**
 * JNI bridge for XDG Desktop Portal FileChooser via libdbus-1.
 *
 * Implements OpenFile/SaveFile calls to org.freedesktop.portal.FileChooser
 * and waits for the Response signal on org.freedesktop.portal.Request.
 *
 * Linked libraries: libdbus-1, libpthread
 */

#include <jni.h>
#include <dbus/dbus.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

/* Portal constants */
static const char *PORTAL_BUS        = "org.freedesktop.portal.Desktop";
static const char *PORTAL_PATH       = "/org/freedesktop/portal/desktop";
static const char *FILECHOOSER_IFACE = "org.freedesktop.portal.FileChooser";
static const char *REQUEST_IFACE     = "org.freedesktop.portal.Request";
static const char *PROPERTIES_IFACE  = "org.freedesktop.DBus.Properties";

/* ------------------------------------------------------------------ */
/*  Helpers                                                            */
/* ------------------------------------------------------------------ */

static void generate_handle_token(char *buf, size_t len) {
    static const char alphanum[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    static int seeded = 0;
    if (!seeded) {
        srand((unsigned)(time(NULL) ^ (long)buf));
        seeded = 1;
    }
    for (size_t i = 0; i < len - 1; i++) {
        buf[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
    }
    buf[len - 1] = '\0';
}

/**
 * Compute the expected response object path from the connection's unique
 * name and the handle token.
 * E.g. unique=":1.42", token="abc" -> "/org/freedesktop/portal/desktop/request/1_42/abc"
 */
static char *compute_response_path(DBusConnection *conn, const char *handle_token) {
    const char *unique = dbus_bus_get_unique_name(conn);
    size_t ulen = strlen(unique);
    char *sender = malloc(ulen + 1);
    int j = 0;
    for (size_t i = 0; i < ulen; i++) {
        if (unique[i] == ':') continue;
        sender[j++] = (unique[i] == '.') ? '_' : unique[i];
    }
    sender[j] = '\0';

    size_t plen = strlen("/org/freedesktop/portal/desktop/request/")
                  + strlen(sender) + 1 + strlen(handle_token) + 1;
    char *path = malloc(plen);
    snprintf(path, plen, "/org/freedesktop/portal/desktop/request/%s/%s",
             sender, handle_token);
    free(sender);
    return path;
}

/* ------------------------------------------------------------------ */
/*  D-Bus dict builder helpers for a{sv}                               */
/* ------------------------------------------------------------------ */

static void dict_open(DBusMessageIter *parent, DBusMessageIter *dict) {
    dbus_message_iter_open_container(parent, DBUS_TYPE_ARRAY, "{sv}", dict);
}

static void dict_close(DBusMessageIter *parent, DBusMessageIter *dict) {
    dbus_message_iter_close_container(parent, dict);
}

static void dict_append_string(DBusMessageIter *dict, const char *key, const char *value) {
    DBusMessageIter entry, variant;
    dbus_message_iter_open_container(dict, DBUS_TYPE_DICT_ENTRY, NULL, &entry);
    dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);
    dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "s", &variant);
    dbus_message_iter_append_basic(&variant, DBUS_TYPE_STRING, &value);
    dbus_message_iter_close_container(&entry, &variant);
    dbus_message_iter_close_container(dict, &entry);
}

static void dict_append_boolean(DBusMessageIter *dict, const char *key, dbus_bool_t value) {
    DBusMessageIter entry, variant;
    dbus_message_iter_open_container(dict, DBUS_TYPE_DICT_ENTRY, NULL, &entry);
    dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);
    dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "b", &variant);
    dbus_message_iter_append_basic(&variant, DBUS_TYPE_BOOLEAN, &value);
    dbus_message_iter_close_container(&entry, &variant);
    dbus_message_iter_close_container(dict, &entry);
}

static void dict_append_byte_array(DBusMessageIter *dict, const char *key,
                                   const unsigned char *data, int len) {
    DBusMessageIter entry, variant, array;
    dbus_message_iter_open_container(dict, DBUS_TYPE_DICT_ENTRY, NULL, &entry);
    dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);
    dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "ay", &variant);
    dbus_message_iter_open_container(&variant, DBUS_TYPE_ARRAY, "y", &array);
    for (int i = 0; i < len; i++) {
        dbus_message_iter_append_basic(&array, DBUS_TYPE_BYTE, &data[i]);
    }
    dbus_message_iter_close_container(&variant, &array);
    dbus_message_iter_close_container(&entry, &variant);
    dbus_message_iter_close_container(dict, &entry);
}

/**
 * Append the "filters" option: a(sa(us))
 *
 * Produces one "Supported files" entry with all extensions, plus one entry per
 * individual extension.
 */
static void dict_append_filters(DBusMessageIter *dict,
                                const char **extensions, int ext_count) {
    if (ext_count <= 0) return;

    DBusMessageIter entry, variant, filters_array;

    dbus_message_iter_open_container(dict, DBUS_TYPE_DICT_ENTRY, NULL, &entry);
    {
        const char *key = "filters";
        dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);
        dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT,
                                         "a(sa(us))", &variant);
        dbus_message_iter_open_container(&variant, DBUS_TYPE_ARRAY,
                                         "(sa(us))", &filters_array);
        {
            /* "Supported files" — all extensions combined */
            DBusMessageIter filter_struct, patterns_array;
            dbus_message_iter_open_container(&filters_array, DBUS_TYPE_STRUCT,
                                             NULL, &filter_struct);
            {
                const char *label = "Supported files";
                dbus_message_iter_append_basic(&filter_struct, DBUS_TYPE_STRING, &label);
                dbus_message_iter_open_container(&filter_struct, DBUS_TYPE_ARRAY,
                                                 "(us)", &patterns_array);
                for (int i = 0; i < ext_count; i++) {
                    DBusMessageIter pattern_struct;
                    dbus_message_iter_open_container(&patterns_array, DBUS_TYPE_STRUCT,
                                                     NULL, &pattern_struct);
                    dbus_uint32_t glob_type = 0;
                    char pattern[256];
                    snprintf(pattern, sizeof(pattern), "*.%s", extensions[i]);
                    const char *pptr = pattern;
                    dbus_message_iter_append_basic(&pattern_struct, DBUS_TYPE_UINT32, &glob_type);
                    dbus_message_iter_append_basic(&pattern_struct, DBUS_TYPE_STRING, &pptr);
                    dbus_message_iter_close_container(&patterns_array, &pattern_struct);
                }
                dbus_message_iter_close_container(&filter_struct, &patterns_array);
            }
            dbus_message_iter_close_container(&filters_array, &filter_struct);

            /* Individual extension filters */
            for (int i = 0; i < ext_count; i++) {
                DBusMessageIter fstruct, parr, pstruct;
                dbus_message_iter_open_container(&filters_array, DBUS_TYPE_STRUCT,
                                                 NULL, &fstruct);
                dbus_message_iter_append_basic(&fstruct, DBUS_TYPE_STRING, &extensions[i]);
                dbus_message_iter_open_container(&fstruct, DBUS_TYPE_ARRAY,
                                                 "(us)", &parr);
                {
                    dbus_message_iter_open_container(&parr, DBUS_TYPE_STRUCT, NULL, &pstruct);
                    dbus_uint32_t glob_type = 0;
                    char pattern[256];
                    snprintf(pattern, sizeof(pattern), "*.%s", extensions[i]);
                    const char *pptr = pattern;
                    dbus_message_iter_append_basic(&pstruct, DBUS_TYPE_UINT32, &glob_type);
                    dbus_message_iter_append_basic(&pstruct, DBUS_TYPE_STRING, &pptr);
                    dbus_message_iter_close_container(&parr, &pstruct);
                }
                dbus_message_iter_close_container(&fstruct, &parr);
                dbus_message_iter_close_container(&filters_array, &fstruct);
            }
        }
        dbus_message_iter_close_container(&variant, &filters_array);
        dbus_message_iter_close_container(&entry, &variant);
    }
    dbus_message_iter_close_container(dict, &entry);
}

/* ------------------------------------------------------------------ */
/*  Response signal parsing                                            */
/* ------------------------------------------------------------------ */

/**
 * Parse the Response signal: (u a{sv}).
 * On success (response==0), extracts the "uris" string array.
 * Returns the number of URIs, or -1 on cancellation/error.
 * Caller must free each uri and the array itself.
 */
static int parse_response_signal(DBusMessage *msg, char ***out_uris) {
    DBusMessageIter iter;
    if (!dbus_message_iter_init(msg, &iter)) return -1;

    /* response: uint32 */
    if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_UINT32) return -1;
    dbus_uint32_t response;
    dbus_message_iter_get_basic(&iter, &response);
    if (response != 0) return -1; /* cancelled or error */

    if (!dbus_message_iter_next(&iter)) return -1;

    /* results: a{sv} */
    if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_ARRAY) return -1;
    DBusMessageIter dict_iter;
    dbus_message_iter_recurse(&iter, &dict_iter);

    while (dbus_message_iter_get_arg_type(&dict_iter) == DBUS_TYPE_DICT_ENTRY) {
        DBusMessageIter entry_iter, variant_iter;
        dbus_message_iter_recurse(&dict_iter, &entry_iter);

        const char *key = NULL;
        dbus_message_iter_get_basic(&entry_iter, &key);
        dbus_message_iter_next(&entry_iter);

        if (strcmp(key, "uris") == 0) {
            dbus_message_iter_recurse(&entry_iter, &variant_iter);
            if (dbus_message_iter_get_arg_type(&variant_iter) == DBUS_TYPE_ARRAY) {
                DBusMessageIter arr_iter;
                dbus_message_iter_recurse(&variant_iter, &arr_iter);

                /* Count URIs */
                int count = 0;
                DBusMessageIter counter = arr_iter;
                while (dbus_message_iter_get_arg_type(&counter) == DBUS_TYPE_STRING) {
                    count++;
                    dbus_message_iter_next(&counter);
                }

                char **uris = malloc(sizeof(char *) * count);
                for (int i = 0; i < count; i++) {
                    const char *uri;
                    dbus_message_iter_get_basic(&arr_iter, &uri);
                    uris[i] = strdup(uri);
                    dbus_message_iter_next(&arr_iter);
                }

                *out_uris = uris;
                return count;
            }
        }
        dbus_message_iter_next(&dict_iter);
    }

    return -1;
}

/* ------------------------------------------------------------------ */
/*  Portal call + wait for response                                    */
/* ------------------------------------------------------------------ */

/**
 * Call a FileChooser method and wait for the Response signal.
 * The method message must already have (s s a{sv}) appended.
 * Returns number of URIs, or -1 on failure.
 */
static int call_portal_and_wait(DBusConnection *conn, DBusMessage *call_msg,
                                const char *response_path, char ***out_uris) {
    DBusError err;
    dbus_error_init(&err);

    /* Add match rule for the Response signal on the specific request path */
    char match[512];
    snprintf(match, sizeof(match),
             "type='signal',"
             "interface='%s',"
             "member='Response',"
             "path='%s'",
             REQUEST_IFACE, response_path);
    dbus_bus_add_match(conn, match, &err);
    if (dbus_error_is_set(&err)) {
        dbus_error_free(&err);
        return -1;
    }
    dbus_connection_flush(conn);

    /* Send the method call */
    DBusMessage *reply = dbus_connection_send_with_reply_and_block(
        conn, call_msg, -1, &err);
    if (reply == NULL) {
        dbus_error_free(&err);
        dbus_bus_remove_match(conn, match, NULL);
        return -1;
    }
    dbus_message_unref(reply);

    /* Wait for the Response signal */
    int result = -1;
    while (1) {
        if (!dbus_connection_read_write(conn, 500)) {
            break; /* connection closed */
        }

        DBusMessage *msg;
        while ((msg = dbus_connection_pop_message(conn)) != NULL) {
            if (dbus_message_is_signal(msg, REQUEST_IFACE, "Response") &&
                strcmp(dbus_message_get_path(msg), response_path) == 0) {

                result = parse_response_signal(msg, out_uris);
                dbus_message_unref(msg);
                goto done;
            }
            dbus_message_unref(msg);
        }
    }

done:
    dbus_bus_remove_match(conn, match, NULL);
    dbus_error_free(&err);
    return result;
}

/**
 * Convert a URI array result to a Java String[].
 */
static jobjectArray uris_to_jarray(JNIEnv *env, char **uris, int count) {
    if (count <= 0 || uris == NULL) return NULL;

    jclass stringClass = (*env)->FindClass(env, "java/lang/String");
    jobjectArray result = (*env)->NewObjectArray(env, count, stringClass, NULL);
    for (int i = 0; i < count; i++) {
        jstring juri = (*env)->NewStringUTF(env, uris[i]);
        (*env)->SetObjectArrayElement(env, result, i, juri);
        (*env)->DeleteLocalRef(env, juri);
        free(uris[i]);
    }
    free(uris);
    return result;
}

/* ------------------------------------------------------------------ */
/*  JNI: nativeIsAvailable                                             */
/* ------------------------------------------------------------------ */
JNIEXPORT jboolean JNICALL
Java_io_github_vinceglb_filekit_dialogs_platform_jni_NativeXdgPortalBridge_nativeIsAvailable(
    JNIEnv *env, jclass cls)
{
    (void)env; (void)cls;
    DBusError err;
    dbus_error_init(&err);

    DBusConnection *conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if (conn == NULL) {
        dbus_error_free(&err);
        return JNI_FALSE;
    }

    /* Call org.freedesktop.DBus.Properties.Get("org.freedesktop.portal.FileChooser", "version") */
    DBusMessage *msg = dbus_message_new_method_call(
        PORTAL_BUS, PORTAL_PATH, PROPERTIES_IFACE, "Get");
    if (msg == NULL) {
        dbus_connection_unref(conn);
        dbus_error_free(&err);
        return JNI_FALSE;
    }

    const char *iface = FILECHOOSER_IFACE;
    const char *prop = "version";
    dbus_message_append_args(msg,
        DBUS_TYPE_STRING, &iface,
        DBUS_TYPE_STRING, &prop,
        DBUS_TYPE_INVALID);

    DBusMessage *reply = dbus_connection_send_with_reply_and_block(
        conn, msg, 2000, &err);
    dbus_message_unref(msg);

    jboolean available = JNI_FALSE;
    if (reply != NULL) {
        available = JNI_TRUE;
        dbus_message_unref(reply);
    }

    dbus_error_free(&err);
    dbus_connection_unref(conn);
    return available;
}

/* ------------------------------------------------------------------ */
/*  JNI: nativeOpenFile                                                */
/* ------------------------------------------------------------------ */
JNIEXPORT jobjectArray JNICALL
Java_io_github_vinceglb_filekit_dialogs_platform_jni_NativeXdgPortalBridge_nativeOpenFile(
    JNIEnv *env, jclass cls,
    jstring jParentWindow, jstring jTitle,
    jboolean multiple, jboolean openDirectory,
    jobjectArray jExtensions, jstring jCurrentFolder)
{
    (void)cls;
    DBusError err;
    dbus_error_init(&err);

    DBusConnection *conn = dbus_bus_get_private(DBUS_BUS_SESSION, &err);
    if (conn == NULL) {
        dbus_error_free(&err);
        return NULL;
    }

    /* Generate handle token and compute response path */
    char handle_token[33];
    generate_handle_token(handle_token, sizeof(handle_token));
    char *response_path = compute_response_path(conn, handle_token);

    /* Get Java strings */
    const char *parent_window = (*env)->GetStringUTFChars(env, jParentWindow, NULL);
    const char *title = (*env)->GetStringUTFChars(env, jTitle, NULL);

    /* Build the D-Bus method call: OpenFile(s parent_window, s title, a{sv} options) */
    DBusMessage *call_msg = dbus_message_new_method_call(
        PORTAL_BUS, PORTAL_PATH, FILECHOOSER_IFACE, "OpenFile");

    DBusMessageIter args, options_dict;
    dbus_message_iter_init_append(call_msg, &args);
    dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &parent_window);
    dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &title);

    /* Build options dict */
    dict_open(&args, &options_dict);
    {
        const char *ht = handle_token;
        dict_append_string(&options_dict, "handle_token", ht);

        dbus_bool_t b_multiple = multiple ? TRUE : FALSE;
        dict_append_boolean(&options_dict, "multiple", b_multiple);

        dbus_bool_t b_directory = openDirectory ? TRUE : FALSE;
        dict_append_boolean(&options_dict, "directory", b_directory);

        /* Extensions / filters */
        if (jExtensions != NULL) {
            jsize ext_count = (*env)->GetArrayLength(env, jExtensions);
            if (ext_count > 0) {
                const char **ext_strs = malloc(sizeof(char *) * ext_count);
                for (jsize i = 0; i < ext_count; i++) {
                    jstring jext = (*env)->GetObjectArrayElement(env, jExtensions, i);
                    ext_strs[i] = (*env)->GetStringUTFChars(env, jext, NULL);
                }

                dict_append_filters(&options_dict, ext_strs, ext_count);

                for (jsize i = 0; i < ext_count; i++) {
                    jstring jext = (*env)->GetObjectArrayElement(env, jExtensions, i);
                    (*env)->ReleaseStringUTFChars(env, jext, ext_strs[i]);
                    (*env)->DeleteLocalRef(env, jext);
                }
                free(ext_strs);
            }
        }

        /* Current folder */
        if (jCurrentFolder != NULL) {
            const char *folder = (*env)->GetStringUTFChars(env, jCurrentFolder, NULL);
            size_t flen = strlen(folder);
            /* null-terminated byte array */
            dict_append_byte_array(&options_dict, "current_folder",
                                   (const unsigned char *)folder, (int)(flen + 1));
            (*env)->ReleaseStringUTFChars(env, jCurrentFolder, folder);
        }
    }
    dict_close(&args, &options_dict);

    (*env)->ReleaseStringUTFChars(env, jParentWindow, parent_window);
    (*env)->ReleaseStringUTFChars(env, jTitle, title);

    /* Call and wait */
    char **uris = NULL;
    int count = call_portal_and_wait(conn, call_msg, response_path, &uris);
    dbus_message_unref(call_msg);

    jobjectArray result = uris_to_jarray(env, uris, count);

    free(response_path);
    dbus_connection_close(conn);
    dbus_connection_unref(conn);
    dbus_error_free(&err);
    return result;
}

/* ------------------------------------------------------------------ */
/*  JNI: nativeSaveFile                                                */
/* ------------------------------------------------------------------ */
JNIEXPORT jobjectArray JNICALL
Java_io_github_vinceglb_filekit_dialogs_platform_jni_NativeXdgPortalBridge_nativeSaveFile(
    JNIEnv *env, jclass cls,
    jstring jParentWindow, jstring jTitle,
    jstring jCurrentName, jstring jCurrentFolder)
{
    (void)cls;
    DBusError err;
    dbus_error_init(&err);

    DBusConnection *conn = dbus_bus_get_private(DBUS_BUS_SESSION, &err);
    if (conn == NULL) {
        dbus_error_free(&err);
        return NULL;
    }

    char handle_token[33];
    generate_handle_token(handle_token, sizeof(handle_token));
    char *response_path = compute_response_path(conn, handle_token);

    const char *parent_window = (*env)->GetStringUTFChars(env, jParentWindow, NULL);
    const char *title = (*env)->GetStringUTFChars(env, jTitle, NULL);
    const char *current_name = (*env)->GetStringUTFChars(env, jCurrentName, NULL);

    DBusMessage *call_msg = dbus_message_new_method_call(
        PORTAL_BUS, PORTAL_PATH, FILECHOOSER_IFACE, "SaveFile");

    DBusMessageIter args, options_dict;
    dbus_message_iter_init_append(call_msg, &args);
    dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &parent_window);
    dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &title);

    dict_open(&args, &options_dict);
    {
        const char *ht = handle_token;
        dict_append_string(&options_dict, "handle_token", ht);
        dict_append_string(&options_dict, "current_name", current_name);

        if (jCurrentFolder != NULL) {
            const char *folder = (*env)->GetStringUTFChars(env, jCurrentFolder, NULL);
            size_t flen = strlen(folder);
            dict_append_byte_array(&options_dict, "current_folder",
                                   (const unsigned char *)folder, (int)(flen + 1));
            (*env)->ReleaseStringUTFChars(env, jCurrentFolder, folder);
        }
    }
    dict_close(&args, &options_dict);

    (*env)->ReleaseStringUTFChars(env, jParentWindow, parent_window);
    (*env)->ReleaseStringUTFChars(env, jTitle, title);
    (*env)->ReleaseStringUTFChars(env, jCurrentName, current_name);

    char **uris = NULL;
    int count = call_portal_and_wait(conn, call_msg, response_path, &uris);
    dbus_message_unref(call_msg);

    jobjectArray result = uris_to_jarray(env, uris, count);

    free(response_path);
    dbus_connection_close(conn);
    dbus_connection_unref(conn);
    dbus_error_free(&err);
    return result;
}
