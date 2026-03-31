import org.apache.tools.ant.taskdefs.condition.Os

plugins {
    alias(libs.plugins.filekit.kotlinMultiplatformLibrary)
    alias(libs.plugins.vanniktech.mavenPublish)
}

val nativeDir = layout.projectDirectory.dir("src/jvmMain/native")
val nativeResourceDir = layout.projectDirectory.dir("src/jvmMain/resources/filekit/native")

val buildNativeLinux by tasks.registering(Exec::class) {
    description = "Compiles the Linux JNI bridge into a shared library"
    group = "build"
    val prebuiltX64 = nativeResourceDir.dir("linux-x64").file("libfilekit_linux_window.so").asFile.exists()
    val prebuiltAarch64 = nativeResourceDir.dir("linux-aarch64").file("libfilekit_linux_window.so").asFile.exists()
    enabled = Os.isFamily(Os.FAMILY_UNIX) && !Os.isFamily(Os.FAMILY_MAC) && !prebuiltX64 && !prebuiltAarch64

    inputs.dir(nativeDir.dir("linux"))
    outputs.dir(nativeResourceDir)
    workingDir(nativeDir.dir("linux"))
    commandLine("bash", "build.sh")
}

tasks.named("jvmProcessResources") {
    dependsOn(buildNativeLinux)
}

kotlin {
    android {
        androidResources {
            enable = true
        }
    }

    sourceSets {
        commonMain.dependencies {
            api(projects.filekitCore)
            implementation(libs.kotlinx.coroutines.core)
        }

        androidMain.dependencies {
            implementation(libs.androidx.activity.ktx)
        }

        androidHostTest.dependencies {
            implementation(libs.test.android.robolectric)
        }

        jvmMain.dependencies {
            implementation(libs.dbus.java.core)
            implementation(libs.dbus.java.transport.native.unixsocket)
        }

        wasmJsMain.dependencies {
            implementation(libs.kotlinx.browser)
        }
    }
}
