
#include "cvjni.h"
#include "detectHand.h"
#define DEBUG

const int HAND_NUM = 3;
const int NONE = -1;
const int GUU = 0;
const int CHOKI = 1;
const int PAA = 2;

const int CELL_X = 5;	//1セル内の横画素数
const int CELL_Y = 5;	//1セル内の縦画素数
const int CELL_BIN = 9;	//輝度勾配方向の分割数（普通は９）
const int BLOCK_X = 3;	//1ブロック内の横セル数
const int BLOCK_Y = 3;	//1ブロック内の縦セル数

const int RESIZE_X = 40;	//リサイズ後の画像の横幅
const int RESIZE_Y = 40;	//リサイズ後の画像の縦幅

//以下のパラメータは、上の値から自動的に決まります

const int CELL_WIDTH = RESIZE_X / CELL_X;	//セルの数（横）
const int CELL_HEIGHT = RESIZE_Y / CELL_Y;	//セルの数（縦）
const int BLOCK_WIDTH = CELL_WIDTH - BLOCK_X + 1;		//ブロックの数（横）
const int BLOCK_HEIGHT = CELL_HEIGHT - BLOCK_Y + 1;	//ブロックの数（縦）

const int BLOCK_DIM	= BLOCK_X * BLOCK_Y * CELL_BIN;		//１ブロックの特徴量次元
const int TOTAL_DIM	= BLOCK_DIM * BLOCK_WIDTH * BLOCK_HEIGHT;	//HoG全体の次元

const int H_BINS = 18;
const int S_BINS = 25;

double* featHand[HAND_NUM] = {NULL, NULL, NULL};
double vMin, vMax;
CvHistogram* skinHist;

//extern double* feat_hand[3];

static void dumpToFile(const char *fname, char *buf, uint32_t size)
{
    int nw, cnt = 0;
    uint32_t written = 0;

    LOGD("opening file [%s]\n", fname);
    int fd = open(fname, O_RDWR | O_CREAT);
    if (fd < 0) {
        LOGE("failed to create file [%s]: %s", fname, strerror(errno));
        return;
    }

    LOGD("writing %d bytes to file [%s]\n", size, fname);
    while (written < size) {
        nw = ::write(fd,
                     buf + written,
                     size - written);
        if (nw < 0) {
            LOGE("failed to write to file [%s]: %s",
                 fname, strerror(errno));
            break;
        }
        written += nw;
        cnt++;
    }
    LOGD("done writing %d bytes to file [%s] in %d passes\n",
         size, fname, cnt);
    ::close(fd);
}

static inline suseconds_t get_usecs(void)
{
       struct timeval tv;
       gettimeofday(&tv, 0);
       return tv.tv_sec * (suseconds_t) 1000000 + tv.tv_usec;
}


// // Helper to load pixels from a byte stream received over a socket.
 static IplImage* loadPixels16(jbyte* pixels, int width, int height) {

 	int x, y;
 	IplImage *img = cvCreateImage(cvSize(width, height), IPL_DEPTH_8U, 3);

 	for ( y = 0; y < height; y++ ) {
         for ( x = 0; x < width; x++) {
        	 unsigned short col0 = pixels[ (y * width + x)*2+0];
        	 unsigned short col1 = pixels[ (y * width + x)*2+1];
        	 unsigned short color = (col0 << 8) | col1;

        	 // blue
             IMAGE( img, x, y, 0 ) = (unsigned short)((color >> 0) & 0x1f);
             // green
             IMAGE( img, x, y, 1 ) = (unsigned short)((color >> 5) & 0x3f);
             // red
             IMAGE( img, x, y, 2 ) = (unsigned short)((color >> 11) & 0x1f);
         }
     }

 	return img;
 }


 // Helper to load pixels from a byte stream received over a socket.
 static IplImage* loadPixels32(jbyte* pixels, int width, int height) {

 	int x, y, pos, int_size = sizeof(int);
 	IplImage *img = cvCreateImage(cvSize(width, height), IPL_DEPTH_8U, 3);

 	for ( y = 0; y < height; y++ ) {
 		pos = y * width * int_size;
        for ( x = 0; x < width; x++, pos += int_size ) {
             // blue
             IMAGE( img, x, y, 0 ) = pixels[pos+3];
             // green
             IMAGE( img, x, y, 1 ) = pixels[pos+2];
             // red
             IMAGE( img, x, y, 2 ) = pixels[pos+1];
         }
     }

 	return img;
 }

IplImage* getIplImageFromByteArray(JNIEnv* env,
                                   jbyte* src_pixels,
                                   jint width,
                                   jint height,
                                   jint colorByteNum) {

	if (src_pixels == NULL) {
		LOGE("Error getting byte array of pixels.");
		return NULL;
	}

    IplImage *image = NULL;
    switch(colorByteNum){
    case 2:
        image = loadPixels16(src_pixels, width, height);
    	break;
    case 4:
#ifdef DEBUG
//    	dumpToFile("/tmp/preview01.bin", (char*)src_pixels, width * height * 4);
#endif
        image = loadPixels32(src_pixels, width, height);
#ifdef DEBUG
//        dumpToFile("/tmp/preview02.bin", image->imageData, image->width * image->height * 3);
        cvSaveImage("/tmp/preview02.bmp",image);
#endif
    	break;

    }

	if(image == NULL) {
		LOGE("Error loading pixel array.");
		return NULL;
	}

	return image;
}

