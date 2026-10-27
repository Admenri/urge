package com.admenri.urge;

import android.app.Activity;
import android.content.pm.ApplicationInfo;
import android.util.Log;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Enumeration;
import java.util.List;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

/**
 * The Android host: SDL's own activity, plus what a game needs that a browser
 * tab and a desktop window do not.
 *
 * <p>The one thing that has to happen before the engine starts is unpacking the
 * game out of the APK.  A game ships in {@code assets/}, which is a zip inside
 * the APK; the engine asks PhysicsFS for a directory it can mount and enumerate
 * (RGSS looks files up by name, case-insensitively, with or without the
 * extension), so the files have to exist as files.  On the first launch of a new
 * build they are unpacked into the app's own external files directory --
 * {@code Android/data/<package>/files} -- which then serves as both the game's
 * read path and its save path, the same way a desktop game's folder does.
 *
 * <p>What decides "a new build" is the MD5 of the installed APK, recorded in
 * {@code URGE_BUNDLE_MD5} next to the game.  A normal launch is therefore one
 * fingerprint and no unpacking at all.
 */
public class URGEActivity extends SDLActivity {
    private static final String TAG = "URGE";

    /** Recorded in the game directory once its contents match the APK. */
    private static final String BUNDLE_MARKER = "URGE_BUNDLE_MD5";

    /** Where the game's files sit inside the APK. */
    private static final String ASSET_ROOT = "assets/";

    private static final int BUFFER_SIZE = 64 * 1024;

    /**
     * The engine is built as a shared library called {@code main}, and this is
     * the one SDL loads and then looks {@code SDL_main} up in.  SDL's default
     * is the last entry of {@link #getLibraries()}, spelled out here so a
     * rename of the CMake target cannot silently change it.
     */
    @Override
    protected String getMainSharedObject() {
        return "libmain.so";
    }

    /**
     * Unpacks the game out of the APK and returns the directory it lives in, or
     * null when it could not be prepared.
     *
     * <p>Called from the engine's startup ({@code app/platform/android.cc}) on
     * SDL's own thread, not the UI thread: a few hundred megabytes of unpacking
     * must not block the UI, and the engine cannot start until it is done.
     *
     * <p>Files are only ever written, never deleted: the same directory holds
     * the player's saves, and a game update must not take them with it.  The
     * cost is that a resource dropped from a new build stays on disk, which
     * nothing reads.
     */
    public static String ensureBundle(Activity activity) {
        File root = activity.getExternalFilesDir(null);
        if (root == null) {
            // No external storage (rare, but a device can report none); the
            // internal directory always works and is private to the app.
            root = activity.getFilesDir();
        }
        if (root == null) {
            Log.e(TAG, "there is no writable directory to unpack the game into");
            return null;
        }
        if (!root.isDirectory() && !root.mkdirs()) {
            Log.e(TAG, "cannot create " + root);
            return null;
        }

        List<File> apks = installedApks(activity);
        String digest = apkDigest(apks);

        File marker = new File(root, BUNDLE_MARKER);
        if (digest != null && digest.equals(readText(marker))) {
            Log.i(TAG, "game bundle is current (" + digest + ")");
            return root.getAbsolutePath();
        }

        Log.i(TAG, "unpacking the game into " + root + " (" + digest + ")");
        try {
            unpack(apks, root);
        } catch (IOException error) {
            Log.e(TAG, "unpacking the game failed", error);
            return null;
        }

        // Written last, so an interrupted unpack leaves the old fingerprint
        // behind and the next launch starts over instead of trusting half a
        // tree.
        if (digest != null) {
            writeText(marker, digest);
        }
        return root.getAbsolutePath();
    }

    /**
     * The APKs this installation is made of: the base one, then any split ones
     * an app bundle produced, in a fixed order so the fingerprint is the same
     * on every launch.
     */
    private static List<File> installedApks(Activity activity) {
        ApplicationInfo info = activity.getApplicationInfo();
        List<File> apks = new ArrayList<>();

        if (info.sourceDir != null) {
            apks.add(new File(info.sourceDir));
        }
        if (info.splitSourceDirs != null) {
            String[] splits = info.splitSourceDirs.clone();
            Arrays.sort(splits);
            for (String split : splits) {
                if (split != null) {
                    apks.add(new File(split));
                }
            }
        }
        return apks;
    }

