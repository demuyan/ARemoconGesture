/*
OpenCV for Android NDK
Copyright (c) 2006-2009 SIProp Project http://www.siprop.org/

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it freely,
subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/
#include "cvjni.h"
#include <time.h>

#define THRESHOLD	10
#define THRESHOLD_MAX_VALUE	255

#define CONTOUR_MAX_LEVEL	1
#define LINE_THICKNESS	2
#define LINE_TYPE	8

#define HAAR_SCALE (1.4)
#define IMAGE_SCALE (2)
#define MIN_NEIGHBORS (2)
#define HAAR_FLAGS_SINGLE_FACE (0 | CV_HAAR_FIND_BIGGEST_OBJECT | CV_HAAR_DO_ROUGH_SEARCH)
#define HAAR_FLAGS_ALL_FACES (0)
// Other options we dropped out:
// CV_HAAR_DO_CANNY_PRUNING | CV_HAAR_SCALE_IMAGE
#define MIN_SIZE_WIDTH (20)
#define MIN_SIZE_HEIGHT (20)
#define PAD_FACE_SIZE (10)
#define PAD_FACE_AREA (40)
#define PAD_FACE_AREA_2 (PAD_FACE_AREA * 2)

// Initialize a socket capture to grab images from a socket connection.
JNIEXPORT
jboolean
JNICALL
Java_org_siprop_opencv_OpenCV_createSocketCapture(JNIEnv* env,
												  jobject thiz,
												  jstring address_str,
												  jstring port_str,
												  jint width,
												  jint height) {
	const char *address_chars = env->GetStringUTFChars(address_str, 0);
	if (address_chars == 0) {
		LOGV("Error loading socket address.");
		return false;
	}
	
	const char *port_chars = env->GetStringUTFChars(port_str, 0);
	if (port_chars == 0) {
		env->ReleaseStringUTFChars(address_str, address_chars);
		LOGV("Error loading socket port.");
		return false;
	}
													
	m_capture = cvCreateSocketCapture(address_chars, port_chars, width, height);
	env->ReleaseStringUTFChars(address_str, address_chars);
	env->ReleaseStringUTFChars(port_str, port_chars);
	if (m_capture == 0)
	{
		LOGV("Error creating socket capture.");
		return false;
	}
	
	return true;
}

JNIEXPORT
void
JNICALL
Java_org_siprop_opencv_OpenCV_releaseSocketCapture(JNIEnv* env,
												   jobject thiz) {
	if (m_capture) {
		cvReleaseCapture(&m_capture);
		m_capture = 0;
	}
}

JNIEXPORT
jboolean
JNICALL
Java_org_siprop_opencv_OpenCV_grabSourceImageFromCapture(JNIEnv* env,
														 jobject thiz) {
	if (m_capture == 0)
	{
		LOGE("Capture was never initialized.");
		return false;
	}
	
	if (cvGrabFrame(m_capture) == 0)
	{
		LOGE("Failed to grab frame from the capture.");
		return false;
	}
	
	IplImage *frame = cvRetrieveFrame(m_capture);
	if (frame == 0)
	{
		LOGE("Failed to retrieve frame from the capture.");
		return false;
	}
	
	if (m_sourceImage) {
		cvReleaseImage(&m_sourceImage);
		m_sourceImage = 0;
	}
	
	m_sourceImage = cvCreateImage(cvGetSize(frame), IPL_DEPTH_8U, 
		frame->nChannels);
	
	// Check the origin of image. If top left, copy the image frame to frame_copy.
	// Else flip and copy the image.
	if (frame->origin == IPL_ORIGIN_TL) {
	    cvCopy(frame, m_sourceImage, 0);
	}
	else {
	    cvFlip(frame, m_sourceImage, 0);
	}
	
	return true;
}

// Generate and return a boolean array from the source image.
// Return 0 if a failure occurs or if the source image is undefined.
JNIEXPORT
jbooleanArray
JNICALL
Java_org_siprop_opencv_OpenCV_getSourceImage(JNIEnv* env,
									    	 jobject thiz)
{
	if (m_sourceImage == 0) {
		LOGE("Error source image was not set.");
		return 0;
	}
	
	CvMat stub;
    CvMat *mat_image = cvGetMat(m_sourceImage, &stub);
    int channels = CV_MAT_CN( mat_image->type );
    int ipl_depth = cvCvToIplDepth(mat_image->type);

	WLNonFileByteStream *strm = new WLNonFileByteStream();
    loadImageBytes(mat_image->data.ptr, mat_image->step, mat_image->width,
		mat_image->height, ipl_depth, channels, strm);
	
	int imageSize = strm->GetSize();
	jbooleanArray res_array = env->NewBooleanArray(imageSize);
    if (res_array == 0) {
		LOGE("Unable to allocate a new boolean array for the source image.");
        return 0;
    }
    env->SetBooleanArrayRegion(res_array, 0, imageSize, (jboolean*)strm->GetByte());
	
	strm->Close();
	SAFE_DELETE(strm);
	
	return res_array;
}

// Given an integer array of image data, load an IplImage.
// It is the responsibility of the caller to release the IplImage.
IplImage* getIplImageFromIntArray(JNIEnv* env, jintArray array_data, 
								  jint width, jint height) {
	// Load Image
	int *pixels = env->GetIntArrayElements(array_data, 0);
	if (pixels == 0) {
		LOGE("Error getting int array of pixels.");
		return 0;
	}
	
	IplImage *image = loadPixels(pixels, width, height);
	env->ReleaseIntArrayElements(array_data, pixels, 0);
	if(image == 0) {
		LOGE("Error loading pixel array.");
		return 0;
	}
	
	return image;
}


#define	YCbCrtoR(Y,Cb,Cr)	(1000*Y + 1371*(Cr-128))/1000
#define	YCbCrtoG(Y,Cb,Cr)	(1000*Y - 336*(Cb-128) - 698*(Cr-128))/1000
#define	YCbCrtoB(Y,Cb,Cr)	(1000*Y + 1732*(Cb-128))/1000

void convertPixcelYCbCr422ToRGB(jbyte y0, jbyte y1, jbyte cb0, jbyte cr0, jint* dst_buf)
{
	// bit order is
	// YCbCr = [Cr0 Y1 Cb0 Y0], RGB=[R1,G1,B1,R0,G0,B0].

	jint r0, g0, b0, r1, g1, b1;
	jint rgb0, rgb1;
	jint rgb;

	#if 1 // 4 frames/s @192MHz, 12MHz ; 6 frames/s @450MHz, 12MHz
	r0 = YCbCrtoR(y0, cb0, cr0);
	g0 = YCbCrtoG(y0, cb0, cr0);
	b0 = YCbCrtoB(y0, cb0, cr0);
	r1 = YCbCrtoR(y1, cb0, cr0);
	g1 = YCbCrtoG(y1, cb0, cr0);
	b1 = YCbCrtoB(y1, cb0, cr0);
	#endif

	if (r0>255 ) r0 = 255;
	if (r0<0) r0 = 0;
	if (g0>255 ) g0 = 255;
	if (g0<0) g0 = 0;
	if (b0>255 ) b0 = 255;
	if (b0<0) b0 = 0;

	if (r1>255 ) r1 = 255;
	if (r1<0) r1 = 0;
	if (g1>255 ) g1 = 255;
	if (g1<0) g1 = 0;
	if (b1>255 ) b1 = 255;
	if (b1<0) b1 = 0;

	// ARGB 32bitbit format
	dst_buf[0] = (((jshort)r0)<<24) | (((jshort)g0)<<16) | b0;	//RGB888.
	dst_buf[1] = (((jshort)r1)<<24) | (((jshort)g1)<<16) | b1;	//RGB888.

}

void convertYCbCrToRGB(jbyte* src_pixels, jint* dst_pixels, int width, int height)
{
	int len = width * height / 2;

	for (int i = 0; i < len; i++){

		jbyte y0 =  src_pixels[i*4+0];
		jbyte y1 =  src_pixels[i*4+1];
		jbyte cb0 = src_pixels[i*4+2];
		jbyte cr0 = src_pixels[i*4+3];

		convertPixcelYCbCr422ToRGB(y0, y1, cb0, cr0, (jint*)&dst_pixels[i*2]);
	}
}

IplImage* dummyPixels(int width, int height) {

	int x, y, pos, int_size = sizeof(int);
	IplImage *img = cvCreateImage(cvSize(width, height), IPL_DEPTH_8U, 3);

	for ( y = 0; y < height; y++ ) {
		pos = y * width * int_size;
        for ( x = 0; x < width; x++, pos += int_size ) {
            // blue
            IMAGE( img, x, y, 0 ) = 0x80;
            // green
            IMAGE( img, x, y, 1 ) = 0x80;
            // red
            IMAGE( img, x, y, 2 ) = 0x80;
        }
    }

	return img;
}



// Given an integer array of image data, load an IplImage.
// It is the responsibility of the caller to release the IplImage.
IplImage* getIplImageFromByteArray(JNIEnv* env,
								  jbyteArray array_data,
								  jint width,
								  jint height) {

	// Load Image
	jbyte* src_pixels = env->GetByteArrayElements(array_data, 0);
	if (src_pixels == 0) {
		LOGE("Error getting byte array of pixels.");
		return 0;
	}

	int image_size = width * height * 2;
	jintArray dst_buf = env->NewIntArray(image_size);
	jint* dst_pixels = env->GetIntArrayElements(dst_buf,0);

////	if (pixel_format == 16) // YCbCr422
////	{
		convertYCbCrToRGB(src_pixels, dst_pixels, width, height);
////	}

	IplImage *image = loadPixels(dst_pixels, width, height);
//	IplImage *image = dummyPixels(width,height);

	env->ReleaseIntArrayElements(dst_buf, dst_pixels, JNI_ABORT);
	env->DeleteLocalRef(dst_buf);
	env->ReleaseByteArrayElements(array_data, src_pixels, JNI_ABORT);

	if(image == 0) {
		LOGE("Error loading pixel array.");
		return 0;
	}

	return image;
}


// Set the source image and return true if successful or false otherwise.
JNIEXPORT
jboolean
JNICALL
Java_org_siprop_opencv_OpenCV_setSourceImage(JNIEnv* env,
											 jobject thiz,
											 jbyteArray photo_data,
											 jint width,
											 jint height,
											 jint pixel_format)
{	
	// Release the image if it hasn't already been released.
	if (m_sourceImage) {
		cvReleaseImage(&m_sourceImage);
		m_sourceImage = 0;
	}
	m_facesFound = 0;
	
	m_sourceImage = getIplImageFromByteArray(env, photo_data, width, height);
	if (m_sourceImage == 0) {
		LOGE("Error source image could not be created.");
		return false;
	}
	
	return true;
}

JNIEXPORT
jbooleanArray
JNICALL
Java_org_siprop_opencv_OpenCV_findContours(JNIEnv* env,
										jobject thiz,
										jint width,
										jint height) {
	IplImage *grayImage = cvCreateImage( cvGetSize(m_sourceImage), IPL_DEPTH_8U, 1 );
	IplImage *binaryImage = cvCreateImage( cvGetSize(m_sourceImage), IPL_DEPTH_8U, 1 );
	IplImage *contourImage = cvCreateImage( cvGetSize(m_sourceImage), IPL_DEPTH_8U, 3 );

	cvCvtColor( m_sourceImage, grayImage, CV_BGR2GRAY );

	cvThreshold( grayImage, binaryImage, THRESHOLD, THRESHOLD_MAX_VALUE, CV_THRESH_BINARY );


	CvMemStorage* storage = cvCreateMemStorage( 0 );
	CvSeq* find_contour = 0;


	int find_contour_num = cvFindContours( 
		binaryImage,
		storage,
		&find_contour,
		sizeof( CvContour ),
		CV_RETR_LIST,
		CV_CHAIN_APPROX_NONE,
		cvPoint( 0, 0 )
	);

	CvScalar red = CV_RGB( 255, 0, 0 );
	cvDrawContours( 
		m_sourceImage,
		find_contour,
		red,
		red,
		CONTOUR_MAX_LEVEL,
		LINE_THICKNESS,
		LINE_TYPE,
		cvPoint( 0, 0 )
	);   

	int imageSize;
	CvMat stub, *mat_image;
    int channels, ipl_depth;
    mat_image = cvGetMat( m_sourceImage, &stub );
    channels = CV_MAT_CN( mat_image->type );

    ipl_depth = cvCvToIplDepth(mat_image->type);

	LOGV("Load loadImageBytes.");
	WLNonFileByteStream* strm = new WLNonFileByteStream();
    loadImageBytes(mat_image->data.ptr, mat_image->step, mat_image->width,
                             mat_image->height, ipl_depth, channels, strm);

	imageSize = strm->GetSize();
	jbooleanArray res_array = env->NewBooleanArray(imageSize);
	LOGV("Load NewBooleanArray.");
    if (res_array == 0) {
        return 0;
    }
    env->SetBooleanArrayRegion(res_array, 0, imageSize, (jboolean*)strm->GetByte());
	LOGV("Load SetBooleanArrayRegion.");

	LOGV("Release sourceImage");
	if (m_sourceImage) {
		cvReleaseImage(&m_sourceImage);
		m_sourceImage = 0;
	}
	LOGV("Release binaryImage");
	cvReleaseImage( &binaryImage );
	LOGV("Release grayImage");
	cvReleaseImage( &grayImage );
	LOGV("Release contourImage");
	cvReleaseImage( &contourImage );
	LOGV("Release storage");
	cvReleaseMemStorage( &storage );
	LOGV("Delete strm");
	strm->Close();
	SAFE_DELETE(strm);

	return res_array;
}

JNIEXPORT
jboolean
JNICALL
Java_org_siprop_opencv_OpenCV_initFaceDetection(JNIEnv* env,
												jobject thiz,
												jstring cascade_path_str) {
	
	// First call release to ensure the memory is empty.
	Java_org_siprop_opencv_OpenCV_releaseFaceDetection(env, thiz);
												
	char buffer[100];
	clock_t total_time_start = clock();
	
	m_smallestFaceSize.width = MIN_SIZE_WIDTH;
	m_smallestFaceSize.height = MIN_SIZE_HEIGHT;
	
	const char *cascade_path_chars = env->GetStringUTFChars(cascade_path_str, 0);
	if (cascade_path_chars == 0) {
		LOGE("Error getting cascade string.");
		return false;
	}
	
	m_cascade = (CvHaarClassifierCascade*)cvLoad(cascade_path_chars);
	env->ReleaseStringUTFChars(cascade_path_str, cascade_path_chars);
	if (m_cascade == 0) {
		LOGE("Error loading cascade.");
		return false;
	}
	
	m_storage = cvCreateMemStorage(0);
	
	clock_t total_time_finish = clock() - total_time_start;
	sprintf(buffer, "Total Time to init: %f", (double)total_time_finish / (double)CLOCKS_PER_SEC);
	LOGV(buffer);
	
	return true;
}

// Release all of the memory used by face tracking.
JNIEXPORT
void
JNICALL
Java_org_siprop_opencv_OpenCV_releaseFaceDetection(JNIEnv* env,
												   jobject thiz) {
											
	m_facesFound = 0;
	m_faceCropArea.width = m_faceCropArea.height = 0;
	
	if (m_cascade) {
		cvReleaseHaarClassifierCascade(&m_cascade);
		m_cascade = 0;
	}
	
	if (m_sourceImage) {
		cvReleaseImage(&m_sourceImage);
		m_sourceImage = 0;
	}
	
	if (m_grayImage) {
		cvReleaseImage(&m_grayImage);
		m_grayImage = 0;
	}
	
	if (m_smallImage) {
		cvReleaseImage(&m_smallImage);
		m_smallImage = 0;
	}
	
	if (m_storage) {
		cvReleaseMemStorage(&m_storage);
		m_storage = 0;
	}
}

// Initalize the small image and the gray image using the input source image.
// If a previous face was specified, we will limit the ROI to that face.
void initFaceDetectionImages(IplImage *sourceImage, double scale = 1.0) {
	if (m_grayImage == 0) {
		m_grayImage = cvCreateImage(cvGetSize(sourceImage), IPL_DEPTH_8U, 1);
	}
	
	if (m_smallImage == 0) {
		m_smallImage = cvCreateImage(cvSize(cvRound(sourceImage->width / scale), 
			cvRound(sourceImage->height / scale)), IPL_DEPTH_8U, 1);
	}
	
	if(m_faceCropArea.width > 0 && m_faceCropArea.height > 0) {
		cvSetImageROI(m_smallImage, m_faceCropArea);
	
		CvRect tPrev = cvRect(m_faceCropArea.x * scale, m_faceCropArea.y * scale, 
			m_faceCropArea.width * scale, m_faceCropArea.height * scale);
		cvSetImageROI(sourceImage, tPrev);
		cvSetImageROI(m_grayImage, tPrev);
	} else {
		cvResetImageROI(m_smallImage);
		cvResetImageROI(m_grayImage);
	}
	
    cvCvtColor(sourceImage, m_grayImage, CV_BGR2GRAY);
    cvResize(m_grayImage, m_smallImage, CV_INTER_LINEAR);
    cvEqualizeHist(m_smallImage, m_smallImage);
	cvClearMemStorage(m_storage);
	
	cvResetImageROI(sourceImage);
}

// Given a sequence of rectangles, return an array of Android Rect objects
// or null if any errors occur.
jobjectArray seqRectsToAndroidRects(JNIEnv* env, CvSeq *rects) {
	if (rects == 0 || rects->total <= 0) {
		LOGE("No rectangles were specified!");
		return 0;
	}
	
	jclass jcls = env->FindClass("android/graphics/Rect");
	if (jcls == 0) {
		LOGE("Unable to find class android.graphics.Rect");
		return 0;
	}
	
	jmethodID jconstruct = env->GetMethodID(jcls, "<init>", "(IIII)V");
	if (jconstruct == 0) {
		LOGE("Unable to find constructor Rect(int, int, int, int)");
		return 0;
	}
	
	jobjectArray ary = env->NewObjectArray(rects->total, jcls, 0);
	if (ary == 0) {
		LOGE("Unable to create Rect array");
		return 0;
	}
	
	for (int i = 0; i < rects->total; i++) {
		char buffer[100];
		CvRect *rect = (CvRect*)cvGetSeqElem(rects, i);
		if (rect == 0) {
			sprintf(buffer, "Invalid Rectangle #%d", i);
			LOGE(buffer);
			return 0;
		}
		
		jobject jrect = env->NewObject(jcls, jconstruct, rect->x, rect->y, 
			rect->width, rect->height);
		if (jrect == 0) {
			sprintf(buffer, "Unable to create Android Rect object for rectangle #%d", i);
			LOGE(buffer);
			return 0;
		}
		
		env->SetObjectArrayElement(ary, i, jrect);
		env->DeleteLocalRef(jrect);
	}
	
	return ary;
}

// Identify all of the faces in the source image and return an array
// of Android Rect objects with the face coordinates.  If any errors
// occur, a 0 array will be returned.
JNIEXPORT
jobjectArray
JNICALL
Java_org_siprop_opencv_OpenCV_findAllFaces(JNIEnv* env,
									       jobject thiz) {
	char buffer[100];
	clock_t total_time_start = clock();
	
	if (m_cascade == 0 || m_storage == 0) {
		LOGE("Error find faces was not initialized.");
		return 0;
	}
	
	if (m_sourceImage == 0) {
		LOGE("Error source image was not set.");
		return 0;
	}
	
	initFaceDetectionImages(m_sourceImage, IMAGE_SCALE);

	clock_t haar_detect_time_start = clock();
    m_facesFound = mycvHaarDetectObjects(m_smallImage, m_cascade, m_storage, HAAR_SCALE, 
		MIN_NEIGHBORS, HAAR_FLAGS_ALL_FACES, cvSize(MIN_SIZE_WIDTH, MIN_SIZE_HEIGHT));
		
	clock_t haar_detect_time_finish = clock() - haar_detect_time_start;
	sprintf(buffer, "Total Time to cvHaarDetectObjects in findAllFaces: %f", (double)haar_detect_time_finish / (double)CLOCKS_PER_SEC);
	LOGV(buffer);
	
	jobjectArray faceRects = 0;
	if (m_facesFound == 0 || m_facesFound->total <= 0) {
		LOGV("FACES_DETECTED 0");
	} else {
		sprintf(buffer, "FACES_DETECTED %d", m_facesFound->total);
		LOGV(buffer);
		m_faceCropArea.width = m_faceCropArea.height = 0;
		faceRects = seqRectsToAndroidRects(env, m_facesFound);
	}
	
	clock_t total_time_finish = clock() - total_time_start;
	sprintf(buffer, "Total Time to findAllFaces: %f", (double)total_time_finish / (double)CLOCKS_PER_SEC);
	LOGV(buffer);
	
	return faceRects;
}

// Store the previous face found in the scene.
void storePreviousFace(CvRect* face) {
	char buffer[100];
	if (m_faceCropArea.width > 0 && m_faceCropArea.height > 0) {
		face->x += m_faceCropArea.x;
		face->y += m_faceCropArea.y;
		sprintf(buffer, "Face rect + m_faceCropArea: (%d, %d) to (%d, %d)", face->x, face->y, 
			face->x + face->width, face->y + face->height);
		LOGV(buffer);
	}
	
	int startX = MAX(face->x - PAD_FACE_AREA, 0);
	int startY = MAX(face->y - PAD_FACE_AREA, 0);
	int w = m_smallImage->width - startX - face->width - PAD_FACE_AREA_2;
	int h = m_smallImage->height - startY - face->height - PAD_FACE_AREA_2;
	int sw = face->x - PAD_FACE_AREA, sh = face->y - PAD_FACE_AREA;
	m_faceCropArea = cvRect(startX, startY, 
		face->width + PAD_FACE_AREA_2 + ((w < 0) ? w : 0) + ((sw < 0) ? sw : 0),
		face->height + PAD_FACE_AREA_2 + ((h < 0) ? h : 0) + ((sh < 0) ? sh : 0));
	sprintf(buffer, "m_faceCropArea: (%d, %d) to (%d, %d)", m_faceCropArea.x, m_faceCropArea.y, 
		m_faceCropArea.x + m_faceCropArea.width, m_faceCropArea.y + m_faceCropArea.height);
	LOGV(buffer);
}

// Given a rectangle, return an Android Rect object or null if any 
// errors occur.
jobject rectToAndroidRect(JNIEnv* env, CvRect *rect) {
	if (rect == 0) {
		LOGE("No rectangle was specified!");
		return 0;
	}
	
	jclass jcls = env->FindClass("android/graphics/Rect");
	if (jcls == 0) {
		LOGE("Unable to find class android.graphics.Rect");
		return 0;
	}
	
	jmethodID jconstruct = env->GetMethodID(jcls, "<init>", "(IIII)V");
	if (jconstruct == 0) {
		LOGE("Unable to find constructor Rect(int, int, int, int)");
		return 0;
	}
	
	return env->NewObject(jcls, jconstruct, rect->x, rect->y, 
		rect->width, rect->height);
}

// Identify a single face in the source image and return an Android
// Android Rect object with the face coordinates.  This method is
// optimized by focusing on a single face and cropping the detection
// region to the area where the face is located plus some additional
// padding to account for slight head movements.  If any errors occur, 
// a 0 array will be returned.
JNIEXPORT
jobject
JNICALL
Java_org_siprop_opencv_OpenCV_findSingleFace(JNIEnv* env,
											 jobject thiz) {
	char buffer[100];
	clock_t total_time_start = clock();
	
	if (m_cascade == 0 || m_storage == 0) {
		LOGE("Error find faces was not initialized.");
		return 0;
	}
	
	if (m_sourceImage == 0) {
		LOGE("Error source image was not "
				"set.");
		return 0;
	}
	
	initFaceDetectionImages(m_sourceImage, IMAGE_SCALE);

	clock_t haar_detect_time_start = clock();
    m_facesFound = mycvHaarDetectObjects(m_smallImage, m_cascade, m_storage, HAAR_SCALE, 
		MIN_NEIGHBORS, HAAR_FLAGS_SINGLE_FACE, m_smallestFaceSize);
		
	clock_t haar_detect_time_finish = clock() - haar_detect_time_start;
	sprintf(buffer, "Total Time to cvHaarDetectObjects in findSingleFace: %f", (double)haar_detect_time_finish / (double)CLOCKS_PER_SEC);
	LOGV(buffer);
	
	jobject faceRect = 0;
	if (m_facesFound == 0 || m_facesFound->total <= 0) {
		LOGV("FACES_DETECTED 0");
		m_faceCropArea.width = m_faceCropArea.height = 0;
		m_smallestFaceSize.width = MIN_SIZE_WIDTH;
		m_smallestFaceSize.height = MIN_SIZE_HEIGHT;
	} else {
		LOGV("FACES_DETECTED 1");
		CvRect *face = (CvRect*)cvGetSeqElem(m_facesFound, 0);
		if (face == 0) {
			LOGE("Invalid rectangle detected");
			return 0;
		}
		m_smallestFaceSize.width = MAX(face->width - PAD_FACE_SIZE, MIN_SIZE_WIDTH);
		m_smallestFaceSize.height = MAX(face->height - PAD_FACE_SIZE, MIN_SIZE_HEIGHT);
		faceRect = rectToAndroidRect(env, face);
		storePreviousFace(face);
	}
	
	clock_t total_time_finish = clock() - total_time_start;
	sprintf(buffer, "Total Time to findSingleFace: %f", (double)total_time_finish / (double)CLOCKS_PER_SEC);
	LOGV(buffer);
	
	return faceRect;
}

// Draw a rectangle on the source image around the specified face rectangle.
// Scale the face area to the draw area based on the specified scale.
void highlightFace(IplImage *sourceImage, CvRect *face, double scale = 1.0) {
	char buffer[100];
	sprintf(buffer, "Face Rectangle: (x: %d, y: %d) to (w: %d, h: %d)", 
		face->x, face->y, face->width, face->height);
	LOGV(buffer);
	CvPoint pt1 = cvPoint(int(face->x * scale), int(face->y * scale));
	CvPoint pt2 = cvPoint(int((face->x + face->width) * scale), 
		int((face->y + face->height) * scale));
	sprintf(buffer, "Draw Rectangle: (%d, %d) to (%d, %d)", pt1.x, pt1.y, pt2.x, pt2.y);
	LOGV(buffer);
    cvRectangle(sourceImage, pt1, pt2, CV_RGB(255, 0, 0), 3, 8, 0);
}

// Draw rectangles on the source image around each face that was found.
// Scale the face area to the draw area based on the specified scale.
// Return true if at least one face was highlighted and false otherwise.
bool highlightFaces(IplImage *sourceImage, CvSeq *faces, double scale = 1.0) {
	if (faces == 0 || faces->total <= 0) {
		LOGV("No faces were highlighted!");
		return false;
	} else {
		LOGV("Drawing rectangles on each face");
		int count;
		CvRect* face;
		for (int i = 0; i < faces->total; i++) {
			face = (CvRect*)cvGetSeqElem(faces, i);
			highlightFace(sourceImage, face, scale);
		}
	}
	
	return true;
}

// Highlight the faces that were detected in the source image.
// Return true if one or more faces is highlighted or false otherwise.
JNIEXPORT
jboolean
JNICALL
Java_org_siprop_opencv_OpenCV_highlightFaces(JNIEnv* env,
									    	 jobject thiz) {
	if (m_facesFound == 0 || m_facesFound->total <= 0) {
		LOGV("No faces found to highlight!");
		return false;
	} else {
		highlightFaces(m_sourceImage, m_facesFound, IMAGE_SCALE);
	}
	
	return true;
}

//JNIEXPORT
//jboolean
//JNICALL
//Java_org_siprop_opencv_OpenCV_samples(JNIEnv* env,
//									 jobject thiz
//){
//	return false;
//}

void GetMaskHSV(IplImage* src, IplImage* mask, TypeHSV* hsv);

JNIEXPORT
jintArray
JNICALL
Java_org_siprop_opencv_OpenCV_fingerTipRegion(JNIEnv* env,
										jobject thiz,
										jbyteArray src_frame, //	jintArray current_frame,
										jint width,
										jint height) {

	TypeHSV blueHSV,orangeHSV;

	blueHSV.minH = 25;	blueHSV.maxH = 90;
	blueHSV.minS = 30;	blueHSV.maxS = 140;
	blueHSV.minV = 40;	blueHSV.maxV = 215;

	orangeHSV.minH = 111;	orangeHSV.maxH = 115;
	orangeHSV.minS = 210;	orangeHSV.maxS = 255;
	orangeHSV.minV = 30;	orangeHSV.maxV = 255;

    IplImage *  draw_image  = cvCreateImage(cvSize (width, height), IPL_DEPTH_8U, 3);

	// Load Image
	IplImage * current_frame = getIplImageFromByteArray(env, src_frame, width, height);

	IplImage* mask_blue   = cvCreateImage(cvSize (width, height), IPL_DEPTH_8U, 3);
	IplImage* mask_orange = cvCreateImage(cvSize (width, height), IPL_DEPTH_8U, 3);

    // draw faces
    cvFlip (current_frame, draw_image, 1);
	cvReleaseImage( &current_frame );

	GetMaskHSV(draw_image, mask_blue, &blueHSV);
	GetMaskHSV(draw_image, mask_orange, &orangeHSV);

	cvReleaseImage( &draw_image );

	IplImage* mask = cvCreateImage(cvSize(width, height), IPL_DEPTH_8U, 3);

	cvOr(mask_blue,mask_orange,mask);

	cvReleaseImage( &mask_blue );
	cvReleaseImage( &mask_orange );

	IplImage *chB=cvCreateImage(cvGetSize(mask),8,1);
	IplImage *labelImg = cvCreateImage(cvGetSize(chB),IPL_DEPTH_LABEL,1);

	cvSplit(mask,chB,NULL,NULL,NULL);

	CvBlobs blobs;
	cvLabel(chB, labelImg, blobs);

	for (CvBlobs::const_iterator it=blobs.begin(); it!=blobs.end(); ++it)
	{
		CvBlob *blob=(*it).second;

		if ( (blob->maxx - blob->minx) > 10 && (blob->maxy - blob->miny) > 10){
			LOGV("blob hit");

//			cvRectangle(mask,cvPoint(blob->minx,blob->miny),cvPoint(blob->maxx,blob->maxy),CV_RGB(255.,0.,0.));
		}
	}

	cvReleaseImage( &mask );
	cvReleaseImage( &chB );
	cvReleaseImage( &labelImg );

	jintArray result;
	int size = 10;
	result = env->NewIntArray(size);
	if (result == NULL) {
	     return NULL; /* out of memory error thrown */
	}

	int i;
	// fill a temp structure to use to populate the java int array
	jint fill[256];
	for (i = 0; i < size; i++) {
		fill[i] = i+10; // put whatever logic you want to populate the values here.
	}
	// move from the temp structure to the java structure
	env->SetIntArrayRegion(result, 0, size, fill);
	return result;

}