// ヒストグラム作成
void getHistgram(IplImage* src, CvHistogram** hist, double* vmin, double* vmax)
{
	CvSize size = cvGetSize(src);

	IplImage* hsv = cvCreateImage( size, IPL_DEPTH_8U, 3 );
	cvCvtColor( src, hsv, CV_BGR2HSV );

	IplImage* h_plane  = cvCreateImage( size, IPL_DEPTH_8U, 1 );
	IplImage* s_plane  = cvCreateImage( size, IPL_DEPTH_8U, 1 );
	IplImage* v_plane  = cvCreateImage( size, IPL_DEPTH_8U, 1 );
	IplImage* planes[] = { h_plane, s_plane };
	cvCvtPixToPlane( hsv, h_plane, s_plane, v_plane, 0 );

	int    hist_size[] = { H_BINS, S_BINS };
	float  h_ranges[]  = { 0, 180 };          // hue is [0,180]
	float  s_ranges[]  = { 0, 255 };
	float* ranges[]    = { h_ranges, s_ranges };
	*hist = cvCreateHist(2,
						hist_size,
						CV_HIST_ARRAY,
						ranges,
						1);

	cvCalcHist( planes, *hist, 0, 0 );
	cvReleaseImage(&h_plane);
	cvReleaseImage(&s_plane);

	cvMinMaxLoc(v_plane, vmin, vmax);
	*vmin = 1;
	cvReleaseImage(&v_plane);
}

void getHandArea (IplImage* imgHsv,
				   IplImage** imgHandArea,
				   CvHistogram* hist,
				   double* vmin,
				   double* vmax)
{
	CvMemStorage* storage = cvCreateMemStorage(0);

	CvSize size = cvGetSize(imgHsv);
	IplImage* imgDst = cvCreateImage(size, IPL_DEPTH_8U, 1);
	cvZero(imgDst);

	IplImage* imgBackproj = cvCreateImage(size, IPL_DEPTH_8U, 1);
	IplImage* imgMask = cvCreateImage(size, IPL_DEPTH_8U, 1);

	{
		IplImage* h_plane = cvCreateImage(size, IPL_DEPTH_8U,1);
		IplImage* s_plane = cvCreateImage(size, IPL_DEPTH_8U,1);
		IplImage* v_plane = cvCreateImage(size, IPL_DEPTH_8U,1);
		IplImage* planes[] = {h_plane, s_plane};

		cvCvtPixToPlane(imgHsv, h_plane, s_plane, v_plane, NULL);
		cvCalcBackProject(planes, imgBackproj, hist);

		cvThreshold(v_plane, imgMask, *vmin, *vmax, CV_THRESH_BINARY);
		cvAnd(imgBackproj, imgMask, imgBackproj);

		cvReleaseImage(&h_plane);
		cvReleaseImage(&s_plane);
		cvReleaseImage(&v_plane);
	}

	CvSeq* contours = NULL;
	{

		cvThreshold(imgBackproj, imgDst, 10,255, CV_THRESH_BINARY);
//		cvThreshold(imgBackproj, imgDst, 40,255, CV_THRESH_BINARY);

		//各種のモルフォロジー演算を実行する
//		IplConvKernel* element = cvCreateStructuringElementEx(5, 5, 3, 3, CV_SHAPE_RECT, NULL);
//		cvMorphologyEx(imgDst, imgDst, NULL, element, CV_MOP_OPEN, 1);

		cvErode(imgDst, imgDst,NULL,1);
		cvDilate(imgDst, imgDst,NULL,1);


		cvFindContours(imgDst, storage, &contours);

		CvSeq* hand_ptr = NULL;
		double maxArea = -1;
		for (CvSeq* c= contours; c != NULL; c = c->h_next){
			double area = abs(cvContourArea(c, CV_WHOLE_SEQ));
			if (maxArea < area)
			{
				maxArea = area;
				hand_ptr = c;
			}
		}

		cvZero(imgDst);

//		cvDrawContours(imgDst, contours, cvScalarAll(255), cvScalarAll(0),200);

		if (hand_ptr == NULL){
			LOGV("HandArea not found");

			*imgHandArea = cvCreateImage(cvSize(1, 1), IPL_DEPTH_8U, 1);
		} else {
			hand_ptr->h_next = NULL;
	//		hand_ptr = cvApproxPoly(hand_ptr, sizeof(CvContour), storage, CV_POLY_APPROX_DP,3,1);

			cvDrawContours(imgDst, hand_ptr, cvScalarAll(255), cvScalarAll(0),100);
			CvRect rect= cvBoundingRect(hand_ptr,0);
#ifdef DEBUG
			LOGV("HandArea found:x=%d,y=%d,width=%d,height=%d",rect.x, rect.y, rect.width, rect.height);
#endif
			cvSetImageROI(imgDst, rect);
			*imgHandArea = cvCreateImage(cvSize(rect.width, rect.height), IPL_DEPTH_8U, 1);
			cvCopy(imgDst, *imgHandArea);

			cvResetImageROI(imgDst);
		}
//		cvRectangle(imgDst, cvPoint(rect.x,rect.y), cvPoint(rect.x+rect.width,rect.y+rect.height),cvScalar(255,255,0));
	}

	cvReleaseImage(&imgBackproj);
	cvReleaseImage(&imgMask);

	cvReleaseImage(&imgDst);

	cvReleaseMemStorage(&storage);
}

