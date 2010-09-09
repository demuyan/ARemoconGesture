package com.example.hands;

import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Message;
import android.util.Log;

import java.io.IOException;

//import static com.android.camera.Util.Assert;

//
// CameraHolder is used to hold an android.hardware.Camera instance.
//
// The open() and release() calls are similar to the ones in
// android.hardware.Camera. The difference is if keep() is called before
// release(), CameraHolder will try to hold the android.hardware.Camera
// instance for a while, so if open() call called soon after, we can avoid
// the cost of open() in android.hardware.Camera.
//
// This is used in switching between Camera and VideoCamera activities.
//
public class CameraHolder {
	
    private static final String TAG = "CameraHolder";
    private android.hardware.Camera mCameraDevice;
    private long mKeepBeforeTime = 0;  // Keep the Camera before this time.
    private Handler mHandler;
    private int mUsers = 0;  // number of open() - number of release()

    // Use a singleton.
    private static CameraHolder sHolder;
    public static synchronized CameraHolder instance() {
        if (sHolder == null) {
            sHolder = new CameraHolder();
        }
        return sHolder;
    }

    private static final int RELEASE_CAMERA = 1;
    private class MyHandler extends Handler {
        MyHandler(Looper looper) {
            super(looper);
        }

        @Override
        public void handleMessage(Message msg) {
            switch(msg.what) {
                case RELEASE_CAMERA:
                    releaseCamera();
                    break;
            }
        }
    }

    public static void Assert(boolean cond) {
        if (!cond) {
            throw new AssertionError();
        }
    }

    private CameraHolder() {
        HandlerThread ht = new HandlerThread("CameraHolder");
        ht.start();
        mHandler = new MyHandler(ht.getLooper());
    }

    public synchronized android.hardware.Camera open() {
        Assert(mUsers == 0);
        if (mCameraDevice == null) {
            mCameraDevice = android.hardware.Camera.open();
        } else {
//            try {
//                mCameraDevice.reconnect();
//            } catch (IOException e) {
//                Log.e(TAG, "reconnect failed.");
//            }
        }
        ++mUsers;
        mHandler.removeMessages(RELEASE_CAMERA);
        mKeepBeforeTime = 0;
        return mCameraDevice;
    }

    public synchronized void release() {
        Assert(mUsers == 1);
        --mUsers;
        mCameraDevice.stopPreview();
        releaseCamera();
    }

    private synchronized void releaseCamera() {
        Assert(mUsers == 0);
        Assert(mCameraDevice != null);
        long now = System.currentTimeMillis();
        if (now < mKeepBeforeTime) {
            mHandler.sendEmptyMessageDelayed(RELEASE_CAMERA,
                    mKeepBeforeTime - now);
            return;
        }
        mCameraDevice.release();
        mCameraDevice = null;
    }

    public synchronized void keep() {
        // We allow (mUsers == 0) for the convenience of the calling activity.
        // The activity may not have a chance to call open() before the user
        // choose the menu item to switch to another activity.
        Assert(mUsers == 1 || mUsers == 0);
        // Keep the camera instance for 3 seconds.
        mKeepBeforeTime = System.currentTimeMillis() + 3000;
    }
}