void GetMaskHSV(IplImage* src, IplImage* mask, TypeHSV* hsv)
{
	int x = 0, y = 0;
	uchar H, S, V;

	CvPixelPosition8u pos_src, pos_dst;
	uchar* p_src;
	uchar* p_dst;
	IplImage* tmp;

	tmp = cvCreateImage(cvGetSize(src), IPL_DEPTH_8U, 3);


	cvCvtColor(src, tmp, CV_RGB2HSV);

	CV_INIT_PIXEL_POS(pos_src, (unsigned char*) tmp->imageData,
					  tmp->widthStep,cvGetSize(tmp), x, y, tmp->origin);

	CV_INIT_PIXEL_POS(pos_dst, (unsigned char*) mask->imageData,
					  mask->widthStep, cvGetSize(mask), x, y, mask->origin);

	for(y = 0; y < tmp->height; y++) {
		for(x = 0; x < tmp->width; x++) {
			p_src = CV_MOVE_TO(pos_src, x, y, 3);
			p_dst = CV_MOVE_TO(pos_dst, x, y, 3);

			H = p_src[0];
			S = p_src[1];
			V = p_src[2];

			if( hsv->minH <= H && H <= hsv->maxH &&
			   hsv->minS <= S && S <= hsv->maxS &&
			   hsv->minV <= V && V <= hsv->maxV
			   ) {
				p_dst[0] = 255;
				p_dst[1] = 255;
				p_dst[2] = 255;
			} else {
				p_dst[0] = 0;
				p_dst[1] = 0;
				p_dst[2] = 0;
			}
		}
	}

//	if(erosions > 0)  cvErode(mask, mask, 0, erosions);
//	if(dilations > 0) cvDilate(mask, mask, 0, dilations);

	cvReleaseImage(&tmp);
}