void GetHoG(IplImage* src, double* feat)
{

	//はじめに、画像サイズを変換（CELL_X/Yの倍数とする）
	IplImage* img = cvCreateImage(cvSize(RESIZE_X,RESIZE_Y), 8, 1);
	cvResize(src, img);

	//画像サイズ
	const int width = RESIZE_X;
	const int height = RESIZE_Y;

	//各セルの輝度勾配ヒストグラム
	double hist[CELL_WIDTH][CELL_HEIGHT][CELL_BIN];
	memset(hist, 0, CELL_WIDTH*CELL_HEIGHT*CELL_BIN*sizeof(double));

	//各ピクセルにおける輝度勾配強度mと勾配方向degを算出し、ヒストグラムへ
	//※端っこのピクセルでは、計算しない
	for(int y=0; y<height; y++){
		for(int x=0; x<width; x++){
			if(x==0 || y==0 || x==width-1 || y==height-1){
				continue;
			}
			double dx = img->imageData[y*img->widthStep+(x+1)] - img->imageData[y*img->widthStep+(x-1)];
			double dy = img->imageData[(y+1)*img->widthStep+x] - img->imageData[(y-1)*img->widthStep+x];
			double m = sqrt(dx*dx+dy*dy);
			double deg = (atan2(dy, dx)+CV_PI) * 180.0 / CV_PI;	//0.0～360.0の範囲になるよう変換
			int bin = CELL_BIN * deg/360.0;
			if(bin < 0) bin=0;
			if(bin >= CELL_BIN) bin = CELL_BIN-1;
			hist[(int)(x/CELL_X)][(int)(y/CELL_Y)][bin] += m;
		}
	}

	//ブロックごとで正規化
	for(int y=0; y<BLOCK_HEIGHT; y++){
		for(int x=0; x<BLOCK_WIDTH; x++){

			//このブロックの特徴ベクトル（次元BLOCK_DIM=BLOCK_X*BLOCK_Y*CELL_BIN）
			double vec[BLOCK_DIM];
			memset(vec, 0, BLOCK_DIM*sizeof(double));
			for(int j=0; j<BLOCK_Y; j++){
				for(int i=0; i<BLOCK_X; i++){
					for(int d=0; d<CELL_BIN; d++){
						int index = j*(BLOCK_X*CELL_BIN) + i*CELL_BIN + d;
						vec[index] = hist[x+i][y+j][d];
					}
				}
			}

			//ノルムを計算し、正規化
			double norm = 0.0;
			for(int i=0; i<BLOCK_DIM; i++){
				norm += vec[i]*vec[i];
			}
			for(int i=0; i<BLOCK_DIM; i++){
				vec[i] /= sqrt(norm + 1.0);
			}

			//featに代入
			for(int i=0; i<BLOCK_DIM; i++){
				int index = y*BLOCK_WIDTH*BLOCK_DIM + x*BLOCK_DIM + i;
				feat[index] = vec[i];
			}
		}
	}
	cvReleaseImage(&img);
	return;
}

/******************************************
 ２つの特徴ベクトルの距離を計算する
 feat1,feat2：HoG特徴ベクトル
 *******************************************/
//double GetDistance(double* feat1, double* feat2)
//{
//	double dist = 0.0;
//	for(int i=0; i<TOTAL_DIM; i++){
//		dist += fabs(feat1[i] - feat2[i])*fabs(feat1[i] - feat2[i]);
//	}
//	return sqrt(dist);
//}

//void getHandsDistance(IplImage* handImg, double** _featHand, double* dist)
//{
//
//	double featSrc[TOTAL_DIM];
//
//	GetHoG(handImg, featSrc);
//	for (int i = 0; i < HAND_NUM; i++){
//		dist[i] = GetDistance(featSrc, _featHand[i]);
//	}
//}


//JNIEXPORT
//jboolean
//JNICALL
//Java_com_example_hands_DetectHand_initFeatHand( JNIEnv* env, jobject thiz, int _no, jdoubleArray _featData)
//{
//    if ((_no < 0) || (HAND_NUM <= _no))
//        return JNI_FALSE;
//
//    jsize  length = env->GetArrayLength(_featData);
//
//    if (featHand[_no] != NULL){
//        free(featHand[_no]);
//            featHand[_no] = NULL;
//        }
//    jdouble* srcBuf = env->GetPrimitiveArrayCritical(_featData,0);
//    if (srcBuf == NULL){
//
//    }
//
//    jdouble* dstBuf = (jdouble*)malloc(length*sizeof(double));
//    memcpy(dstBuf, srcBuf, length);
//	featHand[_no] = dstBuf;
//
//    env->ReleasePrimitiveArrayCritical(_featData, srcBuf, 0);
//
//    return JNI_TRUE;
//}

JNIEXPORT
jint
JNICALL
Java_com_example_hands_DetectHand_getFeatSize( JNIEnv* env, jobject thiz)
{
	return TOTAL_DIM;
}

#undef DISP_TIME
/*
 * ハンドジェスチャーを判別するところ
 */
