package com.example.hands;

import java.util.List;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.os.Handler;
import android.util.AttributeSet;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;

public class OverlayLayer extends View {

	protected static final String TAG = "OverlayLayer";
	protected int handDetectNo = 0;
	protected Handler mHandler = new Handler();
	
	public int getHandDetectNo() {
		return handDetectNo;
	}

	public void setHandDetectNo(int handDetectNo) {
		this.handDetectNo = handDetectNo;
	}

	public OverlayLayer(Context context, AttributeSet attrs) {
		super(context, attrs);
	}

	protected void onDraw(Canvas canvas)  
	{  
		super.onDraw(canvas);  
		
		Rect rect = new Rect();
		rect.top = 10;
		rect.bottom = rect.top + 100;
		rect.left = 300;
		rect.right = rect.left + 100;
		
		/* Canvasの設定 */  
		canvas.drawColor(Color.TRANSPARENT);  
		
		/* 色指定 */  
		Paint mainPaint = new Paint();
		mainPaint.setStyle(Paint.Style.STROKE);  
		mainPaint.setARGB(255, 255, 0, 0); 
	          
		/* 描画 */
		canvas.drawRect(rect, mainPaint);

//		canvas.drawText(handDetectList[handDetectNo], 10, 100, mainPaint);
	}  

}