#if 0

JNIEXPORT
jbooleanArray
JNICALL
Java_org_siprop_opencv_OpenCV_faceDetect(JNIEnv* env,
										jobject thiz,
										jintArray photo_data1,
										jintArray photo_data2,
										jint width,
										jint height) {
	LOGV("Load desp.");

	int i, x, y;
	int* pixels;
	IplImage *frameImage;
	
	IplImage *backgroundImage = cvCreateImage( cvSize(width, height), IPL_DEPTH_8U, 1 );
	IplImage *grayImage = cvCreateImage( cvSize(width, height), IPL_DEPTH_8U, 1 );
	IplImage *differenceImage = cvCreateImage( cvSize(width, height), IPL_DEPTH_8U, 1 );
	
	IplImage *hsvImage = cvCreateImage( cvSize(width, height), IPL_DEPTH_8U, 3 );
	IplImage *hueImage = cvCreateImage( cvSize(width, height), IPL_DEPTH_8U, 1 );
	IplImage *saturationImage = cvCreateImage( cvSize(width, height), IPL_DEPTH_8U, 1 );
	IplImage *valueImage = cvCreateImage( cvSize(width, height), IPL_DEPTH_8U, 1 );
	IplImage *thresholdImage1 = cvCreateImage( cvSize(width, height), IPL_DEPTH_8U, 1 );
	IplImage *thresholdImage2 = cvCreateImage( cvSize(width, height), IPL_DEPTH_8U, 1 );
	IplImage *thresholdImage3 = cvCreateImage( cvSize(width, height), IPL_DEPTH_8U, 1 );
	IplImage *faceImage = cvCreateImage( cvSize(width, height), IPL_DEPTH_8U, 1 );
	
	CvMoments moment;
	double m_00;
	double m_10;
	double m_01;
	int gravityX;
	int gravityY;

	jbooleanArray res_array;
	int imageSize;



	// Load Image
	pixels = env->GetIntArrayElements(photo_data1, 0);
	frameImage = loadPixels(pixels, width, height);
	if(frameImage == 0) {
		LOGV("Error loadPixels.");
		return 0;
	}
	
	
	cvCvtColor( frameImage, backgroundImage, CV_BGR2GRAY );
	
	
	pixels = env->GetIntArrayElements(photo_data2, 0);
	frameImage = loadPixels(pixels, width, height);
	if(frameImage == 0) {
		LOGV("Error loadPixels.");
		return 0;
	}
	cvCvtColor( frameImage, grayImage, CV_BGR2GRAY );
	cvAbsDiff( grayImage, backgroundImage, differenceImage );
	
	cvCvtColor( frameImage, hsvImage, CV_BGR2HSV );
	LOGV("Load cvCvtColor.");
	cvSplit( hsvImage, hueImage, saturationImage, valueImage, 0 );
	LOGV("Load cvSplit.");
	cvThreshold( hueImage, thresholdImage1, THRESH_BOTTOM, THRESHOLD_MAX_VALUE, CV_THRESH_BINARY );
	cvThreshold( hueImage, thresholdImage2, THRESH_TOP, THRESHOLD_MAX_VALUE, CV_THRESH_BINARY_INV );
	cvAnd( thresholdImage1, thresholdImage2, thresholdImage3, 0 );
	LOGV("Load cvAnd.");
	
	cvAnd( differenceImage, thresholdImage3, faceImage, 0 );
	
	cvMoments( faceImage, &moment, 0 );
	m_00 = cvGetSpatialMoment( &moment, 0, 0 );
	m_10 = cvGetSpatialMoment( &moment, 1, 0 );
	m_01 = cvGetSpatialMoment( &moment, 0, 1 );
	gravityX = m_10 / m_00;
	gravityY = m_01 / m_00;
	LOGV("Load cvMoments.");


	cvCircle( frameImage, cvPoint( gravityX, gravityY ), CIRCLE_RADIUS,
		 CV_RGB( 255, 0, 0 ), LINE_THICKNESS, LINE_TYPE, 0 );




	CvMat stub, *mat_image;
    int channels, ipl_depth;
    mat_image = cvGetMat( frameImage, &stub );
    channels = CV_MAT_CN( mat_image->type );

    ipl_depth = cvCvToIplDepth(mat_image->type);

	WLNonFileByteStream* m_strm = new WLNonFileByteStream();
    loadImageBytes(mat_image->data.ptr, mat_image->step, mat_image->width,
                             mat_image->height, ipl_depth, channels, m_strm);
	LOGV("Load loadImageBytes.");


	imageSize = m_strm->GetSize();
	res_array = env->NewBooleanArray(imageSize);
	LOGV("Load NewByteArray.");
    if (res_array == 0) {
        return 0;
    }
    env->SetBooleanArrayRegion(res_array, 0, imageSize, (jboolean*)m_strm->GetByte());
	LOGV("Load SetBooleanArrayRegion.");




	cvReleaseImage( &backgroundImage );
	cvReleaseImage( &grayImage );
	cvReleaseImage( &differenceImage );
	cvReleaseImage( &hsvImage );
	cvReleaseImage( &hueImage );
	cvReleaseImage( &saturationImage );
	cvReleaseImage( &valueImage );
	cvReleaseImage( &thresholdImage1 );
	cvReleaseImage( &thresholdImage2 );
	cvReleaseImage( &thresholdImage3 );
	cvReleaseImage( &faceImage );
	cvReleaseImage( &frameImage );
	m_strm->Close();
	SAFE_DELETE(m_strm);

	return res_array;

}
#endif