JNIEXPORT
void
JNICALL
Java_com_example_hands_DetectHand_getFeatParams( JNIEnv* env, jobject thiz, jobject imageData, jint width, jint height, jdoubleArray ary)
{
	IplImage* handAreaImg = NULL;

#ifdef DISP_TIME
	suseconds_t t0, t1, t2;
	t0 = get_usecs();
	LOGD("begin getFestParams");
#endif

	CvSize screenSize = cvSize(width,height);

#ifdef DISP_TIME
	t1 = get_usecs();
	LOGD("getFastParams 1 : %d uS",t1-t0);
	t0 = t1;
#endif

	jbyte *buffer = (jbyte*)env->GetDirectBufferAddress(imageData);

#ifdef DISP_TIME
	t1 = get_usecs();
	LOGD("getFastParams 2 : %d uS",t1-t0);
	t0 = t1;
#endif

	IplImage* img = getIplImageFromByteArray(env, buffer, width, height, 2);
//    if (img == NULL) {
//        jniThrowException(env, "java/lang/OutOfMemoryError", "img is NULL");
//        return;
//    }

	IplImage* hsv   = cvCreateImage(screenSize, IPL_DEPTH_8U, 3);
//    if (hsv == NULL) {
//        jniThrowException(env, "java/lang/OutOfMemoryError", "hsv is NULL");
//        cvReleaseImage(&img);
//        return;
//    }

#ifdef DISP_TIME
	t1 = get_usecs();
	LOGD("getFastParams 3 : %d uS",t1-t0);
	t0 = t1;
#endif

	cvCvtColor( img, hsv, CV_BGR2HSV );

#ifdef DISP_TIME
	t1 = get_usecs();
	LOGD("getFastParams 4 : %d uS",t1-t0);
	t0 = t1;
#endif

	getHandArea(hsv, &handAreaImg, skinHist, &vMin, &vMax);

#ifdef DEBUG
	cvSaveImage("/tmp/handarea.bmp",handAreaImg);
#endif

#ifdef DISP_TIME
	t1 = get_usecs();
	LOGD("getFastParams 5 : %d uS",t1-t0);
	t0 = t1;
#endif

	jdouble* retvalArray=(jdouble*)(env->GetPrimitiveArrayCritical(ary,0));
//	jint*  sizeArray=(jint*)(env->GetPrimitiveArrayCritical(handsizeAry,0));

	CvSize size = cvGetSize(handAreaImg);
//	sizeArray[0] = size.width;
//	sizeArray[1] = size.height;

	GetHoG(handAreaImg, retvalArray);

#ifdef DISP_TIME
	t1 = get_usecs();
	LOGD("getFastParams 6 : %d uS",t1-t0);
	t0 = t1;
#endif

//	double featSrc[TOTAL_DIM];
//	GetHoG(handAreaImg, featSrc);
//	for (int i = 0; i < HAND_NUM; i++){
//		retvalArray[i] = GetDistance(featSrc, feat_hand[i]);
//	}

//    env->ReleasePrimitiveArrayCritical(handsizeAry, sizeArray, 0);
    env->ReleasePrimitiveArrayCritical(ary, retvalArray, 0);

	cvReleaseImage(&handAreaImg);
	cvReleaseImage(&img);
	cvReleaseImage(&hsv);

#ifdef DISP_TIME
	t1 = get_usecs();
	LOGD("getFastParams 7 : %d uS",t1-t0);
	t0 = t1;
#endif

}

JNIEXPORT
jboolean
JNICALL
Java_com_example_hands_DetectHand_setSkinHistgram( JNIEnv* env, 
                                               jobject thiz,
                                               jobject imageData,
                                               jint width,
                                               jint height)
{
	jbyte *buffer = (jbyte*)env->GetDirectBufferAddress(imageData);
	IplImage* histImg = getIplImageFromByteArray(env, buffer, width, height, 4);
    if (histImg != NULL){
        getHistgram(histImg, &skinHist, &vMin, &vMax);
#ifdef DEBUG
        LOGD("vMin=%f",vMin);
        LOGD("vMax=%f",vMax);
#endif
    }

    return JNI_TRUE;
}


//int uinput_create()
//{
//	struct uinput_dev dev;
//	int fd, aux;
//
//	LOGD("begin initTouchscreen!");
//
//	fd = open("/dev/uinput", O_RDWR);
//	LOGD("/dev/uinput=%d",fd);
//	if (fd < 0) {
//		fd = open("/dev/input/uinput", O_RDWR);
//		if (fd < 0) {
//			fd = open("/dev/misc/uinput", O_RDWR);
//			if (fd < 0) {
//				return -1;
//			}
//		}
//	}
//
//	memset(&dev, 0, sizeof(dev));
//	strcpy(dev.name, DEV_NAME);
//	write(fd, &dev, sizeof(dev));
//
//	ioctl(fd, UI_SET_EVBIT, EV_ABS);
//	ioctl(fd, UI_SET_ABSBIT, ABS_X);
//	ioctl(fd, UI_SET_ABSBIT, ABS_Y);
//	ioctl(fd, UI_SET_EVBIT, EV_KEY);
//	ioctl(fd, UI_SET_KEYBIT, BTN_TOUCH);
//
//	ioctl(fd, UI_DEV_CREATE, 0);
//
//	return fd;
//}

//void send_event(int fd, uint16_t type, uint16_t code, int32_t value)
//{
//	struct uinput_event event;
//	int err;
//
//	memset(&event, 0, sizeof(event));
//	event.type	= type;
//	event.code	= code;
//	event.value	= value;
//
//	err = write(fd, &event, sizeof(event));
//}

//void initFinterTip()
//{
//    fingerTip[BLUE].minH = 25; fingerTip[BLUE].maxH = 90;
//    fingerTip[BLUE].minS = 30; fingerTip[BLUE].maxS = 140;
//    fingerTip[BLUE].minV = 40; fingerTip[BLUE].maxV = 215;
//
//    fingerTip[ORANGE].minH = 111; fingerTip[ORANGE].maxH = 115;
//    fingerTip[ORANGE].minS = 210; fingerTip[ORANGE].maxS = 255;
//    fingerTip[ORANGE].minV = 30;  fingerTip[ORANGE].maxV = 255;
//}


/*
 * Write an absolute (touch screen) event.
 */
//int sendAbsMovement(int x, int y)
//{
//    struct uinput_event iev;
//    int actual;
//
//	LOGD("begin sendAbsMovement");
//
//	memset(&iev, 0, sizeof(iev));
//    iev.type = EV_ABS;
//    iev.code = ABS_X;
//    iev.value = x;
//
//    actual = write(fd, &iev, sizeof(iev));
//    if (actual != (int) sizeof(iev)) {
//        return -1;
//    }
//
//	memset(&iev, 0, sizeof(iev));
//    iev.code = ABS_Y;
//    iev.value = y;
//
//    actual = write(fd, &iev, sizeof(iev));
//    if (actual != sizeof(iev)) {
////        wsLog("WARNING: send abs movement event partial Y write (%d of %d)\n", actual, sizeof(iev));
//        return -1;
//    }
//
//	LOGD("end sendAbsMovement");
//
//    return 0;
//}

