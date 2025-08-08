package com.admenri.urge;

import android.content.Context;
import android.content.res.AssetManager;
import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

/**
 * A utility class to extract all assets from the APK to the app's external files directory.
 */
public class AssetExtractor {
    private static final String TAG = "AssetExtractor";
    private static final int BUFFER_SIZE = 1024;

    /**
     * Extracts all assets from the APK to the Android/data/&lt;package_name&gt;/files/ directory.
     *
     * @param context The application context.
     * @param force   If true, it will overwrite existing files. If false, it will skip existing files.
     */
    public static void extractAssets(final Context context, boolean force) {
        AssetManager assetManager = context.getAssets();
        File destinationDir = context.getExternalFilesDir(null);

        if (destinationDir == null) {
            Log.e(TAG, "Failed to get external files directory.");
            return;
        }

        try {
            // The root asset folder is represented by an empty string ""
            copyAssetFolder(assetManager, "", destinationDir.getAbsolutePath(), force);
        } catch (IOException e) {
            Log.e(TAG, "Failed to extract assets.", e);
        }
    }

    /**
     * Recursively copies an asset folder and its contents to the destination directory.
     *
     * @param assetManager    The AssetManager instance.
     * @param sourceAssetPath The path of the asset folder to copy.
     * @param destPath        The destination path in the device storage.
     * @param force           Whether to overwrite existing files.
     * @throws IOException If an I/O error occurs.
     */
    private static void copyAssetFolder(AssetManager assetManager, String sourceAssetPath, String destPath, boolean force) throws IOException {
        String[] assets = assetManager.list(sourceAssetPath);

        // If the assets array is empty, it's either a file or an empty directory.
        // We'll treat it as a file to be copied. The copyAssetFile will handle it.
        if (assets.length == 0) {
            copyAssetFile(assetManager, sourceAssetPath, destPath, force);
        } else {
            File dir = new File(destPath);
            if (!dir.exists() && !dir.mkdirs()) {
                Log.w(TAG, "Could not create directory: " + dir.getPath());
            }

            for (String asset : assets) {
                String newSourcePath = sourceAssetPath.isEmpty() ? asset : sourceAssetPath + File.separator + asset;
                String newDestPath = destPath + File.separator + asset;
                copyAssetFolder(assetManager, newSourcePath, newDestPath, force);
            }
        }
    }

    /**
     * Copies a single asset file to the destination directory.
     *
     * @param assetManager    The AssetManager instance.
     * @param sourceAssetPath The full path of the asset file.
     * @param destPath        The destination path in the device storage.
     * @param force           Whether to overwrite the existing file.
     * @throws IOException If an I/O error occurs.
     */
    private static void copyAssetFile(AssetManager assetManager, String sourceAssetPath, String destPath, boolean force) throws IOException {
        File destFile = new File(destPath);

        if (!force && destFile.exists()) {
            Log.i(TAG, "Skipping existing file: " + sourceAssetPath);
            return;
        }

        // Use try-with-resources to ensure streams are closed automatically
        try (InputStream in = assetManager.open(sourceAssetPath);
             OutputStream out = new FileOutputStream(destFile)) {

            byte[] buffer = new byte[BUFFER_SIZE];
            int read;
            while ((read = in.read(buffer)) != -1) {
                out.write(buffer, 0, read);
            }
            Log.d(TAG, "Copied " + sourceAssetPath + " to " + destPath);
        } catch (Exception e) {
            // assetManager.open() throws IOException if the path is a directory.
            // We can safely ignore this as the outer loop will handle directory traversal.
            Log.d(TAG, "Skipping directory: " + sourceAssetPath);
        }
    }
}