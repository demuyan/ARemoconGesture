
#ifndef _DETECT_HAND_H
#define _DETECT_HAND_H

#include <stdlib.h>
#include <string.h>
#include <jni.h>
#include <android/log.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <errno.h>
#include <fcntl.h>


#include "cv.h"
#include "cxcore.h"
// #include "cvaux.h"
#include "highgui.h"
#include "ml.h"
#include "utils.h"
#include "WLNonFileByteStream.h"
#include "grfmt_bmp.h"

#include "JNIHelp.h"

#define LOG_TAG "Detect Hand"
#define DEV_NAME "Detect Hand"
#define LOGV(...) __android_log_print(ANDROID_LOG_SILENT, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

//ANDROID_LOG_UNKNOWN, ANDROID_LOG_DEFAULT, ANDROID_LOG_VERBOSE, ANDROID_LOG_DEBUG, ANDROID_LOG_INFO, ANDROID_LOG_WARN, ANDROID_LOG_ERROR, ANDROID_LOG_FATAL, ANDROID_LOG_SILENT
//LOGV(ANDROID_LOG_DEBUG, "JNI", "");

#define ANDROID_LOG_VERBOSE ANDROID_LOG_DEBUG
#define INVALID_ARGUMENT -18456

#define		SAFE_DELETE(p)			{ if(p){ delete (p); (p)=0; } }
#define		SAFE_DELETE_ARRAY(p)	{ if(p){ delete [](p); (p)=0; } }


#define IMAGE( i, x, y, n )   *(( unsigned char * )(( i )->imageData      \
                                    + ( x ) * sizeof( unsigned char ) * 3 \
                                    + ( y ) * ( i )->widthStep ) + ( n ))

//#define TouchDown (1)
//#define TouchUp   (2)
//#define TouchDrag (3)


//int uinput_create();
//void send_event(int fd, uint16_t type, uint16_t code, int32_t value);
//void initFinterTip();
//int sendAbsMovement(int x, int y);
//int sendAbsButton(int x, int y, int isDown);
//int sendAbsSyn();
//void convertPixcelYCbCr422ToRGB(jbyte y0, jbyte y1, jbyte cb0, jbyte cr0, jint* dst_buf);
//void GetMaskHSV(IplImage* src, IplImage* mask, TypeHSV* hsv);
//void convertYCbCrToRGB(jbyte* src_pixels, jint* dst_pixels, int width, int height);
//IplImage* getIplImageFromByteArray(JNIEnv* env,jbyteArray array_data,jint width,jint height);
//void setPixelRGB565ToIplImage(jshort* col, IplImage* image, int x, int y);

#ifdef __cplusplus
extern "C" {
#endif

//JNIEXPORT
//jboolean
//JNICALL
//Java_com_example_hands_DetectHand_initFeatHand( JNIEnv* env,
//                                                jobject thiz,
//                                                int _no,
//                                                jdoubleArray _featData);

JNIEXPORT
jint
JNICALL
Java_com_example_hands_DetectHand_getFeatSize( JNIEnv* env, jobject thiz);

JNIEXPORT
void
JNICALL
Java_com_example_hands_DetectHand_getFeatParams( JNIEnv* env,
                                           jobject thiz, 
                                           jobject imageData,
                                           jint width, 
                                           jint height, 
                                           jdoubleArray ary);

JNIEXPORT
jboolean
JNICALL
Java_com_example_hands_DetectHand_setSkinHistgram( JNIEnv* env, 
                                                   jobject thiz,
                                                   jobject imageData,
                                                   jint width,
                                                   jint height);



#ifdef __cplusplus
}
#endif

#endif