/*
 * Write an absolute (touch screen) event.
 */
//int sendAbsButton(int x, int y, int isDown)
//{
//    struct uinput_event iev;
//    int actual;
//
//	LOGD("begin sendAbsButton");
//
////    wsLog("absButton x=%d y=%d down=%d\n", x, y, isDown);
//
////    gettimeofday(&iev.time, NULL);
//    iev.type = EV_KEY;
//    iev.code = BTN_TOUCH;
//    iev.value = (isDown != 0) ? 1 : 0;
//
//    actual = write(fd, &iev, sizeof(iev));
//    if (actual !=  sizeof(iev)) {
////        wsLog("WARNING: send touch event partial write (%d of %d)\n", actual, sizeof(iev));
//        return -1;
//    }
//
//	LOGD("end sendAbsButton");
//
//    return 0;
//}

/*
 * Not quite sure what this is for, but the emulator does it.
 */
//int sendAbsSyn()
//{
//    struct uinput_event iev;
//    int actual;
//
//	LOGD("begin sendAbsSyn");
//
//    iev.type = EV_SYN;
//    iev.code = 0;
//    iev.value = 0;
//
//    actual = write(fd, &iev, sizeof(iev));
//    if (actual != sizeof(iev)) {
////        wsLog("WARNING: send abs movement syn (%d of %d)\n", actual, sizeof(iev));
//        return -1;
//    }
//
//	LOGD("end sendAbsSyn");
//
//    return 0;
//}


//#define	YCbCrtoR(Y,Cb,Cr)	(1000*Y + 1371*(Cr-128))/1000
//#define	YCbCrtoG(Y,Cb,Cr)	(1000*Y - 336*(Cb-128) - 698*(Cr-128))/1000
//#define	YCbCrtoB(Y,Cb,Cr)	(1000*Y + 1732*(Cb-128))/1000

//#define	YCbCrtoR(Y,Cb,Cr)	((298*Y + 408*(Cr-128)) / 256)
//#define	YCbCrtoG(Y,Cb,Cr)	((298*Y - 208*(Cb-128) - 100*(Cr-128)) / 256)
//#define	YCbCrtoB(Y,Cb,Cr)	((298*Y + 516*(Cb-128)) / 256)

//void convertPixelYCbCr422ToRGB(jbyte y0, jbyte y1, jbyte cb0, jbyte cr0, jint* dst_buf)
//{
//	// bit order is
//	// YCbCr = [Cr0 Y1 Cb0 Y0], RGB=[R1,G1,B1,R0,G0,B0].
//
//	jint r0, g0, b0, r1, g1, b1;
//	jint rgb0, rgb1;
//	jint rgb;
//
//	#if 1 // 4 frames/s @192MHz, 12MHz ; 6 frames/s @450MHz, 12MHz
//	r0 = YCbCrtoR(y0, cb0, cr0);
//	g0 = YCbCrtoG(y0, cb0, cr0);
//	b0 = YCbCrtoB(y0, cb0, cr0);
//	r1 = YCbCrtoR(y1, cb0, cr0);
//	g1 = YCbCrtoG(y1, cb0, cr0);
//	b1 = YCbCrtoB(y1, cb0, cr0);
//	#endif
//
//	if (r0>255 ) r0 = 255;
//	if (r0<0) r0 = 0;
//	if (g0>255 ) g0 = 255;
//	if (g0<0) g0 = 0;
//	if (b0>255 ) b0 = 255;
//	if (b0<0) b0 = 0;
//
//	if (r1>255 ) r1 = 255;
//	if (r1<0) r1 = 0;
//	if (g1>255 ) g1 = 255;
//	if (g1<0) g1 = 0;
//	if (b1>255 ) b1 = 255;
//	if (b1<0) b1 = 0;
//
//	// ARGB 32bitbit format
//	dst_buf[0] = (((jshort)r0)<<24) | (((jshort)g0)<<16) | b0;	//RGB888.
//	dst_buf[1] = (((jshort)r1)<<24) | (((jshort)g1)<<16) | b1;	//RGB888.
//
//}

//inline int clamp(int x, int a, int b)
//{
//    return x < a ? a : (x > b ? b : x);
//}



//void setPixelRGB565ToIplImage(jshort* col, IplImage* image, int x, int y)
//{
//	int r0, g0, b0;
//
//    r0 = ((*col >> (5+6)) & 0x1f) << 3;
//    g0 = ((*col >> 5) & 0x3f) << 2;
//    b0 = (*col & 0x01f) << 3;
//
//    image->imageData[image->widthStep * y + x * 3 + 0] = b0;
//    image->imageData[image->widthStep * y + x * 3 + 1] = g0;
//    image->imageData[image->widthStep * y + x * 3 + 2] = r0;
//}



