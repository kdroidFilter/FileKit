package io.github.vinceglb.filekit.dialogs.platform.jni

import java.nio.file.Files
import java.nio.file.Path
import java.nio.file.StandardCopyOption
import java.util.logging.Level
import java.util.logging.Logger

/**
 * Centralized native library loader with persistent caching.
 *
 * Extracts native libraries from JAR resources into a stable cache directory
 * (~/.cache/filekit/native/ on Linux/macOS, %LOCALAPPDATA%/filekit/native/ on Windows)
 * so that subsequent launches skip the extraction I/O entirely.
 *
 * The cache is invalidated per-library when the JAR resource size changes.
 */
internal object NativeLibraryLoader {
    private val logger = Logger.getLogger(NativeLibraryLoader::class.java.simpleName)
    private val loadedLibraries = mutableSetOf<String>()
    private val lock = Any()

    fun load(
        libraryName: String,
        callerClass: Class<*>,
        resourcePrefix: String = "/filekit/native",
    ): Boolean {
        synchronized(lock) {
            if (libraryName in loadedLibraries) return true

            // Try system library path first (packaged app with native libs on java.library.path)
            if (trySystemLoad(libraryName)) return true

            // Fallback: extract from JAR with persistent cache
            return tryJarExtraction(libraryName, callerClass, resourcePrefix)
        }
    }

    private fun trySystemLoad(libraryName: String): Boolean =
        try {
            System.loadLibrary(libraryName)
            loadedLibraries += libraryName
            true
        } catch (_: UnsatisfiedLinkError) {
            false
        }

    @Suppress("TooGenericExceptionCaught")
    private fun tryJarExtraction(
        libraryName: String,
        callerClass: Class<*>,
        resourcePrefix: String,
    ): Boolean {
        try {
            val platform = resolvePlatform()
            val fileName = mapLibraryFileName(libraryName, platform)
            val resourcePath = "$resourcePrefix/${platform.resourceDir}/$fileName"

            val stream =
                callerClass.getResourceAsStream(resourcePath) ?: run {
                    logger.fine("Native library not available on this platform: $resourcePath")
                    return false
                }

            val cachedLib =
                stream.use { input ->
                    val cacheDir = resolveCacheDir().resolve(platform.resourceDir)
                    Files.createDirectories(cacheDir)
                    val target = cacheDir.resolve(fileName)

                    val resourceSize = input.available().toLong()
                    if (Files.exists(target) && isCacheValid(target, resourceSize)) {
                        return@use target
                    }

                    val tmp = Files.createTempFile(cacheDir, libraryName, ".tmp")
                    try {
                        Files.copy(input, tmp, StandardCopyOption.REPLACE_EXISTING)
                        Files.move(tmp, target, StandardCopyOption.REPLACE_EXISTING, StandardCopyOption.ATOMIC_MOVE)
                    } catch (_: Exception) {
                        Files.move(tmp, target, StandardCopyOption.REPLACE_EXISTING)
                    }
                    target
                }

            System.load(cachedLib.toAbsolutePath().toString())
            loadedLibraries += libraryName
            return true
        } catch (e: Exception) {
            logger.log(Level.WARNING, "Failed to load $libraryName native library", e)
            return false
        }
    }

    private fun isCacheValid(
        cachedFile: Path,
        resourceSize: Long,
    ): Boolean {
        if (resourceSize <= 0) return true
        return try {
            Files.size(cachedFile) == resourceSize
        } catch (_: Exception) {
            false
        }
    }

    private fun resolveCacheDir(): Path {
        val os = System.getProperty("os.name", "").lowercase()
        val base =
            when {
                os.contains("win") -> {
                    val localAppData = System.getenv("LOCALAPPDATA")
                    if (localAppData != null) {
                        Path.of(localAppData)
                    } else {
                        Path.of(System.getProperty("user.home"), "AppData", "Local")
                    }
                }
                os.contains("mac") -> {
                    Path.of(System.getProperty("user.home"), "Library", "Caches")
                }
                else -> {
                    val xdgCache = System.getenv("XDG_CACHE_HOME")
                    if (xdgCache != null) {
                        Path.of(xdgCache)
                    } else {
                        Path.of(System.getProperty("user.home"), ".cache")
                    }
                }
            }
        return base.resolve("filekit").resolve("native")
    }

    private fun resolvePlatform(): NativePlatform {
        val os = System.getProperty("os.name", "").lowercase()
        val arch =
            System.getProperty("os.arch").let {
                if (it == "aarch64" || it == "arm64") "aarch64" else "x64"
            }
        return when {
            os.contains("mac") || os.contains("darwin") -> NativePlatform("darwin", arch)
            os.contains("win") -> NativePlatform("win32", arch)
            else -> NativePlatform("linux", arch)
        }
    }

    private fun mapLibraryFileName(
        libraryName: String,
        platform: NativePlatform,
    ): String =
        when {
            platform.os == "win32" -> "$libraryName.dll"
            platform.os == "darwin" -> "lib$libraryName.dylib"
            else -> "lib$libraryName.so"
        }

    private data class NativePlatform(
        val os: String,
        val arch: String,
    ) {
        val resourceDir: String get() = "$os-$arch"
    }
}
