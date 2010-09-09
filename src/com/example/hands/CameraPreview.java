package com.example.hands;

import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

import org.siprop.opencv.OpenCV;

import android.app.ListActivity;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.hardware.Camera;
import android.hardware.Camera.*;
import android.util.AttributeSet;
import android.util.Log;
import android.view.KeyEvent;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.os.SystemClock;

public class CameraPreview extends SurfaceView implements
		SurfaceHolder.Callback {

	protected static final String TAG = "CameraPreview";
	protected SurfaceHolder mHolder;
	protected Camera mCamera;
	protected float mAspectRatio;
	protected boolean mPreview = false;
	protected DetectHand mDetectHand;
	protected int mHandNo = 0;

	public int getHandDetectNo() {
		return mHandNo;
	}
	
	public CameraPreview(Context context) {
		super(context);

		/* Make sure we get notified of changes to the service */
		mHolder = getHolder();
		mHolder.addCallback(this);
		mHolder.setType(SurfaceHolder.SURFACE_TYPE_PUSH_BUFFERS);
		mDetectHand = new DetectHand();
		mDetectHand.init();
		setFocusable(true);
	}

	public CameraPreview(Context context, AttributeSet attrs) {
		super(context, attrs);
		mHolder = getHolder();
		mHolder.addCallback(this);
		mHolder.setType(SurfaceHolder.SURFACE_TYPE_PUSH_BUFFERS);

		mDetectHand = new DetectHand();
		mDetectHand.init();
		setFocusable(true);
	}

	public CameraPreview(Context context, AttributeSet attrs, int defStyle) {
		super(context, attrs, defStyle);
		mHolder = getHolder();
		mHolder.addCallback(this);
		mHolder.setType(SurfaceHolder.SURFACE_TYPE_PUSH_BUFFERS);
		
		mDetectHand = new DetectHand();
		mDetectHand.init();
		setFocusable(true);
	}

	private Thread mThread = null;

	// private List<PreviewData> previewList = new ArrayList();
	PreviewData mPreviewData = null;

	private int no = 0;
	Object lock = new Object();
	private boolean skinHistgramFlag = false;
	double[] handDetectParamList = new double[3];
	private boolean dispCameraPreviewFlg = false;

	public boolean getDispCameraPreviewFlg() {
		return dispCameraPreviewFlg;
	}

	public void setDispCameraPreviewFlg(boolean dispCameraPreviewFlg) {
		this.dispCameraPreviewFlg = dispCameraPreviewFlg;
	}
	
	public void surfaceCreated(SurfaceHolder holder) {

		mCamera = CameraHolder.instance().open();
		mThread = new Thread(new Runnable() {

			@Override
			public void run() {
				while (mPreview) {
//					Log.i(TAG, "thread process");

					if ((mPreviewData != null) && (mPreviewData.srcImage != null)) {

//						Log.i(TAG, "exist previewData");

						if (skinHistgramFlag){
							Rect rect = new Rect();
							rect.top = 10;
							rect.bottom = rect.top + 100;
							rect.left = 300;
							rect.right = rect.left + 100;
							mDetectHand.setSkinHistgram(mPreviewData, rect);
							skinHistgramFlag = false;
						}
						
//						double[] list = new double[3];

//						long beginTime = SystemClock.uptimeMillis();
						mDetectHand.process(
								mPreviewData.srcImage,
								mPreviewData.width,
								mPreviewData.height,
								handDetectParamList);

						mHandNo = DetectHand.GUU;
						double val = handDetectParamList[DetectHand.GUU];
						if (val > handDetectParamList[DetectHand.CHOKI]){
							mHandNo = DetectHand.CHOKI;
							val = handDetectParamList[DetectHand.CHOKI];
						}
						if (val > handDetectParamList[DetectHand.PAA]){
							mHandNo = DetectHand.PAA;
							val = handDetectParamList[DetectHand.PAA];
						}
	
						
						setRefreshScreen(true);

						
//						long endTime = SystemClock.uptimeMillis();

						synchronized (lock) {
							mPreviewData = null;
						}
					}
					
					
					try {
						Thread.sleep(10);
					} catch (InterruptedException e) {
						// TODO Auto-generated catch block
						e.printStackTrace();
					}
				}
			}
		});

		
		try {
			mCamera.setPreviewCallback(new PreviewCallback() {

				public void onPreviewFrame(byte[] data, Camera camera) {
					// TODO Auto-generated method stub

					if (mPreview) {
						if (data != null) {
							Parameters params = camera.getParameters();
							Size size = params.getPreviewSize();
							
							if (mPreviewData == null) {
								synchronized (lock) {
									mPreviewData = new PreviewData();
									mPreviewData.srcImage = data;
									mPreviewData.height = size.height;
									mPreviewData.width = size.width;
									
									Log.i(TAG, "push previewList");
								}
							}
						}
					}
				}
			});
			mCamera.setPreviewDisplay(holder);
		} catch (IOException e) {
		}
	}
	
	public void surfaceChanged(SurfaceHolder hold, int format, int width,
			int height) {
		Camera.Parameters parameters = mCamera.getParameters();
		parameters.setPreviewSize(width, height);
		// parameters.setPreviewFrameRate(10);
		mCamera.setParameters(parameters);
		mCamera.startPreview();
		mDetectHand.start();
		mPreview = true;
		mThread.start();
	}

	public void surfaceDestroyed(SurfaceHolder holder) {

		mPreview = false;
		CameraHolder.instance().release();
		mDetectHand.stop();
		mDetectHand.close();
		
		try {
			mThread.join(1000);
			mThread = null;
		} catch (InterruptedException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
		
	}

	public Camera getCamera() {
		return mCamera;
	}

	public DetectHand getDetectHand(){
		return mDetectHand;
	}
	
	public void setAspectRatio(int width, int height) {
		setAspectRatio(((float) width) / ((float) height));
	}

	public void setAspectRatio(float aspectRatio) {
		if (mAspectRatio != aspectRatio) {
			mAspectRatio = aspectRatio;
			requestLayout();
			invalidate();
		}
	}

	public boolean onKeyDown(int keyCode, KeyEvent event){
		switch(keyCode){
		case KeyEvent.KEYCODE_C:
			skinHistgramFlag  = true;
			Log.d(TAG, "Push KeyEvent.KEYCODE_C");
			return true;
		case KeyEvent.KEYCODE_D:
			dispCameraPreviewFlg  = !dispCameraPreviewFlg;
			setRefreshScreen(true);
			Log.d(TAG, "Push KeyEvent.KEYCODE_D");
			return true;
		}
		return false;
	}

	private boolean mRefreshScreen = false;

	public boolean isRefreshScreen() {
		return mRefreshScreen;
	}

	public void setRefreshScreen(boolean mRefreshScreen) {
		this.mRefreshScreen = mRefreshScreen;
	}

	public double getHandDetectParam(int i) {
		if (i < 0 || 2 < i)
			return -1;
		
		return handDetectParamList[i];
	}
	
	
}