    /**
     * The MD5 of every APK this installation is made of, in one pass, or null
     * when it cannot be computed.
     *
     * <p>Null means "unpack on every launch": slower, but never wrong.  It is
     * not cached anywhere else on purpose -- the point of the fingerprint is to
     * notice an update, and anything cheaper (a version code, a timestamp) is a
     * value the platform can change without the files changing, or the other way
     * round.
     */
    private static String apkDigest(List<File> apks) {
        try {
            MessageDigest digest = MessageDigest.getInstance("MD5");
            byte[] buffer = new byte[BUFFER_SIZE];

            for (File apk : apks) {
                try (InputStream in = new FileInputStream(apk)) {
                    int read;
                    while ((read = in.read(buffer)) > 0) {
                        digest.update(buffer, 0, read);
                    }
                }
            }
            return hex(digest.digest());
        } catch (IOException | NoSuchAlgorithmException error) {
            Log.w(TAG, "cannot fingerprint the APK; unpacking on every launch",
                    error);
            return null;
        }
    }

    /**
     * Copies every {@code assets/} entry of every APK into {@code root},
     * overwriting what is there and creating directories as needed.
     */
    private static void unpack(List<File> apks, File root) throws IOException {
        byte[] buffer = new byte[BUFFER_SIZE];
        int files = 0;

        for (File apk : apks) {
            try (ZipFile zip = new ZipFile(apk)) {
                Enumeration<? extends ZipEntry> entries = zip.entries();
                while (entries.hasMoreElements()) {
                    ZipEntry entry = entries.nextElement();
                    if (entry.isDirectory()) {
                        continue;
                    }

                    String name = entry.getName();
                    if (!name.startsWith(ASSET_ROOT)) {
                        continue;
                    }

                    String relative = name.substring(ASSET_ROOT.length());
                    if (relative.isEmpty()) {
                        continue;
                    }
                    if (relative.contains("..")) {
                        Log.w(TAG, "skipping a suspicious asset name: " + name);
                        continue;
                    }

                    File target = new File(root, relative);
                    File parent = target.getParentFile();
                    if (parent != null && !parent.isDirectory() && !parent.mkdirs()) {
                        throw new IOException("cannot create " + parent);
                    }

                    try (InputStream in = zip.getInputStream(entry);
                         OutputStream out = new FileOutputStream(target)) {
                        int read;
                        while ((read = in.read(buffer)) > 0) {
                            out.write(buffer, 0, read);
                        }
                    }

                    files += 1;
                    if ((files % 256) == 0) {
                        Log.i(TAG, "unpacked " + files + " files");
                    }
                }
            }
        }

        if (files == 0) {
            // Not an error here -- the engine reports the missing Game.ini with
            // the path it looked in, which is the more useful message.
            Log.w(TAG, "the APK has no assets/; was it built without a game "
                    + "directory?");
        }
        Log.i(TAG, "unpacked " + files + " files");
    }

    private static String readText(File file) {
        if (!file.isFile()) {
            return null;
        }

        byte[] data = new byte[(int) file.length()];
        try (InputStream in = new FileInputStream(file)) {
            int offset = 0;
            while (offset < data.length) {
                int read = in.read(data, offset, data.length - offset);
                if (read <= 0) {
                    break;
                }
                offset += read;
            }
            return new String(data, 0, offset, "UTF-8").trim();
        } catch (IOException error) {
            return null;
        }
    }

    private static void writeText(File file, String text) {
        try (OutputStream out = new FileOutputStream(file)) {
            out.write((text + "\n").getBytes("UTF-8"));
        } catch (IOException error) {
            Log.w(TAG, "cannot record the bundle fingerprint", error);
        }
    }

    private static String hex(byte[] bytes) {
        StringBuilder text = new StringBuilder(bytes.length * 2);
        for (byte value : bytes) {
            text.append(Character.forDigit((value >> 4) & 0xF, 16));
            text.append(Character.forDigit(value & 0xF, 16));
        }
        return text.toString();
    }
}
