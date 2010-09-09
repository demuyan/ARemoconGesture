/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.example.hands;

// import org.siprop.opencv.OpenCV;


import android.app.Activity;
import android.content.ComponentName;
import android.content.ContentResolver;
import android.content.Intent;
import android.content.ServiceConnection;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.hardware.*;
import android.hardware.Camera.Parameters;
import android.hardware.Camera.PictureCallback;
import android.hardware.Camera.PreviewCallback;
import android.hardware.Camera.Size;

import android.net.Uri;
import android.opengl.Visibility;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.RemoteException;

import android.provider.MediaStore.Images;
import android.view.Display;
import android.view.KeyEvent;
import android.view.MenuItem;
import android.view.SurfaceView;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.view.ViewGroup.LayoutParams;
import android.widget.ImageView;
import android.widget.TextView;

import android.util.Log;

public class MainActivity extends Activity {
	private static final String TAG = "MainActivity";
	private CameraPreview mPreview;
	private OverlayLayer mView;
	private CameraPreview mCameraPreview;
	private OverlayLayer mOverlayLayer;
	private TextView mTextView, mTextViewGuu, mTextViewChoki, mTextViewPaa;
	private Handler mHandler = new Handler();
	private Thread mThread;
	private ImageView mHandImage;
	private Bitmap guu_image;
	private Bitmap choki_image;
	private Bitmap paa_image;
	private Bitmap none_image;
	private ImageView mBlockImage;
	
	public void onCreate(Bundle icicle) {
		super.onCreate(icicle);
		Log.d(TAG, "onCreate");

		requestWindowFeature(Window.FEATURE_NO_TITLE);

//        mPreview = new CameraPreview(this);
//        setContentView(mPreview);
//        mView = new OverlayLayer(this);
//        addContentView(mView, new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT));  
//		getWindow().setFormat(PixelFormat.TRANSLUCENT);

		setContentView(R.layout.camera);
		
		mCameraPreview = (CameraPreview)findViewById(R.id.camera_preview);
		mOverlayLayer = (OverlayLayer)findViewById(R.id.overlay);
		mTextView = (TextView)findViewById(R.id.textview1);

		mTextViewGuu = (TextView)findViewById(R.id.textview2);
		mTextViewChoki = (TextView)findViewById(R.id.textview3);
		mTextViewPaa = (TextView)findViewById(R.id.textview4);

		mHandImage = (ImageView)findViewById(R.id.hand_image);
		mBlockImage = (ImageView)findViewById(R.id.black_bg);

        Bitmap skin = BitmapFactory.decodeResource(getResources(), R.drawable.skin);
        
        guu_image = BitmapFactory.decodeResource(getResources(), R.drawable.guudisp);
        choki_image = BitmapFactory.decodeResource(getResources(), R.drawable.chokidisp);
        paa_image = BitmapFactory.decodeResource(getResources(), R.drawable.paadisp);
        none_image = BitmapFactory.decodeResource(getResources(), R.drawable.nonedisp);

        DetectHand detectHand = mCameraPreview.getDetectHand();
        detectHand.setSkinHistgram(skin);

		mThread = new Thread(new Runnable() {
			protected String[] handDetectList = {"GUU","CHOKI","PAA"}; 
			int mHandDetectNo;
			double[] param = {99,99,99};
			
			@Override
			public void run() {
			
				while(true){
				try {
					mHandDetectNo = mCameraPreview.getHandDetectNo();
					for (int i = 0; i < 3; i++){
						param[i] =  mCameraPreview.getHandDetectParam(i);
					}
					mOverlayLayer.setHandDetectNo(mHandDetectNo);
					if (mCameraPreview.isRefreshScreen()){
						mHandler.post(new Runnable() {
							
							@Override
							public void run() {
//								String msg = "NONE";
//								if (mHandDetectNo > -1)
//									msg = handDetectList[mHandDetectNo];
//								mTextView.setText(msg);
								
								if (mCameraPreview.getDispCameraPreviewFlg())
								{
									mHandImage.setVisibility(View.VISIBLE);
									mBlockImage.setVisibility(View.VISIBLE);
								switch (mHandDetectNo) {
								case 0:
									mHandImage.setImageBitmap(guu_image);
									break;

								case 1:
									mHandImage.setImageBitmap(choki_image);
									break;

								case 2:
									mHandImage.setImageBitmap(paa_image);
									break;

								default:
									mHandImage.setImageBitmap(none_image);
									break;
								}
								
								mTextView.setText("");
								mTextViewGuu.setText("");
								mTextViewChoki.setText("");
								mTextViewPaa.setText("");

								} else {
									mHandImage.setVisibility(View.INVISIBLE);
									mBlockImage.setVisibility(View.INVISIBLE);
								}

								Log.d(TAG, "GUU:"+param[0]);
								Log.d(TAG, "CHOKI:"+param[1]);
								Log.d(TAG, "PAA:"+param[2]);
								
							}
						});
						mCameraPreview.setRefreshScreen(false);
					}
					
					Thread.sleep(100);
				} catch (InterruptedException e) {
					// TODO Auto-generated catch block
					e.printStackTrace();
				}
				}
			}
		});
		mThread.start();
		
	}
	
	
	
	
	
}