//void setPixelYCbCr422ToRGB(jbyte* yuv422, IplImage* image, int x, int y)
//{
//	// bit order is
//	// YCbCr = [Cr0 Y1 Cb0 Y0], RGB=[R1,G1,B1,R0,G0,B0].
//
//    jbyte y0 = yuv422[0];
//    jbyte cb0 = yuv422[1];
//    jbyte y1 = yuv422[2];
//    jbyte cr0 = yuv422[3];
////    jbyte cr0 = yuv422[0];
////    jbyte y1 = yuv422[1];
////    jbyte cb0 = yuv422[2];
////    jbyte y0 = yuv422[3];
//
//	int r0, g0, b0, r1, g1, b1;
//
//    r0 = YCbCrtoR(y0, cb0, cr0);
//	g0 = YCbCrtoG(y0, cb0, cr0);
//	b0 = YCbCrtoB(y0, cb0, cr0);
//
//	r1 = YCbCrtoR(y1, cb0, cr0);
//	g1 = YCbCrtoG(y1, cb0, cr0);
//	b1 = YCbCrtoB(y1, cb0, cr0);
//
//    image->imageData[image->widthStep * y + x * 3 + 0] = clamp(b0, 0, 255);
//    image->imageData[image->widthStep * y + x * 3 + 1] = clamp(g0, 0, 255);
//    image->imageData[image->widthStep * y + x * 3 + 2] = clamp(r0, 0, 255);
//
//    x++;
//    image->imageData[image->widthStep * y + x * 3 + 0] = clamp(b1, 0, 255);
//    image->imageData[image->widthStep * y + x * 3 + 1] = clamp(g1, 0, 255);
//    image->imageData[image->widthStep * y + x * 3 + 2] = clamp(r1, 0, 255);
//
//}


//void GetMaskHSV(IplImage* src, IplImage* mask, TypeHSV* hsv1,TypeHSV* hsv2)
//{
//	int x = 0, y = 0;
//	uchar H, S, V;
//
//	CvPixelPosition8u pos_src, pos_dst;
//	uchar* p_src;
//	uchar* p_dst;
//	IplImage* tmp;
//
//	tmp = cvCreateImage(cvGetSize(src), IPL_DEPTH_8U, 3);
//
//	cvCvtColor(src, tmp, CV_RGB2HSV);
//
//	CV_INIT_PIXEL_POS(pos_src, (unsigned char*) tmp->imageData,
//					  tmp->widthStep,cvGetSize(tmp), x, y, tmp->origin);
//
//	CV_INIT_PIXEL_POS(pos_dst, (unsigned char*) mask->imageData,
//					  mask->widthStep, cvGetSize(mask), x, y, mask->origin);
//
//	for(y = 0; y < tmp->height; y++) {
//		for(x = 0; x < tmp->width; x++) {
//			p_src = CV_MOVE_TO(pos_src, x, y, 3);
//			p_dst = CV_MOVE_TO(pos_dst, x, y, 3);
//
//			H = p_src[0];
//			S = p_src[1];
//			V = p_src[2];
//
//			p_dst[0] = 0;
//			p_dst[1] = 0;
//			p_dst[2] = 0;
//			if(hsv1->minH <= H && H <= hsv1->maxH){
//			   if (hsv1->minS <= S && S <= hsv1->maxS){
//				   if (hsv1->minV <= V && V <= hsv1->maxV){
//					   p_dst[0] = 255;
//					   p_dst[1] = 255;
//					   p_dst[2] = 255;
//				   }
//			   }
//			}
//            if (!p_dst[0] == 0){
//                if(hsv2->minH <= H && H <= hsv2->maxH){
//                    if (hsv2->minS <= S && S <= hsv2->maxS){
//                        if (hsv2->minV <= V && V <= hsv2->maxV){
//                            p_dst[0] = 255;
//                            p_dst[1] = 255;
//                            p_dst[2] = 255;
//
//                        }
//                    }
//                }
//            }
//
//		}
//	}
//
////	if(erosions > 0)  cvErode(mask, mask, 0, erosions);
////	if(dilations > 0) cvDilate(mask, mask, 0, dilations);
//
//	cvReleaseImage(&tmp);
//}


// Given an integer array of image data, load an IplImage.
// It is the responsibility of the caller to release the IplImage.
//IplImage* getIplImageFromByteArray(JNIEnv* env,
//								  jbyteArray array_data,
//								  jint width,
//								  jint height) {
//
//	// Load Image
//	jbyte* byte_pixels = env->GetByteArrayElements(array_data, 0);
//	if (byte_pixels == 0) {
//		LOGE("Error getting byte array of pixels.");
//		return 0;
//	}
//
//    IplImage *image = cvCreateImage(cvSize(width, height), IPL_DEPTH_8U, 3);
//
//  	int x,y,pos=0;
//    int rgb565_size = 2;
//    for ( y = 0; y < height; y++ ) {
//         pos = y * width * rgb565_size;
//         for ( x = 0; x < width; x++, pos += rgb565_size ) {
//             setPixelRGB565ToIplImage((jshort*)&byte_pixels[pos], image, x,y);
//         }
//     }
//
//#ifdef DEBUG
//    dump_to_file("/tmp/preview888.rgb", image->imageData, image->width * image->height * 3);
//#endif
//
//    env->ReleaseByteArrayElements(array_data, byte_pixels, 0);
//	if(image == 0) {
//		LOGE("Error loading pixel array.");
//		return 0;
//	}
//
//	return image;
//}

//JNIEXPORT
//void
//JNICALL
//Java_com_example_hands_FakeTouchscreen_touchInner( JNIEnv* env,
//													jobject thiz, jint action, jint xpos,  jint ypos)
//{
//    if (action == TouchDown) {
//    	LOGD("TouchDown : x=%d : y=%d",xpos,ypos);
//        sendAbsMovement(xpos, ypos);
//        sendAbsButton(xpos, ypos, 1);
//        sendAbsSyn();
//    } else if (action == TouchUp) {
//    	LOGD("TouchUp");
//        sendAbsButton(xpos, ypos, 0);
//        sendAbsSyn();
//    } else if (action == TouchDrag) {
//        sendAbsMovement(xpos, ypos);
//        sendAbsSyn();
//    }
//}

