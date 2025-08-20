package com.admenri.urge;

import android.content.res.AssetManager;
import android.os.Bundle;
import android.util.Log;
import android.content.Context;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.BufferedReader;
import java.security.MessageDigest;

import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.InputStream;
import java.io.OutputStream;

import org.libsdl.app.SDLActivity;
import com.admenri.urge.AssetExtractor;

public class URGEMain extends SDLActivity {
    private static final String TAG = "URGEActivity";
    private static final String MD5_VFY_FILE = "URGE.vfy";
    public static String GAME_PATH;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Current context
        Context context = getContext();

        // Setup external storage dirs
        java.io.File data_dir = context.getExternalFilesDir("");
        if (data_dir != null) {
            GAME_PATH = data_dir.toString();
            Log.i(TAG, "Work Directory: " + GAME_PATH);
        }

        // Setup packages
        String currentMD5 = getApkMD5(context);
        if (!checkMD5Consistent(context, currentMD5)) {
            // Extracting new files (override if file was existed)
            AssetExtractor.extractAssets(this, true);
        }

        // Update resource md5
        try {
            File recordFile = new File(GAME_PATH, MD5_VFY_FILE);
            FileWriter writer = new FileWriter(recordFile);
            writer.write(currentMD5);
            writer.close();
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    public static String getApkMD5(Context context) {
        try {
            String apkPath = context.getPackageResourcePath();
            FileInputStream fis = new FileInputStream(apkPath);
            MessageDigest md = MessageDigest.getInstance("MD5");
            byte[] buffer = new byte[8192];
            int bytesRead;
            while ((bytesRead = fis.read(buffer)) != -1) {
                md.update(buffer, 0, bytesRead);
            }
            byte[] md5sum = md.digest();
            StringBuilder sb = new StringBuilder();
            for (byte b : md5sum) {
                sb.append(String.format("%02x", b));
            }
            fis.close();
            return sb.toString();
        } catch (Exception e) {
            e.printStackTrace();
        }
        return null;
    }

    public static boolean checkMD5Consistent(Context context, String currentMD5) {
        File recordFile = new File(GAME_PATH, MD5_VFY_FILE);
        try {
            if (!recordFile.exists()) return false;

            BufferedReader reader = new BufferedReader(new FileReader(recordFile));
            String savedMD5 = reader.readLine();
            reader.close();

            Log.i(TAG, "Current MD5: " + currentMD5);
            Log.i(TAG, "Local Resource MD5: " + savedMD5);

            return savedMD5 != null && savedMD5.equals(currentMD5);
        } catch (IOException e) {
            e.printStackTrace();
            return false;
        }
    }

    public static void copySingleFile(Context context, String assetPath, String targetPath) throws IOException {
        InputStream in = null;
        OutputStream out = null;
        try {
            in = context.getAssets().open(assetPath);
            out = new FileOutputStream(new File(targetPath, assetPath));
            byte[] buffer = new byte[1 << 16];
            int length;
            while ((length = in.read(buffer)) > 0) {
                out.write(buffer, 0, length);
            }
        } finally {
            if (in != null) in.close();
            if (out != null) out.close();
        }
    }
}