//JNIEXPORT
//void
//JNICALL
//Java_com_example_hands_FakeTouchscreen_setFingerTipColorInner( JNIEnv* env,	jobject thiz,
//													jint no, jint hMin, jint hMax, jint sMin, jint sMax, jint vMin, jint vMax)
//{
//	fingerTip[no].maxH = hMax;
//	fingerTip[no].minH = hMin;
//	fingerTip[no].maxS = sMax;
//	fingerTip[no].minS = hMin;
//	fingerTip[no].maxV = vMax;
//	fingerTip[no].minV = vMin;
//}

//JNIEXPORT
//void
//JNICALL
//Java_com_example_hands_FakeTouchscreen_setResolutionInner( JNIEnv* env,	jobject thiz, jint w, jint h)
//{
//	width = w;
//	height = h;
//}


//JNIEXPORT jintArray
//Java_com_example_hands_FakeTouchscreenService_getPointTouchscreenInner( JNIEnv* env,
//													jobject thiz)
//{
//	return NULL;
//}

//void initTmpRegion(){
//    int i;
//    for (i=0; i < REGION_MAX; i++)
//        tmpRegion[i] = -1;
//}



//JNIEXPORT
//jint
//JNICALL
//Java_com_example_hands_FakeTouchscreen_initInner( JNIEnv* env,
//													jobject thiz )
//{
//	fd = uinput_create();
//	if (fd < 0) {
//		return -1;
//	}
//    initFinterTip();
//    initTmpRegion();
//
//	return 1;
//}

//JNIEXPORT
//void
//JNICALL
//Java_com_example_hands_FakeTouchscreen_closeInner( JNIEnv* env,
//													jobject thiz )
//{
//	close(fd);
//}





//JNIEXPORT
//jintArray
//JNICALL
//Java_com_example_hands_FakeTouchscreen_getRegionInner(JNIEnv* env,
//                                                                jobject thiz){
//
////	tmpRegion[0] = 10;
//// 	tmpRegion[1] = 10;
//// 	tmpRegion[2] = 20;
//// 	tmpRegion[3] = 20;
//// 	tmpRegion[4] = -1;
//
//    // Copy region to fingerTipRegion
//	jintArray fingerTipRegion;
//	fingerTipRegion = env->NewIntArray(REGION_MAX);
//
//	// move from the temp structure to the java structure
//
//	env->SetIntArrayRegion(fingerTipRegion, 0, REGION_MAX, tmpRegion);
//	return fingerTipRegion;
//}


#define DISP_TIME


// JNIEXPORT
// void
// JNICALL
// Java_com_example_hands_FakeTouchscreen_setSourceImageInner(JNIEnv* env,
// 										jobject thiz,
// 										jbyteArray src_frame, //	jintArray current_frame,
// 										jint width,
// 										jint height) {


// 	LOGD("begin setSourceImageInner");

//     IplImage * draw_image  = cvCreateImage(cvSize (width, height), IPL_DEPTH_8U, 3);
//  	IplImage * current_frame = getIplImageFromByteArray(env, src_frame, width, height);

//     cvFlip (current_frame, draw_image, 1);
//     IplImage* mask = current_frame;

//     // make mask
//  	GetMaskHSV(draw_image, mask, &fingerTip[BLUE],&fingerTip[ORANGE]);
// 	cvReleaseImage( &draw_image );

//   	IplImage *chB=cvCreateImage(cvGetSize(mask),8,1);
//   	IplImage *labelImg = cvCreateImage(cvGetSize(chB),IPL_DEPTH_LABEL,1);

//   	cvSplit(mask,chB,NULL,NULL,NULL);

//   	CvBlobs blobs;
//   	cvLabel(chB, labelImg, blobs);

//     initTmpRegion();

//      int i = 0;
//   	for (CvBlobs::const_iterator it=blobs.begin(); it!=blobs.end(); ++it)
//   	{
//   		CvBlob *blob=(*it).second;

//   		if ( (blob->maxx - blob->minx) > MIN_SIZE && (blob->maxy - blob->miny) > MIN_SIZE){
//    			LOGD("!!! blob hit !!!");
            
//               tmpRegion[i++] = blob->minx;
//               tmpRegion[i++] = blob->miny;
//               tmpRegion[i++] = blob->maxx;
//               tmpRegion[i++] = blob->maxy;

//               if (i > REGION_MAX)
//                   break;
//   		}
//   	}

//   	cvReleaseImage( &mask );
//   	cvReleaseImage( &chB );
//   	cvReleaseImage( &labelImg );

// 	LOGD("end setSourceImageInner");

// }

//JNIEXPORT
//void
//JNICALL
//Java_com_example_hands_FakeTouchscreen_setSourceImageInner(JNIEnv* env,
//										jobject thiz,
//										jbyteArray src_frame, //	jintArray current_frame,
//										jint width,
//										jint height) {
//
//#ifdef DISP_TIME
//	suseconds_t t0, t1, t2;
//	t0 = get_usecs();
//	LOGD("begin setSourceImageInner");
//#endif
//
//    IplImage * draw_image  = cvCreateImage(cvSize (width, height), IPL_DEPTH_8U, 3);
//
//#ifdef DISP_TIME
//	t1 = get_usecs();
//	LOGD("SourceImageInner 1 : %d uS",t1-t0);
//	t0 = t1;
//#endif
//
// 	// Load Image
// 	IplImage * current_frame = getIplImageFromByteArray(env, src_frame, width, height);
//
//#ifdef DISP_TIME
//	t1 = get_usecs();
// 	LOGD("SourceImageInner 2 : %d uS",t1-t0);  // second heavy
// 	t0 = t1;
//#endif
//
//    cvFlip (current_frame, draw_image, 1);
//    IplImage* mask = current_frame;
//
//#ifdef DISP_TIME
//	t1 = get_usecs();
//	LOGD("SourceImageInner 3 : %d uS",t1-t0);
//	t0 = t1;
//#endif
//    // make mask
// 	GetMaskHSV(draw_image, mask, &fingerTip[BLUE],&fingerTip[ORANGE]);
//
//#ifdef DISP_TIME
//	t1 = get_usecs();
//	LOGD("SourceImageInner 4 : %d uS",t1-t0); // most heavy
//	t0 = t1;
//#endif
//
//	cvReleaseImage( &draw_image );
//
// 	IplImage *chB=cvCreateImage(cvGetSize(mask),8,1);
// 	IplImage *labelImg = cvCreateImage(cvGetSize(chB),IPL_DEPTH_LABEL,1);
//
// 	cvSplit(mask,chB,NULL,NULL,NULL);
//
// 	CvBlobs blobs;
//// 	cvLabel(chB, labelImg, blobs);
//
//#ifdef DISP_TIME
//	t1 = get_usecs();
//	LOGD("SourceImageInner 5 : %d uS",t1-t0);
//	t0 = t1;
//#endif
//
//    initTmpRegion();
//
////     int i = 0;
////  	for (CvBlobs::const_iterator it=blobs.begin(); it!=blobs.end(); ++it)
////  	{
////  		CvBlob *blob=(*it).second;
//
////  		if ( (blob->maxx - blob->minx) > MIN_SIZE && (blob->maxy - blob->miny) > MIN_SIZE){
////   			LOGD("!!! blob hit !!!");
//
////              tmpRegion[i++] = blob->minx;
////              tmpRegion[i++] = blob->miny;
////              tmpRegion[i++] = blob->maxx;
////              tmpRegion[i++] = blob->maxy;
//
////              if (i > REGION_MAX)
////                  break;
////  		}
////  	}
//
// 	cvReleaseImage( &mask );
// 	cvReleaseImage( &chB );
// 	cvReleaseImage( &labelImg );
//
//#ifdef DISP_TIME
//	t1 = get_usecs();
//	LOGD("SourceImageInner 6 : %d uS",t1-t0);
//	t0 = t1;
//#endif
//	LOGD("finish setSourceImageInner");
//
//}


//#define	YCbCrtoR(Y,Cb,Cr)	(1000*Y + 1371*(Cr-128))/1000
//#define	YCbCrtoG(Y,Cb,Cr)	(1000*Y - 336*(Cb-128) - 698*(Cr-128))/1000
//#define	YCbCrtoB(Y,Cb,Cr)	(1000*Y + 1732*(Cb-128))/1000

//void convertPixcelYCbCr422ToRGB(jbyte y0, jbyte y1, jbyte cb0, jbyte cr0, jint* dst_buf)
//{
//	// bit order is
//	// YCbCr = [Cr0 Y1 Cb0 Y0], RGB=[R1,G1,B1,R0,G0,B0].
//
//	jint r0, g0, b0, r1, g1, b1;
//	jint rgb0, rgb1;
//	jint rgb;
//
//	#if 1 // 4 frames/s @192MHz, 12MHz ; 6 frames/s @450MHz, 12MHz
//	r0 = YCbCrtoR(y0, cb0, cr0);
//	g0 = YCbCrtoG(y0, cb0, cr0);
//    b0 = YCbCrtoB(y0, cb0, cr0);
//	r1 = YCbCrtoR(y1, cb0, cr0);
//	g1 = YCbCrtoG(y1, cb0, cr0);
//	b1 = YCbCrtoB(y1, cb0, cr0);
//	#endif
//
//    r0 = (r0 > 255) ? 255 : ((r0 < 0) ? 0 : r0);
//    g0 = (g0 > 255) ? 255 : ((g0 < 0) ? 0 : g0);
//    b0 = (b0 > 255) ? 255 : ((b0 < 0) ? 0 : b0);
//
//    r1 = (r1 > 255) ? 255 : ((r1 < 0) ? 0 : r1);
//    g1 = (g1 > 255) ? 255 : ((g1 < 0) ? 0 : g1);
//    b1 = (b1 > 255) ? 255 : ((b1 < 0) ? 0 : b1);
//
//// ARGB 32bitbit format
//	dst_buf[0] = (((jshort)r0)<<24) | (((jshort)g0)<<16) | b0;	//RGB888.
//	dst_buf[1] = (((jshort)r1)<<24) | (((jshort)g1)<<16) | b1;	//RGB888.
//
//}

// void convertYCbCrToRGB(jbyte* src_pixels, jint* dst_pixels, int width, int height)
// {
// 	int len = width * height / 2;
//
// 	for (int i = 0; i < len; i++){
//
// 		jbyte y0 =  src_pixels[i*4+0];
// 		jbyte y1 =  src_pixels[i*4+1];
// 		jbyte cb0 = src_pixels[i*4+2];
// 		jbyte cr0 = src_pixels[i*4+3];
//
//		convertPixcelYCbCr422ToRGB(y0, y1, cb0, cr0, (jint*)&dst_pixels[i*2]);
// 	}
// }

// void convertByteToInt(jbyte* srcPixcels, jint* dstPixcels, int width, int height)
// {
//     int len = width * height;
//     for (int i = 0; i < len; i++){
//         jint r = (srcPixcels[i*3+0] & 0xff);
//         jint g = (srcPixcels[i*3+1] & 0xff);
//         jint b = (srcPixcels[i*3+2] & 0xff);
//         dstPixcels[i] = (r << 16) || (g << 8) || (b << 0) ;
//     }
// }

